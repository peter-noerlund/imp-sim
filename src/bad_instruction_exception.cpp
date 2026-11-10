#include "bad_instruction_exception.h"

#include <format>

namespace imp_sim
{

BadInstructionException::BadInstructionException(std::uint32_t instruction)
    : SimulationException(std::format("Bad instruction: 0x{:08X}", instruction))
{
}

} // namespace imp_sim
