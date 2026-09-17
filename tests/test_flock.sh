#!/usr/bin/env bash
# Public CLI regressions for flock.
set -o nounset
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/framework.sh"

# An empty shell program is valid and must not dereference a null argv.
timeout 5 "$MODBOX" flock "$TMPDIR/flock-empty.lock" -c ''
rc=$?
if [[ "$rc" == 0 ]]; then
    pass "flock -c empty string exits normally"
else
    fail "flock -c empty string: expected exit 0, got $rc"
fi

# Verify descriptor inheritance through the executed program, with no timing race.
python3 - "$MODBOX" "$TMPDIR" <<'PY'
import os
import subprocess
import sys

binary, directory = sys.argv[1:]
path = os.path.join(directory, 'flock-inherited.lock')
probe = '''import os, sys
path = sys.argv[1]
found = any(os.path.realpath('/proc/self/fd/' + fd) == path
            for fd in os.listdir('/proc/self/fd'))
sys.exit(0 if found else 9)
'''
for options, expected in [([], 0), (['-F'], 0), (['-o'], 9)]:
    result = subprocess.run([binary, 'flock', *options, path,
                             sys.executable, '-c', probe, path], timeout=5)
    assert result.returncode == expected, (options, result.returncode, expected)
PY
if [[ "$?" == 0 ]]; then
    pass "flock inherits descriptor unless --close is specified"
else
    fail "flock descriptor inheritance"
fi

# Conflict: a held lock must reject a second nonblocking attempt with the
# documented conflict code, then become available again after release.
lock="$TMPDIR/flock-conflict.lock"
touch "$lock"
python3 - "$MODBOX" "$lock" <<'PY'
import fcntl
import os
import subprocess
import sys

binary, path = sys.argv[1:3]
fd = os.open(path, os.O_RDWR)
subprocess.run([binary, 'flock', path, 'true'], check=True, timeout=5)

# Hold the lock synchronously in the test process, on a separate open-file
# description from each contender. No scheduler assumptions or sleeps.
fcntl.flock(fd, fcntl.LOCK_EX)

for options, expected in [(['-n'], 1), (['-n', '-E', '7'], 7),
                          (['-w', '0.1'], 1), (['-w', '0.1', '-E', '9'], 9)]:
    result = subprocess.run([binary, 'flock', *options, path, 'true'],
                            timeout=5, capture_output=True, text=True)
    assert result.returncode == expected, (options, result.returncode, expected)

fcntl.flock(fd, fcntl.LOCK_UN)
os.close(fd)
# Lock must be free again after release.
free = subprocess.run([binary, 'flock', '-n', path, 'true'], timeout=5)
assert free.returncode == 0, free.returncode
PY
if [[ "$?" == 0 ]]; then
    pass "flock conflict exits with conflict code; timeout path behaves identically"
else
    fail "flock conflict/timeout handling"
fi

# fd form: locking an inherited descriptor must succeed and unlock cleanly.
python3 - "$MODBOX" "$lock" <<'PY'
import fcntl
import os
import subprocess
import sys

binary, path = sys.argv[1:]
with open(path, 'r+') as owner, open(path, 'r+') as observer:
    def invoke(*options):
        result = subprocess.run([binary, 'flock', *options, str(owner.fileno())],
                                pass_fds=(owner.fileno(),), capture_output=True, timeout=5)
        assert result.returncode == 0, (options, result.returncode, result.stderr)
    invoke('-x')
    try:
        fcntl.flock(observer, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        pass
    else:
        raise AssertionError('fd lock not retained by parent after modbox exits')
    invoke('-u')
    fcntl.flock(observer, fcntl.LOCK_EX | fcntl.LOCK_NB)
    fcntl.flock(observer, fcntl.LOCK_UN)
    invoke('-s')
    fcntl.flock(observer, fcntl.LOCK_SH | fcntl.LOCK_NB)
    invoke('-u')
PY
if [[ "$?" == 0 ]]; then
    pass "flock fd form locks and unlocks an inherited descriptor"
else
    fail "flock fd form"
fi

# Parsing and execution are checked via captured real status and output.
python3 - "$MODBOX" "$lock" <<'PY'
import os
import subprocess
import sys

binary, path = sys.argv[1:]
def run(args):
    return subprocess.run([binary, 'flock', *args], capture_output=True,
                          text=True, timeout=5, env=dict(os.environ, SHELL='/bin/sh'))
for args, expected in [([], 2), (['-w', 'nan', path, 'true'], 2),
                       (['-w', 'inf', path, 'true'], 2),
                       (['-w', '1e100', path, 'true'], 2),
                       (['-E', '256', path, 'true'], 2),
                       (['-F', '-o', path, 'true'], 2),
                       ([path, '-c', 'exit 17'], 17),
                       (['-F', path, '-c', 'exit 18'], 18),
                       ([path, 'missing-flock-test-command'], 127)]:
    result = run(args)
    assert result.returncode == expected, (args, result.returncode, result.stderr)
result = run(['--verbose', path, 'echo', 'payload'])
lines = result.stdout.splitlines()
assert result.returncode == 0 and len(lines) == 3, result
assert 'getting lock took' in lines[0] and 'executing echo' in lines[1], lines
assert lines[2] == 'payload', lines
PY
if [[ "$?" == 0 ]]; then
    pass "flock validates timeout values and preserves command status/output order"
else
    fail "flock parsing/execution"
fi

[[ "$FAIL_COUNT" == 0 ]]
