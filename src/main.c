#include "shell.h"
#include "uart.h"
#include <poll.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#define DEFAULT_PORT "/tmp/ttyV0"
#define DEFAULT_BAUD 115200
#define UART_BUF_DIM 512

static void save_original_conf(struct termios* const terminal)
{
    tcgetattr(STDIN_FILENO, terminal);
}

// terminal is passes by value, the callee copy of terminal
// is unchanged
static void set_raw_mode(struct termios terminal)
{
    cfmakeraw(&terminal);
    tcsetattr(STDIN_FILENO, TCSANOW, &terminal);
}

static void restore_terminal(struct termios terminal)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &terminal);
}

int main(int argc, char* argv[])
{    

    int16_t tmp = 0;
    char uart_buf[UART_BUF_DIM];
    uart_buf[0] = '\0';
    shell_t shell;
    const char* port = (argc > 1) ? argv[1] : DEFAULT_PORT;
    int baud = (argc > 2) ? atoi(argv[2]) : DEFAULT_BAUD;
    int uart_fd = shell_init(&shell, port, baud);
    if (uart_fd < 0) {
        fprintf(stderr, "Failed to open %s\n", port);
        return EXIT_FAILURE;
    }

    // set Terminal in raw mode, (don't show user input, input is readable before \n is sent)
    struct termios terminal;
    save_original_conf(&terminal);
    set_raw_mode(terminal);

    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = uart_fd;
    fds[1].events = POLLIN;

    fprintf(stdin, "Type @clear to clear stdin, Ctrl-C to exit.\r\n");
    fflush(stdin);

    while (1) {
        // poll(file_descriptior_arr, number_of_file_descriptior, time_out [ms] -1(for ever))
        int ret = poll(fds, 2, -1);

        // poll failed maybe because of SIGINT sent by the user, exit the loop
        if (ret < 0){
            break;
        }

        // if I've been weken up because of file descriptor 0 have data
        if (fds[0].revents & POLLIN) {
            char c;
            if (read(fds[0].fd, &c, 1) <= 0) {
                fds[0].fd = -1;
            } else if (c == '\x03') {
                break;
            } 
            // Add arrow support
            else if (c == '\x1b' || c == '\033') {
                if (read(fds[0].fd, &c, 1) <= 0) {
                    fds[0].fd = -1;
                } else if (c == '[') {
                    if (read(fds[0].fd, &c, 1) <= 0) {
                        fds[0].fd = -1;
                    } 
                    // up arrow
                    else if (c == 'A') {
                        shell_hist_up(shell);
                    } 
                    // down arrow
                    else if (c == 'B') {
                        shell_hist_down(shell);
                    }
                    // right arrow
                    else if (c == 'C') {
                        cursor_right(shell);
                    }
                    // left arrow
                    else if (c == 'D') {
                        cursor_left(shell);
                    }


                }
            } else if (c == '\r') {
                c = '\n';
                if (shell_process_stdin(shell, c) == EXIT_FAILURE) {
                    break;
                }
            } else if (shell_process_stdin(shell, c) == EXIT_FAILURE) {
                break;
            }
        }

        // if I've been weken up because of file descriptor 0 hang up
        if (fds[0].revents & POLLHUP) { }

        // if I've been weken up because of file descriptor 1 have data
        if (fds[1].revents & POLLIN) {
            tmp = uart_read(fds[1].fd, uart_buf, sizeof(uart_buf));
            if (tmp <= 0)
                break;
            uart_buf[tmp] = '\0';
            if (shell_process_uart(shell, uart_buf) == EXIT_FAILURE) {
                break;
            }
        }

        // if I've been weken up because of file descriptor 1 hang up
        if (fds[1].revents & POLLHUP) {
            break;
        }
    }

    shell_free(shell);
    uart_close(fds[1].fd);
    restore_terminal(terminal);
    printf("\r\nDisconnected.\r\n");
    return EXIT_SUCCESS;
}
