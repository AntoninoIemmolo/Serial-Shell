#include "unity.h"
#include "uart.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

static int test_master_fd = -1;
static int test_uart_fd   = -1;

void setUp(void)
{
}

void tearDown(void)
{
    if (test_uart_fd >= 0) {
        uart_close(test_uart_fd);
        test_uart_fd = -1;
    }
    if (test_master_fd >= 0) {
        close(test_master_fd);
        test_master_fd = -1;
    }
}

static int create_pty_pair(void)
{
    int master = posix_openpt(O_RDWR | O_NOCTTY);
    TEST_ASSERT_GREATER_OR_EQUAL(0, master);

    TEST_ASSERT_EQUAL(0, grantpt(master));
    TEST_ASSERT_EQUAL(0, unlockpt(master));

    const char *slave = ptsname(master);
    TEST_ASSERT_NOT_NULL(slave);

    int fd = uart_open(slave, 115200);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);

    test_master_fd = master;
    test_uart_fd   = fd;
    return 0;
}

void test_uart_open_invalid_path(void)
{
    int fd = uart_open("/dev/nonexistent_uart_test", 115200);
    TEST_ASSERT_EQUAL(-1, fd);
}

void test_uart_open_valid_pty(void)
{
    create_pty_pair();
    TEST_ASSERT_GREATER_OR_EQUAL(0, test_uart_fd);
}

void test_uart_write_and_read(void)
{
    create_pty_pair();

    const char *msg = "hello uart";
    int w = uart_write(test_uart_fd, msg, strlen(msg));
    TEST_ASSERT_EQUAL((int)strlen(msg), w);

    char buf[64] = {0};
    int r = (int)read(test_master_fd, buf, sizeof(buf) - 1);
    TEST_ASSERT_GREATER_THAN(0, r);
    TEST_ASSERT_EQUAL_STRING(msg, buf);
}

void test_uart_read_from_master(void)
{
    create_pty_pair();

    const char *msg = "from master";
    int w = (int)write(test_master_fd, msg, strlen(msg));
    TEST_ASSERT_EQUAL((int)strlen(msg), w);

    char buf[64] = {0};
    int r = uart_read(test_uart_fd, buf, sizeof(buf) - 1);
    TEST_ASSERT_EQUAL((int)strlen(msg), r);
    buf[r] = '\0';
    TEST_ASSERT_EQUAL_STRING(msg, buf);
}

void test_uart_config_t_opens(void)
{
    int master = posix_openpt(O_RDWR | O_NOCTTY);
    TEST_ASSERT_GREATER_OR_EQUAL(0, master);
    grantpt(master);
    unlockpt(master);
    const char *slave = ptsname(master);

    uart_config_t cfg;
    strncpy(cfg.path, slave, sizeof(cfg.path) - 1);
    cfg.path[sizeof(cfg.path) - 1] = '\0';
    cfg.outBoundRate = 115200;
    cfg.inBoundRate  = 115200;

    int fd = uart_open_custom(&cfg);
    TEST_ASSERT_GREATER_OR_EQUAL(0, fd);

    uart_close(fd);
    close(master);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_uart_open_invalid_path);
    RUN_TEST(test_uart_open_valid_pty);
    RUN_TEST(test_uart_write_and_read);
    RUN_TEST(test_uart_read_from_master);
    RUN_TEST(test_uart_config_t_opens);
    return UNITY_END();
}
