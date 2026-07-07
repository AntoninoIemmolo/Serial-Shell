CC      := gcc
CFLAGS  := -Wall -Wextra -D_GNU_SOURCE -I src/uart -I src/shell -I unity -I src/shell/history -g -fsanitize=address

LDFLAGS :=

SRC_ARTIFACT := piSerialServer

UART_SRC  := src/uart/uart.c
SHELL_SRC := src/shell/shell.c
MAIN_SRC  := src/main.c
HISTORY_SRC := src/shell/history/history.c
UNITY_SRC := unity/unity.c

.PHONY: all clean test test_uart test_shell test_integration

all: $(SRC_ARTIFACT)

$(SRC_ARTIFACT): $(MAIN_SRC) $(UART_SRC) $(SHELL_SRC) $(HISTORY_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_uart: tests/test_uart.c $(UART_SRC) $(UNITY_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_shell: tests/test_shell.c $(SHELL_SRC) $(UNITY_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_integration: tests/test_integration.c $(UART_SRC) $(SHELL_SRC) $(UNITY_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_echo: tests/test_echo.c $(UART_SRC) $(SHELL_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_newline: tests/test_newline.c $(UART_SRC) $(SHELL_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests/test_serial_file: tests/test_serial_file.c
	$(CC) $(CFLAGS) -o $@ $^

tests/test_uart_newline: tests/test_uart_newline.c
	$(CC) $(CFLAGS) -o $@ $^

test_uart: tests/test_uart
	$<

test_shell: tests/test_shell
	$<

test_integration: tests/test_integration
	$<

test_echo: tests/test_echo
	$<

test_newline: tests/test_newline
	$<

test_serial_file: tests/test_serial_file
	$<

test_uart_newline: tests/test_uart_newline
	$<

test_manual: all
	pkill -9 socat 2>/dev/null; sleep 0.1 || true
	python3 tests/manual_test.py
	pkill -9 socat 2>/dev/null || true

test: test_uart test_shell test_integration test_echo test_newline test_serial_file test_uart_newline test_manual

clean:
	rm -f $(SRC_ARTIFACT) tests/test_uart tests/test_shell tests/test_integration tests/test_echo tests/test_newline tests/test_serial_file tests/test_uart_newline
