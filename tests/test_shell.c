#include "unity.h"
#include "shell.h"
#include <string.h>

void test_shell_clear(void)
{
    char out[64];
    shell_process("@clear", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("\033[2J\033[H", out);
}

void test_shell_unknown_command(void)
{
    char out[64];
    shell_process("@foo", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Unknown command\r\n", out);
}

void test_shell_empty_input(void)
{
    char out[64];
    out[0] = 'X';
    shell_process("", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("", out);
}

void test_shell_no_prefix(void)
{
    char out[64];
    out[0] = 'X';
    shell_process("hello", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("", out);
}

void test_shell_just_at(void)
{
    char out[64];
    shell_process("@", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Unknown command\r\n", out);
}

void test_shell_clear_with_trailing_newline(void)
{
    char out[64];
    shell_process("@clear\n", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Unknown command\r\n", out);
}

void test_shell_null_input(void)
{
    char out[64];
    out[0] = 'X';
    shell_process(NULL, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("", out);
}

void test_shell_output_too_small(void)
{
    char tiny[2];
    tiny[0] = 'X';
    shell_process("@clear", tiny, 1);
    TEST_ASSERT_EQUAL('\0', tiny[0]);
}

void test_shell_binary_only(void)
{
    char out[64];
    out[0] = 'X';
    shell_process("\x01\x02\x03\x04", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("", out);
}

void test_shell_binary_before_at(void)
{
    char out[64];
    out[0] = 'X';
    shell_process("\x01@clear", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("", out);
}

void test_shell_binary_trailing_after_clear(void)
{
    char out[64];
    shell_process("@clear\x01", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Unknown command\r\n", out);
}

void test_shell_high_ascii_only(void)
{
    char out[64];
    out[0] = 'X';
    shell_process("\x80\x81\xFE\xFF", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("", out);
}

void test_shell_at_with_binary_after(void)
{
    char out[64];
    shell_process("@\x00clear", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Unknown command\r\n", out);
}

void test_shell_embedded_null(void)
{
    char out[64];
    out[0] = 'X';
    shell_process("@cl\x00ear", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Unknown command\r\n", out);
}

void test_shell_escape_sequences(void)
{
    char out[64];
    out[0] = 'X';
    shell_process("\033[1;34m", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("", out);
}

void test_shell_mixed_text_and_binary(void)
{
    char out[64];
    shell_process("@cl\x01" "\x02" "ear", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Unknown command\r\n", out);
}

void test_shell_long_input_no_at(void)
{
    char long_input[300];
    memset(long_input, 'A', sizeof(long_input) - 1);
    long_input[sizeof(long_input) - 1] = '\0';

    char out[64];
    out[0] = 'X';
    shell_process(long_input, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("", out);
}

void test_shell_long_input_with_at(void)
{
    char long_input[300];
    memset(long_input, 'A', sizeof(long_input) - 1);
    long_input[0] = '@';
    long_input[sizeof(long_input) - 1] = '\0';

    char out[64];
    shell_process(long_input, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Unknown command\r\n", out);
}

void test_shell_repeated_special_chars(void)
{
    char input[200];
    memset(input, 0xFF, sizeof(input) - 1);
    input[sizeof(input) - 1] = '\0';

    char out[64];
    out[0] = 'X';
    shell_process(input, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("", out);
}

void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_shell_clear);
    RUN_TEST(test_shell_unknown_command);
    RUN_TEST(test_shell_empty_input);
    RUN_TEST(test_shell_no_prefix);
    RUN_TEST(test_shell_just_at);
    RUN_TEST(test_shell_clear_with_trailing_newline);
    RUN_TEST(test_shell_null_input);
    RUN_TEST(test_shell_output_too_small);
    RUN_TEST(test_shell_binary_only);
    RUN_TEST(test_shell_binary_before_at);
    RUN_TEST(test_shell_binary_trailing_after_clear);
    RUN_TEST(test_shell_high_ascii_only);
    RUN_TEST(test_shell_at_with_binary_after);
    RUN_TEST(test_shell_embedded_null);
    RUN_TEST(test_shell_escape_sequences);
    RUN_TEST(test_shell_mixed_text_and_binary);
    RUN_TEST(test_shell_long_input_no_at);
    RUN_TEST(test_shell_long_input_with_at);
    RUN_TEST(test_shell_repeated_special_chars);
    return UNITY_END();
}
