#include "executable.h"

#include "elf.h"

#include <array>
#include <cstddef>
#include <fstream>
#include <stdexcept>

namespace imp_sim
{

Executable::Executable(const std::filesystem::path& filename)
    : m_data(16 * 1024 * 1024)
{
    std::ifstream file;
    file.exceptions(std::ifstream::badbit | std::ifstream::failbit);
    file.open(filename, std::ifstream::in | std::ifstream::binary);

    std::array<std::uint8_t, EI_NIDENT> ident;
    if (file.readsome(reinterpret_cast<char*>(ident.data()), ident.size()) == ident.size() &&
        ident[EI_MAG0] == ELFMAG0 && ident[EI_MAG1] == ELFMAG1 && ident[EI_MAG2] == ELFMAG2 && ident[EI_MAG3] == ELFMAG3)
    {
        loadELF(file);
    }
    else
    {
        loadBinary(file);
    }
}

void Executable::loadBinary(std::istream& stream)
{
    stream.seekg(0, std::istream::end);
    auto fileSize = stream.tellg();
    if (static_cast<std::size_t>(fileSize) > m_data.size())
    {
        throw std::runtime_error("File too large");
    }
    stream.seekg(0, std::istream::beg);
    stream.read(reinterpret_cast<char*>(m_data.data()), std::min<std::size_t>(fileSize, m_data.size()));
}

void Executable::loadELF(std::istream& stream)
{
    stream.seekg(0, std::istream::beg);

    Elf32_Ehdr hdr;
    stream.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));

    if (hdr.e_ident[EI_MAG0] != ELFMAG0 || hdr.e_ident[EI_MAG1] != ELFMAG1 || hdr.e_ident[EI_MAG2] != ELFMAG2 || hdr.e_ident[EI_MAG3] != ELFMAG3)
    {
        throw std::runtime_error("Invalid ELF executable");
    }
    if (hdr.e_ident[EI_CLASS] != ELFCLASS32)
    {
        throw std::runtime_error("Only 32-bit ELF files supported");
    }
    if (hdr.e_ident[EI_DATA] != ELFDATA2LSB)
    {
        throw std::runtime_error("Only little-endian ELF files supported");
    }
    if (hdr.e_ident[EI_VERSION] != EV_CURRENT)
    {
        throw std::runtime_error("Only ELF version 1 supported");
    }

    if (hdr.e_phentsize < sizeof(Elf32_Phdr))
    {
        throw std::runtime_error("Program headers too small");
    }
    stream.seekg(hdr.e_phoff, std::istream::beg);
    for (std::size_t i = 0; i != hdr.e_phnum; ++i)
    {
        stream.seekg(static_cast<std::streamoff>(hdr.e_phoff) + i * hdr.e_phentsize, std::istream::beg);

        Elf32_Phdr programHeader;
        stream.read(reinterpret_cast<char*>(&programHeader), sizeof(programHeader));

        if (programHeader.p_type != PL_LOAD || programHeader.p_filesz == 0)
        {
            continue;
        }

        auto startOffset = programHeader.p_paddr;
        auto endOffset = startOffset + programHeader.p_filesz;

        if (endOffset > m_data.size())
        {
            throw std::runtime_error("Section overflows ROM buffer");
        }

        stream.seekg(programHeader.p_offset, std::istream::beg);
        stream.read(reinterpret_cast<char*>(m_data.data() + startOffset), programHeader.p_filesz);
    }

    if (hdr.e_shentsize < sizeof(Elf32_Shdr))
    {
        throw std::runtime_error("Section headers too small");
    }
    stream.seekg(hdr.e_shoff, std::istream::beg);
    for (std::size_t i = 0; i != hdr.e_shnum; ++i)
    {
        stream.seekg(static_cast<std::streamoff>(hdr.e_shoff) + i * hdr.e_shentsize, std::istream::beg);

        Elf32_Shdr sectionHeader;
        stream.read(reinterpret_cast<char*>(&sectionHeader), sizeof(sectionHeader));

        if (sectionHeader.sh_type == SHT_SYMTAB)
        {
            if (sectionHeader.sh_link >= hdr.e_shnum)
            {
                throw std::runtime_error("Symbol table points to invalid string table section");
            }

            stream.seekg(static_cast<std::streamoff>(hdr.e_shoff) + sectionHeader.sh_link * hdr.e_shentsize, std::istream::beg);
            Elf32_Shdr stringTableSectionHeader;
            stream.read(reinterpret_cast<char*>(&stringTableSectionHeader), sizeof(stringTableSectionHeader));

            std::string stringTable;
            stringTable.resize(stringTableSectionHeader.sh_size);
            stream.seekg(stringTableSectionHeader.sh_offset);
            stream.read(stringTable.data(), stringTable.size());

            if (static_cast<std::size_t>(sectionHeader.sh_info) * sectionHeader.sh_entsize > sectionHeader.sh_size)
            {
                throw std::runtime_error("Symbol count exceeeds symbol table capacity");
            }
            if (sectionHeader.sh_entsize < sizeof(Elf32_Sym))
            {
                throw std::runtime_error("Symbol size too small");
            }

            std::size_t symbolCount = sectionHeader.sh_size / sectionHeader.sh_entsize;

            for (std::size_t j = 0; j != symbolCount; ++j)
            {
                stream.seekg(static_cast<std::streamoff>(sectionHeader.sh_offset) + j * sectionHeader.sh_entsize);

                Elf32_Sym symbol;
                stream.read(reinterpret_cast<char*>(&symbol), sizeof(symbol));

                if (symbol.st_name == 0 || elf32StType(symbol.st_info) != STT_FUNC)
                {
                    continue;
                }

                if (symbol.st_name > stringTable.size())
                {
                    throw std::runtime_error("Symbol name exceeds string table size");
                }

                const char* name = stringTable.data() + symbol.st_name;

                m_symbols[symbol.st_value] = std::string(name);
            }

            break;
        }
    }
}

std::string_view Executable::resolve(std::uint32_t address) const noexcept
{
    auto it = m_symbols.lower_bound(address);
    if (it == m_symbols.end())
    {
        return {};
    }
    if (it->first == address)
    {
        return it->second;
    }
    if (it == m_symbols.begin())
    {
        return {};
    }
    --it;
    return it->second;
}

} // namespace imp_sim
