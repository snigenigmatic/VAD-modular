cmake_minimum_required(VERSION 3.15)

# Add this line to tell CMake where vcpkg is
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
elseif(DEFINED ENV{HOME})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

project(VideoOrchestrator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Add local cpr library ---
add_subdirectory(vendor/cpr)

# --- Find system packages (now via vcpkg) ---
find_package(nlohmann_json REQUIRED)

# Print found packages info
message(STATUS "Using local cpr from vendor/cpr")
message(STATUS "Found nlohmann_json: ${nlohmann_json_VERSION}")

# --- Define our executable ---
add_executable(orchestrator src/main.cpp)

# --- Link libraries ---
target_link_libraries(orchestrator PRIVATE
    cpr::cpr
    nlohmann_json::nlohmann_json
)