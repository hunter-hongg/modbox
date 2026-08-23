#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <argtable3.h>
#include "commands/jq.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"
#include "commands/arg_util.hpp"

namespace {
struct OutputOptions {
    bool raw = false;
    bool compact = false;
    bool slurp = false;
};

struct JsonValue {
    enum Type { Null, Bool, Number, String, Array, Object } type;
    bool b = false;
    double num = 0;
    std::string str;
    std::vector<JsonValue> arr;
    std::unordered_map<std::string, JsonValue> obj;
    JsonValue() : type(Null) {}
    static JsonValue make_null() { JsonValue v; v.type = Null; return v; }
    static JsonValue make_bool(bool v) { JsonValue x; x.type = Bool; x.b = v; return x; }
    static JsonValue make_number(double v) { JsonValue x; x.type = Number; x.num = v; return x; }
    static JsonValue make_string(const std::string& s) { JsonValue x; x.type = String; x.str = s; return x; }
    static JsonValue make_array(std::vector<JsonValue> a) { JsonValue x; x.type = Array; x.arr = std::move(a); return x; }
    static JsonValue make_object(std::unordered_map<std::string, JsonValue> o) { JsonValue x; x.type = Object; x.obj = std::move(o); return x; }
};

class JsonParser {
    const std::string& s;
    size_t i = 0;
    void skip_ws() { while (i < s.size() && isspace((unsigned char)s[i])) i++; }
    char peek() { return i < s.size() ? s[i] : '\0'; }
    char get() { return i < s.size() ? s[i++] : '\0'; }
    JsonValue parse_value() {
        skip_ws();
        char c = peek();
        if (c == 'n') { expect("null"); return JsonValue::make_null(); }
        if (c == 't') { expect("true"); return JsonValue::make_bool(true); }
        if (c == 'f') { expect("false"); return JsonValue::make_bool(false); }
        if (c == '"') return parse_string();
        if (c == '[') return parse_array();
        if (c == '{') return parse_object();
        if (c == '-' || isdigit((unsigned char)c)) return parse_number();
        return JsonValue::make_null();
    }
    void expect(const char* lit) {
        for (const char* p = lit; *p; ++p) { get(); }
    }
    JsonValue parse_string() {
        get(); // "
        std::string out;
        while (true) {
            char c = get();
            if (c == '\0' || c == '"') break;
            if (c == '\\') { char e = get(); if (e == '"' || e == '\\' || e == '/') out.push_back(e); else out.push_back(e); }
            else out.push_back(c);
        }
        return JsonValue::make_string(out);
    }
    JsonValue parse_number() {
        size_t start = i;
        if (peek() == '-') get();
        while (isdigit(peek())) get();
        if (peek() == '.') { get(); while (isdigit(peek())) get(); }
        if (peek() == 'e' || peek() == 'E') { get(); if (peek()=='-'||peek()=='+') get(); while (isdigit(peek())) get(); }
        double v = strtod(s.c_str()+start, nullptr);
        return JsonValue::make_number(v);
    }
    JsonValue parse_array() {
        get(); // [
        std::vector<JsonValue> a;
        skip_ws();
        if (peek() == ']') { get(); return JsonValue::make_array(a); }
        while (true) {
            a.push_back(parse_value());
            skip_ws();
            if (peek() == ',') { get(); continue; }
            if (peek() == ']') { get(); break; }
        }
        return JsonValue::make_array(a);
    }
    JsonValue parse_object() {
        get(); // {
        std::unordered_map<std::string, JsonValue> o;
        skip_ws();
        if (peek() == '}') { get(); return JsonValue::make_object(o); }
        while (true) {
            skip_ws();
            JsonValue k = parse_string();
            skip_ws(); if (get() != ':') break;
            JsonValue v = parse_value();
            o[k.str] = v;
            skip_ws();
            if (peek() == ',') { get(); continue; }
            if (peek() == '}') { get(); break; }
        }
        return JsonValue::make_object(o);
    }
public:
    explicit JsonParser(const std::string& str) : s(str) {}
    JsonValue parse() { JsonValue v = parse_value(); return v; }
};

static void escape_json_string(const std::string& s) {
    for(char c: s){ if(c=='"'||c=='\\') putchar('\\'); putchar(c);}
}

static void json_print(const JsonValue& v, bool compact, bool raw) {
    if (v.type == JsonValue::String && raw) { printf("%s\n", v.str.c_str()); return; }
    if (compact) {
        // print without spaces
        if (v.type == JsonValue::Null) printf("null");
        else if (v.type == JsonValue::Bool) printf(v.b ? "true" : "false");
        else if (v.type == JsonValue::Number) printf("%g", v.num);
        else if (v.type == JsonValue::String) { putchar('"'); escape_json_string(v.str); putchar('"');}
        else if (v.type == JsonValue::Array) { printf("["); for(size_t i=0;i<v.arr.size();++i){ if(i) printf(","); json_print(v.arr[i], true, raw);} printf("]"); }
        else if (v.type == JsonValue::Object) { printf("{"); bool first=true; for(auto& p: v.obj){ if(!first) printf(","); first=false; putchar('"'); escape_json_string(p.first); printf("\":"); json_print(p.second,true,raw);} printf("}"); }
        printf("\n");
        return;
    }
    // pretty
    if (v.type == JsonValue::Null) printf("null\n");
    else if (v.type == JsonValue::Bool) printf(v.b ? "true\n" : "false\n");
    else if (v.type == JsonValue::Number) printf("%g\n", v.num);
    else if (v.type == JsonValue::String) { if (raw) printf("%s\n", v.str.c_str()); else { printf("\""); escape_json_string(v.str); printf("\"\n"); } }
    else if (v.type == JsonValue::Array) { printf("[\n"); for(auto& e:v.arr){ printf("  "); json_print(e,false,raw);} printf("]\n");}
    else if (v.type == JsonValue::Object) { printf("{\n"); for(auto& p:v.obj){ printf("  \"%s\": ", p.first.c_str()); json_print(p.second,false,raw);} printf("}\n");}
}

static JsonValue get_field(const JsonValue& v, const std::string& name) {
    if (v.type == JsonValue::Object) {
        auto it = v.obj.find(name);
        if (it != v.obj.end()) return it->second;
    }
    return JsonValue::make_null();
}
static JsonValue get_index(const JsonValue& v, size_t idx) {
    if (v.type == JsonValue::Array && idx < v.arr.size()) return v.arr[idx];
    return JsonValue::make_null();
}

static double json_to_number(const JsonValue& v) {
    if (v.type == JsonValue::Number) return v.num;
    if (v.type == JsonValue::String) { char* e; double d = strtod(v.str.c_str(), &e); return d; }
    if (v.type == JsonValue::Bool) return v.b ? 1 : 0;
    return 0;
}

static JsonValue apply_filter(const JsonValue& input, const std::string& filt, const OutputOptions&) {
    if (filt == "length") {
        if (input.type == JsonValue::Array) return JsonValue::make_number(input.arr.size());
        if (input.type == JsonValue::Object) return JsonValue::make_number(input.obj.size());
        return JsonValue::make_number(0);
    }
    std::string path = filt;
    if (!path.empty() && path[0]=='.') path = path.substr(1);
    JsonValue current = input;
    size_t pos = 0;
    while (pos < path.size()) {
        size_t dot_pos = path.find('.', pos);
        std::string token = (dot_pos==std::string::npos) ? path.substr(pos) : path.substr(pos, dot_pos-pos);
        size_t bracket_pos = token.find('[');
        std::string name = token;
        long index = -1;
        if (bracket_pos != std::string::npos) {
            name = token.substr(0, bracket_pos);
            size_t bracket_end = token.find(']', bracket_pos);
            if (bracket_end != std::string::npos) {
                std::string num = token.substr(bracket_pos+1, bracket_end-bracket_pos-1);
                index = strtol(num.c_str(), nullptr, 10);
            }
        }
        if (!name.empty()) current = get_field(current, name);
        if (index >= 0) current = get_index(current, (size_t)index);
        if (dot_pos == std::string::npos) break;
        pos = dot_pos+1;
    }
    return current;
}

static void print_help(const char* prog) {
    printf("Usage: %s [options] [filter] [file]\n", prog);
    printf("\n");
    printf("Process JSON inputs using jq-like filters.\n");
    printf("\n");
    printf("      --help     display this help and exit\n");
    printf("      --version  output version information and exit\n");
    printf("\n");
    printf("Options:\n");
    printf("  -r, --raw-output        output raw strings, not JSON\n");
    printf("  -c, --compact-output    compact output\n");
    printf("  -s, --slurp             slurp all inputs into an array\n");
}

int jq_command(int argc, char** argv) {
    if (argc == 1) { print_help(argv[0]); return 2; }
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_lit* version_opt = arg_lit0(NULL, "version", "output version information and exit");
    struct arg_lit* raw_opt = arg_lit0("r", "raw-output", "output raw strings, not JSON");
    struct arg_lit* compact_opt = arg_lit0("c", "compact-output", "compact output");
    struct arg_lit* slurp_opt = arg_lit0("s", "slurp", "slurp all inputs into an array");
    struct arg_str* filter_opt = arg_str0(NULL, NULL, "FILTER", "jq filter expression");
    struct arg_file* file_opt = arg_filen(NULL, NULL, "FILE", 0, 100, "input file(s)");
    struct arg_end* end = arg_end(20);
    ArgTable at({ help_opt, version_opt, raw_opt, compact_opt, slurp_opt, filter_opt, file_opt, end });
    int nerrors = at.parse(argc, argv);
    if (nerrors > 0) { at.print_errors(end, argv[0]); return 1; }
    if (help_opt->count > 0) { print_help(argv[0]); return 0; }
    if (version_opt->count > 0) { print_version("jq"); return 0; }
    OutputOptions opts;
    opts.raw = raw_opt->count > 0;
    opts.compact = compact_opt->count > 0;
    opts.slurp = slurp_opt->count > 0;
    std::string filter;
    if (filter_opt->count > 0) filter = filter_opt->sval[0];
    else filter = ".";
    std::vector<std::string> files;
    for (int i=0;i<file_opt->count;i++) files.push_back(file_opt->filename[i]);
    if (files.empty()) {
        std::string data((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());
        if (data.empty()) return 0;
        JsonParser p(data);
        JsonValue v = p.parse();
        JsonValue out = apply_filter(v, filter, opts);
        json_print(out, opts.compact, opts.raw);
        return 0;
    }
    std::vector<JsonValue> all;
    for (auto& f : files) {
        std::ifstream ifs(f);
        if (!ifs) { fprintf(stderr, "jq: cannot open %s\n", f.c_str()); return 1; }
        std::string buf((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        JsonParser p(buf);
        JsonValue v = p.parse();
        all.push_back(v);
    }
    JsonValue input;
    if (opts.slurp) {
        input = JsonValue::make_array(all);
    } else {
        if (all.size()==1) input = all[0];
        else input = JsonValue::make_array(all);
    }
    JsonValue out = apply_filter(input, filter, opts);
    json_print(out, opts.compact, opts.raw);
    return 0;
}

} // namespace

REGISTER_COMMAND("jq", jq_command, "Process JSON inputs using jq-like filters");
