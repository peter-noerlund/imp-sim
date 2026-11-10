#include "simulation_exception.h"

#include <cstdint>

namespace imp_sim
{

class BadRegisterException : public SimulationException
{
public:
    explicit BadRegisterException(std::uint8_t reg);
};

} // namespace imp_sim
