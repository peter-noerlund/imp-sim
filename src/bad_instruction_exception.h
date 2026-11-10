#pragma once

#include "simulation_exception.h"

#include <cstdint>

namespace imp_sim
{

class BadInstructionException : public SimulationException
{
public:
    explicit BadInstructionException(std::uint32_t instruction);
};

} // namespace imp_sim

