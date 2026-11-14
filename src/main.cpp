#include "executable.h"
#include "audio_interface.h"
#include "flame_profiler.h"
#include "function_profiler.h"
#include "instruction_hook.h"
#include "interface.h"
#include "none_audio_interface.h"
#include "none_interface.h"
#include "simulator.h"
#include "tty_interface.h"

#ifdef HAVE_ALSA
#include "alsa_audio_interface.h"
#endif // HAVE_ALSA

#ifdef HAVE_X11
#include "x11_interface.h"
#endif // HAVE_X11

#include <cstdio>
#include <cstdlib>
#include <print>
#include <memory>
#include <string_view>

int main(int argc, char** argv)
{
    using namespace imp_sim;

    std::string_view interfaceName;
#ifdef HAVE_X11
    interfaceName = "x11";
#else // HAVE_X11
    interfaceName = "none";
#endif // HAVE_X11

    std::string_view audioInterfaceName;
#ifdef HAVE_ALSA
    audioInterfaceName = "alsa";
#else // HAVE_ALSA
    audioInterfaceName = "none";
#endif // HAVE_ALSA

    std::string_view functionProfileFile;
    std::string_view flameProfileFile;

    std::string_view executableFile;
    bool fast = false;
    bool help = false;

    struct StringArgument
    {
        std::string_view arg;
        std::variant<std::string_view*, bool*> value;
    };
    const auto arguments = std::to_array<StringArgument>({
        {"--interface", &interfaceName},
        {"--audio-interface", &audioInterfaceName},
        {"--function-profile", &functionProfileFile},
        {"--flame-profile", &flameProfileFile},
        {"--fast", &fast},
        {"-h", &help},
        {"--help", &help}
    });


    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg = argv[i];

        if (arg.starts_with("-"))
        {
            bool match = false;
            for (const auto& argument : arguments)
            {
                if (std::holds_alternative<bool*>(argument.value))
                {
                    if (arg == argument.arg)
                    {
                        *std::get<bool*>(argument.value) = true;
                        match = true;
                        break;
                    }
                }
                else if (std::holds_alternative<std::string_view*>(argument.value))
                {
                    auto ptr = std::get<std::string_view*>(argument.value);
                    std::string_view value;
                    if (arg == argument.arg)
                    {
                        if (i + 1 == argc)
                        {
                            throw std::invalid_argument(std::format("Missing value for argument: {}", arg));
                        }
                        *ptr = argv[++i];
                        match = true;
                        break;
                    }
                    else if (arg.starts_with(argument.arg) && arg.size() > argument.arg.size() && arg[argument.arg.size()] == '=')
                    {
                        *ptr = arg.substr(argument.arg.size() + 1);
                        match = true;
                        break;
                    }
                }
            }

            if (!match)
            {
                throw std::invalid_argument(std::format("Invalid argument: {}", arg));
            }
        }
        else
        {
            executableFile = arg;
        }
    }

    if (help)
    {
        std::string interfaceTypes = "none|tty";
#ifdef HAVE_X11
        interfaceTypes += "|x11";
#endif // HAVE_X11
        
        std::string audioInterfaceTypes = "none";
#ifdef HAVE_ALSA
        audioInterfaceTypes += "|alsa";
#endif // HAVE_ALSA

        std::println("Usage: imp-sim [--interface={}] [--audio-interface={}] [--fast] [--function-profile=FILE] EXECUTABLE", interfaceTypes, audioInterfaceTypes);

        return EXIT_SUCCESS;
    }

    if (executableFile.empty())
    {
        std::println(stderr, "Missing argument");
        return EXIT_FAILURE;
    }

    try
    {
        std::unique_ptr<Interface> interface;
        if (interfaceName == "none")
        {
            interface = std::make_unique<NoneInterface>();
        }
        else if (interfaceName == "tty")
        {
            interface = std::make_unique<TtyInterface>();
        }
#ifdef HAVE_X11
        else if (interfaceName == "x11")
        {
            interface = std::make_unique<X11Interface>();
        }
#endif // HAVE_X11
        else
        {
            throw std::invalid_argument(std::format("Unsupported interface: {}", interfaceName));
        }

        std::unique_ptr<AudioInterface> audioInterface;
        if (audioInterfaceName == "none")
        {
            audioInterface = std::make_unique<NoneAudioInterface>();
        }
#ifdef HAVE_ALSA
        else if (audioInterfaceName == "alsa")
        {
            audioInterface = std::make_unique<AlsaAudioInterface>();
        }
#endif // HAVE_ALSA
        else
        {
            throw std::invalid_argument(std::format("Unsupported audio interface: {}", audioInterfaceName));
        }


        Executable executable(executableFile);

        Simulator simulator(executable, *interface, *audioInterface);

        std::vector<std::unique_ptr<InstructionHook>> hooks;
        if (!functionProfileFile.empty())
        {
            hooks.push_back(std::make_unique<FunctionProfiler>(executable, functionProfileFile));
        }
        if (!flameProfileFile.empty())
        {
            hooks.push_back(std::make_unique<FlameProfiler>(executable, flameProfileFile));
        }

        for (auto& hook : hooks)
        {
            simulator.addHook(*hook);
        }

        simulator.run(!fast);

        return EXIT_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        std::println(stderr, "Caught exception: {}", exception.what());
        return EXIT_FAILURE;
    }
}
