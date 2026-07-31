#include "commands/sha1sum.hpp"
#include "commands/command_macros.hpp"
#include "commands/hashsum_common.hpp"

int sha1sum_command(int argc, char** argv) {
    static const HashAlgoSpec spec{"sha1sum", "SHA1",
                                   "Print or check SHA1 (160-bit) checksums.",
                                   EVP_sha1, false};
    return hashsum_main(argc, argv, spec);
}

REGISTER_COMMAND("sha1sum", sha1sum_command, "Compute SHA1 checksum");
