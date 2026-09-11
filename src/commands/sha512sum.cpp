#include "commands/sha512sum.hpp"
#include "commands/command_macros.hpp"
#include "commands/hashsum_common.hpp"
#include <openssl/evp.h>

int sha512sum_command(int argc, char** argv) {
    static const HashAlgoSpec spec{.prog="sha512sum", .tag="SHA512",
                                   .blurb="Print or check SHA512 (512-bit) checksums.",
                                   .md=EVP_sha512, .variable_length=false};
    return hashsum_main(argc, argv, spec);
}

REGISTER_COMMAND("sha512sum", sha512sum_command, "Compute SHA512 checksum");
