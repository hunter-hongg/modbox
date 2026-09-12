#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

#include <syslog.h>

#include <argtable3.h>

#include "commands/logger.hpp"
#include "commands/arg_util.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

namespace {

// A facility name mapped to its syslog(3) constant.
struct FacilityEntry {
    const char* name;
    int value;
};

// Facility names accepted by GNU logger's -p/--priority facility prefix.
const FacilityEntry kFacilities[] = {
    {.name = "auth", .value = LOG_AUTH},
#ifdef LOG_AUTHPRIV
    {.name = "authpriv", .value = LOG_AUTHPRIV},
#endif
    {.name = "cron", .value = LOG_CRON},
    {.name = "daemon", .value = LOG_DAEMON},
    {.name = "ftp", .value = LOG_FTP},
    {.name = "kern", .value = LOG_KERN},
    {.name = "lpr", .value = LOG_LPR},
    {.name = "mail", .value = LOG_MAIL},
    {.name = "news", .value = LOG_NEWS},
    {.name = "syslog", .value = LOG_SYSLOG},
    {.name = "user", .value = LOG_USER},
    {.name = "uucp", .value = LOG_UUCP},
    {.name = "local0", .value = LOG_LOCAL0},
    {.name = "local1", .value = LOG_LOCAL1},
    {.name = "local2", .value = LOG_LOCAL2},
    {.name = "local3", .value = LOG_LOCAL3},
    {.name = "local4", .value = LOG_LOCAL4},
    {.name = "local5", .value = LOG_LOCAL5},
    {.name = "local6", .value = LOG_LOCAL6},
    {.name = "local7", .value = LOG_LOCAL7},
};

// Severity names accepted by -p/--priority (with or without facility prefix).
struct SeverityEntry {
    const char* name;
    int value;
};

const SeverityEntry kSeverities[] = {
    {.name = "emerg", .value = LOG_EMERG},
    {.name = "alert", .value = LOG_ALERT},
    {.name = "crit", .value = LOG_CRIT},
    {.name = "err", .value = LOG_ERR},
    {.name = "warning", .value = LOG_WARNING},
    {.name = "notice", .value = LOG_NOTICE},
    {.name = "info", .value = LOG_INFO},
    {.name = "debug", .value = LOG_DEBUG},
};

bool lookup_facility(const std::string& name, int* out) {
    const auto* const it = std::ranges::find_if(kFacilities, [&](const FacilityEntry& f) { return name == f.name; });
    if (it == std::end(kFacilities)) {
        return false;
    }
    *out = it->value;
    return true;
}

bool lookup_severity(const std::string& name, int* out) {
    const auto* const it = std::ranges::find_if(kSeverities, [&](const SeverityEntry& s) { return name == s.name; });
    if (it == std::end(kSeverities)) {
        return false;
    }
    *out = it->value;
    return true;
}

// Parse a -p argument of the form "[facility.]severity". When no facility
// prefix is given the caller's default facility is preserved. Returns false on
// an unrecognized facility or severity name.
bool parse_priority(const std::string& spec, int* facility, int* severity) {
    const size_t dot = spec.find('.');
    if (dot != std::string::npos) {
        if (!lookup_facility(spec.substr(0, dot), facility)) {
            return false;
        }
    }
    const std::string severity_name = (dot == std::string::npos) ? spec : spec.substr(dot + 1);
    return lookup_severity(severity_name, severity);
}

std::string join_args(int count, const char* const* values) {
    std::string message;
    for (int i = 0; i < count; i++) {
        if (i > 0) {
            message += " ";
        }
        message += values[i];
    }
    return message;
}

std::string read_stream(std::FILE* stream) {
    std::string data;
    char buf[4096];
    size_t n = 0;
    while ((n = fread(buf, 1, sizeof(buf), stream)) > 0) {
        data.append(buf, n);
    }
    return data;
}

// Read a whole file (or stdin for "-") into a string. Returns false on error.
bool read_file_arg(const std::string& path, std::string* out) {
    if (path == "-") {
        *out = read_stream(stdin);
        return true;
    }
    std::FILE* fp = std::fopen(path.c_str(), "rb");
    if (fp == nullptr) {
        (void)fprintf(stderr, "logger: %s: %s\n", path.c_str(), std::strerror(errno));
        return false;
    }
    *out = read_stream(fp);
    (void)std::fclose(fp);
    return true;
}

void strip_trailing_newline(std::string* text) {
    if (!text->empty() && text->back() == '\n') {
        text->pop_back();
    }
}

// Collect the message from positional arguments, a --file argument, or stdin.
bool collect_message(arg_str* msg_args, arg_str* file_opt, std::string* out) {
    if (file_opt->count > 0) {
        if (!read_file_arg(file_opt->sval[0], out)) {
            return false;
        }
        strip_trailing_newline(out);
        return true;
    }
    if (msg_args->count > 0) {
        *out = join_args(msg_args->count, msg_args->sval);
        return true;
    }
    *out = read_stream(stdin);
    strip_trailing_newline(out);
    return true;
}

}  // namespace

int logger_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0("V", "version", "output version information and exit");
    struct arg_str* priority_opt = arg_str0("p", "priority", "<pri>", "pri is facility.severity");
    struct arg_str* tag_opt = arg_str0("t", "tag", "<tag>", "mark every line with the specified tag");
    struct arg_str* file_opt = arg_str0("f", "file", "<file>", "log the content of the specified file");
    struct arg_lit* stderr_opt = arg_lit0("s", "stderr", "log the message to standard error as well as to the system log");
    struct arg_lit* pid_opt = arg_lit0("i", "id", "log the process id with each message");
    struct arg_str* msg_args = arg_strn(NULL, NULL, "<message>", 0, argc, "message to log");
    struct arg_end* end = arg_end(20);

    ArgTable at({help_opt, version_opt, priority_opt, tag_opt, file_opt, stderr_opt, pid_opt, msg_args, end});
    int const nerrors = at.parse(argc, argv);

    if (help_opt->count > 0) {
        printf("Usage: logger [OPTION]... [MESSAGE]\n");
        printf("Enter messages into the system log.\n");
        printf("\n");
        printf("  -f, --file <file>      log the content of the specified file\n");
        printf("  -i, --id               log the process id with each message\n");
        printf("  -p, --priority <pri>   pri is facility.severity (default user.notice)\n");
        printf("  -s, --stderr           also log the message to standard error\n");
        printf("  -t, --tag <tag>        mark every line with the specified tag\n");
        printf("  -h, --help             display this help and exit\n");
        printf("  -V, --version          output version information and exit\n");
        printf("\n");
        printf("Facilities: auth, authpriv, cron, daemon, ftp, kern, lpr, mail,\n");
        printf("            news, syslog, user, uucp, local0..local7\n");
        printf("Severities: emerg, alert, crit, err, warning, notice, info, debug\n");
        return 0;
    }

    if (version_opt->count > 0) {
        print_version("logger");
        return 0;
    }

    if (nerrors > 0) {
        return print_arg_errors(end, argv[0]);
    }

    int facility = LOG_USER;
    int severity = LOG_NOTICE;
    if (priority_opt->count > 0) {
        if (!parse_priority(priority_opt->sval[0], &facility, &severity)) {
            (void)fprintf(stderr, "logger: unknown facility/priority: %s\n", priority_opt->sval[0]);
            return 1;
        }
    }

    if (file_opt->count > 0 && msg_args->count > 0) {
        (void)fprintf(stderr, "logger: --file and a message argument are mutually exclusive\n");
        return 1;
    }

    std::string message;
    if (!collect_message(msg_args, file_opt, &message)) {
        return 1;
    }

    openlog(tag_opt->count > 0 ? tag_opt->sval[0] : nullptr,
            (pid_opt->count > 0 ? LOG_PID : 0) | LOG_NDELAY, facility);
    syslog(severity, "%s", message.c_str());
    if (stderr_opt->count > 0) {
        (void)fprintf(stderr, "%s\n", message.c_str());
    }
    closelog();
    return 0;
}

REGISTER_COMMAND("logger", logger_command, "Enter messages into the system log");
