#include "simulator.h"

#include "audio_interface.h"
#include "bad_instruction_exception.h"
#include "executable.h"
#include "instruction_hook.h"
#include "interface.h"
#include "simulation_exception.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <thread>
#include <print>

namespace imp_sim
{

Simulator::Simulator(Executable& executable, Interface& interface, AudioInterface& audioInterface)
    : m_executable(executable)
    , m_interface(interface)
    , m_audioInterface(audioInterface)
    , m_ram(8 * 1024 * 1024)
{
}

Simulator::~Simulator()
{
    m_interface.stop();
    m_audioInterface.playFrequency(0);
}

void Simulator::addHook(InstructionHook& hook)
{
    m_instructionHooks.push_back(&hook);
}

void Simulator::run(bool realTime, bool trace)
{
    using namespace std::chrono_literals;

    m_interface.start(*this);

    m_registers[3] = 0x01000400;

    auto startTime = std::chrono::high_resolution_clock::now();
    unsigned long long int nextAdjustment = 666666;

    m_pc = 0;
    while (true)
    {
        bool interrupt = false;
        if ((m_mstatus & s_mstatusMIE) != 0 && (m_mie & s_mieMTIE) != 0 && m_cycleCounter > m_interruptCycle && m_interruptCycle != 0)
        {
            m_interruptCycle = 0;

            m_mstatus = s_mstatusMPP | s_mstatusMPIE;
            m_mcause = s_mcauseInterrupt | s_mcauseTimer;
            m_mepc = m_pc;
            m_pc = 8;
            interrupt = true;
        }

        if (trace)
        {
            std::print("PC=0x{:08X}", m_pc);
            for (unsigned int i = 0; i != 16; ++i)
            {
                std::print(" x{}=0x{:08X}", i, static_cast<std::uint32_t>(m_registers[i]));
            }
            std::println("");
        }

        executeInstruction(interrupt);

        if (realTime && m_cycleCounter >= nextAdjustment)
        {
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - startTime);
            auto expectedElapsed = 15ns * m_cycleCounter;
            if (elapsed < expectedElapsed)
            {
                auto shortageTime = expectedElapsed - elapsed;
                std::this_thread::sleep_for(shortageTime);
            }

            nextAdjustment = m_cycleCounter + 666666;
        }
    }

    m_interface.stop();
    m_audioInterface.playFrequency(0);
}

void Simulator::executeInstruction(bool interrupt)
{
    enum Opcode : std::uint8_t
    {
        LOAD = 0x03,
        CUSTOM0 = 0x0B,
        MISC_MEM = 0x0F,
        OP_IMM = 0x13,
        AUIPC = 0x17,
        STORE = 0x23,
        OP = 0x33,
        LUI = 0x37,
        BRANCH = 0x63,
        JALR = 0x67,
        JAL = 0x6F,
        SYSTEM = 0x73
    };

    auto pc = m_pc;
    std::uint32_t inst = load32(pc);

    m_cycleCounter += 40;

    auto opcode = static_cast<Opcode>(inst & 0x7F);
    switch (opcode)
    {
        case Opcode::LUI:
            execLuiInst(inst);
            break;

        case Opcode::AUIPC:
            execAuipcInst(inst);
            break;

        case Opcode::JAL:
            execJalInst(inst);
            break;

        case Opcode::JALR:
            execJalrInst(inst);
            break;

        case Opcode::BRANCH:
            execBranchInst(inst);
            break;

        case Opcode::LOAD:
            execLoadInst(inst);
            break;

        case Opcode::STORE:
            execStoreInst(inst);
            break;

        case Opcode::OP_IMM:
            execOpImmInst(inst);
            break;

        case Opcode::OP:
            execOpInst(inst);
            break;

        case Opcode::MISC_MEM:
            execMiscMemInst(inst);
            break;

        case Opcode::SYSTEM:
            execSystemInst(inst);
            break;

#ifdef TT_XMUL16
        case Opcode::CUSTOM0:
            execCustom0Inst(inst);
            break;
#endif // TT_XMULat

        default:
            throw BadInstructionException(inst);
    }

    for (auto hook : m_instructionHooks)
    {
        hook->postInstruction(pc, inst, m_cycleCounter, interrupt);
    }
}

void Simulator::execLuiInst(std::uint32_t inst)
{
    setReg(getRd(inst), getImmU(inst));
    m_pc += 4;
    m_cycleCounter += 8;
}

void Simulator::execAuipcInst(std::uint32_t inst)
{
    setReg(getRd(inst), m_pc + getImmU(inst));
    m_pc += 4;
    m_cycleCounter += 8;
}

void Simulator::execJalInst(std::uint32_t inst)
{
    setReg(getRd(inst), m_pc + 4);
    m_pc += getImmJ(inst);
    m_cycleCounter += 16;
}

void Simulator::execJalrInst(std::uint32_t inst)
{
    if (getFunct3(inst) != 0)
    {
        throw BadInstructionException(inst);
    }

    auto rd = getRd(inst);
    auto rs1 = getRs1(inst);
    auto imm = getImmI(inst);
    auto base = getReg(rs1);
    auto address = static_cast<std::uint32_t>(base + imm);

    setReg(rd, m_pc + 4);
    m_pc = address;
    m_cycleCounter += 16;
}

void Simulator::execBranchInst(std::uint32_t inst)
{
    enum class BranchFunct3 : std::uint8_t
    {
        BEQ = 0,
        BNE = 1,
        BLT = 4,
        BGE = 5,
        BLTU = 6,
        BGEU = 7
    };

    auto rs1 = getRs1(inst);
    auto rs2 = getRs2(inst);
    auto src1 = getReg(rs1);
    auto src2 = getReg(rs2);
    auto offset = getImmB(inst);
    auto funct3 = static_cast<BranchFunct3>(getFunct3(inst));

    bool cond;
    switch (funct3)
    {
        case BranchFunct3::BEQ:
            cond = src1 == src2;
            break;

        case BranchFunct3::BNE:
            cond = src1 != src2;
            break;

        case BranchFunct3::BLT:
            cond = src1 < src2;
            break;

        case BranchFunct3::BGE:
            cond = src1 >= src2;
            break;

        case BranchFunct3::BLTU:
            cond = static_cast<std::uint32_t>(src1) < static_cast<std::uint32_t>(src2);
            break;

        case BranchFunct3::BGEU:
            cond = static_cast<std::uint32_t>(src1) >= static_cast<std::uint32_t>(src2);
            break;

        default:
            throw BadInstructionException(inst);
    }

    if (cond)
    {
        m_pc += offset;
    }
    else
    {
        m_pc += 4;
    }
    m_cycleCounter += 16;
}

void Simulator::execLoadInst(std::uint32_t inst)
{
    auto width = getFunct3(inst);
    auto base = getReg(getRs1(inst));
    auto offset = getImmI(inst);
    auto address = base + offset;

    std::int32_t value;
    switch (width)
    {
        case 0:
            value = static_cast<std::int8_t>(load8(address));
            break;

       case 1:
            value = static_cast<std::int16_t>(load16(address));
            break;

       case 2:
            value = load32(address);
            break;

       case 4:
            value = static_cast<std::uint32_t>(load8(address));
            break;

       case 5:
            value = static_cast<std::uint32_t>(load16(address));
            break;

       default:
            throw BadInstructionException(inst);
    }

    setReg(getRd(inst), value);

    m_pc += 4;

    switch ((address >> 24) & 0x3)
    {
        case 0: // Flash
            m_cycleCounter += 8 + 40 + 8;
            break;

        case 1: // PSRAM
            m_cycleCounter += 8 + 40 + 8;
            break;

        default: // Devices
            m_cycleCounter += 8 + 8 + 8;
            break;
    }
}

void Simulator::execStoreInst(std::uint32_t inst)
{
    auto width = getFunct3(inst);
    auto base = getReg(getRs1(inst));
    auto src = getReg(getRs2(inst));
    auto offset = getImmS(inst);
    auto address = base + offset;

    m_cycleCounter += 16;

    switch (width)
    {
        case 0:
            store8(address, static_cast<std::uint8_t>(src));
            m_cycleCounter += 24;   // 2 * (2 + 6 + 2)
            break;

        case 1:
            store16(address, static_cast<std::uint16_t>(src));
            m_cycleCounter += 24;
            break;

        case 2:
            store32(address, static_cast<std::uint32_t>(src));
            m_cycleCounter += 32;
            break;

        default:
            throw BadInstructionException(inst);
    }

    m_pc += 4;
}

void Simulator::execOpImmInst(std::uint32_t inst)
{
    enum class OpImmFunct3 : std::uint8_t
    {
        ADDI = 0,
        OP_IMM_1 = 1,
        SLTI = 2,
        SLTIU = 3,
        XORI = 4,
        OP_IMM_5 = 5,
        ORI = 6,
        ANDI = 7,
    };

    enum class OpImm1Funct7 : std::uint8_t
    {
        SLLI = 0x00,
        ZBB = 0x30
    };

    enum class OpImm1ZbbFunct5 : std::uint8_t
    {
        CLZ = 0x00,
        CTZ = 0x01,
        CPOP = 0x02,
        SEXT_B = 0x04,
        SEXT_H = 0x05
    };

    enum class OpImm5Funct7 : std::uint8_t
    {
        SRLI = 0x00,
        ORC_B = 0x14,
        SRAI = 0x20,
        RORI = 0x30,
        REV8 = 0x34
    };

    auto src = getReg(getRs1(inst));
    auto imm = getImmI(inst);
    auto funct3 = static_cast<OpImmFunct3>(getFunct3(inst));
    auto funct7 = getFunct7(inst);
    std::int32_t value;

    switch (funct3)
    {
        case OpImmFunct3::ADDI:
            value = src + imm;
            m_cycleCounter += 8;
            break;

        case OpImmFunct3::OP_IMM_1:
            switch (static_cast<OpImm1Funct7>(funct7))
            {
                case OpImm1Funct7::SLLI:
                    value = src << imm;
                    m_cycleCounter += 16;
                    break;

#ifdef TT_ZBB
                case OpImm1Funct7::ZBB:
                    switch (static_cast<OpImm1ZbbFunct5>(getRs2(inst)))
                    {
                        case OpImm1ZbbFunct5::CLZ:
                            value = src == 0 ? 32 : __builtin_clz(src);
                            m_cycleCounter += 16;
                            break;

                        case OpImm1ZbbFunct5::CTZ:
                            value = src == 0 ? 32 : __builtin_ctz(src);
                            m_cycleCounter += 16;
                            break;

                        case OpImm1ZbbFunct5::SEXT_B:
                            value = static_cast<std::int8_t>(src);
                            m_cycleCounter += 16;
                            break;

                        case OpImm1ZbbFunct5::SEXT_H:
                            value = static_cast<std::int16_t>(src);
                            m_cycleCounter += 16;
                            break;

#ifndef TT_ZBB_SUBSET
                        case OpImm1ZbbFunct5::CPOP:
                            value = __builtin_popcount(src);
                            m_cycleCounter += 16;
                            break;
#endif // TT_ZBB_SUBSET

                        default:
                            throw BadInstructionException(inst);
                    }
                    break;

#endif // TT_ZBB

                default:
                    throw BadInstructionException(inst);
            }
            break;

        case OpImmFunct3::SLTI:
            value = src < imm ? 1 : 0;
            m_cycleCounter += 8;
            break;

        case OpImmFunct3::SLTIU:
            value = static_cast<std::uint32_t>(src) < static_cast<std::uint32_t>(imm) ? 1 : 0;
            m_cycleCounter += 8;
            break;

        case OpImmFunct3::XORI:
            value = src ^ imm;
            m_cycleCounter += 8;
            break;

        case OpImmFunct3::OP_IMM_5:
            switch (static_cast<OpImm5Funct7>(funct7))
            {
                case OpImm5Funct7::SRLI:
                    value = static_cast<std::uint32_t>(src) >> imm;
                    m_cycleCounter += 16;
                    break;

                case OpImm5Funct7::SRAI:
                    value = src >> (imm & 0x1F);
                    m_cycleCounter += 16;
                    break;

#if defined(TT_ZBB) && !defined(TT_ZBB_SUBSET)
                case OpImm5Funct7::ORC_B:
                    value =
                        ((src & 0xFF000000) ? 0x01000000 : 0x00000000) |
                        ((src & 0x00FF0000) ? 0x00010000 : 0x00000000) |
                        ((src & 0x0000FF00) ? 0x00000100 : 0x00000000) |
                        ((src & 0x000000FF) ? 0x00000001 : 0x00000000);
                    m_cycleCounter += 16;
                    break;

                case OpImm5Funct7::RORI:
                    value = (static_cast<std::uint32_t>(src) >> (getRs2(inst))) | (src << (32 - getRs2(inst)));
                    m_cycleCounter += 16;
                    break;

                case OpImm5Funct7::REV8:
                    value =
                        (src << 24) |
                        ((src & 0xFF00) << 8) |
                        ((src & 0xFF0000) >> 8) |
                        ((static_cast<std::uint32_t>(src) & 0xFF000000) >> 24);
                    m_cycleCounter += 16;
                    break;
#endif // TT_ZBB || TT_ZBB_SUBSET

                default:
                    throw BadInstructionException(inst);
            }
            break;

        case OpImmFunct3::ORI:
            value = src | imm;
            m_cycleCounter += 8;
            break;

        case OpImmFunct3::ANDI:
            value = src & imm;
            m_cycleCounter += 8;
            break;

        default:
            throw BadInstructionException(inst);
    }

    setReg(getRd(inst), value);

    m_pc += 4;
}

void Simulator::execOpInst(std::uint32_t inst)
{
    enum class OpFunct10 : std::uint16_t
    {
        // RV32E base
        ADD     = 0x000,
        SLL     = 0x001,
        SLT     = 0x002,
        SLTU    = 0x003,
        XOR     = 0x004,
        SRL     = 0x005,
        OR      = 0x006,
        AND     = 0x007,
        SUB     = 0x100,
        SRA     = 0x105,
#ifdef TT_ZBB
        // Zbb
        ZEXT_H  = 0x024,
        MIN     = 0x02C,
        MINU    = 0x02D,
        MAX     = 0x02E,
        MAXU    = 0x02F,
        ROL     = 0x081,
        ROR     = 0x085,
        XNOR    = 0x104,
        ORN     = 0x106,
        ANDN    = 0x107
#endif // TT_ZBB
    };

    auto rs1 = getRs1(inst);
    auto rs2 = getRs2(inst);
    auto src1 = getReg(rs1);
    auto src2 = getReg(rs2);
    auto funct3 = getFunct3(inst);
    auto funct7 = getFunct7(inst);
    auto funct10 = static_cast<OpFunct10>((static_cast<std::int16_t>(funct7) << 3) | funct3);
    std::int32_t value;

    switch (funct10)
    {
        case OpFunct10::ADD:
            value = src1 + src2;
            m_cycleCounter += 8;
            break;

        case OpFunct10::SUB:
            value = src1 - src2;
            m_cycleCounter += 8;
            break;

        case OpFunct10::SLL:
            value = src1 << src2;
            m_cycleCounter += 16;
            if (src2 != 0)
            {
                m_cycleCounter += 8;
            }
            break;

        case OpFunct10::SLT:
            value = src1 < src2 ? 1 : 0;
            m_cycleCounter += 8;
            break;

        case OpFunct10::SLTU:
            value = static_cast<std::uint32_t>(src1) < static_cast<std::uint32_t>(src2) ? 1 : 0;
            m_cycleCounter += 8;
            break;

        case OpFunct10::XOR:
            value = src1 ^ src2;
            m_cycleCounter += 8;
            break;

        case OpFunct10::SRL:
            value = static_cast<std::uint32_t>(src1) >> static_cast<std::uint32_t>(src2);
            m_cycleCounter += 16;
            if (src2 != 1)
            {
                m_cycleCounter += 8;
            }
            break;

        case OpFunct10::SRA:
            value = src1 >> static_cast<std::uint32_t>(src2);
            m_cycleCounter += 16;
            if (src2 != 1)
            {
                m_cycleCounter += 8;
            }
            break;

        case OpFunct10::OR:
            value = src1 | src2;
            m_cycleCounter += 8;
            break;

        case OpFunct10::AND:
            value = src1 & src2;
            m_cycleCounter += 8;
            break;

#ifdef TT_ZBB
        case OpFunct10::XNOR:
            value = ~(src1 ^ src2);
            m_cycleCounter += 8;
            break;

        case OpFunct10::ORN:
            value = src1 | ~src2;
            m_cycleCounter += 8;
            break;

        case OpFunct10::ANDN:
            value = src1 & ~src2;
            m_cycleCounter += 8;
            break;

        case OpFunct10::MIN:
            value = (src1 < src2) ? src1 : src2;
            m_cycleCounter += 16;
            break;

        case OpFunct10::MINU:
            value = (static_cast<std::uint32_t>(src1) < static_cast<std::uint32_t>(src2)) ? src1 : src2;
            m_cycleCounter += 16;
            break;

        case OpFunct10::MAX:
            value = (src1 < src2) ? src2 : src1;
            m_cycleCounter += 16;
            break;

        case OpFunct10::MAXU:
            value = (static_cast<std::uint32_t>(src1) < static_cast<std::uint32_t>(src2)) ? src2 : src1;
            m_cycleCounter += 16;
            break;

        case OpFunct10::ZEXT_H:
            if (rs2 != 0)
            {
                throw BadInstructionException(inst);
            }
            value = src1 & 0x0000FFFF;
            m_cycleCounter += 16;
            break;

#ifndef TT_ZBB_SUBSET
        case OpFunct10::ROL:
            value = (src1 << (src2 & 0x1F)) | (static_cast<std::uint32_t>(src1) >> (32 - (src2 & 0x1F)));
            m_cycleCounter += 16;
            break;

        case OpFunct10::ROR:
            value = (static_cast<std::uint32_t>(src1) >> (src2 & 0x1F)) | (src1 << (32 - (src2 & 0x1F)));
            m_cycleCounter += 16;
            break;
#endif // TT_ZBB_SUBSET
#endif // TT_ZBB

        default:
            throw BadInstructionException(inst);
    }

    setReg(getRd(inst), value);

    m_pc += 4;
}

void Simulator::execMiscMemInst(std::uint32_t)
{
    // TODO
    m_pc += 4;
}

void Simulator::execSystemInst(std::uint32_t inst)
{
    enum class SystemFunct3 : std::uint8_t
    {
        PRIV = 0,
        CSRRW = 1,
        CSRRS = 2,
        CSRRC = 3,
        CSRRWI = 5,
        CSRRSI = 6,
        CSRRCI = 7
    };

    auto funct3 = static_cast<SystemFunct3>(getFunct3(inst));
    auto csrOrFunc = getFunct12(inst);
    auto rs1OrImm = getRs1(inst);
    auto rd = getRd(inst);

    switch (funct3)
    {
        case SystemFunct3::PRIV:
            if (rs1OrImm == 0 && rd == 0 && csrOrFunc == 0x302)
            {
                // MRET
                m_mstatus = s_mstatusMPP | ((m_mstatus & s_mstatusMPIE) == 0 ? 0 : s_mstatusMIE);
                m_pc = m_mepc;
                m_cycleCounter += 16; // ?
                return;
            }
            else
            {
                throw BadInstructionException(inst);
            }
            break;

        case SystemFunct3::CSRRW:   // CSR Read and Write (register)
            if (rd != 0)
            {
                setReg(rd, getCsr(csrOrFunc));
            }
            setCsr(csrOrFunc, getReg(rs1OrImm));
            break;

        case SystemFunct3::CSRRS:   // CSR Read and Set (register)
        {
            auto val = getCsr(csrOrFunc);
            setReg(rd, val);
            setCsr(csrOrFunc, val | getReg(rs1OrImm));
            break;
        }

        case SystemFunct3::CSRRC:   // CSR Read and Clear (register)
        {
            auto val = getCsr(csrOrFunc);
            setReg(rd, val);
            setCsr(csrOrFunc, val & ~getReg(rs1OrImm));
            break;
        }

        case SystemFunct3::CSRRWI:  // CSR Read and Write (immediate)
        {
            if (rd != 0)
            {
                setReg(rd, getCsr(csrOrFunc));
            }
            setCsr(csrOrFunc, rs1OrImm);
            break;
        }

        case SystemFunct3::CSRRSI:  // CSR Read and Set (immediate)
        {
            auto val = getCsr(csrOrFunc);
            setReg(rd, val);
            setCsr(csrOrFunc, val | rs1OrImm);
            break;
        }

        case SystemFunct3::CSRRCI:  // CSR Read and Clear (immediate)
        {
            auto val = getCsr(csrOrFunc);
            setReg(rd, val);
            setCsr(csrOrFunc, val & ~rs1OrImm);
            break;
        }

        default:
            throw BadInstructionException(inst);
    }
            
    m_pc += 4;
    m_cycleCounter += 8;
}

#ifdef TT_XMUL16

void Simulator::execCustom0Inst(std::uint32_t inst)
{
    // fffffff rrrrr rrrrr fff rrrrr ooooooo
    auto funct3 = getFunct3(inst);
    auto rs1 = getRs1(inst);
    auto rs2 = getRs2(inst);
    auto rd = getRd(inst);
    auto src1 = getReg(rs1) & 0x0000FFFF;
    auto src2 = getReg(rs2) & 0x0000FFFF;

    std::int32_t dst = 0;
    switch (funct3)
    {
        case 0: // MUL16
            dst = static_cast<std::int32_t>(static_cast<std::int16_t>(src1)) * static_cast<std::int32_t>(static_cast<std::int16_t>(src2));
            break;

        case 1: // MUL16SU
            dst = static_cast<std::int32_t>(static_cast<std::int16_t>(src1)) * src2;
            break;

        case 2: // MUL16U
            dst = src1 * src2;
            break;
    }

    setReg(rd, dst);

    m_pc += 4;
    m_cycleCounter += 24;
}

#endif // TT_XMUL16

std::uint8_t Simulator::load8(std::uint32_t address) const
{
    auto value = load32(address & ~3);
    auto mod = address & 3;

    return static_cast<std::uint8_t>(value >> (mod * 8));
}

std::uint16_t Simulator::load16(std::uint32_t address) const
{
    auto value = load32(address & ~1);
    auto mod = address & 1;

    return static_cast<std::uint16_t>(value >> (mod * 16));
}

std::uint32_t Simulator::load32(std::uint32_t address) const
{
    if ((address & 0x02000000) == 0)
    {
        if ((address & 0x01000000) == 0)
        {
            auto flash = m_executable.data();
            if (address + 3 > flash.size())
            {
                throw std::runtime_error(std::format("Address out of range: 0x{:X}", address));
            }
            return
                static_cast<std::uint32_t>(flash[address % flash.size()]) |
                (static_cast<std::uint32_t>(flash[(address + 1) % flash.size()]) << 8) |
                (static_cast<std::uint32_t>(flash[(address + 2) % flash.size()]) << 16) |
                (static_cast<std::uint32_t>(flash[(address + 3) % flash.size()]) << 24);
        }
        else
        {
            if ((address - 0x1000000) + 3 > m_ram.size())
            {
                throw std::runtime_error(std::format("Address out of range: 0x{:X}", address));
            }
            return
                static_cast<std::uint32_t>(m_ram.at(address % m_ram.size())) |
                (static_cast<std::uint32_t>(m_ram.at((address + 1) % m_ram.size())) << 8) |
                (static_cast<std::uint32_t>(m_ram.at((address + 2) % m_ram.size())) << 16) |
                (static_cast<std::uint32_t>(m_ram.at((address + 3) % m_ram.size())) << 24);
        }
    }
    else if (((address >> 2) & 0x3) == 1)
    {
        return m_interface.input();
    }
    else
    {
        return 0;
    }
}

std::uint32_t Simulator::getCsr(std::uint16_t csr)
{
    switch (static_cast<CsrReg>(csr))
    {
        case CsrReg::MSTATUS:
            return m_mstatus;

        case CsrReg::MIE:
            return m_mie;

        case CsrReg::MTVEC:
            return 4;

        case CsrReg::MSCRATCH:
            return 0;

        case CsrReg::MEPC:
            return m_mepc;

        case CsrReg::MCAUSE:
            return m_mcause;

        case CsrReg::MTVAL:
            return 0;
    }

    return 0;
}

void Simulator::setCsr(std::uint16_t csr, std::uint32_t value)
{
    switch (static_cast<CsrReg>(csr))
    {
        case CsrReg::MSTATUS:
            m_mstatus = (m_mstatus & ~s_mstatusWriteMask) | (value & s_mstatusWriteMask);
            return;

        case CsrReg::MIE:
            m_mie = (m_mie & ~s_mieWriteMask) | (value & s_mieWriteMask);
            return;

        case CsrReg::MTVEC:
            return;

        case CsrReg::MSCRATCH:
            return;

        case CsrReg::MEPC:
            m_mepc = value;
            break;

        case CsrReg::MCAUSE:
            m_mcause = (m_mcause & ~s_mcauseWriteMask) | (value & s_mcauseWriteMask);
            break;

        case CsrReg::MTVAL:
            return;
    }

    return;
}

void Simulator::store8(std::uint32_t address, std::uint8_t value)
{
    if ((address & 0x2000000) == 0)
    {
        if ((address & 0x1000000) != 0)
        {
            m_ram.at(address % m_ram.size()) = value;
        }
    }
    else
    {
        auto mod = address & 3;
        store32(address & ~3, static_cast<std::uint32_t>(value) << (mod * 8));
    }
}

void Simulator::store16(std::uint32_t address, std::uint16_t value)
{
    if ((address & 0x2000000) == 0)
    {
        if ((address & 0x1000000) != 0)
        {
            m_ram.at(address % m_ram.size()) = static_cast<std::uint8_t>(value);
            m_ram.at((address + 1) % m_ram.size()) = static_cast<std::uint8_t>(value >> 8);
        }
    }
    else
    {
        auto mod = address & 3;
        store32(address & ~3, static_cast<std::uint32_t>(value) << (mod * 8));
    }
}

void Simulator::store32(std::uint32_t address, std::uint32_t value)
{
    if ((address & 0x2000000) == 0)
    {
        if ((address & 0x1000000) != 0)
        {
            m_ram.at(address % m_ram.size()) = static_cast<std::uint8_t>(value);
            m_ram.at((address + 1) % m_ram.size()) = static_cast<std::uint8_t>(value >> 8);
            m_ram.at((address + 2) % m_ram.size()) = static_cast<std::uint8_t>(value >> 16);
            m_ram.at((address + 3) % m_ram.size()) = static_cast<std::uint8_t>(value >> 24);
        }
    }
    else
    {
        switch ((address >> 2) & 0x3)
        {
            case 0: // Timer
                m_interruptCycle = m_cycleCounter + (value & 0x7FFFF);
                break;

            case 1: // Input
                break;

            case 2: // Audio
                m_audioInterface.playFrequency(65000000.0 / static_cast<float>(value & 0x0003FFFC));
                break;

            case 3: // Video
            {
                bool enabled = (value & 0x80000000) != 0;
                auto address = value & 0x1FFFE000;
                m_interface.setVideoConfig(enabled, address);
                break;
            }
        }
    }
}

} // namespace imp_sim
