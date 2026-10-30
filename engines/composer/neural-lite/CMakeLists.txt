# SPDX-License-Identifier: MIT
# Copyright (c) 2026 ljrseals-dev

add_library(hush_neural_lite STATIC
    src/composer.cpp
)
target_include_directories(hush_neural_lite
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(hush_neural_lite
    PUBLIC
        hush_composer_interface
        hush_composer_harmony
)
target_compile_options(hush_neural_lite PRIVATE -Wall -Wextra )
add_library(hush::neural_lite ALIAS hush_neural_lite)

add_executable(hush-neural-lite src/main.cpp)
target_include_directories(hush-neural-lite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_link_libraries(hush-neural-lite PRIVATE hush_neural_lite)
target_compile_options(hush-neural-lite PRIVATE -Wall -Wextra )

enable_testing()
add_executable(lite_tests tests/lite_tests.cpp)
target_include_directories(lite_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
target_link_libraries(lite_tests PRIVATE hush_neural_lite)
target_compile_options(lite_tests PRIVATE -Wall -Wextra )
add_test(NAME lite_tests COMMAND lite_tests)

install(TARGETS hush_neural_lite hush-neural-lite
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin)
install(DIRECTORY include/hush DESTINATION include)
