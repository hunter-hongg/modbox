#ifndef TEE_HPP
#define TEE_HPP

struct TeeOptions {
    int append = 0;           // -a / --append
    int ignore_interrupts = 0; // -i / --ignore-interrupts
    int error_action = 0;     // -p / --error-action (0=warn, 1=warn-nopipe, 2=ignore)
};

void tee_command(int argc, char** argv);

#endif
