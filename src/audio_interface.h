#pragma once

namespace imp_sim
{

class AudioInterface
{
public:
    virtual ~AudioInterface() = default;

    virtual void playFrequency(float frequency) = 0;
};

} // namespace imp_sim
