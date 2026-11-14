#include "callgrind_profiler.h"

#include "executable.h"

#include <format>
#include <fstream>
#include <iterator>

namespace imp_sim
{

CallgrindProfiler::CallgrindProfiler(const Executable& executable, const std::filesystem::path& filename)
    : m_executable(executable)
    , m_filename(filename)
{
    m_addressToFunctionIndex.reserve(16 * 1024 * 1024 / 4);

    auto symbols = executable.symbols();
    unsigned int functionIndex = s_invalidFunctionIndex;

    auto it = symbols.begin();
    for (std::uint32_t address = 0; address != 16 * 1024 * 1024; address += 4)
    {
        if (it != symbols.end() && address >= it->first)
        {
            functionIndex = static_cast<unsigned int>(m_functions.size());
            m_functions.emplace_back(it->second, functionIndex);

            ++it;
        }

        m_addressToFunctionIndex.push_back(functionIndex);
    }
}

CallgrindProfiler::~CallgrindProfiler()
{
    dump();
}

void CallgrindProfiler::postInstruction(std::uint32_t pc, std::uint32_t instr, unsigned long long int cycles, bool interrupt)
{
    if (interrupt)
    {
        m_interrupted = true;
        m_interruptContext.callStack.clear();
    }

    Context* context = m_interrupted ? &m_interruptContext : &m_context;

    auto elapsedCycles = cycles - m_currentCycles;

    auto index = m_addressToFunctionIndex.at(pc / 4);
    auto& functionInfo = m_functions.at(index);

    functionInfo.instructionCount++;
    functionInfo.cycleCount += elapsedCycles;

    if (context->callStack.empty())
    {
        // First instruction
        context->callStack.emplace_back(index, 1, elapsedCycles);
    }
    else if (context->callStack.back().index == index)
    {
        // Next instruction in same function
        context->callStack.back().instructionCount++;
        context->callStack.back().cycleCount += elapsedCycles;
    }
    else
    {
        // We switched function

        if (context->prevInstr == 0x00008067 || context->prevInstr == 0x00028067) // ret or jr t0
        {
            // Previous instruction was return, so unwind
            unwindStack(*context, index);
            context->callStack.back().instructionCount++;
            context->callStack.back().cycleCount += elapsedCycles;
        }
        else
        {
            // Previous instruction was not call, so we just flowed into function - add to stack anyway
            context->callStack.emplace_back(index, 1, elapsedCycles);
        }
    }

    m_currentCycles = cycles;

    context->prevInstr = instr;

    if (instr == 0x30200073) // MRET
    {
        unwindStack(m_interruptContext, s_invalidFunctionIndex);
        m_interrupted = false;
    }
}

void CallgrindProfiler::unwindStack(Context& context, unsigned int index)
{
    while (!context.callStack.empty() && context.callStack.back().index != index)
    {
        auto callee = context.callStack.back();
        context.callStack.pop_back();
        if (context.callStack.empty())
        {
            break;
        }

        auto& caller = context.callStack.back();
        caller.instructionCount += callee.instructionCount;
        caller.cycleCount += callee.cycleCount;

        auto& function = m_functions.at(caller.index);

        bool found = false;
        for (auto& functionCall : function.functionCalls)
        {
            if (functionCall.index == callee.index)
            {
                functionCall.count++;
                functionCall.instructionCount += callee.instructionCount;
                functionCall.cycleCount += callee.cycleCount;
                found = true;
                break;
            }
        }

        if (!found)
        {
            function.functionCalls.emplace_back(callee.index, callee.instructionCount, callee.cycleCount);
        }
    }
}

void CallgrindProfiler::dump()
{
    unwindStack(m_context, s_invalidFunctionIndex);
    unwindStack(m_interruptContext, s_invalidFunctionIndex);

    std::ofstream file;
    file.open(m_filename, std::ofstream::out | std::ofstream::trunc);
    if (!file.good())
    {
        return;
    }

    auto it = std::ostream_iterator<char>(file);
    std::format_to(it,
            "# callgrind format\n"
            "events: Cycles Instructions\n\n");

    for (const auto& function : m_functions)
    {
        if (function.instructionCount == 0)
        {
            continue;
        }

        std::format_to(it, "fn=({}) {}\n", function.index, function.name);
    }

    std::format_to(it, "\n");

    for (const auto& function : m_functions)
    {
        if (function.instructionCount == 0)
        {
            continue;
        }

        std::format_to(it, "fn=({})\n0 {} {}\n", function.index, function.cycleCount,  function.instructionCount);

        for (const auto& functionCall : function.functionCalls)
        {
            std::format_to(it, "cfn=({})\ncalls={} 0\n0 {} {}\n", functionCall.index, functionCall.count, functionCall.cycleCount, functionCall.instructionCount);
        }

        std::format_to(it, "\n");
    }
}

} // namespace imp_sim
