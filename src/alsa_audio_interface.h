#pragma once

#include "audio_interface.h"

namespace imp_sim
{

class AlsaAudioInterface : public AudioInterface
{
public:
    AlsaAudioInterface();
    void playFrequency(float frequency) override;
};

} // namespace imp_sim
