# SPDX-License-Identifier: MIT
# Copyright (c) 2026 ljrseals-dev
# Tier 3 umbrella — Session / Track / RenderGraph / Transport (subdirectory of HUSH DAW)

option(HUSH_USE_STUBS "Use built-in engine stubs when real Tier engines are unavailable" ON)

add_library(hush_daw_core STATIC
    src/session.cpp
    src/transport.cpp
    src/render_graph.cpp
    src/session_io.cpp
)

target_include_directories(hush_daw_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(HUSH_USE_STUBS)
    target_include_directories(hush_daw_core
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/stubs/include>
    )
    message(STATUS "hush_daw_core: using self-contained engine stubs")
else()
    # Link real engines when present
    if(TARGET hush::immersive)
        target_link_libraries(hush_daw_core PUBLIC hush::immersive)
    endif()
    if(TARGET hush::score_cli)
        target_link_libraries(hush_daw_core PUBLIC hush::score_cli)
    elseif(TARGET hush::score)
        target_link_libraries(hush_daw_core PUBLIC hush::score)
    endif()
    if(TARGET hush::video)
        target_link_libraries(hush_daw_core PUBLIC hush::video)
    endif()
    message(STATUS "hush_daw_core: linking available real Tier engines")
endif()

add_library(hush::daw_core ALIAS hush_daw_core)

add_executable(hush-session cli/hush_session.cpp)
target_link_libraries(hush-session PRIVATE hush_daw_core)
target_compile_options(hush-session PRIVATE -Wall -Wextra)

if(BUILD_TESTING OR HUSH_BUILD_TESTS)
  add_executable(test_daw_core tests/test_daw_core.cpp)
  target_link_libraries(test_daw_core PRIVATE hush_daw_core)
  add_test(NAME daw_core COMMAND test_daw_core)
endif()

install(TARGETS hush_daw_core hush-session
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)
if(HUSH_USE_STUBS)
    install(DIRECTORY stubs/include/ DESTINATION include)
endif()
