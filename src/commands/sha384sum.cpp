#include "commands/sha384sum.hpp"
#include "commands/command_macros.hpp"
#include "commands/hashsum_common.hpp"
#include <openssl/evp.h>

int sha384sum_command(int argc, char** argv) {
    static const HashAlgoSpec spec{.prog="sha384sum", .tag="SHA384",
                                   .blurb="Print or check SHA384 (384-bit) checksums.",
                                   .md=EVP_sha384, .variable_length=false};
    return hashsum_main(argc, argv, spec);
}

REGISTER_COMMAND("sha384sum", sha384sum_command, "Compute SHA384 checksum");
