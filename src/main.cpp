#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <filesystem>

#include "commands/help.hpp"
#include "commands/dd.hpp"
#include "commands/cat.hpp"
#include "commands/ls.hpp"
#include "commands/dir.hpp"
#include "commands/vdir.hpp"
#include "commands/lsc.hpp"
#include "commands/cp.hpp"
#include "commands/ln.hpp"
#include "commands/mv.hpp"
#include "commands/grep.hpp"
#include "commands/rg.hpp"
#include "commands/find.hpp"
#include "commands/fd.hpp"
#include "commands/tac.hpp"
#include "commands/sort.hpp"
#include "commands/shuf.hpp"
#include "commands/seq.hpp"
#include "commands/rev.hpp"
#include "commands/du.hpp"
#include "commands/dust.hpp"
#include "commands/head.hpp"
#include "commands/tail.hpp"
#include "commands/rm.hpp"
#include "commands/mkdir.hpp"
#include "commands/mkfifo.hpp"
#include "commands/mknod.hpp"
#include "commands/touch.hpp"
#include "commands/tsort.hpp"
#include "commands/uniq.hpp"
#include "commands/paste.hpp"
#include "commands/prompts.hpp"
#include "commands/nl.hpp"
#include "commands/diff.hpp"
#include "commands/comm.hpp"
#include "commands/uname.hpp"
#include "commands/whoami.hpp"
#include "commands/expand.hpp"
#include "commands/unexpand.hpp"
#include "commands/top.hpp"
#include "commands/htop.hpp"
#include "commands/mtop.hpp"
#include "commands/split.hpp"
#include "commands/csplit.hpp"
#include "commands/ptx.hpp"
#include "commands/cut.hpp"
#include "commands/tr.hpp"
#include "commands/sed.hpp"
#include "commands/sh.hpp"
#include "commands/ps.hpp"
#include "commands/sha256sum.hpp"
#include "commands/sha1sum.hpp"
#include "commands/md5sum.hpp"
#include "commands/chmod.hpp"
#include "commands/chown.hpp"
#include "commands/chgrp.hpp"
#include "commands/base64.hpp"
#include "commands/base32.hpp"
#include "commands/awk.hpp"
#include "commands/zoxide.hpp"
#include "commands/install.hpp"
#include "commands/link.hpp"
#include "commands/unlink.hpp"
#include "commands/test.hpp"
#include "commands/yes.hpp"
#include "commands/expr.hpp"
#include "commands/factor.hpp"
#include "commands/stat.hpp"
#include "commands/sync.hpp"
#include "commands/tee.hpp"
#include "commands/pwd.hpp"
#include "commands/stty.hpp"
#include "commands/tty.hpp"
#include "commands/numfmt.hpp"
#include "commands/sleep.hpp"
#include "commands/nice.hpp"
#include "commands/nohup.hpp"
#include "commands/time.hpp"

using CommandFunc = void (*)(int, char**);

static void execute_command(const std::string& command, int argc, char** argv) {
    static const std::unordered_map<std::string, CommandFunc> commands = {
        {"help", help_command},
        {"cat", cat_command},
        {"ls", ls_command},
        {"dir", dir_command},
        {"vdir", vdir_command},
        {"cp", cp_command},
        {"dd", dd_command},
        {"ln", ln_command},
        {"mv", mv_command},
        {"grep", grep_command},
        {"find", find_command},
        {"rg", rg_command},
        {"lsc", lsc_command},
        {"fd", fd_command},
        {"tac", tac_command},
        {"sort", sort_command},
        {"shuf", shuf_command},
        {"seq", seq_command},
        {"sh", sh_command},
        {"rev", rev_command},
        {"du", du_command},
        {"dust", dust_command},
        {"head", head_command},
        {"tail", tail_command},
        {"rm", rm_command},
        {"mkdir", mkdir_command},
        {"mkfifo", mkfifo_command},
        {"mknod", mknod_command},
        {"touch", touch_command},
        {"tsort", tsort_command},
        {"uniq", uniq_command},
        {"paste", paste_command},
        {"prompts", prompts_command},
        {"nl", nl_command},
        {"ptx", ptx_command},
        {"diff", diff_command},
        {"comm", comm_command},
        {"uname", uname_command},
        {"whoami", whoami_command},
        {"expand", expand_command},
        {"unexpand", unexpand_command},
        {"top", top_command},
        {"htop", htop_command},
        {"mtop", mtop_command},
        {"split", split_command},
        {"csplit", csplit_command},
        {"cut", cut_command},
        {"tr", tr_command},
        {"sed", sed_command},
        {"ps", ps_command},
        {"sha256sum", sha256sum_command},
        {"sha1sum", sha1sum_command},
        {"md5sum", md5sum_command},
        {"chmod", chmod_command},
        {"chown", chown_command},
        {"chgrp", chgrp_command},
        {"base64", base64_command},
        {"base32", base32_command},
        {"zoxide", zoxide_command},
        {"install", install_command},
        {"link", link_command},
        {"unlink", unlink_command},
        {"awk", awk_command},
        {"test", test_command},
        {"[", test_command},
        {"yes", yes_command},
        {"expr", expr_command},
        {"factor", factor_command},
        {"stat", stat_command},
        {"sync", sync_command},
        {"tee", tee_command},
        {"pwd", pwd_command},
        {"stty", stty_command},
        {"tty", tty_command},
        {"numfmt", numfmt_command},
        {"sleep", sleep_command},
        {"nice", nice_command},
        {"nohup", nohup_command},
        {"time", time_command},
    };

    auto it = commands.find(command);
    if (it != commands.end()) {
        it->second(argc, argv);
    } else {
        std::string runname = std::filesystem::path(argv[0]).filename().string();
        printf("Unknown command: %s\n", command.c_str());
        output_help(argv[0], runname.c_str());
    }
}

int main(int argc, char* argv[]) {
    std::string runname = std::filesystem::path(argv[0]).filename().string();
    if (runname == "modbox" && argc == 1) {
        output_help(argv[0], runname.c_str());
        return 0;
    }
    if (runname == "modbox") {
        execute_command(argv[1], argc - 1, argv + 1);
    } else {
        execute_command(runname, argc, argv);
    }
    return 0;
}
