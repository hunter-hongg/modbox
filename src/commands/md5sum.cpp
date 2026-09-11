#include "commands/md5sum.hpp"
#include "commands/command_macros.hpp"
#include "commands/hashsum_common.hpp"
#include <openssl/evp.h>

int md5sum_command(int argc, char** argv) {
    static const HashAlgoSpec spec{.prog="md5sum", .tag="MD5",
                                   .blurb="Print or check MD5 (128-bit) checksums.",
                                   .md=EVP_md5, .variable_length=false};
    return hashsum_main(argc, argv, spec);
}

REGISTER_COMMAND("md5sum", md5sum_command, "Compute MD5 checksum");
