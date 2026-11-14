#pragma once

#include "instruction_hook.h"

#include <cstdint>
#include <fstream>
#include <filesystem>
#include <vector>

namespace imp_sim
{

class Executable;

class FlameProfiler : public InstructionHook
{
public:
    FlameProfiler(const Executable& executable, const std::filesystem::path& filename);

    void postInstruction(std::uint32_t pc, std::uint32_t instr, unsigned long long int cycles, bool interrupt) override;

private:
    const Executable& m_executable;
    std::ofstream m_stream;
    std::vector<std::string_view> m_symbolStack;
    std::vector<std::uint32_t> m_addresStack;
    unsigned long long int m_stackCycles = 0;
    std::uint32_t m_prevPc = 0;
    bool m_call = false;
    bool m_interrupted = false;
};

} // namespace imp_sim
