#include <stdint.h>
#include <stdlib.h>

static volatile uint32_t* g_devices = (volatile uint32_t*)0x02000000;

volatile uint8_t color = 0x00;

unsigned int g_counter = 0;

void interrupt_handler()
{
    if (g_counter++ % 128 == 0)
    {
        color = (color + 1) & 0x3F;
    }
    g_devices[0] = 500000;
}

int main()
{
    uint32_t* unaligned_buffer = (uint32_t*)malloc(64 * 128 * 2);

    uintptr_t ptr = (uintptr_t)unaligned_buffer;
    uint32_t alignment = 64 * 128;
    if ((ptr % alignment) != 0)
    {
        ptr += alignment - (ptr % alignment);
    }
    volatile uint32_t* video_buffer = (volatile uint32_t*)ptr;

    uint8_t current_color = color;

    g_devices[3] = (uint32_t)video_buffer | 0x80000000;

    for (;;)
    {
        current_color = color;

        uint32_t pixels = 0x00004444 | ((uint32_t)current_color << 16);
        for (int y = 0; y != 96; ++y)
        {
            for (int x = 0; x != 16; ++x)
            {
                video_buffer[y * 16 + x] = pixels * g_counter;
            }
        }

        g_devices[0] = 500000;

        while (current_color == color);
    }
}
