#include "commands/b2sum.hpp"
#include "commands/command_macros.hpp"
#include "commands/hashsum_common.hpp"
#include <openssl/evp.h>

int b2sum_command(int argc, char** argv) {
    static const HashAlgoSpec spec{.prog="b2sum", .tag="BLAKE2",
                                   .blurb="Print or check BLAKE2 (512-bit) checksums.",
                                   .md=EVP_blake2b512, .variable_length=true};
    return hashsum_main(argc, argv, spec);
}

REGISTER_COMMAND("b2sum", b2sum_command, "Compute BLAKE2 checksum");
