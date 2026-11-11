#pragma once

#include "audio_interface.h"
#include "unique_snd_pcm.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace imp_sim
{

class AlsaAudioInterface : public AudioInterface
{
public:
    AlsaAudioInterface();
    ~AlsaAudioInterface();
    void playFrequency(float frequency) override;

private:
    void playThread();

    UniqueSndPcm m_handle;
    bool m_started = false;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    bool m_stop = false;
    float m_frequency = 0.0;
    std::thread m_thread;
};

} // namespace imp_sim
