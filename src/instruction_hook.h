#pragma once

#include <cstdint>

namespace imp_sim
{

class InstructionHook
{
public:
    virtual ~InstructionHook() = default;

    virtual void postInstruction(std::uint32_t pc, std::uint32_t instr, unsigned long long int cycles) = 0;
};

} // namespace imp_sim
