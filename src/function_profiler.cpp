#include "function_profiler.h"

#include "executable.h"

#include <iterator>
#include <format>

namespace imp_sim
{

FunctionProfiler::FunctionProfiler(const Executable& executable, const std::filesystem::path& filename)
    : m_executable(executable)
{
    m_stream.exceptions(std::ofstream::badbit | std::ofstream::failbit);
    m_stream.open(filename, std::ofstream::out | std::ofstream::trunc);
}

void FunctionProfiler::postInstruction(std::uint32_t pc, std::uint32_t, unsigned long long int cycles, bool)
{
    auto symbol = m_executable.resolve(pc);
    if (symbol != m_currentSymbol)
    {
        if (!m_currentSymbol.empty())
        {
            std::format_to(std::ostream_iterator<char>(m_stream), "{} {}\n", symbol, (cycles - m_currentCycles));
        }
        m_currentCycles = cycles;
        m_currentSymbol = std::move(symbol);
    }
}

} // namespace imp_sim
