#pragma once

#include <cstdint>
#include <iostream>
#include <filesystem>
#include <map>
#include <span>
#include <vector>
#include <string>

namespace imp_sim
{

class Executable
{
public:
    explicit Executable(const std::filesystem::path& filename);

    constexpr std::span<const std::uint8_t> data() const noexcept
    {
        return m_data;
    }

    std::string_view resolve(std::uint32_t address) const noexcept;

    constexpr const std::map<std::uint32_t, std::string> symbols() const noexcept
    {
        return m_symbols;
    }

private:
    void loadBinary(std::istream& stream);
    void loadELF(std::istream& stream);

    std::vector<std::uint8_t> m_data;
    std::map<std::uint32_t, std::string> m_symbols;
};

} // namespace imp_sim
