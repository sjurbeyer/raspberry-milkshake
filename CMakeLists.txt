cmake_minimum_required(VERSION 3.25)
project(raspberry_milkshake LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(tomlplusplus
  GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
  GIT_TAG v3.4.0
  SYSTEM)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.7.1
  SYSTEM)
FetchContent_MakeAvailable(tomlplusplus Catch2)

add_library(rm_core
  src/core/clock.cpp
  src/core/cue_time.cpp
  src/core/manifest.cpp
  src/core/sequencer.cpp
  src/core/policy.cpp
  src/core/channel_map.cpp
  src/core/led_layout.cpp
  src/core/dir_catalog.cpp
  src/core/demo_audio.cpp
)
target_include_directories(rm_core PUBLIC src)
target_link_libraries(rm_core PUBLIC tomlplusplus::tomlplusplus)
target_compile_options(rm_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(rm-validate tools/rm-validate/main.cpp)
target_link_libraries(rm-validate PRIVATE rm_core)
target_compile_options(rm-validate PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

add_executable(rm_tests
  tests/clock_test.cpp
  tests/cue_time_test.cpp
  tests/manifest_test.cpp
  tests/sequencer_test.cpp
  tests/policy_test.cpp
  tests/channel_map_test.cpp
  tests/led_layout_test.cpp
  tests/dir_catalog_test.cpp
  tests/demo_audio_test.cpp
)
target_link_libraries(rm_tests PRIVATE rm_core Catch2::Catch2WithMain)
target_compile_definitions(rm_tests PRIVATE RM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_compile_options(rm_tests PRIVATE -Wall -Wextra -Wpedantic)

add_test(NAME rm_validate_example
  COMMAND rm-validate --no-check-files ${CMAKE_SOURCE_DIR}/examples/collections/night-drive)
add_test(NAME rm_validate_reports_missing_files
  COMMAND rm-validate ${CMAKE_SOURCE_DIR}/examples/collections/night-drive)
set_tests_properties(rm_validate_reports_missing_files PROPERTIES
  PASS_REGULAR_EXPRESSION "audio file not found")
add_test(NAME rm_validate_reports_invalid_manifest
  COMMAND rm-validate --no-check-files ${CMAKE_SOURCE_DIR}/tests/fixtures/invalid/no-title)
set_tests_properties(rm_validate_reports_invalid_manifest PROPERTIES
  PASS_REGULAR_EXPRESSION "collection.toml:1: error: \\[title\\] missing required key")
set_tests_properties(rm_validate_example rm_validate_reports_missing_files
  rm_validate_reports_invalid_manifest PROPERTIES LABELS unit)

# Qt touch UI (Qt 6.8 in the dev image, the same series Raspberry Pi OS Trixie ships).
option(RM_BUILD_UI "Build the Qt touch UI" OFF)
if(RM_BUILD_UI)
  find_package(Qt6 6.4 REQUIRED COMPONENTS Core Test)
  add_library(rm_ui
    src/ui/library_model.cpp
    src/ui/library_filter.cpp
    src/ui/app_controller.cpp
  )
  set_target_properties(rm_ui PROPERTIES AUTOMOC ON)
  target_include_directories(rm_ui PUBLIC src)
  target_link_libraries(rm_ui PUBLIC rm_core Qt6::Core)
  target_compile_options(rm_ui PRIVATE -Wall -Wextra -Wpedantic)

  add_executable(rm_ui_tests tests/ui_model_test.cpp)
  target_link_libraries(rm_ui_tests PRIVATE rm_ui Qt6::Test Catch2::Catch2WithMain)
  target_compile_definitions(rm_ui_tests PRIVATE RM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  catch_discover_tests(rm_ui_tests PROPERTIES LABELS ui)
endif()

# projectM host: needs EGL + OpenGL ES 3 (Mesa llvmpipe in the dev container, V3D on the Pi).
option(RM_BUILD_RENDER "Build the projectM host and render tests" OFF)
if(RM_BUILD_RENDER)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(ENABLE_GLES ON CACHE BOOL "" FORCE)
  set(ENABLE_PLAYLIST OFF CACHE BOOL "" FORCE)
  set(ENABLE_SYSTEM_PROJECTM_EVAL OFF CACHE BOOL "" FORCE)
  set(ENABLE_SDL_UI OFF CACHE BOOL "" FORCE)
  set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(projectm
    GIT_REPOSITORY https://github.com/projectM-visualizer/projectm.git
    GIT_TAG v4.1.7
    GIT_SHALLOW TRUE
    SYSTEM)
  # projectM derives its version macros from the top-level project's version, which is not ours.
  set(CMAKE_PROJECT_VERSION 4.1.7)
  set(CMAKE_PROJECT_VERSION_MAJOR 4)
  set(CMAKE_PROJECT_VERSION_MINOR 1)
  set(CMAKE_PROJECT_VERSION_PATCH 7)
  FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20
    GIT_SHALLOW TRUE
    SYSTEM)
  set(HTTPLIB_COMPILE OFF CACHE BOOL "" FORCE)
  set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
  set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
  set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.56.0
    GIT_SHALLOW TRUE
    SYSTEM)
  FetchContent_MakeAvailable(stb httplib)
  FetchContent_MakeAvailable(projectm)
  unset(CMAKE_PROJECT_VERSION)
  unset(CMAKE_PROJECT_VERSION_MAJOR)
  unset(CMAKE_PROJECT_VERSION_MINOR)
  unset(CMAKE_PROJECT_VERSION_PATCH)
  find_package(OpenGL REQUIRED COMPONENTS EGL GLES3)

  add_library(rm_render
    src/render/egl_context.cpp
    src/render/projectm_host.cpp
    src/render/png.cpp
    src/render/show_runner.cpp
    src/render/downscaler.cpp
    src/render/jpeg.cpp
  )
  target_include_directories(rm_render PUBLIC src)
  target_include_directories(rm_render SYSTEM PRIVATE ${stb_SOURCE_DIR})
  target_link_libraries(rm_render PUBLIC rm_core libprojectM::projectM OpenGL::EGL OpenGL::GLES3)
  target_compile_options(rm_render PRIVATE -Wall -Wextra -Wpedantic)

  find_package(Threads REQUIRED)
  add_executable(rm-devserver tools/rm-devserver/main.cpp)
  target_link_libraries(rm-devserver PRIVATE rm_render httplib::httplib Threads::Threads)
  target_compile_options(rm-devserver PRIVATE -Wall -Wextra -Wpedantic)

  add_executable(rm_render_tests tests/render_test.cpp tests/show_runner_test.cpp tests/output_test.cpp)
  target_link_libraries(rm_render_tests PRIVATE rm_render Catch2::Catch2WithMain)
  target_compile_definitions(rm_render_tests PRIVATE RM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  target_compile_options(rm_render_tests PRIVATE -Wall -Wextra -Wpedantic)
  catch_discover_tests(rm_render_tests TEST_SPEC "~[.]" PROPERTIES LABELS render)
endif()

# Tests tagged [sequencer] get their own label; everything else is "unit".
catch_discover_tests(rm_tests TEST_SPEC "[sequencer]" PROPERTIES LABELS sequencer)
catch_discover_tests(rm_tests TEST_SPEC "~[sequencer]" PROPERTIES LABELS unit)
