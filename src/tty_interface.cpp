#include "tty_interface.h"

#include "simulator.h"
#include "video_decoder.h"

#include <cstdio>
#include <print>

namespace imp_sim
{

void TtyInterface::start(Simulator& simulator)
{
    std::unique_lock lock(m_stateMutex);
    while (m_state == State::Stopping)
    {
        m_stateCondition.wait(lock);
    }

    if (m_state == State::Started)
    {
        return;
    }

    if (m_state == State::Stopped)
    {
        m_state = State::Started;
        m_simulator = &simulator;
        m_thread = std::thread([this]()
        {
            interfaceThread();
        });
    }
}

void TtyInterface::stop()
{
    std::unique_lock lock(m_stateMutex);
    while (m_state != State::Stopped)
    {    
        if (m_state == State::Started)
        {
            m_state = State::Stopping;
            m_stateCondition.notify_all();
        }

        while (m_state == State::Stopping)
        {
            m_stateCondition.wait(lock);
            m_thread.join();
        }
    }
    m_simulator = nullptr;
}

void TtyInterface::interfaceThread()
{
    std::unique_lock lock(m_stateMutex);
    while (m_state == State::Started)
    {
        lock.unlock();

        if (m_videoEnabled.load(std::memory_order_acquire))
        {
            auto address = m_videoAddress.load(std::memory_order_acquire);
            render(address);
        }

        lock.lock();
        m_stateCondition.wait_for(lock, std::chrono::milliseconds(17));
    }

    m_state = State::Stopped;
    m_stateCondition.notify_all();
}

void TtyInterface::render(std::uint32_t address)
{
    constexpr auto width = 128;
    constexpr auto height = 96;
    std::print("\033[0;0H");
    for (unsigned int y = 0; y != height; ++y)
    {
        for (unsigned int x = 0; x != width; x += 8)
        {
            auto chunk = m_simulator->load32(address);
            address += 4;

            auto pixels = VideoDecoder::decode(chunk);

            for (auto pixel : pixels)
            {
                auto red = (pixel & 0x30) << 2;
                auto green = (pixel & 0x0C) << 4;
                auto blue = (pixel & 0x03) << 6;

                std::print("\033[38;2;{};{};{}m\342\226\210\342\226\210", red, green, blue);
            }
        }
        std::print("\n");
    }
    std::print("\033[0m");
    std::fflush(stdout);
}

void TtyInterface::setVideoConfig(bool enabled, std::uint32_t address)
{
    if (!enabled)
    {
        m_videoEnabled.store(false, std::memory_order_release);
    }
    m_videoAddress.store(address, std::memory_order_release);
    if (enabled)
    {
        m_videoEnabled.store(true, std::memory_order_release);
    }
}

std::uint32_t TtyInterface::input()
{
    return 0;
}

} // namespace imp_sim
