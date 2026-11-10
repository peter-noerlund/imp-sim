#pragma once

#include "interface.h"

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace imp_sim
{

class Simulator;

class TtyInterface : public Interface
{
public:
    TtyInterface() = default;

    void start(Simulator& simulator) override;
    void stop() override;
    void setVideoConfig(bool enabled, std::uint32_t address) override;
    std::uint32_t input() override;

private:
    void interfaceThread();
    void render(std::uint32_t address);


    enum class State
    {
        Stopped,
        Started,
        Stopping
    };
    Simulator* m_simulator = nullptr;
    std::thread m_thread;
    std::atomic_bool m_videoEnabled = false;
    std::atomic_uint32_t m_videoAddress = 0;
    std::condition_variable m_stateCondition;
    std::mutex m_stateMutex;
    State m_state = State::Stopped;
};

} // namespace imp_sim
