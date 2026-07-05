#include "unity.h"
#include "uart.h"
#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define MAX_RESPONSES 16

#define ANSI_CLEAR_STR  "\033[2J\033[H"
#define UNKNOWN_STR     "Unknown command\r\n"
#define ANSI_CLEAR_LEN  7
#define UNKNOWN_LEN     17

#define TEST_DIR "/tmp/piSerialTest_XXXXXX"

static char test_dir[64];
static char tty0_path[128];
static char tty1_path[128];
static pid_t socat_pid = -1;
static int device_fd = -1;
static int uart_fd = -1;

void setUp(void)
{
    test_dir[0] = '\0';
    tty0_path[0] = '\0';
    tty1_path[0] = '\0';
    socat_pid = -1;
    device_fd = -1;
    uart_fd = -1;
}

void tearDown(void)
{
    if (device_fd >= 0) close(device_fd);
    if (uart_fd >= 0) uart_close(uart_fd);
    if (socat_pid > 0) {
        kill(socat_pid, SIGTERM);
        waitpid(socat_pid, NULL, WNOHANG);
    }
    if (test_dir[0] != '\0')
        rmdir(test_dir);
}

static int setup_socat_pair(void)
{
    strcpy(test_dir, TEST_DIR);
    if (!mkdtemp(test_dir))
        return -1;

    snprintf(tty0_path, sizeof(tty0_path), "%s/tty0", test_dir);
    snprintf(tty1_path, sizeof(tty1_path), "%s/tty1", test_dir);

    char pty0_arg[256], pty1_arg[256];
    snprintf(pty0_arg, sizeof(pty0_arg), "PTY,link=%s", tty0_path);
    snprintf(pty1_arg, sizeof(pty1_arg), "PTY,link=%s", tty1_path);

    socat_pid = fork();
    if (socat_pid == 0) {
        execlp("socat", "socat", "-d", "-d",
               pty0_arg, pty1_arg, NULL);
        _exit(1);
    }
    if (socat_pid < 0)
        return -1;

    usleep(500000);

    struct stat st;
    if (stat(tty0_path, &st) != 0 || stat(tty1_path, &st) != 0)
        return -1;

    device_fd = uart_open(tty1_path, 115200);
    uart_fd   = uart_open(tty0_path, 115200);

    if (device_fd < 0 || uart_fd < 0)
        return -1;

    return 0;
}

static void relay_to_uart(void)
{
    char buf[256];
    int n = uart_read(uart_fd, buf, sizeof(buf) - 1);
    if (n <= 0) return;
    buf[n] = '\0';

    char *p = buf;
    while (*p == '\r' || *p == '\n') p++;

    size_t slen = strlen(p);
    while (slen > 0 && (p[slen - 1] == '\r' || p[slen - 1] == '\n'))
        p[--slen] = '\0';

    char response[256];
    shell_process(p, response, sizeof(response));

    if (response[0] != '\0')
        uart_write(uart_fd, response, strlen(response));
}

void test_integration_clear_command(void)
{
    if (setup_socat_pair() != 0)
        TEST_IGNORE_MESSAGE("socat not available or setup failed");

    write(device_fd, "@clear\n", 7);
    usleep(200000);
    relay_to_uart();

    char buf[64] = {0};
    usleep(100000);
    int n = (int)read(device_fd, buf, sizeof(buf) - 1);
    buf[n > 0 ? n : 0] = '\0';

    TEST_ASSERT_EQUAL_STRING(ANSI_CLEAR_STR, buf);
}

void test_integration_unknown_command(void)
{
    if (setup_socat_pair() != 0)
        TEST_IGNORE_MESSAGE("socat not available or setup failed");

    write(device_fd, "@foo\n", 5);
    usleep(200000);
    relay_to_uart();

    char buf[64] = {0};
    usleep(100000);
    int n = (int)read(device_fd, buf, sizeof(buf) - 1);
    buf[n > 0 ? n : 0] = '\0';

    TEST_ASSERT_EQUAL_STRING(UNKNOWN_STR, buf);
}

void test_integration_multiple_commands(void)
{
    if (setup_socat_pair() != 0)
        TEST_IGNORE_MESSAGE("socat not available or setup failed");

    write(device_fd, "@clear\n", 7);
    usleep(200000);
    relay_to_uart();

    char buf[64] = {0};
    usleep(100000);
    int n = (int)read(device_fd, buf, sizeof(buf) - 1);
    buf[n > 0 ? n : 0] = '\0';
    TEST_ASSERT_EQUAL_STRING(ANSI_CLEAR_STR, buf);

    write(device_fd, "@bar\n", 5);
    usleep(200000);
    relay_to_uart();

    memset(buf, 0, sizeof(buf));
    usleep(100000);
    n = (int)read(device_fd, buf, sizeof(buf) - 1);
    buf[n > 0 ? n : 0] = '\0';
    TEST_ASSERT_EQUAL_STRING(UNKNOWN_STR, buf);
}

static int relay_all_lines(void)
{
    char buf[1024];
    int n = uart_read(uart_fd, buf, sizeof(buf) - 1);
    if (n <= 0) return 0;
    buf[n] = '\0';

    char *saveptr;
    char *line = strtok_r(buf, "\n", &saveptr);
    while (line) {
        while (*line == '\r') line++;

        size_t slen = strlen(line);
        while (slen > 0 && (line[slen - 1] == '\r'))
            line[--slen] = '\0';

        char response[256];
        shell_process(line, response, sizeof(response));
        if (response[0] != '\0')
            uart_write(uart_fd, response, strlen(response));

        line = strtok_r(NULL, "\n", &saveptr);
    }
    return n;
}

static int read_device_responses(char *out, size_t outlen)
{
    usleep(100000);
    int n = (int)read(device_fd, out, outlen - 1);
    if (n > 0) out[n] = '\0';
    return n > 0 ? n : 0;
}

void test_integration_file_transfer(void)
{
    if (setup_socat_pair() != 0)
        TEST_IGNORE_MESSAGE("socat not available or setup failed");

    const char *file_content =
        "This is a regular text line\n"
        "@clear\n"
        "Binary: \x01\x02\x03\x04\xFF\xFE\n"
        "@unknown\n"
        "Another normal line\n"
        "@clear\n"
        "Final line\n";

    write(device_fd, file_content, strlen(file_content));
    usleep(500000);

    relay_all_lines();

    char responses[512];
    int n = read_device_responses(responses, sizeof(responses));

    responses[n > 0 ? n : 0] = '\0';

    TEST_ASSERT_NOT_NULL(strstr(responses, ANSI_CLEAR_STR));
    TEST_ASSERT_NOT_NULL(strstr(responses, UNKNOWN_STR));

    int clear_count = 0;
    int unknown_count = 0;
    char *p = responses;
    while (*p) {
        if (strncmp(p, ANSI_CLEAR_STR, ANSI_CLEAR_LEN) == 0) {
            clear_count++;
            p += ANSI_CLEAR_LEN;
        } else if (strncmp(p, UNKNOWN_STR, UNKNOWN_LEN) == 0) {
            unknown_count++;
            p += UNKNOWN_LEN;
        } else {
            p++;
        }
    }
    TEST_ASSERT_EQUAL(2, clear_count);
    TEST_ASSERT_EQUAL(1, unknown_count);
}

void test_integration_special_chars_file(void)
{
    if (setup_socat_pair() != 0)
        TEST_IGNORE_MESSAGE("socat not available or setup failed");

    unsigned char file_with_special[] = {
        0x01, 0x02, 0xFF, 0xFE, 0x7F, '\n',
        '@', 'c', 'l', 'e', 'a', 'r', '\n',
        0x1B, 0x5B, 0x32, 0x4A, '\n',
        '@', 0xFF, 'x', '\n',
        '@', 'c', 'l', 0x01, 'e', 'a', 'r', '\n',
        '\n',
        '@', 'f', 'o', 'o', '\n',
        0
    };

    write(device_fd, file_with_special, sizeof(file_with_special) - 1);
    usleep(500000);

    relay_all_lines();

    char responses[512];
    int n = read_device_responses(responses, sizeof(responses));
    responses[n > 0 ? n : 0] = '\0';

    TEST_ASSERT_NOT_NULL(strstr(responses, ANSI_CLEAR_STR));
    TEST_ASSERT_NOT_NULL(strstr(responses, UNKNOWN_STR));

    int clear_count = 0;
    int unknown_count = 0;
    char *p = responses;
    while (*p) {
        if (strncmp(p, ANSI_CLEAR_STR, ANSI_CLEAR_LEN) == 0) {
            clear_count++;
            p += ANSI_CLEAR_LEN;
        } else if (strncmp(p, UNKNOWN_STR, UNKNOWN_LEN) == 0) {
            unknown_count++;
            p += UNKNOWN_LEN;
        } else {
            p++;
        }
    }
    TEST_ASSERT_EQUAL(1, clear_count);
    TEST_ASSERT_EQUAL(3, unknown_count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_integration_clear_command);
    RUN_TEST(test_integration_unknown_command);
    RUN_TEST(test_integration_multiple_commands);
    RUN_TEST(test_integration_file_transfer);
    RUN_TEST(test_integration_special_chars_file);
    return UNITY_END();
}
