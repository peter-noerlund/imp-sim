#pragma once

#include <alsa/asoundlib.h>

#include <memory>

namespace imp_sim
{

struct SndPcmFreer
{
    inline void operator()(snd_pcm_t* handle) const noexcept
    {
        snd_pcm_close(handle);
    }
};

using UniqueSndPcm = std::unique_ptr<snd_pcm_t, SndPcmFreer>;

} // namespace imp_sim
