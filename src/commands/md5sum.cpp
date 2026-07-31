#include "commands/md5sum.hpp"
#include "commands/command_macros.hpp"
#include "commands/hashsum_common.hpp"

int md5sum_command(int argc, char** argv) {
    static const HashAlgoSpec spec{"md5sum", "MD5",
                                   "Print or check MD5 (128-bit) checksums.",
                                   EVP_md5, false};
    return hashsum_main(argc, argv, spec);
}

REGISTER_COMMAND("md5sum", md5sum_command, "Compute MD5 checksum");
