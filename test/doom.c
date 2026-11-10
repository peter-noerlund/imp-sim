#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "doom.h"

static volatile uint32_t* g_devices = (volatile uint32_t*)0x02000000;

void interrupt_handler()
{
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

    memcpy((void*)video_buffer, doom, sizeof(doom));

    g_devices[3] = (uint32_t)video_buffer | 0x80000000;

    for (;;);
}
