#include "none_interface.h"

namespace imp_sim
{

void NoneInterface::start(Simulator&)
{
}

void NoneInterface::stop()
{
}

void NoneInterface::setVideoConfig(bool, std::uint32_t)
{
}

std::uint32_t NoneInterface::input()
{
    return 0;
}

} // namespace imp_sim
