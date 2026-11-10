#include "simulator.h"

#include "bad_instruction_exception.h"
#include "executable.h"
#include "interface.h"
#include "simulation_exception.h"

#include <algorithm>
#include <cstdio>
#include <print>
#include <functional>

namespace imp_sim
{

Simulator::Simulator(Executable& executable, Interface& interface)
    : m_executable(executable)
    , m_interface(interface)
    , m_ram(8 * 1024 * 1024)
{
}

void Simulator::run(bool)
{
    m_pc = 0;
    while (true)
    {
        if (!m_interrupted && m_interruptCycle != 0 && m_cycleCounter > m_interruptCycle)
        {
            m_interruptCycle = 0;
            m_meip = m_pc;
            m_pc = 4;
            m_interrupted = true;
        }
        executeInstruction();
    }
}

void Simulator::executeInstruction()
{
    enum Opcode : std::uint8_t
    {
        LOAD = 0x03,
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

    std::uint32_t inst = load32(m_pc);

    m_cycleCounter += 56;

    try
    {
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

            default:
                throw BadInstructionException(inst);
        }
    }
    catch (const SimulationException& exception)
    {
        std::println("Caught exception: {}", exception.what());
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
    m_pc = getImmJ(inst);
    m_cycleCounter += 16;
}

void Simulator::execJalrInst(std::uint32_t inst)
{
    if (getFunct3(inst) != 0)
    {
        throw BadInstructionException(inst);
    }

    setReg(getRd(inst), m_pc + 4);
    m_pc = static_cast<std::uint32_t>(getReg(getRs1(inst)) + getImmI(inst));
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

    auto src1 = getReg(getRs1(inst));
    auto src2 = getReg(getRs2(inst));
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

    if ((address & 0x02000000) == 0)
    {
        // RAM/Flash access
        m_cycleCounter += 8 + 56 + 8;
    }
    else
    {
        // Device access
        m_cycleCounter += 8 + 8 + 8;
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
            m_cycleCounter += 32;
            break;

        case 1:
            store16(address, static_cast<std::uint16_t>(src));
            m_cycleCounter += 40;
            break;

        case 2:
            store32(address, static_cast<std::uint32_t>(src));
            m_cycleCounter += 48;
            break;

        default:
            throw BadInstructionException(inst);
    }

    m_pc += 4;
}

void Simulator::execOpImmInst(std::uint32_t inst)
{
    enum class OpImmFunct3 : std::uint32_t
    {
        ADDI = 0,
        SLLI = 1,
        SLTI = 2,
        SLTIU = 3,
        XORI = 4,
        SRxI = 5,
        ORI = 6,
        ANDI = 7,
    };

    auto src = getReg(getRs1(inst));
    auto imm = getImmI(inst);
    auto funct3 = static_cast<OpImmFunct3>(getFunct3(inst));
    std::int32_t value;

    switch (funct3)
    {
        case OpImmFunct3::ADDI:
            value = src + imm;
            m_cycleCounter += 8;
            break;

        case OpImmFunct3::SLLI:
            if (imm < 0 || imm > 31)
            {
                throw BadInstructionException(inst);
            }
            value = src << imm;
            m_cycleCounter += 16;
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
            value = src | imm;
            m_cycleCounter += 8;
            break;

        case OpImmFunct3::SRxI: // SRLI and SRAI
            switch (imm & 0xFE0)
            {
                case 0x000: // SRLI
                    value = static_cast<std::uint32_t>(src) >> imm;
                    break;

                case 0x400: // SRAI
                    value = src >> (imm & 0x1F);
                    break;

                default:
                    throw BadInstructionException(inst);
            }
            m_cycleCounter += 16;
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
    enum class OpFunct3 : std::uint8_t
    {
        ADD_SUB = 0,
        SLL = 1,
        SLT = 2,
        SLTU = 3,
        XOR = 4,
        SRL_SRAL = 5,
        OR = 6,
        AND = 7
    };

    auto src1 = getReg(getRs1(inst));
    auto src2 = getReg(getRs2(inst));
    auto funct3 = static_cast<OpFunct3>(getFunct3(inst));
    auto funct7 = getFunct7(inst);
    std::int32_t value;

    if (funct3 != OpFunct3::ADD_SUB && funct3 != OpFunct3::SRL_SRAL && funct7 != 0)
    {
        throw BadInstructionException(inst);
    }

    switch (funct3)
    {
        case OpFunct3::ADD_SUB:
            if (funct7 == 0)
            {
                value = src1 + src2;
            }
            else if (funct7 == 0x20)
            {
                value = src1 - src2;
            }
            else
            {
                throw BadInstructionException(inst);
            }
            m_cycleCounter += 8;
            break;

        case OpFunct3::SLL:
            value = src1 << src2;
            m_cycleCounter += 16;
            break;

        case OpFunct3::SLT:
            value = src1 < src2 ? 1 : 0;
            m_cycleCounter += 8;
            break;

        case OpFunct3::SLTU:
            value = static_cast<std::uint32_t>(src1) < static_cast<std::uint32_t>(src2) ? 1 : 0;
            m_cycleCounter += 8;
            break;

        case OpFunct3::XOR:
            value = src1 ^ src2;
            m_cycleCounter += 8;
            break;

        case OpFunct3::SRL_SRAL:
            if (funct7 == 0)
            {
                value = static_cast<std::uint32_t>(src1) >> static_cast<std::uint32_t>(src2);
            }
            else if (funct7 == 0x20)
            {
                value = src1 >> static_cast<std::uint32_t>(src2);
            }
            else
            {
                throw BadInstructionException(inst);
            }
            m_cycleCounter += 16;
            break;

        case OpFunct3::OR:
            value = src1 | src2;
            m_cycleCounter += 8;
            break;

        case OpFunct3::AND:
            value = src1 & src2;
            m_cycleCounter += 8;
            break;

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
    auto rs1 = getRd(inst);
    auto rd = getRd(inst);

    switch (funct3)
    {
        case SystemFunct3::PRIV:
            if (rs1 == 0 && rd == 0 && csrOrFunc == 0x18)
            {
                // MRET
                m_interrupted = false;
                m_pc = m_meip;
                m_cycleCounter += 16; // ?
            }
            else
            {
                throw BadInstructionException(inst);
            }
            break;

        case SystemFunct3::CSRRW:
        case SystemFunct3::CSRRS:
        case SystemFunct3::CSRRC:
        case SystemFunct3::CSRRWI:
        case SystemFunct3::CSRRSI:
        case SystemFunct3::CSRRCI:
            setReg(rd, csrOrFunc == 0x305 ? 4 : 0);
            m_pc += 4;
            m_cycleCounter += 8;
            break;

        default:
            throw BadInstructionException(inst);
    }
}

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
            return
                static_cast<std::uint32_t>(flash[address % flash.size()]) |
                (static_cast<std::uint32_t>(flash[(address + 1) % flash.size()]) << 8) |
                (static_cast<std::uint32_t>(flash[(address + 2) % flash.size()]) << 16) |
                (static_cast<std::uint32_t>(flash[(address + 3) % flash.size()]) << 24);
        }
        else
        {
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
                // TODO
                break;

            case 3: // Video
            {
                bool enabled = (value & 0x80000000) != 0;
                auto address = value & 0x01FFC0000;
                m_interface.setVideoConfig(enabled, address);
                break;
            }
        }
    }
}


} // namespace imp_sim
