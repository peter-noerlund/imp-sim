#pragma once

#include <string>

#include <stdexcept>

namespace imp_sim
{

class SimulationException : public std::runtime_error
{
public:
    explicit SimulationException(const std::string& message);
    explicit SimulationException(const char* message);
    SimulationException(const SimulationException& other) = default;
};

} // namespace imp_sim
