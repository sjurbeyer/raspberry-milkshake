cmake_minimum_required(VERSION 3.25)
project(raspberry_milkshake LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(tomlplusplus
  GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
  GIT_TAG v3.4.0)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.7.1)
FetchContent_MakeAvailable(tomlplusplus Catch2)

add_library(rm_core
  src/core/clock.cpp
)
target_include_directories(rm_core PUBLIC src)
target_link_libraries(rm_core PUBLIC tomlplusplus::tomlplusplus)
target_compile_options(rm_core PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

add_executable(rm_tests
  tests/clock_test.cpp
)
target_link_libraries(rm_tests PRIVATE rm_core Catch2::Catch2WithMain)
target_compile_definitions(rm_tests PRIVATE RM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_compile_options(rm_tests PRIVATE -Wall -Wextra -Wpedantic)

# Tests tagged [sequencer] get their own label; everything else is "unit".
catch_discover_tests(rm_tests TEST_SPEC "[sequencer]" PROPERTIES LABELS sequencer)
catch_discover_tests(rm_tests TEST_SPEC "~[sequencer]" PROPERTIES LABELS unit)
