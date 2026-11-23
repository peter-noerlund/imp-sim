cmake_minimum_required(VERSION 3.14)

list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_SOURCE_DIR})

project(imp-sim LANGUAGES CXX)

option(TT_CACHE "Simulate 32-bit cache" OFF)
option(TT_QUAD "Run PSRAM in Quad mode" OFF)
option(TT_CONT_READ "Run Flash in continous read mode" OFF)
option(TT_ZMMUL "Enable Zmmul extension with 32-cycles" OFF)
option(TT_M "Enable M extension with 32-cycles" OFF)
option(TT_ZBA "Enable Zba extension" OFF)

find_package(ALSA)
find_package(stb CONFIG REQUIRED)
find_package(X11)

add_executable(imp-sim
    src/bad_instruction_exception.cpp
    src/bad_register_exception.cpp
    src/callgrind_profiler.cpp
    src/executable.cpp
    src/flame_profiler.cpp
    src/function_profiler.cpp
    src/main.cpp
    src/none_audio_interface.cpp
    src/none_interface.cpp
    src/simulation_exception.cpp
    src/simulator.cpp
    src/tty_interface.cpp
    src/video_decoder.cpp
)
target_compile_features(imp-sim PRIVATE cxx_std_23)
target_include_directories(imp-sim PRIVATE src)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(imp-sim PRIVATE -Wall -W -Wextra -Wpedantic)
endif()
if(TT_CACHE)
    target_compile_definitions(imp-sim PRIVATE TT_CACHE)
endif()
if(TT_QUAD)
    target_compile_definitions(imp-sim PRIVATE TT_QUAD)
endif()
if(TT_CONT_READ)
    target_compile_definitions(imp-sim PRIVATE TT_CONT_READ)
endif()
if(TT_ZMMUL)
    target_compile_definitions(imp-sim PRIVATE TT_ZMMUL)
endif()
if(TT_M)
    target_compile_definitions(imp-sim PRIVATE TT_M)
endif()
if(TT_ZBA)
    target_compile_definitions(imp-sim PRIVATE TT_ZBA)
endif()

if(X11_FOUND)
    target_sources(imp-sim PRIVATE src/x11_interface.cpp)
    target_compile_definitions(imp-sim PRIVATE HAVE_X11)
    target_include_directories(imp-sim PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(imp-sim PRIVATE ${X11_LIBRARIES})
endif()

if(ALSA_FOUND)
    target_sources(imp-sim PRIVATE src/alsa_audio_interface.cpp)
    target_compile_definitions(imp-sim PRIVATE HAVE_ALSA)
    target_link_libraries(imp-sim PRIVATE ALSA::ALSA)
endif()

add_executable(encode-image tools/encode_image.cpp)
target_compile_features(encode-image PRIVATE cxx_std_23)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(encode-image PRIVATE -Wall -W -Wextra -Wpedantic)
endif()
target_compile_definitions(encode-image PRIVATE STB_IMAGE_IMPLEMENTATION)
target_link_libraries(encode-image PRIVATE stb::stb)

add_executable(encode-font tools/encode_font.cpp)
target_compile_features(encode-font PRIVATE cxx_std_23)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(encode-font PRIVATE -Wall -W -Wextra -Wpedantic)
endif()

