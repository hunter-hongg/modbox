#include "commands/sha256sum.hpp"
#include "commands/command_macros.hpp"
#include "commands/hashsum_common.hpp"
#include <openssl/evp.h>

int sha256sum_command(int argc, char** argv) {
    static const HashAlgoSpec spec{.prog="sha256sum", .tag="SHA256",
                                   .blurb="Print or check SHA256 (256-bit) checksums.",
                                   .md=EVP_sha256, .variable_length=false};
    return hashsum_main(argc, argv, spec);
}

REGISTER_COMMAND("sha256sum", sha256sum_command, "Compute SHA256 checksum");
