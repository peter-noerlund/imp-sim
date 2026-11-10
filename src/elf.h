#pragma once

#include <cstdint>
#include <cstddef>

namespace imp_sim
{

using Elf32_Off = std::uint32_t;
using Elf32_Addr = std::uint32_t;
using Elf32_Half = std::uint16_t;
using Elf32_Word = std::uint32_t;
using Elf32_Sword = std::int32_t;

enum ObjectFileTypes : std::uint16_t
{
    ET_NONE = 0,
    ET_REL = 1,
    ET_EXEC = 2,
    ET_DYN = 3,
    ET_CORE = 4,
    ET_LOOS = 0xFE00,
    ET_HIOS = 0xFEFF,
    ET_LOPROC = 0xFF00,
    ET_HIPROC = 0xFFFF
};

enum ElfVersion : Elf32_Word
{
    EV_NONE = 0,
    EV_CURRENT = 1
};

enum IdentIndex
{
    EI_MAG0 = 0,
    EI_MAG1 = 1,
    EI_MAG2 = 2,
    EI_MAG3 = 3,
    EI_CLASS = 4,
    EI_DATA = 5,
    EI_VERSION = 6,
    EI_OSABI = 7,
    EI_ABIVERSION = 8,
    EI_PAD = 9,
    EI_NIDENT = 16
};

enum ElfMagic : std::uint8_t
{
    ELFMAG0 = 0x7F,
    ELFMAG1 = 'E',
    ELFMAG2 = 'L',
    ELFMAG3 = 'F'
};

enum ElfClass : std::uint8_t
{
    ELFCLASSNONE = 0,
    ELFCLASS32 = 1,
    ELFCLASS64 = 2
};

enum ElfData : std::uint8_t
{
    ELFDATANONE = 0,
    ELFDATA2LSB = 1,
    ELFDATA2MSB = 2
};

enum ElfSegmentType : Elf32_Word
{
    PL_NULL = 0,
    PL_LOAD = 1,
    PL_DYNAMIC = 2,
    PL_INTERP = 3,
    PL_NOTE = 4,
    PL_SHLIB = 5,
    PL_PHDR = 6,
    PL_TLS = 7,
    PL_LOOS = 0x60000000,
    PL_HIOS = 0x6FFFFFFF,
    PL_LOPROC = 0x70000000,
    PL_HIPROC = 0x7FFFFFFF
};

enum ElfSectionType : Elf32_Word
{
    SHT_NULL = 0,
    SHT_PROGBITS = 1,
    SHT_SYMTAB = 2,
    SHT_STRTAB = 3,
    SHT_RELA = 4,
    SHT_HASH = 5,
    SHT_DYNAMIC = 6,
    SHT_NOTE = 7,
    SHT_NOBITS = 8,
    SHT_REL = 9,
    SHT_SHLIB = 10,
    SHT_DYNSYM = 11,
    SHT_INIT_ARRAY = 14,
    SHT_FINI_ARRAY = 15,
    SHT_PREINIT_ARRAY = 16,
    SHT_GROUP = 17,
    SHT_SYMTAB_SHNDX = 18,
    SHT_RELR = 19,
    SHT_LOOS = 0x60000000,
    SHT_HIOS = 0x6FFFFFFF,
    SHT_LOPROC = 0x70000000,
    SHT_HIPROC = 0x7FFFFFFF,
    SHT_LOUSER = 0x80000000,
    SHT_HIUSER = 0xFFFFFFFF
};

enum ElfSymbolBinding : std::uint8_t
{
    STB_LOCAL = 0,
    STB_GLOBAL = 1,
    STB_WEAK = 2,
    STB_LOOS = 10,
    STB_HIOS = 12,
    STB_LOPROC = 13,
    STB_HIPROC = 15
};

enum ElfSymbolType : std::uint8_t
{
    STT_NOTYPE = 0,
    STT_OBJECT = 1,
    STT_FUNC = 2,
    STT_SECTION = 3,
    STT_FILE = 4,
    STT_COMMON = 5,
    STT_TLS = 6,
    STT_LOOS = 10,
    STT_HIOS = 12,
    STT_LOPROC = 13,
    STT_HIPROC = 15
};

constexpr std::uint8_t elf32StBind(std::uint8_t info) noexcept
{
    return info >> 4;
}

constexpr std::uint8_t elf32StType(std::uint8_t info) noexcept
{
    return info & 0x0F;
}

struct Elf32_Ehdr
{
    std::uint8_t e_ident[EI_NIDENT];
    Elf32_Half e_type;
    Elf32_Half e_machine;
    Elf32_Word e_version;
    Elf32_Addr e_entry;
    Elf32_Off e_phoff;
    Elf32_Off e_shoff;
    Elf32_Word e_flags;
    Elf32_Half e_ehsize;
    Elf32_Half e_phentsize;
    Elf32_Half e_phnum;
    Elf32_Half e_shentsize;
    Elf32_Half e_shnum;
    Elf32_Half e_shstrndx;
};

struct Elf32_Phdr
{
    Elf32_Word p_type;
    Elf32_Off p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
};

struct Elf32_Shdr
{
    Elf32_Word sh_name;
    Elf32_Word sh_type;
    Elf32_Word sh_flags;
    Elf32_Addr sh_addr;
    Elf32_Off sh_offset;
    Elf32_Word sh_size;
    Elf32_Word sh_link;
    Elf32_Word sh_info;
    Elf32_Word sh_addralign;
    Elf32_Word sh_entsize;
};

struct Elf32_Sym
{
    Elf32_Word st_name;
    Elf32_Addr st_value;
    Elf32_Word st_size;
    std::uint8_t st_info;
    std::uint8_t st_other;
    Elf32_Half st_shndx;
};

} // namespace imp_sim
