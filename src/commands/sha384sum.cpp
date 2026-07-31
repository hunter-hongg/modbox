#include "commands/sha384sum.hpp"
#include "commands/command_macros.hpp"
#include "commands/hashsum_common.hpp"

int sha384sum_command(int argc, char** argv) {
    static const HashAlgoSpec spec{"sha384sum", "SHA384",
                                   "Print or check SHA384 (384-bit) checksums.",
                                   EVP_sha384, false};
    return hashsum_main(argc, argv, spec);
}

REGISTER_COMMAND("sha384sum", sha384sum_command, "Compute SHA384 checksum");
