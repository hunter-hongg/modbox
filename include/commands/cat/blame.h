#ifndef CAT_BLAME_H
#define CAT_BLAME_H

typedef struct {
    char commit[9];
    char author[32];
    char date[16];
} BlameInfo;

BlameInfo* parse_blame(const char* path, int* count);
void free_blame(BlameInfo* blame, int count);

#endif
