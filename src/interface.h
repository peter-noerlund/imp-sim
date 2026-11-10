#pragma once

#include <cstdint>

namespace imp_sim
{

class Simulator;

class Interface
{
public:
    virtual ~Interface() = default;

    virtual void start(Simulator& simulator) = 0;
    virtual void stop() = 0;
    virtual void setVideoConfig(bool enabled, std::uint32_t address) = 0;
    virtual std::uint32_t input() = 0;
};

} // namespace imp_sim
