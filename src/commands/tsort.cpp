#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <argtable3.h>
#include "commands/tsort.hpp"
#include "commands/command_macros.hpp"

void tsort_command(int argc, char** argv) {
    struct arg_lit* help_opt = arg_lit0("h", "help", "display this help and exit");
    struct arg_file* file_arg = arg_filen(NULL, NULL, "FILE", 0, 1, "input file");
    struct arg_end* end = arg_end(20);

    void* argtable[] = {help_opt, file_arg, end};
    int nerrors = arg_parse(argc, argv, argtable);

    if (help_opt->count > 0) {
        printf("Usage: %s [OPTION]... [FILE]\n", argv[0]);
        printf("Write totally ordered list consistent with the partial ordering in FILE.\n");
        printf("\n");
        printf("  -h, --help    display this help and exit\n");
        printf("\n");
        printf("Input format: pairs of whitespace-separated items.\n");
        printf("Each pair 'A B' means A must precede B in the output.\n");
        printf("With no FILE, or when FILE is -, read standard input.\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    FILE* fp = stdin;
    if (file_arg->count > 0 && strcmp(file_arg->filename[0], "-") != 0) {
        fp = fopen(file_arg->filename[0], "r");
        if (!fp) {
            fprintf(stderr, "%s: cannot open '%s': %s\n",
                    argv[0], file_arg->filename[0], strerror(errno));
            arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
            return;
        }
    }

    // Read all whitespace-separated tokens from input
    std::vector<std::string> tokens;
    char buf[65536];

    while (fgets(buf, sizeof(buf), fp)) {
        char* p = buf;
        while (*p) {
            // skip whitespace
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
                p++;
            }
            if (!*p) {
                break;
            }

            // read token
            char* start = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
                p++;
            }
            tokens.emplace_back(start, p - start);
        }
    }

    if (fp != stdin) {
        fclose(fp);
    }

    if (tokens.empty()) {
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return;
    }

    // Build directed graph (adjacency list) and in-degree counts.
    // Every token appearing is a node; every consecutive pair (i, i+1) is an edge.
    std::map<std::string, std::vector<std::string>> graph;
    std::map<std::string, int> indegree;

    for (const auto& t : tokens) {
        if (graph.find(t) == graph.end()) {
            graph[t] = {};
            indegree[t] = 0;
        }
    }

    for (size_t i = 0; i + 1 < tokens.size(); i += 2) {
        const std::string& from = tokens[i];
        const std::string& to = tokens[i + 1];
        graph[from].push_back(to);
        indegree[to]++;
    }

    // Kahn's algorithm for topological sort
    std::queue<std::string> q;
    for (const auto& [node, deg] : indegree) {
        if (deg == 0) {
            q.push(node);
        }
    }

    std::vector<std::string> sorted;
    while (!q.empty()) {
        std::string node = q.front();
        q.pop();
        sorted.push_back(node);

        for (const auto& neighbor : graph[node]) {
            indegree[neighbor]--;
            if (indegree[neighbor] == 0) {
                q.push(neighbor);
            }
        }
    }

    // Output topological order (nodes not in sorted set are cycle nodes,
    // appended at the end to match GNU tsort behavior)
    for (const auto& s : sorted) {
        printf("%s\n", s.c_str());
    }

    bool has_cycle = (sorted.size() != graph.size());

    if (has_cycle) {
        fprintf(stderr, "%s: input contains a cycle\n", argv[0]);
        for (const auto& [node, _] : graph) {
            if (indegree[node] > 0) {
                printf("%s\n", node.c_str());
            }
        }
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));

    if (has_cycle) {
        exit(1);
    }
}

REGISTER_COMMAND("tsort", tsort_command, "Topological sort");
