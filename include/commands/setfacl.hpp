#ifndef SETFACL_HPP
#define SETFACL_HPP

struct SetfaclOptions {
    int is_recursive = 0;
    int is_test_mode = 0;
    int no_mask = 0;
    int recalc_mask = 1;
    int is_physical = 1;
    int is_logical = 0;
    int is_dereference = 0;
    int preserve_root = 0;
    int one_file_system = 0;
    int remove_all = 0;
    int remove_default = 0;
    const char *set_acl = nullptr;
    const char *set_file = nullptr;
    const char *restore_file = nullptr;
    const char *modify_file = nullptr;
    const char *remove_file = nullptr;

    // Accumulated from repeated -m / -x flags
    std::vector<const char *> modify_entries;
    std::vector<const char *> remove_specs;
};

int setfacl_command(int argc, char **argv);

#endif // SETFACL_HPP
