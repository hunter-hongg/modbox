#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include "commands/jq.hpp"
#include "commands/command_macros.hpp"
#include "commands/version_util.hpp"

namespace {
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

static void json_print(const JsonValue& v, bool compact, bool raw) {
    if (v.type == JsonValue::String && raw) { printf("%s\n", v.str.c_str()); return; }
    if (compact) {
        // print without spaces
        if (v.type == JsonValue::Null) printf("null");
        else if (v.type == JsonValue::Bool) printf(v.b ? "true" : "false");
        else if (v.type == JsonValue::Number) printf("%g", v.num);
        else if (v.type == JsonValue::String) { putchar('"'); for(char c: v.str){ if(c=='"'||c=='\\') putchar('\\'); putchar(c);} putchar('"');}
        else if (v.type == JsonValue::Array) { printf("["); for(size_t i=0;i<v.arr.size();++i){ if(i) printf(","); json_print(v.arr[i], true, raw);} printf("]"); }
        else if (v.type == JsonValue::Object) { printf("{"); bool first=true; for(auto& p: v.obj){ if(!first) printf(","); first=false; putchar('"'); for(char c:p.first){ if(c=='"'||c=='\\') putchar('\\'); putchar(c);} printf("\":"); json_print(p.second,true,raw);} printf("}"); }
        printf("\n");
        return;
    }
    // pretty
    if (v.type == JsonValue::Null) printf("null\n");
    else if (v.type == JsonValue::Bool) printf(v.b ? "true\n" : "false\n");
    else if (v.type == JsonValue::Number) printf("%g\n", v.num);
    else if (v.type == JsonValue::String) { if (raw) printf("%s\n", v.str.c_str()); else { printf("\""); for(char c: v.str){ if(c=='"'||c=='\\') putchar('\\'); putchar(c);} printf("\"\n"); } }
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

static bool compare_gt(const JsonValue& a, const JsonValue& b) { return json_to_number(a) > json_to_number(b); }
static bool compare_eq(const JsonValue& a, const JsonValue& b) { return json_to_number(a) == json_to_number(b); }

static JsonValue apply_filter(const JsonValue& input, const std::string& filt, bool raw, bool compact) {
    // Very simplified: support dot path like .name, .a.b[0]
    // Also support length
    if (filt == "length") {
        if (input.type == JsonValue::Array) return JsonValue::make_number(input.arr.size());
        if (input.type == JsonValue::Object) return JsonValue::make_number(input.obj.size());
        return JsonValue::make_number(0);
    }
    std::string path = filt;
    if (!path.empty() && path[0]=='.') path = path.substr(1);
    // split by '.' but keep [ ]
    JsonValue cur = input;
    size_t pos = 0;
    while (pos < path.size()) {
        size_t dot = path.find('.', pos);
        std::string token = (dot==std::string::npos) ? path.substr(pos) : path.substr(pos, dot-pos);
        // parse token with optional [index]
        size_t br = token.find('[');
        std::string name = token;
        long idx = -1;
        if (br != std::string::npos) {
            name = token.substr(0, br);
            size_t br2 = token.find(']', br);
            if (br2 != std::string::npos) {
                std::string num = token.substr(br+1, br2-br-1);
                idx = strtol(num.c_str(), nullptr, 10);
            }
        }
        if (!name.empty()) cur = get_field(cur, name);
        if (idx >= 0) cur = get_index(cur, (size_t)idx);
        if (dot == std::string::npos) break;
        pos = dot+1;
    }
    return cur;
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
    bool raw = false, compact = false, slurp = false;
    std::string filter;
    std::vector<std::string> files;
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--help")==0) { print_help(argv[0]); return 0; }
        if (strcmp(a, "--version")==0) { print_version("jq"); return 0; }
        if (strcmp(a, "-r")==0 || strcmp(a, "--raw-output")==0) { raw = true; continue; }
        if (strcmp(a, "-c")==0 || strcmp(a, "--compact-output")==0) { compact = true; continue; }
        if (strcmp(a, "-s")==0 || strcmp(a, "--slurp")==0) { slurp = true; continue; }
        if (a[0]=='-') { fprintf(stderr, "jq: unknown option %s\n", a); return 1; }
        if (filter.empty() && a[0]=='.') { filter = a; continue; }
        if (filter.empty() && (a[0]!='-' || strlen(a)>1)) { /* treat as filter */ filter = a; continue; }
        files.push_back(a);
    }
    if (filter.empty()) filter = ".";
    if (files.empty()) {
        // read stdin
        std::string data((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());
        if (data.empty()) return 0;
        JsonParser p(data);
        JsonValue v = p.parse();
        JsonValue out = apply_filter(v, filter, raw, compact);
        json_print(out, compact, raw);
        return 0;
    }
    // file processing
    std::vector<JsonValue> all;
    for (auto& f : files) {
        FILE* fp = fopen(f.c_str(), "r");
        if (!fp) { fprintf(stderr, "jq: cannot open %s\n", f.c_str()); return 1; }
        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp); fseek(fp,0,SEEK_SET);
        std::string buf(sz+1, '\0');
        fread(&buf[0],1,sz,fp); fclose(fp);
        JsonParser p(buf);
        JsonValue v = p.parse();
        all.push_back(v);
    }
    JsonValue input;
    if (slurp) {
        input = JsonValue::make_array(all);
    } else {
        if (all.size()==1) input = all[0];
        else input = JsonValue::make_array(all);
    }
    JsonValue out = apply_filter(input, filter, raw, compact);
    json_print(out, compact, raw);
    return 0;
}

} // namespace

REGISTER_COMMAND("jq", jq_command, "Process JSON inputs using jq-like filters");
