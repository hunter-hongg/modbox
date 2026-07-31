#ifndef HASHSUM_COMMON_HPP
#define HASHSUM_COMMON_HPP

#include <openssl/evp.h>

struct HashAlgoSpec {
    const char* prog;      // "sha256sum"
    const char* tag;       // "SHA256" (BSD-style --tag prefix)
    const char* blurb;     // "Print or check SHA256 (256-bit) checksums."
    const EVP_MD* (*md)(); // getter, never called at static-init time
    bool variable_length;  // b2sum only: -l/--length + variable-width check
};

int hashsum_main(int argc, char** argv, const HashAlgoSpec& spec);

#endif
