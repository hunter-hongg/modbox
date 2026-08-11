#ifndef XZ_TEST
#define XZ_TEST

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

// Test helper: run modbox xz with given args and check exit status
int run_xz(const char* modbox, const char** args, int nargs) {
    std::string cmd = std::string(modbox) + " xz";
    for (int i = 0; i < nargs; i++) {
        cmd += " ";
        cmd += args[i];
    }
    return system(cmd.c_str());
}

// Test helper: create a test file with given content
void create_test_file(const char* path, const char* content) {
    FILE* f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "Failed to create test file: %s\n", path);
        exit(1);
    }
    fprintf(f, "%s", content);
    fclose(f);
}

// Test helper: read entire file into buffer
std::string read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return "";
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string content(size, '\0');
    fread(&content[0], 1, size, f);
    fclose(f);
    return content;
}

// Test helper: check if file exists
bool file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

// Test helper: check xz magic bytes
bool has_xz_magic(const std::string& data) {
    return data.size() >= 6 &&
           (uint8_t)data[0] == 0xfd &&
           (uint8_t)data[1] == 0x37 &&
           (uint8_t)data[2] == 0x7a &&
           (uint8_t)data[3] == 0x58 &&
           (uint8_t)data[4] == 0x5a &&
           (uint8_t)data[5] == 0x00;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <modbox-path>\n", argv[0]);
        return 1;
    }
    const char* modbox = argv[1];
    const char* tmpdir = "/tmp/modbox-xz-test-XXXXXX";
    char* dir = mkdtemp(tmpdir);
    if (!dir) {
        fprintf(stderr, "Failed to create temp dir\n");
        return 1;
    }

    int failures = 0;

    // T01: Help flag
    {
        const char* args[] = {"--help"};
        int status = run_xz(modbox, args, 1);
        if (WEXITSTATUS(status) != 0) {
            fprintf(stderr, "T01 FAIL: --help should exit 0\n");
            failures++;
        }
    }

    // T02: Version flag
    {
        const char* args[] = {"--version"};
        int status = run_xz(modbox, args, 1);
        if (WEXITSTATUS(status) != 0) {
            fprintf(stderr, "T02 FAIL: --version should exit 0\n");
            failures++;
        }
    }

    // T03: Compress single file
    {
        std::string filepath = std::string(dir) + "/test.txt";
        std::string xzpath = filepath + ".xz";
        create_test_file(filepath.c_str(), "the quick brown fox jumps over the lazy dog\n");

        const char* args[] = {filepath.c_str()};
        int status = run_xz(modbox, args, 1);
        if (WEXITSTATUS(status) != 0) {
            fprintf(stderr, "T03 FAIL: compress should exit 0\n");
            failures++;
        } else if (!file_exists(xzpath.c_str())) {
            fprintf(stderr, "T03 FAIL: %s.xz should exist\n", filepath.c_str());
            failures++;
        } else if (!file_exists(filepath.c_str())) {
            // Good - original should be removed
        } else {
            fprintf(stderr, "T03 FAIL: original file should be removed\n");
            failures++;
        }
    }

    // T04: Decompress single file
    {
        std::string filepath = std::string(dir) + "/test2.txt";
        std::string xzpath = filepath + ".xz";
        create_test_file(filepath.c_str(), "decompression test\n");

        // Compress first
        const char* compress_args[] = {filepath.c_str()};
        run_xz(modbox, compress_args, 1);

        // Now decompress
        const char* decompress_args[] = {"-d", xzpath.c_str()};
        int status = run_xz(modbox, decompress_args, 2);
        if (WEXITSTATUS(status) != 0) {
            fprintf(stderr, "T04 FAIL: decompress should exit 0\n");
            failures++;
        } else if (!file_exists(filepath.c_str())) {
            fprintf(stderr, "T04 FAIL: %s should exist after decompress\n", filepath.c_str());
            failures++;
        }
    }

    // T05: Round-trip test
    {
        std::string filepath = std::string(dir) + "/roundtrip.txt";
        std::string original = "round-trip test content\nwith multiple lines\n";
        create_test_file(filepath.c_str(), original.c_str());

        // Compress
        const char* compress_args[] = {"-k", filepath.c_str()};
        int status = run_xz(modbox, compress_args, 2);
        if (WEXITSTATUS(status) != 0) {
            fprintf(stderr, "T05 FAIL: compress for round-trip should exit 0\n");
            failures++;
        }

        std::string xzpath = filepath + ".xz";
        if (!file_exists(xzpath.c_str())) {
            fprintf(stderr, "T05 FAIL: xz file should exist\n");
            failures++;
        }

        // Decompress to stdout
        std::string stdout_args[] = {"-dc", xzpath.c_str()};
        // Note: we can't easily capture stdout from system(), so just check exit status
        status = run_xz(modbox, stdout_args, 2);
        if (WEXITSTATUS(status) != 0) {
            fprintf(stderr, "T05 FAIL: decompress to stdout should exit 0\n");
            failures++;
        }
    }

    // T06: Compress with -c (stdout)
    {
        std::string filepath = std::string(dir) + "/stdout.txt";
        create_test_file(filepath.c_str(), "stdout test\n");

        const char* args[] = {"-c", filepath.c_str()};
        // Can't capture stdout easily, but check exit status
        int status = run_xz(modbox, args, 2);
        if (WEXITSTATUS(status) != 0) {
            fprintf(stderr, "T06 FAIL: compress to stdout should exit 0\n");
            failures++;
        }
    }

    // T07: Multiple files
    {
        std::string file1 = std::string(dir) + "/multi1.txt";
        std::string file2 = std::string(dir) + "/multi2.txt";
        create_test_file(file1.c_str(), "file one\n");
        create_test_file(file2.c_str(), "file two\n");

        const char* args[] = {file1.c_str(), file2.c_str()};
        int status = run_xz(modbox, args, 2);
        if (WEXITSTATUS(status) != 0) {
            fprintf(stderr, "T07 FAIL: multiple files should exit 0\n");
            failures++;
        }
    }

    // Cleanup
    std::string rm_cmd = std::string("rm -rf ") + dir;
    system(rm_cmd.c_str());

    if (failures > 0) {
        fprintf(stderr, "\n%d test(s) failed\n", failures);
        return 1;
    }

    printf("All xz tests passed\n");
    return 0;
}

#endif // XZ_TEST
