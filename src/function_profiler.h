#pragma once

#include "instruction_hook.h"

#include <cstdint>
#include <fstream>
#include <filesystem>
#include <vector>

namespace imp_sim
{

class Executable;

class FunctionProfiler : public InstructionHook
{
public:
    FunctionProfiler(const Executable& executable, const std::filesystem::path& filename);

    void postInstruction(std::uint32_t pc, std::uint32_t instr, unsigned long long int cycles) override;

private:
    const Executable& m_executable;
    std::ofstream m_stream;
    std::string m_currentSymbol;
    unsigned long long int m_currentCycles = 0;
};

} // namespace imp_sim
