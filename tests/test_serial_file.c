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
    unlink("/tmp/serial_file_tty0");
    unlink("/tmp/serial_file_tty1");
}

static void set_device_raw(int fd)
{
    struct termios tio;
    tcgetattr(fd, &tio);
    tio.c_iflag = 0;
    tio.c_oflag = 0;
    tio.c_cflag = CS8 | CREAD | CLOCAL;
    tio.c_lflag = 0;
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 0;
    tcsetattr(fd, TCSANOW, &tio);
}

static int setup(void)
{
    unlink("/tmp/serial_file_tty0");
    unlink("/tmp/serial_file_tty1");

    char pty0_arg[256], pty1_arg[256];
    snprintf(pty0_arg, sizeof(pty0_arg), "PTY,link=/tmp/serial_file_tty0");
    snprintf(pty1_arg, sizeof(pty1_arg), "PTY,link=/tmp/serial_file_tty1");

    socat_pid = fork();
    if (socat_pid == 0) {
        execlp("socat", "socat", "-d", "-d", pty0_arg, pty1_arg, NULL);
        _exit(1);
    }
    if (socat_pid < 0) return -1;
    usleep(500000);

    struct stat st;
    if (stat("/tmp/serial_file_tty0", &st) != 0) return -1;
    if (stat("/tmp/serial_file_tty1", &st) != 0) return -1;

    if (pipe(server_stdout) != 0) return -1;

    server_pid = fork();
    if (server_pid == 0) {
        dup2(server_stdout[1], STDOUT_FILENO);
        close(server_stdout[0]); close(server_stdout[1]);
        execl("./piSerialServer", "./piSerialServer", "/tmp/serial_file_tty1", NULL);
        _exit(1);
    }
    if (server_pid < 0) return -1;
    close(server_stdout[1]);
    usleep(500000);

    device_fd = open("/tmp/serial_file_tty0", O_RDWR | O_NOCTTY);
    if (device_fd < 0) return -1;
    set_device_raw(device_fd);

    char drain[512];
    usleep(300000);
    fd_set rfds;
    struct timeval tv = {0, 0};
    FD_ZERO(&rfds);
    FD_SET(server_stdout[0], &rfds);
    while (select(server_stdout[0] + 1, &rfds, NULL, NULL, &tv) > 0) {
        read(server_stdout[0], drain, sizeof(drain));
        FD_ZERO(&rfds);
        FD_SET(server_stdout[0], &rfds);
        tv.tv_sec = 0; tv.tv_usec = 50000;
    }

    return 0;
}

static int read_server_stdout(char *out, size_t olen, int timeout_ms)
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
    return n;
}

static void send_via_serial(const char *data, size_t len)
{
    size_t pos = 0;
    while (pos < len) {
        int w = (int)write(device_fd, data + pos, len - pos);
        if (w > 0) pos += w;
        else break;
    }
}

static int capture_all(char *buf, size_t olen)
{
    int total = 0;
    while (total < (int)olen - 1) {
        int n = read_server_stdout(buf + total, olen - total, 500);
        if (n <= 0) break;
        total += n;
    }
    buf[total] = '\0';
    return total;
}

static int check_all(const char *captured, const char *needles[], int count)
{
    for (int i = 0; i < count; i++) {
        if (strstr(captured, needles[i]) == NULL) {
            printf("FAIL: missing '%s'\n", needles[i]);
            return 0;
        }
    }
    return 1;
}

static int test_plain_text_file(void)
{
    printf("Test: plain text file via serial ... ");
    fflush(stdout);

    const char *file = "Hello from device!\nThis is line 2\nAnd line 3\n";
    send_via_serial(file, strlen(file));

    char buf[4096]; capture_all(buf, sizeof(buf));
    const char *needles[] = {"Hello from device!", "This is line 2", "And line 3"};
    int ok = check_all(buf, needles, 3);
    if (ok) printf("PASS\n");
    return !ok;
}

static int test_file_with_at_commands(void)
{
    printf("Test: file with @clear command via serial ... ");
    fflush(stdout);

    const char *file = "status: ok\n@clear\ndone\n";
    send_via_serial(file, strlen(file));

    char buf[4096]; capture_all(buf, sizeof(buf));
    const char *needles[] = {"status: ok", "@clear", "done"};
    int ok = check_all(buf, needles, 3);
    if (ok) printf("PASS\n");
    return !ok;
}

static int test_file_with_binary(void)
{
    printf("Test: file with binary bytes via serial ... ");
    fflush(stdout);

    char file[32];
    int n = snprintf(file, sizeof(file),
        "%c%c%cHello%c%c%cWorld%c%c\n",
        0x01, 0x02, 0x03, 0x10, 0x1F, 0x7F, 0xFF, 0xFE);
    send_via_serial(file, (size_t)n);

    char buf[4096]; capture_all(buf, sizeof(buf));
    const char *needles[] = {"Hello", "World"};
    int ok = check_all(buf, needles, 2);
    if (ok) printf("PASS\n");
    return !ok;
}

static int test_long_file_via_serial(void)
{
    printf("Test: long file via serial ... ");
    fflush(stdout);

    char file[4096];
    int pos = 0;
    for (int i = 0; i < 50; i++) {
        pos += snprintf(file + pos, sizeof(file) - pos,
            "Line number %d of the test file\n", i);
    }
    send_via_serial(file, (size_t)pos);

    char buf[8192]; capture_all(buf, sizeof(buf));
    const char *needles[] = {"Line number 0", "Line number 49", "Line number 25"};
    int ok = check_all(buf, needles, 3);
    if (ok) printf("PASS\n");
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
    result |= test_plain_text_file();
    result |= test_file_with_at_commands();
    result |= test_file_with_binary();
    result |= test_long_file_via_serial();

    cleanup();
    return result;
}
