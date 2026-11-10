#include "bad_register_exception.h"

#include <format>

namespace imp_sim
{

BadRegisterException::BadRegisterException(std::uint8_t reg)
    : SimulationException(std::format("Bad register: 0x{:02X}", reg))
{
}

} // namespace imp_sim
