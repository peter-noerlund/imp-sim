#pragma once

#include "audio_interface.h"

namespace imp_sim
{

class NoneAudioInterface : public AudioInterface
{
public:
    NoneAudioInterface() = default;

    void playFrequency(float frequency) override;
};

} // namespace imp_sim
