#include "simulation_exception.h"

namespace imp_sim
{

SimulationException::SimulationException(const std::string& message)
    : runtime_error(message)
{
}

SimulationException::SimulationException(const char* message)
    : runtime_error(message)
{
}

} // namespace imp_sim
