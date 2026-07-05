#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

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
    char tty0_path[128] = "/tmp/echo_test_tty0";
    char tty1_path[128] = "/tmp/echo_test_tty1";

    unlink(tty0_path);
    unlink(tty1_path);

    char pty0_arg[256], pty1_arg[256];
    snprintf(pty0_arg, sizeof(pty0_arg), "PTY,link=%s", tty0_path);
    snprintf(pty1_arg, sizeof(pty1_arg), "PTY,link=%s", tty1_path);

    socat_pid = fork();
    if (socat_pid == 0) {
        execlp("socat", "socat", "-d", "-d", pty0_arg, pty1_arg, NULL);
        _exit(1);
    }
    if (socat_pid < 0) return -1;

    usleep(500000);

    struct stat st;
    if (stat(tty0_path, &st) != 0 || stat(tty1_path, &st) != 0)
        return -1;

    if (pipe(server_stdin) != 0 || pipe(server_stdout) != 0)
        return -1;

    server_pid = fork();
    if (server_pid == 0) {
        dup2(server_stdin[0], STDIN_FILENO);
        dup2(server_stdout[1], STDOUT_FILENO);
        close(server_stdin[0]); close(server_stdin[1]);
        close(server_stdout[0]); close(server_stdout[1]);
        execl("./piSerialServer", "./piSerialServer", tty0_path, NULL);
        _exit(1);
    }
    if (server_pid < 0) return -1;

    close(server_stdin[0]);
    close(server_stdout[1]);
    usleep(500000);

    return 0;
}

static int send_and_capture(const char *input, char *output, size_t olen)
{
    write(server_stdin[1], input, strlen(input));

    usleep(500000);

    fd_set rfds;
    struct timeval tv = {1, 0};
    FD_ZERO(&rfds);
    FD_SET(server_stdout[0], &rfds);

    int ret = select(server_stdout[0] + 1, &rfds, NULL, NULL, &tv);
    if (ret <= 0) {
        output[0] = '\0';
        return 0;
    }

    int n = (int)read(server_stdout[0], output, olen - 1);
    if (n > 0) output[n] = '\0';
    else output[0] = '\0';
    return n > 0 ? n : 0;
}

int main(void)
{
    if (setup() != 0) {
        printf("SKIP: setup failed (socat?)\n");
        cleanup();
        return 0;
    }

    char buf[512];
    int pass = 1;

    send_and_capture("hello\n", buf, sizeof(buf));
    if (strstr(buf, "hello") == NULL) {
        printf("FAIL: 'hello' not echoed in output. Got: %s\n", buf);
        pass = 0;
    } else {
        printf("PASS: 'hello' echoed back\n");
    }

    send_and_capture("@clear\n", buf, sizeof(buf));
    if (strstr(buf, "@clear") == NULL) {
        printf("FAIL: '@clear' not echoed in output. Got: %s\n", buf);
        pass = 0;
    } else {
        printf("PASS: '@clear' echoed back\n");
    }

    send_and_capture("ciao fra\n", buf, sizeof(buf));
    if (strstr(buf, "ciao fra") == NULL) {
        printf("FAIL: 'ciao fra' not echoed. Got: %s\n", buf);
        pass = 0;
    } else {
        printf("PASS: 'ciao fra' echoed back\n");
    }

    send_and_capture("@foo\n", buf, sizeof(buf));
    if (strstr(buf, "@foo") == NULL) {
        printf("FAIL: '@foo' not echoed. Got: %s\n", buf);
        pass = 0;
    } else {
        printf("PASS: '@foo' echoed back\n");
    }

    cleanup();
    return pass ? 0 : 1;
}
