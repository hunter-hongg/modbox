#include "commands/sha256sum.hpp"
#include "commands/command_macros.hpp"
#include "commands/hashsum_common.hpp"

int sha256sum_command(int argc, char** argv) {
    static const HashAlgoSpec spec{"sha256sum", "SHA256",
                                   "Print or check SHA256 (256-bit) checksums.",
                                   EVP_sha256, false};
    return hashsum_main(argc, argv, spec);
}

REGISTER_COMMAND("sha256sum", sha256sum_command, "Compute SHA256 checksum");
