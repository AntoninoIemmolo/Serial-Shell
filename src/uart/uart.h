#ifndef UART_H
#define UART_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    char path[100];
    uint32_t outBoundRate;
    uint32_t inBoundRate;
} uart_config_t;

int  uart_open(const char *path, int baud);
int  uart_open_custom(const uart_config_t *cfg);
void uart_close(int fd);
int  uart_read(int fd, char *buf, size_t len);
int  uart_write(int fd, const char *buf, size_t len);

#endif
