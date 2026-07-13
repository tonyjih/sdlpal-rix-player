cmake_minimum_required(VERSION 3.20)
project(sdlpal_rix_player VERSION 0.1.0 LANGUAGES C CXX)

set(SDLPAL_ROOT "" CACHE PATH "Path to an SDLPAL source checkout")

# Convenient fallbacks:
# 1. repository cloned as sdlpal/tools/rix-player
# 2. SDLPAL_ROOT environment variable
# 3. optional third_party/sdlpal checkout
if(NOT SDLPAL_ROOT)
    get_filename_component(_embedded_candidate "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
    if(EXISTS "${_embedded_candidate}/adplug/rix.cpp")
        set(SDLPAL_ROOT "${_embedded_candidate}")
    elseif(DEFINED ENV{SDLPAL_ROOT} AND NOT "$ENV{SDLPAL_ROOT}" STREQUAL "")
        file(TO_CMAKE_PATH "$ENV{SDLPAL_ROOT}" SDLPAL_ROOT)
    elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/third_party/sdlpal/adplug/rix.cpp")
        set(SDLPAL_ROOT "${CMAKE_CURRENT_LIST_DIR}/third_party/sdlpal")
    endif()
endif()

get_filename_component(SDLPAL_ROOT "${SDLPAL_ROOT}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")

foreach(_required
        "${SDLPAL_ROOT}/adplug/rix.cpp"
        "${SDLPAL_ROOT}/common.h"
        "${SDLPAL_ROOT}/sdl_compat/sdl_compat.h")
    if(NOT EXISTS "${_required}")
        message(FATAL_ERROR
            "SDLPAL source checkout was not found.\n"
            "Pass -DSDLPAL_ROOT=/path/to/sdlpal, set the SDLPAL_ROOT environment variable,\n"
            "or clone this repository as sdlpal/tools/rix-player.\n"
            "Missing: ${_required}")
    endif()
endforeach()

message(STATUS "Using SDLPAL source tree: ${SDLPAL_ROOT}")

# Windows defaults to a self-contained executable with SDL3 and MinGW runtimes
# linked statically. Set -DRIX_PLAYER_STATIC=OFF for a DLL-based build.
option(RIX_PLAYER_STATIC
    "Build a self-contained Windows executable without sidecar runtime DLLs"
    ${WIN32})

if(WIN32 AND RIX_PLAYER_STATIC)
    find_package(SDL3 CONFIG REQUIRED COMPONENTS SDL3-static)
    if(NOT TARGET SDL3::SDL3-static)
        message(FATAL_ERROR
            "SDL3's static CMake target was not found. On MSYS2 UCRT64 install "
            "mingw-w64-ucrt-x86_64-sdl3, or configure with "
            "-DRIX_PLAYER_STATIC=OFF for a DLL-based build.")
    endif()
    set(SDL3_TARGET SDL3::SDL3-static)
else()
    find_package(SDL3 CONFIG QUIET)
    if(SDL3_FOUND AND TARGET SDL3::SDL3)
        set(SDL3_TARGET SDL3::SDL3)
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
        set(SDL3_TARGET PkgConfig::SDL3)
    endif()
endif()

find_package(Threads REQUIRED)

file(GLOB ADPLUG_C CONFIGURE_DEPENDS "${SDLPAL_ROOT}/adplug/*.c")
file(GLOB ADPLUG_CXX CONFIGURE_DEPENDS "${SDLPAL_ROOT}/adplug/*.cpp")

add_executable(sdlpal-rix-player
    rix_player.cpp
    ${ADPLUG_C}
    ${ADPLUG_CXX}
)

if(WIN32)
    set_target_properties(sdlpal-rix-player PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_include_directories(sdlpal-rix-player PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}"
    "${SDLPAL_ROOT}/sdl_compat"
    "${SDLPAL_ROOT}"
)

target_compile_definitions(sdlpal-rix-player PRIVATE
    USE_SDL3=1
    PAL_HAS_PLATFORM_SPECIFIC_UTILS=1
)

target_compile_features(sdlpal-rix-player PRIVATE cxx_std_17)
set_target_properties(sdlpal-rix-player PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)

if(MSVC)
    target_compile_options(sdlpal-rix-player PRIVATE /W4 /permissive-)
    if(WIN32 AND RIX_PLAYER_STATIC)
        set_property(TARGET sdlpal-rix-player PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
else()
    target_compile_options(sdlpal-rix-player PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-Wall;-Wextra;-fno-strict-aliasing>
        $<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-fno-strict-aliasing>
    )
endif()

target_link_libraries(sdlpal-rix-player PRIVATE
    ${SDL3_TARGET}
    Threads::Threads
)

if(WIN32 AND RIX_PLAYER_STATIC AND MINGW)
    target_link_options(sdlpal-rix-player PRIVATE
        -static
        -static-libgcc
        -static-libstdc++
    )
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(sdlpal-rix-player PRIVATE m)
endif()
