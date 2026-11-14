#pragma once

#include "instruction_hook.h"

#include <cstddef>
#include <filesystem>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace imp_sim
{

class Executable;

class CallgrindProfiler : public InstructionHook
{
public:
    CallgrindProfiler(const Executable& executable, const std::filesystem::path& filename);
    ~CallgrindProfiler();

    void postInstruction(std::uint32_t pc, std::uint32_t instr, unsigned long long int cycles, bool interrupt) override;

private:
    struct FunctionCall
    {
        unsigned int index = 0;
        unsigned long long int count = 1;
        unsigned long long int instructionCount = 0;
        unsigned long long int cycleCount = 0;

        constexpr FunctionCall() noexcept = default;

        constexpr FunctionCall(unsigned int index, unsigned long long int instructionCount, unsigned long long int cycleCount) noexcept
            : index(index)
            , instructionCount(instructionCount)
            , cycleCount(cycleCount)
        {
        }
    };

    struct Function
    {
        std::string name;
        unsigned int index = 0;
        unsigned long long int instructionCount = 0;
        unsigned long long int cycleCount = 0;
        std::vector<FunctionCall> functionCalls;

        inline Function(std::string_view name, unsigned int index) noexcept
            : name(name)
            , index(index)
        {
        }
    };

    struct Context
    {
        std::vector<FunctionCall> callStack;
        std::uint32_t prevInstr = 0;
    };

    void dump();

    std::size_t getFunctionIndex(std::string_view symbol);

    static constexpr unsigned int s_invalidFunctionIndex = std::numeric_limits<unsigned int>::max();
    void unwindStack(Context& context, unsigned int index);

 

    const Executable& m_executable;
    std::filesystem::path m_filename;
   
    std::vector<Function> m_functions;
    std::vector<unsigned int> m_addressToFunctionIndex;

    Context m_context;
    Context m_interruptContext;

    unsigned long long int m_currentCycles = 0;
    bool m_interrupted = false;
};

} // namespace imp_sim
