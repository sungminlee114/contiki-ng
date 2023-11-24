#ifndef _SHELL_COMMANDS_H_
#define _SHELL_COMMANDS_H_

typedef char (shell_commands_func)(struct pt *pt, char *args);

struct shell_command_t {
  const char *name;
  shell_commands_func *func;
  const char *help;
};

extern struct shell_command_t builtin_shell_commands[];

#define SHELL_OUTPUT(format, ...) do {             \
    char buffer[192];                                           \
    snprintf(buffer, sizeof(buffer), format, ##__VA_ARGS__);    \
    shell_printf(buffer);                                      \
  } while(0);


#endif /* _SHELL_COMMANDS_H_ */