#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cerrno>
#include <ctime>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <argtable3.h>
#include "commands/arg_util.hpp"

#include "commands/shred.hpp"
#include "commands/command_macros.hpp"

namespace {

struct ShredOptions {
    int iterations = 3;
    int zero_pass = 0;
    int remove = 0;
    int verbose = 0;
    int force = 0;
    int exact_size = 0;
    int random_source = 0;
};

void secure_erase(int fd, off_t size, bool use_zero, int verbose) {
    const size_t buf_size = 65536;
    std::vector<uint8_t> buf(buf_size);

    if (use_zero) {
        memset(buf.data(), 0, buf_size);
    }

    off_t remaining = size;
    off_t written = 0;

    while (remaining > 0) {
        size_t const chunk = remaining > static_cast<off_t>(buf_size) ? buf_size : static_cast<size_t>(remaining);

        if (!use_zero) {
            for (size_t i = 0; i < chunk; i++) {
                buf[i] = static_cast<uint8_t>(rand() % 256);
            }
        }

        ssize_t const n = write(fd, buf.data(), chunk);
        if (n <= 0) {
            (void)fprintf(stderr, "shred: write error: %s\n", strerror(errno));
            return;
        }
        written += n;
        remaining -= n;

        if ((verbose != 0) && written % (1024 * 1024) == 0) {
            (void)fprintf(stderr, "shred: %lld bytes written\r", static_cast<long long>(written));
        }
    }

    if (verbose != 0) {
        (void)fprintf(stderr, "shred: %lld bytes written\n", static_cast<long long>(written));
    }
}

void shred_file(const char* filename, ShredOptions& opts) {
    struct stat st;
    if (stat(filename, &st) != 0) {
        (void)fprintf(stderr, "shred: %s: %s\n", filename, strerror(errno));
        return;
    }

    if (!S_ISREG(st.st_mode)) {
        (void)fprintf(stderr, "shred: %s: refusing to shred non-regular file\n", filename);
        return;
    }

    int fd = open(filename, O_RDWR);
    if (fd < 0) {
        if (opts.force != 0) {
            if (chmod(filename, st.st_mode | S_IWUSR) != 0) {
                (void)fprintf(stderr, "shred: %s: cannot make writable: %s\n", filename, strerror(errno));
                return;
            }
            fd = open(filename, O_RDWR);
        }
        if (fd < 0) {
            (void)fprintf(stderr, "shred: %s: cannot open: %s\n", filename, strerror(errno));
            return;
        }
    }

    off_t const file_size = st.st_size;

    if (opts.verbose != 0) {
        (void)fprintf(stderr, "shred: %s: %lld bytes, %d pass%s\n",
                filename, static_cast<long long>(file_size), opts.iterations,
                opts.iterations == 1 ? "" : "es");
    }

    if (opts.iterations > 0) {
        for (int i = 0; i < opts.iterations; i++) {
            if (opts.verbose != 0) {
                (void)fprintf(stderr, "shred: pass %d/%d: random\n", i + 1, opts.iterations);
            }
            if (lseek(fd, 0, SEEK_SET) == static_cast<off_t>(-1)) {
                (void)fprintf(stderr, "shred: %s: lseek error: %s\n", filename, strerror(errno));
                close(fd);
                return;
            }
            secure_erase(fd, file_size, false, opts.verbose);
        }
    }

    if (opts.zero_pass != 0) {
        if (opts.verbose != 0) {
            (void)fprintf(stderr, "shred: final pass: zero\n");
        }
        if (lseek(fd, 0, SEEK_SET) == static_cast<off_t>(-1)) {
            (void)fprintf(stderr, "shred: %s: lseek error: %s\n", filename, strerror(errno));
            close(fd);
            return;
        }
        secure_erase(fd, file_size, true, opts.verbose);
    }

    if (fsync(fd) != 0) {
        (void)fprintf(stderr, "shred: %s: fsync error: %s\n", filename, strerror(errno));
    }

    close(fd);

    if (opts.remove != 0) {
        if (opts.verbose != 0) {
            (void)fprintf(stderr, "shred: removing %s\n", filename);
        }
        if (unlink(filename) != 0) {
            (void)fprintf(stderr, "shred: %s: cannot remove: %s\n", filename, strerror(errno));
        }
    }
}

}

int shred_command(int argc, char** argv) {
    struct arg_int* iter_opt = arg_int0("n", "iterations", "num", "overwrite N times (default 3)");
    struct arg_lit* zero_opt = arg_lit0("z", "zero", "add a final overwrite with zeros");
    struct arg_lit* remove_opt = arg_lit0("u", "remove", "remove file after overwriting");
    struct arg_lit* verbose_opt = arg_lit0("v", "verbose", "show progress");
    struct arg_lit* force_opt = arg_lit0("f", "force", "change permissions to allow overwriting");
    struct arg_lit* size_opt = arg_lit0(NULL, "exact", "end at file size (default)");
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file* files_arg = arg_filen(NULL, NULL, "FILE", 1, 100, "files to shred");
    struct arg_end* end = arg_end(20);

    ArgTable at({iter_opt, zero_opt, remove_opt, verbose_opt, force_opt,
                 size_opt, help_opt, files_arg, end});

    int const nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... FILE...\n", argv[0]);
        printf("Overwrite the specified FILE(s) repeatedly, in order to make\n");
        printf("it harder for even very expensive hardware probing to recover\n");
        printf("the data.\n");
        printf("\n");
        printf("  -n, --iterations=N   overwrite N times (default 3)\n");
        printf("  -z, --zero           add a final overwrite with zeros\n");
        printf("  -u, --remove         remove file after overwriting\n");
        printf("  -f, --force          change permissions to allow overwriting\n");
        printf("  -v, --verbose        show progress\n");
        printf("  -h, --help           display this help and exit\n");
        printf("\n");
        printf("At least one of --iterations, --zero, or --remove must be specified.\n");
        return 0;
    }

    if (nerrors > 0) {
        return at.print_errors(end, argv[0]);
    }

    ShredOptions opts;

    if (iter_opt->count > 0) {
        opts.iterations = iter_opt->ival[0];
        opts.iterations = std::max(opts.iterations, 0);
    }

    opts.zero_pass = static_cast<int>(zero_opt->count > 0);
    opts.remove = static_cast<int>(remove_opt->count > 0);
    opts.verbose = static_cast<int>(verbose_opt->count > 0);
    opts.force = static_cast<int>(force_opt->count > 0);
    opts.exact_size = static_cast<int>(size_opt->count > 0);

    if (opts.iterations == 0 && (opts.zero_pass == 0) && (opts.remove == 0)) {
        (void)fprintf(stderr, "shred: no action specified (use -n, -z, or -u)\n");
        return 0;
    }

    srand(static_cast<unsigned int>(time(nullptr)));

    for (int i = 0; i < files_arg->count; i++) {
        shred_file(files_arg->filename[i], opts);
    }

    return 0;
}

REGISTER_COMMAND("shred", shred_command, "Overwrite file to hide its contents");
