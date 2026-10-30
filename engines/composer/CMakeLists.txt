# SPDX-License-Identifier: MIT
# Copyright (c) 2026 ljrseals-dev
# Composer engine (subdirectory of HUSH DAW) — Tier 3 STABLE

# Shared interface (header-only)
add_library(hush_composer_interface INTERFACE)
target_include_directories(hush_composer_interface INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(TARGET hush::score)
    target_link_libraries(hush_composer_interface INTERFACE hush::score)
endif()
add_library(hush::composer ALIAS hush_composer_interface)

# Shared harmony (used by both backends)
add_library(hush_composer_harmony STATIC
    neural-lite/src/harmony.cpp
    neural-lite/src/style_loader.cpp
    neural-lite/src/smf.cpp
)
target_include_directories(hush_composer_harmony
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/neural-lite/src
)
target_link_libraries(hush_composer_harmony PUBLIC hush_composer_interface)
target_compile_options(hush_composer_harmony PRIVATE -Wall -Wextra)

# Lite backend (always available, pure C++17)
add_subdirectory(neural-lite)

# Neural backend (skipped cleanly if ONNX Runtime is absent)
add_subdirectory(neural)

# Factory
add_library(hush_composer_factory STATIC
    src/composer_factory.cpp
)
target_include_directories(hush_composer_factory
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/neural-lite/include
            ${CMAKE_CURRENT_SOURCE_DIR}/neural-lite/src
)
target_link_libraries(hush_composer_factory
    PUBLIC  hush_composer_interface
            hush_neural_lite
)
if(TARGET hush_neural)
    target_link_libraries(hush_composer_factory PUBLIC hush_neural)
    target_compile_definitions(hush_composer_factory PUBLIC HUSH_HAS_ONNXRUNTIME=1)
endif()
target_compile_options(hush_composer_factory PRIVATE -Wall -Wextra)
add_library(hush::composer_factory ALIAS hush_composer_factory)

install(DIRECTORY include/hush DESTINATION include)
install(TARGETS hush_composer_harmony hush_composer_factory
        ARCHIVE DESTINATION lib)
