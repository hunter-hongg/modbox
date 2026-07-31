#include "commands/sha224sum.hpp"
#include "commands/command_macros.hpp"
#include "commands/hashsum_common.hpp"

int sha224sum_command(int argc, char** argv) {
    static const HashAlgoSpec spec{"sha224sum", "SHA224",
                                   "Print or check SHA224 (224-bit) checksums.",
                                   EVP_sha224, false};
    return hashsum_main(argc, argv, spec);
}

REGISTER_COMMAND("sha224sum", sha224sum_command, "Compute SHA224 checksum");
