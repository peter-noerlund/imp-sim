#include <stdint.h>
#include <stdlib.h>

#define DEVICE_TIMER 0
#define DEVICE_INPUT 1
#define DEVICE_AUDIO 2
#define DEVICE_VIDEO 3

static volatile uint32_t* g_devices = (volatile uint32_t*)0x02000000;

volatile uint8_t color = 0x00;

unsigned int g_counter = 0;
unsigned int play = 0;

void interrupt_handler()
{
    if (g_counter++ % 128 == 0)
    {
        color = (color + 1) & 0x3F;

        if (play)
        {
            g_devices[DEVICE_AUDIO] = 65000000 / 2 / 440;
            play = 0;
        }
        else
        {
            g_devices[DEVICE_AUDIO] = 65000000 / 2 / 880;
            play = 1;
        }
    }
    g_devices[0] = 500000;
}

inline void setCsrFlags(uint16_t csr, uint32_t flags)
{
    __asm__ volatile ("csrs %0, %1" : : "i"(csr), "r"(flags));
}

inline void clearCsrFlags(uint16_t csr, uint32_t flags)
{
    __asm__ volatile ("csrc %0, %1" : : "i"(csr), "r"(flags));
}

inline void setCsr(uint32_t csr, uint32_t flags)
{
    __asm__ volatile ("csrw %0, %1" : : "i"(csr), "r"(flags));
}

#define CSR_MSTATUS 0x300
#define CSR_MIE 0x304
#define CSR_MTVEC 0x305

#define MSTATUS_MIE 0x8
#define MIE_TIMER 0x80

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

    g_devices[DEVICE_VIDEO] = (uint32_t)video_buffer | 0x80000000;

    setCsr(CSR_MTVEC, 0x00000004); // Just for show - it is hardcoded
    setCsrFlags(CSR_MIE, MIE_TIMER);
    setCsrFlags(CSR_MSTATUS, MSTATUS_MIE);

    for (;;)
    {
        current_color = color;

        uint32_t pixels = 0x00004444 | ((uint32_t)current_color << 16);
        for (int y = 0; y != 96; ++y)
        {
            for (int x = 0; x != 16; ++x)
            {
                video_buffer[y * 16 + x] = pixels;
            }
        }

        g_devices[DEVICE_TIMER] = 500000;

        while (current_color == color);
    }
}
