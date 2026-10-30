cmake_minimum_required(VERSION 3.16)
project(HushDawAligned VERSION 2.2.1 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# OFFLINE-FIRST CONTRACT
# - No FetchContent, ExternalProject, or download scripts in this tree.
# - System packages only (optional ALSA/X11/Qt are find_package when enabled).
# - Default build is headers + tests for cosmic bridge + engine contracts.
# - NO "DAW checker" / validation harness / test-orchestration daemon.
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HUSH_BUILD_COSMIC_BRIDGE "Build Cosmic Studio bridge (SPSC, adapter)" ON)
option(HUSH_BUILD_ENGINE_CORE "Build bus-graph + offline engine contracts" ON)
option(HUSH_BUILD_TESTS "Build contract tests" ON)

# Explicitly refuse network bootstrap patterns if a parent injects them
if(DEFINED FETCHCONTENT_FULLY_DISCONNECTED)
  # keep disconnected
else()
  set(FETCHCONTENT_FULLY_DISCONNECTED ON CACHE BOOL "HUSH offline: never FetchContent" FORCE)
endif()

add_subdirectory(engine)

if(HUSH_BUILD_COSMIC_BRIDGE)
  add_subdirectory(cosmic-studio)
endif()


option(HUSH_BUILD_TIER3 "Build Tier 3 daw-core session + composer" ON)
option(HUSH_BUILD_COMPOSER "Build composer (neural-lite) engine" ON)
option(HUSH_USE_STUBS "daw_core uses engine stubs when Tier1 engines absent" ON)

# CTest for subdirectory engines
include(CTest)

if(HUSH_BUILD_TIER3)
  add_subdirectory(engines/daw_core)
endif()
if(HUSH_BUILD_COMPOSER)
  add_subdirectory(engines/composer)
endif()

if(HUSH_BUILD_TESTS)
  enable_testing()
  # Privacy denylist audit (Cosmic Studio parity) — not a DAW checker
  add_test(
    NAME hush_privacy_source_audit
    COMMAND ${CMAKE_COMMAND}
      -DHUSH_SOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/privacy_source_audit.cmake
  )
  # Offline CMake self-check: CMakeLists must not contain download APIs
  add_test(
    NAME hush_offline_cmake_audit
    COMMAND ${CMAKE_COMMAND}
      -DHUSH_SOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/offline_cmake_audit.cmake
  )
endif()
