#include "commands/sha1sum.hpp"
#include "commands/command_macros.hpp"
#include "commands/hashsum_common.hpp"
#include <openssl/evp.h>

int sha1sum_command(int argc, char** argv) {
    static const HashAlgoSpec spec{.prog="sha1sum", .tag="SHA1",
                                   .blurb="Print or check SHA1 (160-bit) checksums.",
                                   .md=EVP_sha1, .variable_length=false};
    return hashsum_main(argc, argv, spec);
}

REGISTER_COMMAND("sha1sum", sha1sum_command, "Compute SHA1 checksum");
