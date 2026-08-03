#ifndef HTTP_CLIENT_HPP
#define HTTP_CLIENT_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

struct UrlParts {
    std::string scheme;   // "http" or "https"
    std::string host;
    int port = 0;         // 0 means use default
    std::string path;
    std::string query;
    std::string fragment;
};

struct HttpResponse {
    int status_code = 0;
    std::string status_text;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    std::string final_url;
    double time_total = 0.0;
    double time_connect = 0.0;
    long size_download = 0;
    long size_upload = 0;
    int num_redirects = 0;
};

struct CurlOptions {
    std::string method = "GET";
    std::string url;
    std::string output_file;          // -o
    std::string remote_name;          // -O
    std::string dump_header_file;     // -D
    std::vector<std::pair<std::string, std::string>> custom_headers;
    std::string user;
    std::string password;
    std::string post_data;
    bool data_urlencode = false;
    bool get_with_data = false;       // -G
    bool silent = false;              // -s
    bool verbose = false;             // -v
    bool include_headers = false;     // -i
    bool head_only = false;           // -I
    bool follow_redirects = false;    // -L
    int max_redirs = 20;
    bool insecure = false;            // -k
    double max_time = 0.0;
    double connect_timeout = 0.0;
    int retry_count = 0;
    bool retry_connrefused = false;
    bool fail_on_http_error = false;  // -f
    bool fail_with_body = false;      // --fail-with-body
    bool show_writeout = false;
    std::string writeout_format;
    bool show_progress = true;
    bool progress_bar = false;
};

// Parse a URL string into components. Returns false on failure.
bool parse_url(const char* url_str, UrlParts& out);

// Get default port for scheme
int default_port(const std::string& scheme);

// Build the request target (path + query) for the HTTP request line
std::string build_request_target(const UrlParts& url);

// Encode a string for use in application/x-www-form-urlencoded
std::string url_encode(const std::string& input);

// Send an HTTP request and return the response.
// Returns 0 on success, non-zero on error (error message written to stderr).
int http_request(const CurlOptions& opts, HttpResponse& response);

#endif
