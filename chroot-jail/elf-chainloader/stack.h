
struct abi_stack {
    long argc;
    char *argv[0]; /* NULL-terminated */
    /* These follow:
       char *env[]; // NULL-terminated
       auxv
    */
};

struct args {
    struct abi_stack *new_stack; /* Output */
    struct abi_stack stack; /* Input */
};
