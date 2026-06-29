#ifndef CAT_BLAME_HPP
#define CAT_BLAME_HPP

#define COMMIT_SHA_BUF_SIZE 9

typedef struct {
    char commit[COMMIT_SHA_BUF_SIZE];
    char author[32];
    char date[16];
} BlameInfo;

BlameInfo* parse_blame(const char* path, int* count);
void free_blame(BlameInfo* blame, int count);

#endif
