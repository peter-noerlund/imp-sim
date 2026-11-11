#include "alsa_audio_interface.h"

#include <alsa/asoundlib.h>

#include <cstdint>
#include <format>
#include <span>
#include <vector>

namespace imp_sim
{

AlsaAudioInterface::AlsaAudioInterface()
{
    snd_pcm_t* handle = nullptr;
    int err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0)
    {
        throw std::runtime_error(std::format("Error opening audio device: {}", snd_strerror(err)));
    }

    m_handle.reset(handle);

    err = snd_pcm_set_params(m_handle.get(), SND_PCM_FORMAT_U8, SND_PCM_ACCESS_RW_INTERLEAVED, 1, 48000, 1, 50000);
    if (err < 0)
    {
        throw std::runtime_error(std::format("Error setting audio parameters: {}", snd_strerror(err)));
    }
}

AlsaAudioInterface::~AlsaAudioInterface()
{
    if (m_started)
    {
        std::unique_lock lock(m_mutex);
        m_stop = true;
        m_condition.notify_all();
        lock.unlock();

        m_thread.join();
    }
}

void AlsaAudioInterface::playFrequency(float frequency)
{
    if (!m_started)
    {
        m_thread = std::thread([this]()
                {
                    playThread();
                });
        m_started = true;
    }

    std::lock_guard lock(m_mutex);
    m_frequency = frequency;
}

void AlsaAudioInterface::playThread()
{
    std::vector<std::uint8_t> audioBuffer(342);

    float frequency = 0.0;
    unsigned int period = 0;
    unsigned int sampleNo = 0;
    bool state = false;
    
    std::unique_lock lock(m_mutex);
    while (!m_stop)
    {
        if (m_frequency != frequency)
        {
            sampleNo = 0;
            period = 24000 / m_frequency;
            frequency = m_frequency;

        }
        lock.unlock();

        if (period != 0)
        {
            for (auto& sample : audioBuffer)
            {
                sample = state ? 127 + 32 : 127 - 32;
                if (++sampleNo >= period)
                {
                    state = !state;
                    sampleNo = 0;
                }
            }

            std::span<const std::uint8_t> samples(audioBuffer);
            while (!samples.empty())
            {
                auto cnt = snd_pcm_writei(m_handle.get(), samples.data(), samples.size());
                if (cnt < 0)
                {
                    throw std::runtime_error(std::format("Error writing audio samples: {}", snd_strerror(cnt)));
                }
                samples = samples.subspan(cnt);
            }
        }

        lock.lock();
        if (period == 0)
        {
            m_condition.wait(lock);
        }
    }
}

} // namespace imp_sim
