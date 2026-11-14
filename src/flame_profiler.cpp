#include "flame_profiler.h"

#include "executable.h"

namespace imp_sim
{

FlameProfiler::FlameProfiler(const Executable& executable, const std::filesystem::path& filename)
    : m_executable(executable)
{
    m_stream.exceptions(std::ofstream::badbit | std::ofstream::failbit);
    m_stream.open(filename, std::ofstream::out | std::ofstream::trunc);

    m_addresStack.push_back(0);
    m_symbolStack.push_back(m_executable.resolve(0x2000000));
}

void FlameProfiler::postInstruction(std::uint32_t pc, std::uint32_t instr, unsigned long long int cycles, bool interrupt)
{
    if (interrupt)
    {
        m_interrupted = true;
        return;
    }
    else if (instr == 0x30200073) // MRET
    {
        m_interrupted = false;
        return;
    }
    else if (m_interrupted)
    {
        return;
    }

    if (m_call || (!m_addresStack.empty() && m_addresStack.back() == pc))
    {
        if (!m_symbolStack.empty())
        {
            for (std::size_t i = 0; i != m_symbolStack.size(); ++i)
            {
                if (i > 0)
                {
                    m_stream << ';';
                }
                m_stream << m_symbolStack[i];
            }
            m_stream << ' ' << (cycles - m_stackCycles) << std::endl;
        }

        if (!m_addresStack.empty() && m_addresStack.back() == pc)
        {
            m_addresStack.pop_back();
            m_symbolStack.pop_back();
        }
        else
        {
            m_symbolStack.push_back(m_executable.resolve(pc));
            m_addresStack.push_back(m_prevPc + 4);
        }
        m_stackCycles = cycles;
    }

    m_call = false;
    if ((instr & 0x7F) == 0x6F || (instr & 0x707F) == 0x67) // jal or jalr
    {
        auto rd = (instr >> 7) & 0x1F;
        if (rd != 0)
        {
            m_call = true;
        }
    }

    m_prevPc = pc;
}

} // namespace imp_sim
