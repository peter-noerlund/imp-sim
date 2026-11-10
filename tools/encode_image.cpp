#include <stb_image.h>

#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <exception>
#include <memory>
#include <print>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace imp_sim
{

template<typename T>
struct StbImageFreer
{
    inline void operator()(T* ptr) const noexcept
    {
        stbi_image_free(ptr);
    }
};

template<typename T>
using UniqueStbImage = std::unique_ptr<T, StbImageFreer<T>>;

constexpr std::uint32_t colorDistance222(std::uint8_t c1, std::uint8_t c2) noexcept
{
    auto r1 = c1 >> 4;
    auto r2 = c2 >> 4;
    auto g1 = (c1 >> 2) & 0x03;
    auto g2 = (c2 >> 2) & 0x03;
    auto b1 = c1 & 0x03;
    auto b2 = c2 & 0x03;
    auto rd = abs(static_cast<std::int8_t>(r1) - static_cast<std::int8_t>(r2));
    auto gd = abs(static_cast<std::int8_t>(g1) - static_cast<std::int8_t>(g2));
    auto bd = abs(static_cast<std::int8_t>(b1) - static_cast<std::int8_t>(b2));
    return static_cast<std::uint32_t>(rd) * rd + static_cast<std::uint32_t>(gd) + gd + static_cast<std::uint32_t>(bd) * bd;
}

constexpr std::uint8_t chrominance(std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept
{
    return r * 3 + g * 7 + b;
}

void encodeImage(const std::filesystem::path& filename, std::set<std::string>& variableNames)
{
    int width = -1;
    int height = -1;
    int components = -1;
    UniqueStbImage<stbi_uc> image(stbi_load(filename.c_str(), &width, &height, &components, 3));
    if (!image)
    {
        throw std::runtime_error(std::format("Error opening file: {}", filename.string()));
    }

    if (width % 8 != 0)
    {
        throw std::invalid_argument("Image width must be a multiple of 8");
    }
    if (width <= 0 || height <= 0)
    {
        throw std::invalid_argument("Invalid resolution");
    }

    std::vector<std::uint32_t> chunks;
    chunks.reserve(width * height / 8);

    auto ptr = image.get();
    for (int y = 0; y != height; ++y)
    {
        for (int x = 0; x != width; x+= 8)
        {
            std::array<std::uint8_t, 8> pixels;
            std::array<std::uint8_t, 8> chrominances;
            std::uint16_t totalChrominance = 0;
            std::uint16_t chrominanceCount = 0;

            // Convert from RGB888 to RGB222 and calculate chrominance
            for (int dx = 0; dx != 8; ++dx)
            {
                auto r = (*ptr++) >> 6;
                auto g = (*ptr++) >> 6;
                auto b = (*ptr++) >> 6;

                auto p = (r << 4) | (g << 2) | b;
                auto c = chrominance(r, g, b);
                pixels[dx] = p;
                chrominances[dx] = c;
                if (p != 0)
                {
                    totalChrominance += c;
                    chrominanceCount++;
                }
            }

            std::uint8_t avgChrominance = chrominanceCount == 0 ? 0 : totalChrominance / chrominanceCount;

            // Group pixels into low and high chrominance
            std::array<std::uint8_t, 2> rs = {};
            std::array<std::uint8_t, 2> gs = {};
            std::array<std::uint8_t, 2> bs = {};
            std::array<std::uint8_t, 2> counts = {};
            for (int dx = 0; dx != 8; ++dx)
            {
                auto color222 = pixels[dx];
                if (color222 == 0)
                {
                    continue;
                }

                auto r = color222 >> 4;
                auto g = (color222 >> 2) & 0x03;
                auto b = color222 & 0x03;

                auto index = chrominances[dx] < avgChrominance ? 0 : 1;

                rs[index] += r;
                gs[index] += g;
                bs[index] += b;
                counts[index]++;
            }

            // Determine colors
            std::array<std::uint8_t, 3> colors;
            colors[0] = 0;
            for (std::size_t i = 0; i != rs.size(); ++i)
            {
                auto count = counts[i];
                if (count != 0)
                {
                    rs[i] /= count;
                    gs[i] /= count;
                    bs[i] /= count;
                }

                colors[i + 1] = (rs[i] << 4) | (gs[i] << 2) | bs[i];
            }

            // Encode
            std::uint32_t chunk = (static_cast<std::uint32_t>(colors[2]) << 24) | (static_cast<std::uint32_t>(colors[1]) << 16);
            for (std::size_t dx = 0; dx != 8; dx += 2)
            {
                std::uint8_t subMask = 0;
                for (std::size_t i = 0; i != 2; ++i)
                {
                    if (pixels[dx + i] == 0)
                    {
                        continue;
                    }
                    else if (chrominances[dx + i] < avgChrominance)
                    {
                        subMask |= 1 << (i * 2);
                    }
                    else
                    {
                        subMask |= 2 << (i * 2);
                    }
                }

                if (subMask == 9) // 1001
                {
                    // Unsupported pattern, so we calculate the best alternative
                    auto p1 = pixels[dx];
                    auto p2 = pixels[dx + 1];

                    subMask = 0;
                    std::uint8_t bestError = colorDistance222(p1, 0) + colorDistance222(p2, 0);
                    for (int i1 = 0; i1 != 3; ++i1)
                    {
                        for (int i2 = 0; i2 != 3; ++i2)
                        {
                            if ((i1 == 0 && i2 == 0) || (i1 == 1 || i2 == 2))
                            {
                                // Black is already tried and [c1 c2] is invalid
                                continue;
                            }

                            std::uint8_t error = colorDistance222(p1, colors[i1]) + colorDistance222(p2, colors[i2]);
                            if (error < bestError)
                            {
                                bestError = error;
                                subMask = i2 << 2 | i1;
                            }
                        }
                    }
                }

                static const auto mappings = std::to_array<std::uint8_t>({
                        0,  // 00_00 [b b]
                        1,  // 00_01 [1 b]
                        2,  // 00_10 [2 b]
                      255,
                        3,  // 01_00 [b 1]
                        4,  // 01_01 [1 1]
                        5,  // 01_02 [2 1]
                      255,
                        6,  // 02_00 [b 2]
                      255,
                        7,  // 02_02 [2 2]
                      255,
                      255,
                      255,
                      255,
                      255
                });

                auto subMask3 = mappings[subMask & 0x0F];
                if (subMask3 == 255)
                {
                    throw std::logic_error(std::format("Invalid submask: 0x{:X}", subMask));
                }

                chunk |= static_cast<std::uint32_t>(subMask3) << (dx * 2);
            }

            chunks.push_back(chunk);
        }
    }

    std::string variableName;
    for (auto c : filename.stem().string())
    {
        if (std::isalnum(c))
        {
            variableName.push_back(c);
        }
        else
        {
            variableName.push_back('_');
        }
    }

    if (variableNames.contains(variableName))
    {
        int suffix = 0;

        std::string suffixedName;
        do
        {
            suffixedName = std::format("{}_{}", variableName, suffix);
        } while (variableNames.contains(suffixedName));

        variableNames.insert(suffixedName);

        variableName = std::move(suffixedName);
    }

    std::print("uint32_t {}[{}] = {{", variableName, chunks.size());
    if (width == 8)
    {
        bool first = true;
        for (auto chunk : chunks)
        {
            if (first)
            {
                first = false;
            }
            else
            {
                std::print(", ");
            }
            std::print("0x{:08X}", chunk);
        }
    }
    else
    {
        std::println("");
        for (int y = 0; y != height; ++y)
        {
            std::print("    ");
            for (int x = 0; x != width / 8; ++x)
            {
                std::print("{}0x{:08X}", x == 0 ? "" : ", ", chunks[y * width / 8 + x]);
            }
            if (y + 1 != height)
            {
                std::print(",");
            }
            std::println("");
        }
    }
    std::println("}};");
}

} // namespace imp_sim

int main(int argc, char** argv)
{
    using namespace imp_sim;

    if (argc < 2)
    {
        std::println(stderr, "Usage: encode-image FILENAME ...");
        return EXIT_FAILURE;
    }

    try
    {
        std::set<std::string> variableNames;
        for (int i = 1; i < argc; ++i)
        {
            encodeImage(argv[i], variableNames);
        }

        return EXIT_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        std::println(stderr, "Caught exception: {}", exception.what());
        return EXIT_FAILURE;
    }
}
