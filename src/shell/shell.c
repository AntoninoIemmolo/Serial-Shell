#include "shell.h"
#include "history.h"
#include "uart.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ANSI_CLEAR "\033[2J\033[H"
#define UNKNOWN_MSG "Unknown command\n"
#define IN_BUF_DIM 512
#define OUT_BUF_DIM 512
#define STDOUT_OPTIONS "\033[1m"
#define STDIN_OPTIONS "\033[33m"
#define REMOVE_OPTIONS "\033[0m"

struct shell {
    int uart_fp;
    char* std_i;
    char* std_o;
    history_t history;
};

static void renderLine(shell_t shell)
{
    fprintf(stdout, "\r\033[2K"); // clear line
    fprintf(stdout, STDOUT_OPTIONS);
    for (size_t i = 0; i < strlen(shell->std_o); i++) {
        if (shell->std_o[i] == '\n') {
            fprintf(stdout, "\r");
            fprintf(stdout, "%c", shell->std_o[i]);
            memccpy(shell->std_o, shell->std_o + i + 1, OUT_BUF_DIM - i - 1, OUT_BUF_DIM);
            i = 0;
        } else {
            fprintf(stdout, "%c", shell->std_o[i]);
        }
    }
    fprintf(stdout, REMOVE_OPTIONS);

    fprintf(stdout, STDIN_OPTIONS);
    fprintf(stdout, "%s", shell->std_i);
    fprintf(stdout, REMOVE_OPTIONS);
    fflush(stdout);
}

// The return value is a "\0" terminated sting that is inteded to be written in the
// stdout, the output buffer is intended to be written in the UART file descriptor
static void shell_process_cmd(shell_t const shell)
{

    // if it's a NULL pointer or a 0 len char arr
    if (!shell || !(shell->std_i) || strlen(shell->std_i) == 0)
        return;

    // cut the std_i buff to remove \r and \n
    char cmd[IN_BUF_DIM];
    memccpy(cmd, shell->std_i, IN_BUF_DIM, IN_BUF_DIM);
    for (size_t i = 0; i < strlen(cmd); i++) {
        if (cmd[i] == '\r' || cmd[i] == '\n') {
            cmd[i] = '\0';
            break;
        }
    }

    historyPush(shell->history, cmd);

    // if it's not a command print it to uart_fp
    if (shell->std_i[0] != '@') {
        // memccpy(shell->std_i, shell->std_i + strlen(shell->std_i), IN_BUF_DIM - strlen(shell->std_i), IN_BUF_DIM);
        uart_write(shell->uart_fp, shell->std_i, strlen(shell->std_i));
    }
    // if it's CLEAR_CMD command print an error msg in the stderr
    else if (strcmp(cmd, CLEAR_CMD) == 0) {
        fprintf(stderr, "%s\n", ANSI_CLEAR);
        fflush(stderr);
    }
    // if it's an unknown command print an error msg in the stderr
    else {
        fprintf(stderr, "ERROR: %s\n", UNKNOWN_MSG);
        fflush(stderr);
    }
    return;
}

int shell_process_stdin(shell_t const shell, char const c)
{
    int tmp = 0;
    historyReset(shell->history);

    if (c == '\n') {
        // [c, i, \0] len = 2
        if (strlen(shell->std_i) + 3 < IN_BUF_DIM) {
            tmp = strlen(shell->std_i);
            shell->std_i[tmp] = '\r';
            shell->std_i[tmp + 1] = c;
            shell->std_i[tmp + 2] = '\0';
        } else {
            fprintf(stderr, "ERROR: input buffer overflow\n");
            fflush(stderr);
            return EXIT_FAILURE;
        }

        shell_process_cmd(shell);
        renderLine(shell);
        memccpy(shell->std_i, shell->std_i + strlen(shell->std_i), IN_BUF_DIM - strlen(shell->std_i), IN_BUF_DIM);
        shell->std_o[0] = '\0';

    }
    // back space
    else if (c == '\b' || c == '\x7f') {
        if (strlen(shell->std_i) > 0) {
            shell->std_i[strlen(shell->std_i) - 1] = '\0';
        }
        renderLine(shell);
    }
    // Accumulate in the buffer the new character and null terminate the string
    else {
        if (strlen(shell->std_i) + 1 < IN_BUF_DIM) {
            tmp = strlen(shell->std_i);
            shell->std_i[tmp] = c;
            shell->std_i[tmp + 1] = '\0';
            renderLine(shell);
        } else {
            fprintf(stderr, "ERROR: input buffer overflow\r\n");
            fflush(stderr);
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

int shell_process_uart(shell_t const shell, char const* const c_arr)
{
    int16_t tmp = strlen(shell->std_o);
    for (size_t i = 0; i < strlen(c_arr) + 1; i++) {
        if (tmp + i < OUT_BUF_DIM) {
            shell->std_o[tmp + i] = c_arr[i];
        } else {
            fprintf(stderr, "ERROR: output buffer overflow\r\n");
            fflush(stderr);
            return EXIT_FAILURE;
        }
    }
    renderLine(shell);
    return EXIT_SUCCESS;
}

int shell_init(shell_t* newShell, char const* const port, int32_t const baud)
{
    int uart_fd = uart_open(port, baud);
    if (uart_fd < 0) {
        return -1;
    }
    *newShell = malloc(sizeof(**newShell));
    (*newShell)->std_o = malloc(sizeof(*((*newShell)->std_o)) * OUT_BUF_DIM);
    (*newShell)->std_o[0] = '\0';
    (*newShell)->std_i = malloc(sizeof(*((*newShell)->std_i)) * IN_BUF_DIM);
    (*newShell)->std_i[0] = '\0';
    (*newShell)->uart_fp = uart_fd;
    (*newShell)->history = historyInit();

    return uart_fd;
}
void shell_free(shell_t shell)
{
    historyFree(shell->history);
    free(shell->std_o);
    free(shell->std_i);
    free(shell);
}

void shell_hist_up(shell_t shell)
{
    char* str = historyGetPrevious(shell->history);
    if (str != NULL) {
        memccpy(shell->std_i, str, strlen(str), IN_BUF_DIM);
        free(str);
    }
    renderLine(shell);
}

void shell_hist_down(shell_t shell)
{
    char* str = historyGetNext(shell->history);
    if (str != NULL) {
        memccpy(shell->std_i, str, strlen(str), IN_BUF_DIM);
        free(str);
    }
    renderLine(shell);
}
