#include "font.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static volatile uint32_t* g_devices = (volatile uint32_t*)0x02000000;


static int console_getc(FILE* file)
{
    (void)file;
    return 0;
}

static int g_console_line = 0;
static int g_console_column = 0;
static volatile unsigned int* g_console_buffer = NULL;

static void console_scroll()
{
    for (int y = 0; y != 12; ++y)
    {
        for (int x = 0; x != 16; ++x)
        {
            for (int row = 0; row != 8; ++row)
            {
                g_console_buffer[(y * 8 + row) * 16 + x] = g_console_buffer[((y + 1) * 8 + row) * 16 + x];
            }
        }
    }
    for (int x = 0; x != 16; ++x)
    {
        for (int row = 0; row != 8; ++row)
        {
            g_console_buffer[(11 * 8 + row) * 16 + x] = 0;
        }
    }

}

static int console_putc(char c, FILE* file)
{
    (void)file;

    if (c == '\n')
    {
        g_console_line++;
        g_console_column = 0;
        if (g_console_line == 12)
        {
            console_scroll();
            g_console_line = 11;
        }
        return 0;
    }

    if (g_console_column == 16)
    {
        g_console_column = 0;
        g_console_line++;
        if (g_console_line == 12)
        {
            console_scroll();
            g_console_line = 11;
        }
    }

    const unsigned int* glyph = font[(unsigned char)c];
    for (int row = 0; row != 8; ++row)
    {
        g_console_buffer[(g_console_line * 8 + row) * 16 + g_console_column] = glyph[row];
    }
    g_console_column++;

    return 0;
}

static FILE console_stdin = FDEV_SETUP_STREAM(NULL, console_getc, NULL, _FDEV_SETUP_WRITE);
static FILE console_stdout = FDEV_SETUP_STREAM(console_putc, NULL, NULL, _FDEV_SETUP_WRITE);

FILE* const stdin = &console_stdin;
FILE* const stdout = &console_stdout;
FILE* const stderr = &console_stdout;


void interrupt_handler()
{
}

int main()
{
    uint32_t* unaligned_buffer = (uint32_t*)calloc(16 * 128, 4);

    uintptr_t ptr = (uintptr_t)unaligned_buffer;
    uint32_t alignment = 64 * 128;
    if ((ptr % alignment) != 0)
    {
        ptr += alignment - (ptr % alignment);
    }
    g_console_buffer = (volatile unsigned int*)ptr;
    g_devices[3] = (uint32_t)g_console_buffer | 0x80000000;

    puts("Hello world");
    puts("Wrapping a very long line");
    puts("Wrapping a very long line");
    puts("Wrapping a very long line");
    puts("Wrapping a very long line");
    puts("Wrapping a very long line");
    puts("Wrapping a very long line");
    puts("Wrapping a very long line");
    puts("Wrapping a very long line");
    printf("buf=%p\n", (const void*)g_console_buffer);

    for (;;);
}
