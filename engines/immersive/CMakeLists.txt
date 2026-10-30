# SPDX-License-Identifier: MIT
# Copyright (c) 2026 ljrseals-dev
# Full immersive engine (subdirectory of HUSH DAW)

# C++ standard inherited from parent (C++20); sources are C++17-compatible

add_compile_options(-Wall -Wextra -Wpedantic)


set(HUSH_IMMERSIVE_SOURCES
  src/speakers.cpp
  src/panner.cpp
  src/ambisonics.cpp
  src/binaural.cpp
  src/adm.cpp
  src/renderer.cpp
  src/resample.cpp
  src/json_util.cpp
  src/transport.cpp
  src/hr_cache.cpp
  src/rotation.cpp
  src/object_panner.cpp
  src/resampler.cpp
)

add_library(hush_immersive ${HUSH_IMMERSIVE_SOURCES})
add_library(hush::immersive ALIAS hush_immersive)

target_include_directories(hush_immersive
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(MYSOFA QUIET libmysofa)
endif()

if(MYSOFA_FOUND)
  target_compile_definitions(hush_immersive PUBLIC HUSH_HAS_LIBSOFA)
  target_include_directories(hush_immersive PRIVATE ${MYSOFA_INCLUDE_DIRS})
  target_link_libraries(hush_immersive PUBLIC ${MYSOFA_LIBRARIES})
  message(STATUS "libmysofa found — SOFA binaural enabled")
else()
  message(STATUS "libmysofa not found — binaural uses built-in spherical head model")
endif()

include(GNUInstallDirs)
install(TARGETS hush_immersive
  EXPORT hush_immersive_targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

add_executable(hush-immersive-render tools/hush-immersive-render.cpp)
target_link_libraries(hush-immersive-render PRIVATE hush::immersive)

enable_testing()
add_executable(hush_immersive_tests tests/immersive_tests.cpp)
target_link_libraries(hush_immersive_tests PRIVATE hush::immersive)
add_test(NAME immersive COMMAND hush_immersive_tests)

add_executable(hush_hr_cache_tests tests/hr_cache_tests.cpp)
target_link_libraries(hush_hr_cache_tests PRIVATE hush::immersive)
add_test(NAME hr_cache COMMAND hush_hr_cache_tests)

add_executable(hush_rotation_tests tests/rotation_tests.cpp)
target_link_libraries(hush_rotation_tests PRIVATE hush::immersive)
add_test(NAME rotation COMMAND hush_rotation_tests)

add_executable(hush_resampler_tests tests/resampler_tests.cpp)
target_link_libraries(hush_resampler_tests PRIVATE hush::immersive)
add_test(NAME resampler COMMAND hush_resampler_tests)
