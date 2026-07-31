#include "commands/sha512sum.hpp"
#include "commands/command_macros.hpp"
#include "commands/hashsum_common.hpp"

int sha512sum_command(int argc, char** argv) {
    static const HashAlgoSpec spec{"sha512sum", "SHA512",
                                   "Print or check SHA512 (512-bit) checksums.",
                                   EVP_sha512, false};
    return hashsum_main(argc, argv, spec);
}

REGISTER_COMMAND("sha512sum", sha512sum_command, "Compute SHA512 checksum");
