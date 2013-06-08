#ifndef _cmd_table_h_
#define _cmd_table_h_

typedef int (*cmd_handler_t)(void *user_data, int argc, char **argv);

typedef struct cmd cmd_t;
struct cmd
{
    char *name;
    cmd_handler_t func;
    int required_arguments;
};

int cmd_dispatch(const char *cmdline,
        const char *delimiters,
        int allow_null_elements,
        cmd_t *command_table,
        void *user_data);

#endif

