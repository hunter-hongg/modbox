#include "commands/sha224sum.hpp"
#include "commands/command_macros.hpp"
#include "commands/hashsum_common.hpp"
#include <openssl/evp.h>

int sha224sum_command(int argc, char** argv) {
    static const HashAlgoSpec spec{.prog="sha224sum", .tag="SHA224",
                                   .blurb="Print or check SHA224 (224-bit) checksums.",
                                   .md=EVP_sha224, .variable_length=false};
    return hashsum_main(argc, argv, spec);
}

REGISTER_COMMAND("sha224sum", sha224sum_command, "Compute SHA224 checksum");
