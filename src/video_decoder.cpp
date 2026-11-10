#include "video_decoder.h"

namespace imp_sim
{

std::array<std::uint8_t, 8> VideoDecoder::decode(std::uint32_t chunk) noexcept
{
    enum class PixelGroup : std::uint8_t
    {
        BlackBlack = 0,
        BlackColor1 = 1,
        BlackColor2 = 2,
        Color1Black = 3,
        Color1Color1 = 4,
        Color1Color2 = 5,
        Color2Black = 6,
        Color2Color2 = 7
    };

    auto c2 = static_cast<std::uint8_t>(chunk >> 24) & 0x3F;
    auto c1 = static_cast<std::uint8_t>(chunk >> 16) & 0x3F;
    std::array<PixelGroup, 4> groups = {
        static_cast<PixelGroup>(chunk & 0x7),
        static_cast<PixelGroup>((chunk >> 4) & 0x07),
        static_cast<PixelGroup>((chunk >> 8) & 0x07),
        static_cast<PixelGroup>((chunk >> 12) & 0x07)
    };

    std::array<std::uint8_t, 8> pixels;
    for (std::size_t i = 0; i != groups.size(); ++i)
    {
        auto group = static_cast<PixelGroup>(groups[i]);

        switch (group)
        {
            case PixelGroup::BlackBlack:
                pixels[i * 2] = 0;
                pixels[i * 2 + 1] = 0;
                break;

            case PixelGroup::BlackColor1:
                pixels[i * 2] = c1;
                pixels[i * 2 + 1] = 0;
                break;

            case PixelGroup::BlackColor2:
                pixels[i * 2] = c2;
                pixels[i * 2 + 1] = 0;
                break;

            case PixelGroup::Color1Black:
                pixels[i * 2] = 0;
                pixels[i * 2 + 1] = c1;
                break;

            case PixelGroup::Color1Color1:
                pixels[i * 2] = c1;
                pixels[i * 2 + 1] = c1;
                break;

            case PixelGroup::Color1Color2:
                pixels[i * 2] = c2;
                pixels[i * 2 + 1] = c2;
                break;

            case PixelGroup::Color2Black:
                pixels[i * 2] = 0;
                pixels[i * 2 + 1] = c2;
                break;

            case PixelGroup::Color2Color2:
                pixels[i * 2] = c2;
                pixels[i * 2 + 1] = c2;
                break;
        }
    }

    return pixels;
}

} // namespace imp_sim
