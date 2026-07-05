#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/select.h>

static pid_t socat_pid = -1;
static pid_t server_pid = -1;
static int server_stdin[2];
static int server_stdout[2];

static void cleanup(void)
{
    if (server_pid > 0) {
        kill(server_pid, SIGTERM);
        waitpid(server_pid, NULL, WNOHANG);
    }
    if (socat_pid > 0) {
        kill(socat_pid, SIGTERM);
        waitpid(socat_pid, NULL, WNOHANG);
    }
    if (server_stdin[0]) close(server_stdin[0]);
    if (server_stdin[1]) close(server_stdin[1]);
    if (server_stdout[0]) close(server_stdout[0]);
    if (server_stdout[1]) close(server_stdout[1]);
}

static int setup(void)
{
    unlink("/tmp/nl_test_tty0");
    unlink("/tmp/nl_test_tty1");

    char pty0_arg[256], pty1_arg[256];
    snprintf(pty0_arg, sizeof(pty0_arg), "PTY,link=/tmp/nl_test_tty0");
    snprintf(pty1_arg, sizeof(pty1_arg), "PTY,link=/tmp/nl_test_tty1");

    socat_pid = fork();
    if (socat_pid == 0) {
        execlp("socat", "socat", "-d", "-d", pty0_arg, pty1_arg, NULL);
        _exit(1);
    }
    if (socat_pid < 0) return -1;
    usleep(500000);

    struct stat st;
    if (stat("/tmp/nl_test_tty0", &st) != 0) return -1;

    if (pipe(server_stdin) != 0 || pipe(server_stdout) != 0) return -1;

    server_pid = fork();
    if (server_pid == 0) {
        dup2(server_stdin[0], STDIN_FILENO);
        dup2(server_stdout[1], STDOUT_FILENO);
        close(server_stdin[0]); close(server_stdin[1]);
        close(server_stdout[0]); close(server_stdout[1]);
        execl("./piSerialServer", "./piSerialServer", "/tmp/nl_test_tty0", NULL);
        _exit(1);
    }
    if (server_pid < 0) return -1;

    close(server_stdin[0]);
    close(server_stdout[1]);
    usleep(500000);
    return 0;
}

static int capture_output(char *out, size_t olen, int timeout_ms)
{
    fd_set rfds;
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    FD_ZERO(&rfds);
    FD_SET(server_stdout[0], &rfds);

    int ret = select(server_stdout[0] + 1, &rfds, NULL, NULL, &tv);
    if (ret <= 0) { out[0] = '\0'; return 0; }

    int n = (int)read(server_stdout[0], out, olen - 1);
    if (n > 0) out[n] = '\0';
    else out[0] = '\0';
    return n > 0 ? n : 0;
}

static int test_stdin_echo_passes_text(void)
{
    int fail = 0;
    char buf[512];

    printf("Test 1: 'hello\\n' echoed back via pipe (no \\r, ONLCR handles it on terminal) ... ");
    fflush(stdout);

    write(server_stdin[1], "hello\n", 6);
    usleep(500000);
    capture_output(buf, sizeof(buf), 2000);

    if (strstr(buf, "hello") != NULL) {
        printf("PASS\n");
    } else {
        printf("FAIL: text not found in output: ");
        for (int i = 0; buf[i] && i < 30; i++) {
            if (buf[i] == '\r') printf("\\r");
            else if (buf[i] == '\n') printf("\\n");
            else putchar(buf[i]);
        }
        printf("\n");
        fail = 1;
    }

    return fail;
}

static int test_output_has_newline(void)
{
    int fail = 0;
    char buf[512];

    printf("Test 2: 'ABC\\n' output contains \\n ... ");
    fflush(stdout);

    write(server_stdin[1], "ABC\n", 4);
    usleep(500000);
    capture_output(buf, sizeof(buf), 2000);

    const char *nl = strchr(buf, '\n');
    if (nl != NULL) {
        printf("PASS\n");
    } else {
        printf("FAIL: no newline found in output\n");
        fail = 1;
    }

    return fail;
}

int main(void)
{
    if (setup() != 0) {
        printf("SKIP: setup failed\n");
        cleanup();
        return 0;
    }

    int result = 0;
    result |= test_stdin_echo_passes_text();
    result |= test_output_has_newline();

    cleanup();
    return result;
}
