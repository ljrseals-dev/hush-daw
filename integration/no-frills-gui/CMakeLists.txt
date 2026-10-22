cmake_minimum_required(VERSION 3.16)
project(hush_no_frills_gui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(X11 REQUIRED)

add_executable(hush_no_frills_gui src/main.cpp)
target_include_directories(hush_no_frills_gui PRIVATE ${X11_INCLUDE_DIR})
target_link_libraries(hush_no_frills_gui PRIVATE ${X11_LIBRARIES})
target_compile_options(hush_no_frills_gui PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)
