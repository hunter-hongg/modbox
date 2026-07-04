#ifndef PTX_HPP
#define PTX_HPP

struct PtxOptions {
    int auto_reference = 0;
    int right_side_refs = 0;
    int traditional = 0;
    int typeset_mode = 0;
    int references = 0;
    
    int width = 72;
    int gap_size = 3;
    
    const char* sentence_regexp = nullptr;
    const char* break_file = nullptr;
    const char* ignore_file = nullptr;
    const char* only_file = nullptr;
};

void ptx_command(int argc, char** argv);

#endif
