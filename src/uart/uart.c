#include "uart.h"

#ifdef _WIN32

#include <windows.h>

int uart_open(const char* path, int baud)
{
    HANDLE hCom = CreateFileA(path,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);
    if (hCom == INVALID_HANDLE_VALUE)
        return -1;

    DCB dcb = { 0 };
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(hCom, &dcb)) {
        CloseHandle(hCom);
        return -1;
    }

    dcb.BaudRate = baud;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;

    if (!SetCommState(hCom, &dcb)) {
        CloseHandle(hCom);
        return -1;
    }

    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    SetCommTimeouts(hCom, &timeouts);

    return (int)(intptr_t)hCom;
}

void uart_close(int fd)
{
    if (fd >= 0)
        CloseHandle((HANDLE)(intptr_t)fd);
}

int uart_read(int fd, char* buf, size_t len)
{
    DWORD read;
    if (!ReadFile((HANDLE)(intptr_t)fd, buf, (DWORD)len, &read, NULL))
        return -1;
    return (int)read;
}

int uart_write(int fd, const char* buf, size_t len)
{
    DWORD written;
    if (!WriteFile((HANDLE)(intptr_t)fd, buf, (DWORD)len, &written, NULL))
        return -1;
    return (int)written;
}

#else /* Linux */

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static speed_t int_to_speed(int baud)
{
    switch (baud) {
    case 0:
        return B0;
    case 50:
        return B50;
    case 75:
        return B75;
    case 110:
        return B110;
    case 134:
        return B134;
    case 150:
        return B150;
    case 200:
        return B200;
    case 300:
        return B300;
    case 600:
        return B600;
    case 1200:
        return B1200;
    case 1800:
        return B1800;
    case 2400:
        return B2400;
    case 4800:
        return B4800;
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
    case 57600:
        return B57600;
    case 115200:
        return B115200;
    case 230400:
        return B230400;
    default:
        return B9600;
    }
}

int uart_open(const char* path, int baud)
{
    int fd = open(path, O_RDWR | O_NOCTTY);
    if (fd < 0)
        return -1;

    struct termios tty;
    memset(&tty, 0, sizeof tty);

    if (tcgetattr(fd, &tty) != 0) {
        close(fd);
        return -1;
    }

    speed_t speed = int_to_speed(baud);
    cfsetospeed(&tty, speed);
    cfsetispeed(&tty, speed);

    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;

    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL | INLCR | IGNCR);
    tty.c_oflag &= ~OPOST;

    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        close(fd);
        return -1;
    }

    printf("Connected to %s.\r\n", path);
    fflush(stdout);

    return fd;
}

void uart_close(int fd)
{
    if (fd >= 0)
        close(fd);
}

int uart_read(int fd, char* buf, size_t len)
{
    return (int)read(fd, buf, len);
}

int uart_write(int fd, const char* buf, size_t len)
{
    return (int)write(fd, buf, len);
}

#endif

int uart_open_custom(const uart_config_t* cfg)
{
    return uart_open(cfg->path, (int)cfg->outBoundRate);
}
