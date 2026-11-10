#pragma once

#include <array>
#include <cstdint>

namespace imp_sim
{

class VideoDecoder
{
public:
    static std::array<std::uint8_t, 8> decode(std::uint32_t chunk) noexcept;
};

} // namespace imp_sim
