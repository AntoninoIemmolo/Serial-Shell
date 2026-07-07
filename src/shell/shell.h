#ifndef SHELL_H
#define SHELL_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLEAR_CMD "@clear"

typedef struct shell* shell_t;
int shell_init(shell_t* newShell, char const* const port, int32_t const baud);
void shell_free(shell_t shell);
int shell_process_stdin(shell_t shell, char const c);
int shell_process_uart(shell_t const shell, char const* const c_arr);
void shell_hist_up(shell_t shell);
void shell_hist_down(shell_t shell);
#endif
