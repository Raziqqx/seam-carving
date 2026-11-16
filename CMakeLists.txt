cmake_minimum_required(VERSION 3.16)
project(seam_carving LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(SEAM_BUILD_TESTS "Build the Catch2 test suite" ON)
option(SEAM_BUILD_BENCH "Build the benchmark program" ON)
option(SEAM_BUILD_APP "Build the interactive Dear ImGui app (needs GLFW + OpenGL)" OFF)
option(SEAM_USE_OPENMP "Use OpenMP for the multithreading experiment (item 5d)" ON)
option(SEAM_WERROR "Treat compiler warnings as errors (used by CI)" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
# The assignment asks for -O2 (CMake's default Release flag is -O3).
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

# Warning flags for OUR code only (not for third-party code such as Dear ImGui).
add_library(seam_warnings INTERFACE)
target_compile_options(seam_warnings INTERFACE -Wall -Wextra)
if(SEAM_WERROR)
    target_compile_options(seam_warnings INTERFACE -Werror)
endif()

find_package(OpenCV 4 REQUIRED COMPONENTS core imgproc imgcodecs videoio)

# ---------------------------------------------------------------------------
# Core library: one source file per module. Every module is listed here from the
# start, so adding work to a module never requires editing this file.
# ---------------------------------------------------------------------------
add_library(seam
    src/types.cpp          # shared helpers: seam_cost, is_valid_seam
    src/energy.cpp         # 1a  energy maps
    src/dp.cpp             # 1b  dynamic programming (+ rolling rows, forward energy)
    src/greedy.cpp         # 3   greedy
    src/maxflow.cpp        # 4   our Edmonds-Karp
    src/graphcut.cpp       # 4   seam graph + min-cut seam
    src/carve.cpp          # 1c  remove / insert loop, horizontal via transpose
    src/improvements.cpp   # 5d  OpenMP + incremental energy
    src/visualize.cpp      # 7a  seam overlay + animation
    src/cli_options.cpp    #     text -> enum helpers
)
target_include_directories(seam PUBLIC include ${OpenCV_INCLUDE_DIRS})
target_link_libraries(seam PUBLIC ${OpenCV_LIBS} PRIVATE seam_warnings)

if(SEAM_USE_OPENMP)
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(seam PUBLIC OpenMP::OpenMP_CXX)
    else()
        message(STATUS "OpenMP not found: the parallel variants will run on one thread")
    endif()
endif()

# ---------------------------------------------------------------------------
# Command-line tool
# ---------------------------------------------------------------------------
add_executable(seamcarve src/main.cpp)
target_link_libraries(seamcarve PRIVATE seam seam_warnings)

# ---------------------------------------------------------------------------
# Benchmark program (writes the CSV files behind every number in the report)
# ---------------------------------------------------------------------------
if(SEAM_BUILD_BENCH)
    add_executable(seam_bench benchmarks/bench.cpp)
    target_link_libraries(seam_bench PRIVATE seam seam_warnings)
endif()

# ---------------------------------------------------------------------------
# Tests: one file per module
# ---------------------------------------------------------------------------
if(SEAM_BUILD_TESTS)
    find_package(Catch2 3 REQUIRED)
    enable_testing()

    set(SEAM_TEST_SOURCES
        tests/test_support.cpp
        tests/test_energy.cpp
        tests/test_dp.cpp
        tests/test_greedy.cpp
        tests/test_maxflow.cpp
        tests/test_graphcut.cpp
        tests/test_carve.cpp
        tests/test_seam_count.cpp
        tests/test_edge_cases.cpp
        tests/test_improvements.cpp
        tests/test_visualize.cpp
    )

    # Boost.Graph is an OPTIONAL referee for our max-flow. It is used only in this test.
    find_package(Boost QUIET)
    if(Boost_FOUND AND EXISTS "${Boost_INCLUDE_DIRS}/boost/graph/edmonds_karp_max_flow.hpp")
        message(STATUS "Boost.Graph found: building the max-flow cross-check test")
        list(APPEND SEAM_TEST_SOURCES tests/test_graphcut_boost.cpp)
    else()
        message(STATUS "Boost.Graph not found: skipping the max-flow cross-check test")
    endif()

    add_executable(seam_tests ${SEAM_TEST_SOURCES})
    target_link_libraries(seam_tests PRIVATE seam seam_warnings Catch2::Catch2WithMain)
    if(Boost_FOUND)
        # SYSTEM: do not report warnings that come from inside Boost's headers.
        target_include_directories(seam_tests SYSTEM PRIVATE ${Boost_INCLUDE_DIRS})
    endif()

    include(Catch)
    # A Catch2 test that calls SKIP() exits with code 4; tell ctest that this means
    # "skipped" (a feature of a milestone that is not merged yet), not "failed".
    catch_discover_tests(seam_tests PROPERTIES SKIP_RETURN_CODE 4)

    # End-to-end test of the command-line tool on a real image.
    add_test(NAME cli_end_to_end
        COMMAND ${CMAKE_COMMAND}
            -DSEAMCARVE=$<TARGET_FILE:seamcarve>
            -DINPUT=${CMAKE_SOURCE_DIR}/data/inputs/tower.png
            -DSMALL_INPUT=${CMAKE_SOURCE_DIR}/data/inputs/tower_small.png
            -DOUTPUT_DIR=${CMAKE_BINARY_DIR}/cli_test
            -P ${CMAKE_SOURCE_DIR}/tests/cli_end_to_end.cmake)
    # The script prints SKIPPED_STEPS=<n>; n > 0 means a feature it needs is not merged yet.
    set_tests_properties(cli_end_to_end PROPERTIES
        SKIP_REGULAR_EXPRESSION "SKIPPED_STEPS=[1-9]")
endif()

# ---------------------------------------------------------------------------
# Interactive app (bonus 7b). Off by default so CI needs no window system.
# ---------------------------------------------------------------------------
if(SEAM_BUILD_APP)
    include(FetchContent)
    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.90.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(imgui)

    find_package(glfw3 3.3 REQUIRED)
    find_package(OpenGL REQUIRED)

    # Dear ImGui ships as plain source files; we compile the ones we need.
    add_library(imgui_backend STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
    target_include_directories(imgui_backend SYSTEM PUBLIC
        ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
    target_link_libraries(imgui_backend PUBLIC glfw OpenGL::GL)

    add_executable(seamcarve_app src/app.cpp)
    target_link_libraries(seamcarve_app PRIVATE seam seam_warnings imgui_backend)
endif()
