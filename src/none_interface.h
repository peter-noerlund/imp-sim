#pragma once

#include "interface.h"

namespace imp_sim
{

class NoneInterface : public Interface
{
public:
    NoneInterface() = default;

    void start(Simulator& simulator) override;
    void stop() override;
    void setVideoConfig(bool enabled, std::uint32_t address) override;
    std::uint32_t input() override;
};

} // namespace imp_sim
