if(NOT stb_FOUND)
    set(stb_FOUND FALSE)

    include(FetchContent)

    FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG        master
    )
    FetchContent_MakeAvailable(stb)

    add_library(stb::stb INTERFACE IMPORTED)
    target_include_directories(stb::stb INTERFACE ${stb_SOURCE_DIR})

    set(stb_FOUND TRUE)
endif()
