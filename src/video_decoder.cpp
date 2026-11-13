#include "video_decoder.h"

namespace imp_sim
{

std::array<std::uint8_t, 8> VideoDecoder::decode(std::uint32_t chunk) noexcept
{
    std::uint8_t c2 = (chunk >> 24) & 0x3F;
    std::uint8_t c1 = (chunk >> 16) & 0x3F;
    
    std::uint8_t r1 = c1 >> 4;
    std::uint8_t g1 = (c1 >> 2) & 0x03;
    std::uint8_t b1 = c1 & 0x03;
    std::uint8_t r2 = c2 >> 4;
    std::uint8_t g2 = (c2 >> 2) & 0x03;
    std::uint8_t b2 = c2 & 0x03;
    std::uint8_t r3 = (r1 + r2) >> 1;
    std::uint8_t g3 = (g1 + g2) >> 1;
    std::uint8_t b3 = (b1 + b2) >> 1;
    std::uint8_t c3 = (r3 << 4) | (g3 << 2) | b3;

    std::array<std::uint8_t, 4> colors = {0, c1, c2, c3};
    std::array<std::uint8_t, 8> pixels;

    for (int i = 0; i != 8; ++i)
    {
        pixels[i] = colors[(chunk >> (i * 2)) & 0x03];
    }

    return pixels;
}

} // namespace imp_sim
