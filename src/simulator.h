#pragma once

#include "bad_register_exception.h"

#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace imp_sim
{

class AudioInterface;
class Executable;
class InstructionHook;
class Interface;

class Simulator
{
public:
    Simulator(Executable& executable, Interface& interface, AudioInterface& audioInterface);
    ~Simulator();

    void run(bool realTime, bool trace);

    void addHook(InstructionHook& hook);

    std::uint8_t load8(std::uint32_t address) const;
    std::uint16_t load16(std::uint32_t address) const;
    std::uint32_t load32(std::uint32_t address) const;
    void store8(std::uint32_t address, std::uint8_t value);
    void store16(std::uint32_t address, std::uint16_t value);
    void store32(std::uint32_t address, std::uint32_t value);

private:
    enum class CsrReg : std::uint16_t
    {
        MSTATUS = 0x300,    // Machine Status. mstatus[3] is MIE (Machine Interrupt Enable) and mstatus[7] is MPIE (Machine Previous Interrupt Enable)
        MIE = 0x304,        // Machine Interrupt Enable mask. mie[7] is machine timer interrupt
        MTVEC = 0x305,      // Machine Trap Vector Base. Stores base address of interrupt vectors (which can be either a list or a single address)
        MSCRATCH = 0x340,   // Scratch register for interrupts/traps
        MEPC = 0x341,       // Machine Exception PC. Stores return address for interrupt/trap
        MCAUSE = 0x342,     // Machine Cause. Stores cause of interrupt/trap. mstatus[31] is interrupt indicator, mstatus[7] is machine timer interrupt
        MTVAL = 0x343       // Machine Trap Value. Stores offending address in case of trap
    };

    static constexpr std::uint32_t s_mstatusMIE  = 0x00000008;  // Machine Interrupt Enable
    static constexpr std::uint32_t s_mstatusMPIE = 0x00000080;  // Machine Previous Interrupt Enable
    static constexpr std::uint32_t s_mstatusMPP  = 0x00001800;
    static constexpr std::uint32_t s_mstatusWriteMask = s_mstatusMIE | s_mstatusMPIE;

    static constexpr std::uint32_t s_mieMTIE = 0x00000080;      // Machine Timer Interrupt Enable
    static constexpr std::uint32_t s_mieWriteMask = s_mieMTIE;

    static constexpr std::uint32_t s_mcauseInterrupt = 0x80000000;  // Interrupt cause
    static constexpr std::uint32_t s_mcauseTimer = 7;
    static constexpr std::uint32_t s_mcauseBreakpoint = 3;
    static constexpr std::uint32_t s_mcauseLoadAddressMisalign = 4;
    static constexpr std::uint32_t s_mcauseStoreAddressMisalign = 6;
    static constexpr std::uint32_t s_mcauseEnvironmentCall = 11;
    static constexpr std::uint32_t s_mcauseWriteMask = s_mcauseInterrupt | s_mcauseTimer | s_mcauseBreakpoint | s_mcauseLoadAddressMisalign | s_mcauseStoreAddressMisalign | s_mcauseEnvironmentCall;

    void executeInstruction(bool interrupt);

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
    std::uint32_t m_mstatus = s_mstatusMPP;
    std::uint32_t m_mie = 0;
    std::uint32_t m_mepc = 0;
    std::uint32_t m_mcause = 0;
#ifdef TT_CACHE
    std::uint32_t m_cachedPc = std::numeric_limits<std::uint32_t>::max();
#endif // TT_CACHE
    bool m_interruptEnable = false;
    bool m_prevInterruptEnable = false;
    bool m_timerInterruptEnable = false;
    bool m_timerInterrupted = false;
    unsigned long long int m_interruptCycle = 0;
    unsigned long long int m_cycleCounter = 0;
    bool m_interrupted = false;

    Executable& m_executable;
    Interface& m_interface;
    AudioInterface& m_audioInterface;
    std::vector<InstructionHook*> m_instructionHooks;
    std::vector<std::uint8_t> m_ram;
};

} // namespace imp_sim
