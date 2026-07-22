#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "commands/numfmt.hpp"
#include "commands/command_macros.hpp"

enum class ScaleUnit { NONE, AUTO, SI, IEC, IEC_I };
enum class RoundMethod { FROM_ZERO, TOWARDS_ZERO, UP, DOWN, NEAREST };
enum class InvalidMode { ABORT, FAIL, WARN, IGNORE };

struct NumfmtOptions {
    ScaleUnit from = ScaleUnit::NONE;
    ScaleUnit to = ScaleUnit::NONE;
    double from_unit = 1.0;
    double to_unit = 1.0;
    int padding = 0;
    RoundMethod round = RoundMethod::FROM_ZERO;
    std::string suffix;
    std::string format;
    std::string delimiter;
    int field = 1;
    int header = 0;
    InvalidMode invalid = InvalidMode::ABORT;
    bool grouping = false;
    bool debug = false;
};

static ScaleUnit parse_scale(const char* s) {
    if (strcmp(s, "auto") == 0) return ScaleUnit::AUTO;
    if (strcmp(s, "si") == 0) return ScaleUnit::SI;
    if (strcmp(s, "iec") == 0) return ScaleUnit::IEC;
    if (strcmp(s, "iec-i") == 0) return ScaleUnit::IEC_I;
    if (strcmp(s, "none") == 0) return ScaleUnit::NONE;
    return ScaleUnit::NONE;
}

static RoundMethod parse_round(const char* s) {
    if (strcmp(s, "up") == 0) return RoundMethod::UP;
    if (strcmp(s, "down") == 0) return RoundMethod::DOWN;
    if (strcmp(s, "from-zero") == 0) return RoundMethod::FROM_ZERO;
    if (strcmp(s, "towards-zero") == 0) return RoundMethod::TOWARDS_ZERO;
    if (strcmp(s, "nearest") == 0) return RoundMethod::NEAREST;
    return RoundMethod::FROM_ZERO;
}

static InvalidMode parse_invalid(const char* s) {
    if (strcmp(s, "abort") == 0) return InvalidMode::ABORT;
    if (strcmp(s, "fail") == 0) return InvalidMode::FAIL;
    if (strcmp(s, "warn") == 0) return InvalidMode::WARN;
    if (strcmp(s, "ignore") == 0) return InvalidMode::IGNORE;
    return InvalidMode::ABORT;
}

static double apply_round(double val, RoundMethod method) {
    switch (method) {
        case RoundMethod::UP: return ceil(val);
        case RoundMethod::DOWN: return floor(val);
        case RoundMethod::FROM_ZERO: return val >= 0 ? ceil(val) : floor(val);
        case RoundMethod::TOWARDS_ZERO: return val >= 0 ? floor(val) : ceil(val);
        case RoundMethod::NEAREST: return round(val);
    }
    return val;
}

struct SuffixEntry {
    const char* suffix;
    double si_factor;
    double iec_factor;
};

static const SuffixEntry suffixes[] = {
    {"K", 1e3, 1024.0},
    {"M", 1e6, 1024.0 * 1024},
    {"G", 1e9, 1024.0 * 1024 * 1024},
    {"T", 1e12, 1024.0 * 1024 * 1024 * 1024},
    {"P", 1e15, 1024.0 * 1024 * 1024 * 1024 * 1024},
    {"E", 1e18, 1024.0 * 1024 * 1024 * 1024 * 1024 * 1024},
    {"Z", 1e21, 1024.0 * 1024 * 1024 * 1024 * 1024 * 1024 * 1024},
    {"Y", 1e24, 1024.0 * 1024 * 1024 * 1024 * 1024 * 1024 * 1024 * 1024},
};
static const int num_suffixes = 8;

static bool parse_input_number(const char* s, double* out, const NumfmtOptions& opts) {
    if (s == nullptr || *s == '\0') return false;

    char* endp = nullptr;
    errno = 0;
    double val = strtod(s, &endp);
    if (errno == ERANGE) return false;
    if (endp == s) return false;

    double multiplier = 1.0;
    if (*endp != '\0') {
        ScaleUnit from = opts.from;
        if (from == ScaleUnit::NONE) return false;

        char suffix_char = toupper((unsigned char)*endp);
        bool has_i = (*(endp + 1) == 'i');
        bool found = false;

        for (int i = 0; i < num_suffixes; i++) {
            if (suffix_char == suffixes[i].suffix[0]) {
                if (from == ScaleUnit::SI) {
                    multiplier = suffixes[i].si_factor;
                } else if (from == ScaleUnit::IEC || from == ScaleUnit::IEC_I) {
                    multiplier = suffixes[i].iec_factor;
                } else if (from == ScaleUnit::AUTO) {
                    if (has_i) {
                        multiplier = suffixes[i].iec_factor;
                    } else {
                        multiplier = suffixes[i].si_factor;
                    }
                }
                found = true;
                endp++;
                if (*endp == 'i') endp++;
                break;
            }
        }
        if (!found) return false;
        if (*endp != '\0') return false;
    }

    *out = val * multiplier * opts.from_unit;
    return true;
}

static std::string format_output(double val, const NumfmtOptions& opts) {
    val = val / opts.to_unit;

    std::string result;
    std::string user_suffix = opts.suffix;

    if (opts.to == ScaleUnit::NONE) {
        double rounded = apply_round(val, opts.round);
        if (!opts.format.empty()) {
            char buf[256];
            snprintf(buf, sizeof(buf), opts.format.c_str(), rounded);
            result = buf;
        } else {
            char buf[64];
            if (rounded == (long long)rounded && fabs(rounded) < 1e15) {
                snprintf(buf, sizeof(buf), "%lld", (long long)rounded);
            } else {
                snprintf(buf, sizeof(buf), "%g", rounded);
            }
            result = buf;
        }
    } else {
        double base = (opts.to == ScaleUnit::SI) ? 1000.0 : 1024.0;
        const char* chosen_suffix = "";
        double display_val = val;
        double abs_val = fabs(val);

        if (abs_val < base) {
            display_val = val;
            chosen_suffix = "";
        } else {
            for (int i = 0; i < num_suffixes; i++) {
                double factor = (opts.to == ScaleUnit::SI) ? suffixes[i].si_factor : suffixes[i].iec_factor;
                double next_factor = (i + 1 < num_suffixes)
                    ? ((opts.to == ScaleUnit::SI) ? suffixes[i + 1].si_factor : suffixes[i + 1].iec_factor)
                    : factor * base;
                if (abs_val < next_factor || i == num_suffixes - 1) {
                    display_val = val / factor;
                    chosen_suffix = suffixes[i].suffix;
                    break;
                }
            }
        }

        char buf[64];
        if (chosen_suffix[0] == '\0') {
            double rounded = apply_round(display_val, opts.round);
            snprintf(buf, sizeof(buf), "%lld", (long long)rounded);
        } else if (fabs(display_val) < 10.0) {
            double rounded = apply_round(display_val * 10.0, opts.round) / 10.0;
            snprintf(buf, sizeof(buf), "%.1f", rounded);
        } else {
            double rounded = apply_round(display_val, opts.round);
            snprintf(buf, sizeof(buf), "%.0f", rounded);
        }
        result = buf;
        result += chosen_suffix;
        if (opts.to == ScaleUnit::IEC_I && chosen_suffix[0] != '\0') {
            result += "i";
        }
    }

    result += user_suffix;

    if (opts.padding != 0) {
        int width = abs(opts.padding);
        int len = (int)result.size();
        if (len < width) {
            int pad = width - len;
            if (opts.padding > 0) {
                result = std::string(pad, ' ') + result;
            } else {
                result += std::string(pad, ' ');
            }
        }
    }

    return result;
}

static bool process_field(const std::string& token, const NumfmtOptions& opts, std::string& out) {
    double val = 0;
    if (!parse_input_number(token.c_str(), &val, opts)) {
        return false;
    }
    out = format_output(val, opts);
    return true;
}

static void process_line(const std::string& line, const NumfmtOptions& opts, bool* had_error) {
    std::string delim = opts.delimiter;
    bool use_whitespace = delim.empty();

    std::vector<std::string> tokens;
    std::vector<std::string> separators;

    if (use_whitespace) {
        size_t i = 0;
        size_t n = line.size();
        size_t leading = 0;
        while (leading < n && (line[leading] == ' ' || line[leading] == '\t')) leading++;
        if (leading > 0) separators.push_back(line.substr(0, leading));
        else separators.push_back("");

        i = leading;
        while (i < n) {
            size_t start = i;
            while (i < n && line[i] != ' ' && line[i] != '\t') i++;
            tokens.push_back(line.substr(start, i - start));
            size_t sep_start = i;
            while (i < n && (line[i] == ' ' || line[i] == '\t')) i++;
            if (i < n || sep_start < n)
                separators.push_back(line.substr(sep_start, i - sep_start));
        }
    } else {
        size_t pos = 0;
        separators.push_back("");
        while (true) {
            size_t found = line.find(delim, pos);
            if (found == std::string::npos) {
                tokens.push_back(line.substr(pos));
                break;
            }
            tokens.push_back(line.substr(pos, found - pos));
            separators.push_back(delim);
            pos = found + delim.size();
        }
    }

    int target_field = opts.field - 1;

    std::string output;
    output += separators[0];
    for (size_t i = 0; i < tokens.size(); i++) {
        if ((int)i == target_field) {
            std::string converted;
            if (process_field(tokens[i], opts, converted)) {
                output += converted;
            } else {
                if (opts.invalid == InvalidMode::ABORT || opts.invalid == InvalidMode::FAIL) {
                    fprintf(stderr, "numfmt: invalid number: '%s'\n", tokens[i].c_str());
                    *had_error = true;
                } else if (opts.invalid == InvalidMode::WARN) {
                    fprintf(stderr, "numfmt: invalid number: '%s'\n", tokens[i].c_str());
                }
                output += tokens[i];
            }
        } else {
            output += tokens[i];
        }
        if (i + 1 < separators.size()) {
            output += separators[i + 1];
        } else if (i + 1 < tokens.size()) {
            output += use_whitespace ? " " : delim;
        }
    }

    fputs(output.c_str(), stdout);
    fputc('\n', stdout);
}

static void print_help() {
    printf("Usage: numfmt [OPTION]... [NUMBER]...\n");
    printf("Reformat NUMBER(s), or the numbers from standard input if none are specified.\n");
    printf("\n");
    printf("      --from=UNIT       auto-scale input numbers to UNITs; default is 'none';\n");
    printf("                          UNIT is one of: none, auto, si, iec, iec-i\n");
    printf("      --from-unit=N     specify the input unit size (instead of the default 1)\n");
    printf("      --to=UNIT         auto-scale output numbers to UNITs; default is 'none';\n");
    printf("                          UNIT is one of: none, si, iec, iec-i\n");
    printf("      --to-unit=N       the output unit size (instead of the default 1)\n");
    printf("      --round=METHOD    use METHOD for rounding when scaling; METHOD is one of:\n");
    printf("                          up, down, from-zero (default), towards-zero, nearest\n");
    printf("      --padding=N       pad the output to N characters; positive N will\n");
    printf("                          right-align; negative N will left-align;\n");
    printf("                          padding is ignored if the output is wider than N\n");
    printf("      --suffix=SUFFIX   add SUFFIX to output numbers, and accept optional\n");
    printf("                          SUFFIX in input numbers\n");
    printf("      --format=FORMAT   use printf style floating-point FORMAT;\n");
    printf("                          see FORMAT below for details\n");
    printf("  -d, --delimiter=X     use X instead of whitespace for field delimiter\n");
    printf("      --field=FIELDS    replace the numbers in these input fields (default=1)\n");
    printf("      --grouping        use locale-defined grouping of digits\n");
    printf("      --header[=N]      print (without converting) the first N header lines;\n");
    printf("                          N defaults to 1 if not specified\n");
    printf("      --invalid=MODE    failure mode for invalid numbers: MODE is one of:\n");
    printf("                          abort (default), fail, warn, ignore\n");
    printf("      --debug           print warnings about invalid input\n");
    printf("  -h, --help            display this help and exit\n");
    printf("      --version         output version information and exit\n");
    printf("\n");
    printf("UNIT options:\n");
    printf("  none   no auto-scaling is done; suffixes will trigger an error\n");
    printf("  auto   accept optional single/two letter suffix:\n");
    printf("           1K = 1000, 1Ki = 1024, 1M = 1000000, 1Mi = 1048576\n");
    printf("  si     accept optional single letter suffix:\n");
    printf("           1K = 1000, 1M = 1000000, ...\n");
    printf("  iec    accept optional single letter suffix:\n");
    printf("           1K = 1024, 1M = 1048576, ...\n");
    printf("  iec-i  accept optional two-letter suffix:\n");
    printf("           1Ki = 1024, 1Mi = 1048576, ...\n");
}

void numfmt_command(int argc, char** argv) {
    NumfmtOptions opts;
    std::vector<const char*> operands;

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            print_help();
            return;
        } else if (strcmp(a, "--version") == 0) {
            printf("numfmt (modbox) 1.0\n");
            return;
        } else if (strncmp(a, "--from=", 7) == 0) {
            opts.from = parse_scale(a + 7);
        } else if (strncmp(a, "--to=", 5) == 0) {
            opts.to = parse_scale(a + 5);
        } else if (strncmp(a, "--from-unit=", 12) == 0) {
            opts.from_unit = strtod(a + 12, nullptr);
        } else if (strncmp(a, "--to-unit=", 10) == 0) {
            opts.to_unit = strtod(a + 10, nullptr);
        } else if (strncmp(a, "--padding=", 10) == 0) {
            opts.padding = atoi(a + 10);
        } else if (strncmp(a, "--round=", 8) == 0) {
            opts.round = parse_round(a + 8);
        } else if (strncmp(a, "--suffix=", 9) == 0) {
            opts.suffix = a + 9;
        } else if (strncmp(a, "--format=", 9) == 0) {
            opts.format = a + 9;
        } else if (strncmp(a, "--field=", 8) == 0) {
            opts.field = atoi(a + 8);
        } else if (strcmp(a, "--grouping") == 0) {
            opts.grouping = true;
        } else if (strcmp(a, "--header") == 0) {
            opts.header = 1;
        } else if (strncmp(a, "--header=", 9) == 0) {
            opts.header = atoi(a + 9);
        } else if (strncmp(a, "--invalid=", 10) == 0) {
            opts.invalid = parse_invalid(a + 10);
        } else if (strcmp(a, "--debug") == 0) {
            opts.debug = true;
        } else if (strcmp(a, "-d") == 0 || strcmp(a, "--delimiter") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "numfmt: option requires an argument -- 'd'\n");
                return;
            }
            opts.delimiter = argv[++i];
        } else if (strncmp(a, "--delimiter=", 12) == 0) {
            opts.delimiter = a + 12;
        } else if (strncmp(a, "-d", 2) == 0 && strlen(a) > 2) {
            opts.delimiter = a + 2;
        } else if (strcmp(a, "--") == 0) {
            for (int j = i + 1; j < argc; j++) {
                operands.push_back(argv[j]);
            }
            break;
        } else if (a[0] == '-' && a[1] == '-') {
            fprintf(stderr, "numfmt: unrecognized option '%s'\n", a);
            return;
        } else {
            operands.push_back(a);
        }
    }

    bool had_error = false;

    if (!operands.empty()) {
        for (const char* op : operands) {
            std::string out;
            if (process_field(op, opts, out)) {
                fputs(out.c_str(), stdout);
                fputc('\n', stdout);
            } else {
                if (opts.invalid == InvalidMode::ABORT || opts.invalid == InvalidMode::FAIL) {
                    fprintf(stderr, "numfmt: invalid number: '%s'\n", op);
                    had_error = true;
                } else if (opts.invalid == InvalidMode::WARN) {
                    fprintf(stderr, "numfmt: invalid number: '%s'\n", op);
                    fputs(op, stdout);
                    fputc('\n', stdout);
                } else {
                    fputs(op, stdout);
                    fputc('\n', stdout);
                }
            }
        }
    } else {
        char* line = nullptr;
        size_t len = 0;
        ssize_t nread;
        int header_lines = opts.header;

        while ((nread = getline(&line, &len, stdin)) != -1) {
            if (nread > 0 && line[nread - 1] == '\n') {
                line[nread - 1] = '\0';
            }
            if (header_lines > 0) {
                puts(line);
                header_lines--;
                continue;
            }
            std::string l(line);
            process_line(l, opts, &had_error);
        }
        free(line);
    }

    (void)had_error;
}

REGISTER_COMMAND("numfmt", numfmt_command, "Reformat numbers");
