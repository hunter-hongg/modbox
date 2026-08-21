#ifndef DIG_HPP
#define DIG_HPP

struct DigOpts {
    std::string domain;
    int          qtype   = T_A;
    std::string  server  = "";
    bool         show_all        = true;
    bool         show_answer     = true;
    bool         show_header     = true;
    bool         show_question   = true;
    bool         show_authority  = false;
    bool         show_additional = false;
    bool         short_mode      = false;
    bool         reverse_lookup  = false;
    std::string  reverse_ip;
    bool         show_trace      = false;
};

int dig_command(int argc, char** argv);

#endif
