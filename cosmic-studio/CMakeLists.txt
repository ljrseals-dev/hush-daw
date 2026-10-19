cmake_minimum_required(VERSION 3.16)
project(HushCosmicBridge VERSION 0.2.1 LANGUAGES CXX)

option(HUSH_COSMIC_BUILD_TESTS "Build bridge contract tests" ON)

add_library(hush_cosmic_bridge INTERFACE)
target_include_directories(hush_cosmic_bridge INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(hush_cosmic_bridge INTERFACE cxx_std_20)

if(MSVC)
  target_compile_options(hush_cosmic_bridge INTERFACE /W4 /permissive-)
else()
  target_compile_options(hush_cosmic_bridge INTERFACE -Wall -Wextra -Wpedantic)
endif()

if(HUSH_COSMIC_BUILD_TESTS)
  enable_testing()
  add_executable(hush_cosmic_bridge_test tests/bridge_test.cpp)
  target_link_libraries(hush_cosmic_bridge_test PRIVATE hush_cosmic_bridge)
  add_test(NAME hush_cosmic_bridge_contract COMMAND hush_cosmic_bridge_test)
endif()
