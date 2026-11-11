#pragma once

#include "bad_register_exception.h"

#include <cstdint>
#include <span>
#include <vector>

namespace imp_sim
{

class Executable;
class Interface;

class Simulator
{
public:
    Simulator(Executable& executable, Interface& interface);

    void run(bool realTime);

    std::uint8_t load8(std::uint32_t address) const;
    std::uint16_t load16(std::uint32_t address) const;
    std::uint32_t load32(std::uint32_t address) const;
    void store8(std::uint32_t address, std::uint8_t value);
    void store16(std::uint32_t address, std::uint16_t value);
    void store32(std::uint32_t address, std::uint32_t value);

private:
    enum class CsrReg : std::uint16_t
    {
        MSTATUS = 0x300,
        MIE = 0x304,
        MTVEC = 0x305,
        MSCRATCH = 0x340,
        MEPC = 0x341,
        MCAUSE = 0x342,
        MTVAL = 0x343
    };

    enum MStatusFlags : std::uint32_t
    {
        MSTATUS_INTERRUPT = 0x80000000,
        MSTATUS_TIMER_INTE
    };

    void executeInstruction();

    void execLuiInst(std::uint32_t inst);
    void execAuipcInst(std::uint32_t inst);
    void execJalInst(std::uint32_t inst);
    void execJalrInst(std::uint32_t inst);
    void execBranchInst(std::uint32_t inst);
    void execLoadInst(std::uint32_t inst);
    void execStoreInst(std::uint32_t inst);
    void execOpImmInst(std::uint32_t inst);
    void execOpInst(std::uint32_t inst);
    void execMiscMemInst(std::uint32_t inst);
    void execSystemInst(std::uint32_t inst);

    std::uint32_t getCsr(std::uint16_t csr);
    void setCsr(std::uint16_t csr, std::uint32_t value);

    constexpr std::int32_t getReg(std::uint8_t reg) const
    {
        if (reg > m_registers.size())
        {
            throw BadRegisterException(reg);
        }
        return m_registers[reg];
    }

    constexpr void setReg(std::uint8_t reg, std::int32_t value)
    {
        if (reg == 0 || reg == 3 || reg == 4)
        {
            return;
        }
        if (reg > m_registers.size())
        {
            throw BadRegisterException(reg);
        }
        m_registers[reg] = value;
    }

    static constexpr std::uint8_t getRd(std::uint32_t inst) noexcept
    {
        return static_cast<std::uint8_t>((inst >> 7) & 0x1F);
    }

    static constexpr std::uint8_t getRs1(std::uint32_t inst) noexcept
    {
        return static_cast<std::uint8_t>((inst >> 15) & 0x1F);
    }

    static constexpr std::uint8_t getRs2(std::uint32_t inst) noexcept
    {
        return static_cast<std::uint8_t>((inst >> 20) & 0x1F);
    }

    static constexpr std::int32_t signExtend(std::uint32_t inst, std::uint32_t mask, std::uint32_t val) noexcept
    {
        return static_cast<std::int32_t>(val | ((inst & 0x80000000) ? mask : 0));
    }

    static constexpr std::int32_t getImmU(std::uint32_t inst) noexcept
    {
        return static_cast<std::int32_t>(inst & 0xFFFFF000);
    }

    static constexpr std::int32_t getImmJ(std::uint32_t inst) noexcept
    {
        // 3322 2222 2222 1111 1111 11
        // 1098 7654 3210 9876 5432 1098 7654 3210
        // daaa aaaa aaab cccc cccc ---- ---- ----
        //
        // a = imm[10:1]
        // b = imm[11]
        // c = imm[19:12]
        // d = imm[20] (sign bit)

        return signExtend(
                inst,
                0xFFF00000,                             // xxxx xxxx xxxx ---- ---- ---- ---- ----
                ((inst >> (21 - 1))  & 0x000007FE) |    // ---- ---- ---- ---- ---- -xxx xxxx xxx0
                ((inst >> (20 - 11)) & 0x00000800) |    // ---- ---- ---- ---- ---- x--- ---- ----
                (inst                & 0x000FF000));    // ---- ---- ---- xxxx xxxx ---- ---- ----
    }

    static constexpr std::int32_t getImmI(std::uint32_t inst) noexcept
    {
        // 3322 2222 2222 1111 1111 11
        // 1098 7654 3210 9876 5432 1098 7654 3210
        // aaaa aaaa aaaa ---- ---- ---- ---- ----
        //
        // a = imm[11:0]

        return signExtend(
                inst,
                0xFFFFF000,                             // xxxx xxxx xxxx xxxx xxxx ---- ---- ----
                (inst >> 20) & 0x00000FFFF);            // ---- ---- ---- ---- ---- xxxx xxxx xxxx
    }

    static constexpr std::int32_t getImmB(std::uint32_t inst) noexcept
    {
        // 3322 2222 2222 1111 1111 11
        // 1098 7654 3210 9876 5432 1098 7654 3210
        // dbbb bbb- ---- ---- ---- aaaa c--- ----
        //
        // a = imm[4:1]
        // b = imm[10:5]
        // c = imm[11]
        // d = imm[12] (sign bit)

        return signExtend(
                inst,
                0xFFFFF000,                             // xxxx xxxx xxxx xxxx xxxx ---- ---- ----
                ((inst >> (8 - 1))  & 0x0000001E) |     // ---- ---- ---- ---- ---- ---- ---x xxx0
                ((inst >> (25 - 5)) & 0x000007E0) |     // ---- ---- ---- ---- ---- -xxx xxx- ----
                ((inst << 4)        & 0x00000800));     // ---- ---- ---- ---- ---- x--- ---- ----
    }

    static constexpr std::int32_t getImmS(std::uint32_t inst) noexcept
    {
        // 3322 2222 2222 1111 1111 11
        // 1098 7654 3210 9876 5432 1098 7654 3210
        // bbbb bbb- ---- ---- ---- aaaa a--- ----
        //
        // a = imm[4:0]
        // b = imm[11:5]

        return signExtend(
                inst,
                0xFFFFF000,                             // xxxx xxxx xxxx xxxx xxxx ---- ---- ----
                ((inst >> 7)        & 0x0000001F) |     // ---- ---- ---- ---- ---- ---- ---x xxxx
                ((inst >> (25 - 5)  & 0x00000FE0)));    // ---- ---- ---- ---- ---- xxxx xxx- ----
    }

    static constexpr std::uint8_t getFunct3(std::uint32_t inst) noexcept
    {
        return (inst >> 12) & 0x07;
    }

    static constexpr std::uint8_t getFunct7(std::uint32_t inst) noexcept
    {
        return inst >> 25;
    }

    static constexpr std::uint16_t getFunct12(std::uint32_t inst) noexcept
    {
        return inst >> 20;
    }

    std::array<std::int32_t, 16> m_registers = {};
    std::uint32_t m_pc = 0;
    std::uint32_t m_mstatus = 0;
    std::uint32_t m_mie = 0;
    std::uint32_t m_mepc = 0;
    std::uint32_t m_mcause = 0;
    std::uint32_t m_mtval = 0;
    unsigned long long int m_interruptCycle = 0;
    unsigned long long int m_cycleCounter = 0;
    bool m_interrupted = false;

    Executable& m_executable;
    Interface& m_interface;
    std::vector<std::uint8_t> m_ram;
};

} // namespace imp_sim
