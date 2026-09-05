cmake_minimum_required(VERSION 3.20)
project(AmnesiaHeadTracking VERSION 0.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Amnesia: The Dark Descent (HPL2 engine) is a 32-bit OpenGL game. The .asi is
# loaded by Ultimate ASI Loader (renamed to wininet.dll, which the game imports).
# Configure with -A Win32.

option(HEADTRACKING_DEBUG_LOGGING "Enable verbose file logging" OFF)

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
    if(HEADTRACKING_DEBUG_LOGGING)
        add_definitions(-DHEADTRACKING_DEBUG_LOGGING=1)
    endif()
    # Static runtime so the .asi has no MSVC redist dependency.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# --- MinHook (vendored) ---
add_library(minhook STATIC
    extern/minhook/src/buffer.c
    extern/minhook/src/hook.c
    extern/minhook/src/trampoline.c
    extern/minhook/src/hde/hde32.c
    extern/minhook/src/hde/hde64.c
)
target_include_directories(minhook PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/extern/minhook/include)

# --- cameraunlock-core (submodule) ---
# Build the shared static lib plus its MinHook-backed HookManager wrapper.
# Tests are off: this is a mod build, not the core's own test run.
set(CAMERAUNLOCK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CAMERAUNLOCK_BUILD_HOOKS ON CACHE BOOL "" FORCE)
add_subdirectory(cameraunlock-core/cpp ${CMAKE_BINARY_DIR}/cameraunlock-core)

# --- Mod (.asi) ---
set(MOD_SOURCES
    src/main.cpp
    src/mod.cpp
    src/config.cpp
    src/camera_hook.cpp
    src/crosshair_hook.cpp
    src/game_state.cpp
)

add_library(AmnesiaHeadTracking SHARED ${MOD_SOURCES})

target_include_directories(AmnesiaHeadTracking PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/minhook/include
)

target_link_libraries(AmnesiaHeadTracking PRIVATE
    cameraunlock
    cameraunlock_hooks
    minhook
)

if(WIN32)
    target_link_libraries(AmnesiaHeadTracking PRIVATE ws2_32)
endif()

if(MSVC)
    target_compile_options(AmnesiaHeadTracking PRIVATE /W4 /permissive- /Zc:__cplusplus)
endif()

# Output as .asi for Ultimate ASI Loader.
set_target_properties(AmnesiaHeadTracking PROPERTIES
    PREFIX ""
    OUTPUT_NAME "AmnesiaHeadTracking"
    SUFFIX ".asi"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

message(STATUS "")
message(STATUS "Amnesia Head Tracking:")
message(STATUS "  Version:      ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Output:       bin/<config>/AmnesiaHeadTracking.asi")
message(STATUS "")
