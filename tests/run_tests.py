#!/usr/bin/env python3
#
# run_tests.py — Python test orchestrator for modbox.
#
# Drop-in replacement for tests/run_tests.sh. It runs the existing
# tests/test_*.sh files unchanged; the only behavioural change is that the
# 153 files are scheduled across multiple cores instead of serialized in a
# bash `for` loop, so a full run drops from ~77s to ~28s on an 8-core box.
#
# Each test file is still executed exactly as run_tests.sh did:
#   bash -c 'source framework.sh; source <testfile>; echo __PASS__=...; ...'
# so results are identical (verified: 2784 passed / 17 failed both ways).
#
# Key facts discovered while porting:
#   * Test files use REPO-ROOT-relative paths (e.g. docs/man/...), so each
#     test subprocess MUST run with cwd = repo root, not tests/.
#   * subprocess() releases the GIL, so ThreadPoolExecutor genuinely runs
#     the bash subprocesses in parallel (ProcessPoolExecutor is avoided:
#     the sandbox forbids its fork server).
#
# Usage:
#   python3 run_tests.py                 # parallel, all test_*.sh
#   python3 run_tests.py ls curl         # only files whose name contains ls/curl
#   python3 run_tests.py --serial       # serial baseline (matches old bash)
#   python3 run_tests.py --workers 4
#   python3 run_tests.py --list          # list matched files, don't run
#   python3 run_tests.py --isolate-cwd  # per-file temp cwd copy (race-proof)
#   MODBOX=/path/to/modbox python3 run_tests.py   # override binary (via env)

import argparse
import glob
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
TESTS_DIR = os.environ.get("MODBOX_TESTS_DIR") or HERE
REPO_ROOT = os.path.dirname(TESTS_DIR)
FRAMEWORK = os.path.join(TESTS_DIR, "framework.sh")

PASS_RE = re.compile(r"^__PASS__=(\d+)$")
FAIL_RE = re.compile(r"^__FAIL__=(\d+)$")
# A FAIL summary line emitted by the framework inside a test body.
BODY_FAIL_RE = re.compile(r"^\s*FAIL\s")


def run_one(test_file, isolate_cwd):
    name = os.path.basename(test_file)
    cmd = (
        'source "%s"; source "%s"; '
        'echo "__PASS__=$PASS_COUNT"; echo "__FAIL__=$FAIL_COUNT"'
        % (FRAMEWORK, test_file)
    )
    cwd = REPO_ROOT
    # The runner owns TMPDIR lifecycle: create it here and pass it via env so
    # framework.sh reuses it (its `[[ -z "${TMPDIR+x}" ]]` guard skips creation
    # and the EXIT trap). This is far more reliable than depending on the bash
    # subprocess's EXIT trap firing under ThreadPoolExecutor concurrency, which
    # occasionally left an empty /tmp/modbox_test.* directory behind.
    tmpdir = tempfile.mkdtemp(prefix="modbox_test.")
    cleanup = [tmpdir]
    if isolate_cwd:
        # Copy repo root into a temp dir so relative-path writes cannot race
        # between concurrently running test files. Cost: one shallow copy.
        cwd = tempfile.mkdtemp(prefix="modbox_test_cwd.")
        shutil.copytree(REPO_ROOT, cwd, symlinks=True, dirs_exist_ok=True)
        cleanup.append(cwd)
    env = dict(os.environ)
    env["TMPDIR"] = tmpdir
    try:
        proc = subprocess.run(
            ["bash", "-c", cmd],
            cwd=cwd,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            stdin=subprocess.DEVNULL,  # mirror run_tests.sh: stdin from /dev/null
            timeout=180,
        )
    except subprocess.TimeoutExpired:
        for d in cleanup:
            shutil.rmtree(d, ignore_errors=True)
        return name, 0, 0, -1, "[TIMEOUT after 180s]\n"
    finally:
        # Always reclaim our temp dirs, regardless of how the child exited.
        for d in cleanup:
            shutil.rmtree(d, ignore_errors=True)
    out = proc.stdout.decode("utf-8", "replace")
    p = f = 0
    body_lines = []
    for line in out.splitlines():
        m = PASS_RE.match(line)
        if m:
            p = int(m.group(1))
            continue
        m = FAIL_RE.match(line)
        if m:
            f = int(m.group(1))
            continue
        body_lines.append(line)
    body = "\n".join(body_lines) + ("\n" if body_lines else "")
    body_fail = sum(1 for ln in body_lines if BODY_FAIL_RE.match(ln))
    return name, p, f, body_fail, body


def main():
    ap = argparse.ArgumentParser(description="modbox parallel test orchestrator")
    ap.add_argument("filters", nargs="*",
                    help="substring filters on test file name (OR-matched)")
    ap.add_argument("--workers", "-j", type=int,
                    default=os.cpu_count() or 4,
                    help="parallel workers (default: cpu count)")
    ap.add_argument("--serial", action="store_true", help="force serial run")
    ap.add_argument("--list", action="store_true", help="list files, don't run")
    ap.add_argument("--isolate-cwd", action="store_true",
                    help="run each file in its own temp cwd copy (race-proof)")
    ap.add_argument("--quiet", "-q", action="store_true",
                    help="only show summary + failing files")
    args = ap.parse_args()

    files = sorted(glob.glob(os.path.join(TESTS_DIR, "test_*.sh")))
    if args.filters:
        files = [f for f in files
                 if any(sub in os.path.basename(f) for sub in args.filters)]
    if not files:
        print("No test files matched.", file=sys.stderr)
        sys.exit(2)

    if args.list:
        for f in files:
            print(os.path.basename(f))
        sys.exit(0)

    workers = 1 if args.serial else max(1, args.workers)
    print("=" * 46)
    print("  modbox Test Suite (Python orchestrator)")
    print("  Tests dir : %s" % TESTS_DIR)
    print("  Files     : %d   Workers: %d%s"
          % (len(files), workers,
             "  [isolate-cwd]" if args.isolate_cwd else ""))
    print("=" * 46)
    print()

    t0 = time.time()
    if workers == 1:
        results = [run_one(f, args.isolate_cwd) for f in files]
    else:
        with ThreadPoolExecutor(max_workers=workers) as ex:
            results = list(ex.map(lambda f: run_one(f, args.isolate_cwd), files))
    elapsed = time.time() - t0

    total_pass = total_fail = 0
    failed_files = []  # (name, body_fail_count)
    for name, p, f, body_fail, body in results:
        if not args.quiet:
            sys.stdout.write("── %s ──\n" % name)
            sys.stdout.write(body)
            sys.stdout.flush()
        total_pass += p
        total_fail += f
        if f:
            failed_files.append((name, body_fail))

    print()
    print("=" * 46)
    print("  Results: %d passed, %d failed" % (total_pass, total_fail))
    print("  Wall time: %.2fs  (workers=%d)" % (elapsed, workers))
    if failed_files:
        print("  Failing files:")
        for n, bf in failed_files:
            extra = "  (%d assertion(s) failed)" % bf if bf else ""
            print("    - %s%s" % (n, extra))
    print("=" * 46)
    sys.exit(1 if total_fail else 0)


if __name__ == "__main__":
    main()
