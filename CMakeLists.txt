cmake_minimum_required(VERSION 3.10)

project(NumberSliderSolver CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Building in Release mode: Optimizations enabled (-O3)")
    add_compile_options(-O3)
else()
    message(STATUS "Building in Debug/RelWithDebInfo mode: Debug info/less aggressive optimizations")
    add_compile_options(-g -O0)
endif()

set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "Disable building spdlog tests" FORCE)
set(SPDLOG_BUILD_SHARED_LIBS OFF CACHE BOOL "Build spdlog as shared library" FORCE)
message(STATUS "Adding spdlog as subdirectory from third_party/spdlog. Tests disabled, building static.")
add_subdirectory(third_party/spdlog)

set(TBB_BUILD_TESTS OFF CACHE BOOL "Disable building oneTBB tests" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build TBB as shared library" FORCE)
message(STATUS "Adding oneTBB as subdirectory from third_party/oneTBB. Tests disabled, building static.")
add_subdirectory(third_party/oneTBB)


set(SOURCE_FILES
    src/main.cpp
    src/PuzzleSolver.cpp
)

add_executable(number_slider_solver ${SOURCE_FILES})

message(STATUS "Linking number_slider_solver with spdlog::spdlog and TBB::tbb")
target_link_libraries(number_slider_solver PRIVATE
    spdlog::spdlog
    TBB::tbb
)

find_package(Threads REQUIRED)
target_link_libraries(number_slider_solver PRIVATE Threads::Threads)

message(STATUS "CMake configuration complete for NumberSliderSolver.")
