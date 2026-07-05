#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>
#include <termios.h>

static pid_t socat_pid = -1;
static pid_t server_pid = -1;
static int server_stdout[2] = {-1, -1};
static int device_fd = -1;

static void cleanup(void)
{
    if (device_fd >= 0) close(device_fd);
    if (server_pid > 0) {
        kill(server_pid, SIGTERM);
        waitpid(server_pid, NULL, WNOHANG);
    }
    if (socat_pid > 0) {
        kill(socat_pid, SIGTERM);
        waitpid(socat_pid, NULL, WNOHANG);
    }
    if (server_stdout[0] >= 0) close(server_stdout[0]);
    if (server_stdout[1] >= 0) close(server_stdout[1]);
    unlink("/tmp/uart_nl_tty0");
    unlink("/tmp/uart_nl_tty1");
}

static int setup(void)
{
    unlink("/tmp/uart_nl_tty0");
    unlink("/tmp/uart_nl_tty1");

    char pty0_arg[256], pty1_arg[256];
    snprintf(pty0_arg, sizeof(pty0_arg), "PTY,link=/tmp/uart_nl_tty0");
    snprintf(pty1_arg, sizeof(pty1_arg), "PTY,link=/tmp/uart_nl_tty1");

    socat_pid = fork();
    if (socat_pid == 0) {
        execlp("socat", "socat", "-d", "-d", pty0_arg, pty1_arg, NULL);
        _exit(1);
    }
    if (socat_pid < 0) return -1;
    usleep(500000);

    struct stat st;
    if (stat("/tmp/uart_nl_tty0", &st) != 0) return -1;
    if (stat("/tmp/uart_nl_tty1", &st) != 0) return -1;

    if (pipe(server_stdout) != 0) return -1;

    server_pid = fork();
    if (server_pid == 0) {
        close(server_stdout[0]);
        dup2(server_stdout[1], STDOUT_FILENO);
        close(server_stdout[1]);
        /* stdin from /dev/null so nothing typed locally */
        int null_fd = open("/dev/null", O_RDONLY);
        dup2(null_fd, STDIN_FILENO);
        close(null_fd);
        execl("./piSerialServer", "./piSerialServer", "/tmp/uart_nl_tty1", NULL);
        _exit(1);
    }
    if (server_pid < 0) return -1;
    close(server_stdout[1]);
    usleep(500000);

    device_fd = open("/tmp/uart_nl_tty0", O_RDWR | O_NOCTTY);
    if (device_fd < 0) return -1;

    {
        struct termios tio;
        tcgetattr(device_fd, &tio);
        tio.c_iflag = 0;
        tio.c_oflag = 0;
        tio.c_cflag = CS8 | CREAD | CLOCAL;
        tio.c_lflag = 0;
        tio.c_cc[VMIN] = 1;
        tio.c_cc[VTIME] = 0;
        if (tcsetattr(device_fd, TCSANOW, &tio) != 0) {
            perror("tcsetattr device_fd");
            return -1;
        }
    }

    /* drain any startup banner from server stdout */
    char drain[512];
    fd_set rfds;
    struct timeval tv = {0, 300000};
    FD_ZERO(&rfds);
    FD_SET(server_stdout[0], &rfds);
    while (select(server_stdout[0] + 1, &rfds, NULL, NULL, &tv) > 0) {
        read(server_stdout[0], drain, sizeof(drain));
        FD_ZERO(&rfds);
        FD_SET(server_stdout[0], &rfds);
        tv.tv_sec = 0; tv.tv_usec = 100000;
    }

    return 0;
}

static int drain_stdout(void)
{
    char buf[4096];
    int total = 0;
    fd_set rfds;
    struct timeval tv = {0, 200000};
    FD_ZERO(&rfds);
    FD_SET(server_stdout[0], &rfds);
    while (select(server_stdout[0] + 1, &rfds, NULL, NULL, &tv) > 0) {
        int n = (int)read(server_stdout[0], buf, sizeof(buf));
        if (n <= 0) break;
        total += n;
        FD_ZERO(&rfds);
        FD_SET(server_stdout[0], &rfds);
        tv.tv_sec = 0; tv.tv_usec = 100000;
    }
    return total;
}

/* Send data from "remote device" to server via UART, return captured stdout */
static int send_and_capture(const char *data, size_t dlen, char *out, size_t olen)
{
    drain_stdout();

    size_t pos = 0;
    while (pos < dlen) {
        int w = (int)write(device_fd, data + pos, dlen - pos);
        if (w > 0) pos += w;
        else break;
    }

    usleep(500000);

    int total = 0;
    fd_set rfds;
    struct timeval tv = {0, 300000};
    FD_ZERO(&rfds);
    FD_SET(server_stdout[0], &rfds);
    while (total < (int)olen - 1 && select(server_stdout[0] + 1, &rfds, NULL, NULL, &tv) > 0) {
        int n = (int)read(server_stdout[0], out + total, olen - 1 - total);
        if (n <= 0) break;
        total += n;
        FD_ZERO(&rfds);
        FD_SET(server_stdout[0], &rfds);
        tv.tv_sec = 0; tv.tv_usec = 200000;
    }
    out[total] = '\0';
    return total;
}

static void dump_bytes(const char *label, const char *data, int len)
{
    printf("  %s (%d bytes): ", label, len);
    for (int i = 0; i < len; i++) {
        if (data[i] == '\r') printf("\\r");
        else if (data[i] == '\n') printf("\\n");
        else if (data[i] >= 32 && data[i] < 127) putchar(data[i]);
        else printf("\\x%02x", (unsigned char)data[i]);
    }
    printf("\n");
}

static int check_text_present(const char *buf, const char *needle)
{
    if (strstr(buf, needle) != NULL) return 1;
    printf("  FAIL: missing '%s'\n", needle);
    return 0;
}

static int test_simple_newline(void)
{
    printf("Test: remote sends 'hello\\n' (\\r stripped, \\n passed to ONLCR) ...\n");

    char buf[256];
    int n = send_and_capture("hello\n", 6, buf, sizeof(buf));
    dump_bytes("captured", buf, n);

    if (n == 0) { printf("  FAIL: no output\n"); return 1; }
    int ok = check_text_present(buf, "hello\n");
    printf("%s\n", ok ? "  PASS" : "");
    return !ok;
}

static int test_multi_line(void)
{
    printf("Test: remote sends 'AB\\nCD\\n' ...\n");

    char buf[256];
    int n = send_and_capture("AB\nCD\n", 6, buf, sizeof(buf));
    dump_bytes("captured", buf, n);

    if (n == 0) { printf("  FAIL: no output\n"); return 1; }
    int ok = 1;
    if (!check_text_present(buf, "AB\n")) ok = 0;
    if (!check_text_present(buf, "CD\n")) ok = 0;
    printf("%s\n", ok ? "  PASS" : "");
    return !ok;
}

static int test_text_then_newline_then_more(void)
{
    printf("Test: remote sends 'ABCDEFG\\nhallo' (simulates typing \\n then text) ...\n");

    char buf[512];
    int n = send_and_capture("ABCDEFG\nhallo", 13, buf, sizeof(buf));
    dump_bytes("captured", buf, n);

    if (n == 0) { printf("  FAIL: no output\n"); return 1; }
    int ok = 1;
    if (!check_text_present(buf, "ABCDEFG\nhallo")) ok = 0;
    printf("%s\n", ok ? "  PASS" : "");
    return !ok;
}

static int test_newline_at_end(void)
{
    printf("Test: remote sends 'word\\n' ...\n");

    char buf[256];
    int n = send_and_capture("word\n", 5, buf, sizeof(buf));
    dump_bytes("captured", buf, n);

    if (n == 0) { printf("  FAIL: no output\n"); return 1; }
    int ok = check_text_present(buf, "word\n");
    printf("%s\n", ok ? "  PASS" : "");
    return !ok;
}

static int test_burst_with_multiple_newlines(void)
{
    printf("Test: remote sends 'hi\\nA\\nhallo\\n' multiple lines ...\n");

    char buf[512];
    int n = send_and_capture("hi\nA\nhallo\n", 11, buf, sizeof(buf));
    dump_bytes("captured", buf, n);

    if (n == 0) { printf("  FAIL: no output\n"); return 1; }
    int ok = 1;
    if (!check_text_present(buf, "hi\n")) ok = 0;
    if (!check_text_present(buf, "A\n")) ok = 0;
    if (!check_text_present(buf, "hallo\n")) ok = 0;
    printf("%s\n", ok ? "  PASS" : "");
    return !ok;
}

static int capture_raw(char *out, size_t olen)
{
    int total = 0;
    fd_set rfds;
    struct timeval tv = {0, 300000};
    FD_ZERO(&rfds);
    FD_SET(server_stdout[0], &rfds);
    while (total < (int)olen - 1 && select(server_stdout[0] + 1, &rfds, NULL, NULL, &tv) > 0) {
        int n = (int)read(server_stdout[0], out + total, olen - 1 - total);
        if (n <= 0) break;
        total += n;
        FD_ZERO(&rfds);
        FD_SET(server_stdout[0], &rfds);
        tv.tv_sec = 0; tv.tv_usec = 200000;
    }
    out[total] = '\0';
    return total;
}

static int test_explicit_crlf(void)
{
    printf("Test: remote sends 'ABC\\r\\n' (\\r stripped, ABC\\n displayed) ...\n");

    char buf[256];
    int n = send_and_capture("ABC\r\n", 5, buf, sizeof(buf));
    dump_bytes("captured", buf, n);

    if (n == 0) { printf("  FAIL: no output\n"); return 1; }
    int ok = 1;
    if (!check_text_present(buf, "ABC\n")) ok = 0;
    if (strstr(buf, "\r") != NULL) {
        printf("  BUG: stray \\r found in output\n");
        ok = 0;
    }
    printf("%s\n", ok ? "  PASS" : "");
    return !ok;
}

static int test_split_read_newline(void)
{
    printf("Test: remote sends '\\r' then '\\n' in separate writes (split read) ...\n");

    char buf[256];
    drain_stdout();

    write(device_fd, "ABC\r", 4);
    usleep(300000);
    write(device_fd, "\nDEF\n", 5);
    usleep(600000);

    int n = capture_raw(buf, sizeof(buf));
    dump_bytes("captured", buf, n);

    if (n == 0) { printf("  FAIL: no output\n"); return 1; }
    int ok = 1;
    if (!check_text_present(buf, "ABC\n")) ok = 0;
    if (!check_text_present(buf, "DEF\n")) ok = 0;
    if (strstr(buf, "\r") != NULL) {
        printf("  BUG: stray \\r found in output\n");
        ok = 0;
    }
    printf("%s\n", ok ? "  PASS" : "");
    return !ok;
}

int main(void)
{
    if (setup() != 0) {
        printf("SKIP: setup failed\n");
        cleanup();
        return 0;
    }

    int result = 0;
    result |= test_simple_newline();
    result |= test_multi_line();
    result |= test_text_then_newline_then_more();
    result |= test_newline_at_end();
    result |= test_burst_with_multiple_newlines();
    result |= test_explicit_crlf();
    result |= test_split_read_newline();

    cleanup();
    return result;
}
