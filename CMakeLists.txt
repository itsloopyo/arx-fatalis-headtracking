cmake_minimum_required(VERSION 3.20)
project(ArxFatalisHeadTracking VERSION 0.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# arx.exe is a 32-bit DirectDraw/Direct3D 7 binary. Configure with -A Win32.
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "ArxFatalisHeadTracking must be built as x86. Configure with -A Win32.")
endif()

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
    # Static runtime so the .asi has no MSVC redist dependency. The game predates
    # every redist a user is likely to have installed.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# --- cameraunlock-core (submodule) ---
# MinHook comes from cameraunlock-core's vendored copy, which BUILD_HOOKS
# switches on, so the licence that ships with the ZIP is the licence of the
# source that was compiled.
set(CAMERAUNLOCK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CAMERAUNLOCK_BUILD_HOOKS ON CACHE BOOL "" FORCE)
add_subdirectory(cameraunlock-core/cpp ${CMAKE_BINARY_DIR}/cameraunlock-core)

set(MOD_SOURCES
    src/dllmain.cpp
    src/config.cpp
    src/path_utils.cpp
    src/hotkeys.cpp
    src/tracking_runtime.cpp
    src/build_profile.cpp
    src/hook_install.cpp
    src/camera_hook.cpp
    src/roll_hook.cpp
    src/camera_diagnostics.cpp
    src/aim_projection.cpp
    src/game_state.cpp
    src/cursor_hook.cpp
    src/lean_trace.cpp
    src/window_centering.cpp
)

add_library(ArxFatalisHeadTracking SHARED ${MOD_SOURCES})

target_include_directories(ArxFatalisHeadTracking PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ArxFatalisHeadTracking PRIVATE cameraunlock minhook)

if(WIN32)
    target_link_libraries(ArxFatalisHeadTracking PRIVATE ws2_32)
endif()

if(MSVC)
    target_compile_options(ArxFatalisHeadTracking PRIVATE /W4 /WX /permissive- /Zc:__cplusplus)
endif()

option(ARX_BUILD_TESTS "Build ArxFatalisHeadTracking unit tests" OFF)

# bin/ is written by the SHIPPING tree only. The test tree configures this same target,
# and both deploy.ps1 and package-release.ps1 read bin/<config>.
if(ARX_BUILD_TESTS)
    set(MOD_OUTPUT_DIR "${CMAKE_BINARY_DIR}/mod")
else()
    set(MOD_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin")
endif()

set_target_properties(ArxFatalisHeadTracking PROPERTIES
    PREFIX ""
    OUTPUT_NAME "ArxFatalisHeadTracking"
    SUFFIX ".asi"
    RUNTIME_OUTPUT_DIRECTORY "${MOD_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${MOD_OUTPUT_DIR}"
)

if(ARX_BUILD_TESTS)
    enable_testing()

    function(arx_add_test name)
        add_executable(${name} tests/${name}.cpp ${ARGN})
        target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(${name} PRIVATE cameraunlock)
        if(MSVC)
            target_compile_options(${name} PRIVATE /W4 /WX /permissive- /Zc:__cplusplus)
        endif()
        add_test(NAME ${name} COMMAND ${name})
    endfunction()

    arx_add_test(arx_tests src/config.cpp src/build_profile.cpp src/lean_trace.cpp)
endif()
