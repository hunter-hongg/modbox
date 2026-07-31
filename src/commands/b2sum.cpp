#include "commands/b2sum.hpp"
#include "commands/command_macros.hpp"
#include "commands/hashsum_common.hpp"

int b2sum_command(int argc, char** argv) {
    static const HashAlgoSpec spec{"b2sum", "BLAKE2",
                                   "Print or check BLAKE2 (512-bit) checksums.",
                                   EVP_blake2b512, true};
    return hashsum_main(argc, argv, spec);
}

REGISTER_COMMAND("b2sum", b2sum_command, "Compute BLAKE2 checksum");
