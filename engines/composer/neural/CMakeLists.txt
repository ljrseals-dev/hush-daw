# SPDX-License-Identifier: MIT
# Copyright (c) 2026 ljrseals-dev


option(HUSH_NEURAL_WITH_CUDA "Enable CUDA execution provider" OFF)
option(HUSH_NEURAL_REGENERATE_MODEL "Regenerate model.onnx at build time" OFF)

find_package(onnxruntime QUIET)
if(NOT onnxruntime_FOUND AND DEFINED HUSH_ONNXRUNTIME_ROOT)
    set(onnxruntime_DIR "${HUSH_ONNXRUNTIME_ROOT}/lib/cmake/onnxruntime")
    find_package(onnxruntime QUIET)
endif()

if(NOT onnxruntime_FOUND)
    message(STATUS "onnxruntime not found — hush_neural will not be built")
else()

    message(STATUS "onnxruntime found — building hush_neural")

    add_library(hush_neural STATIC
        src/tokenizer.cpp
        src/onnx_session.cpp
        src/composer.cpp
    )

    target_include_directories(hush_neural
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../neural-lite/src
    )

    target_link_libraries(hush_neural
        PUBLIC
            hush_composer_interface
            hush_composer_harmony
            onnxruntime::onnxruntime
    )

    target_compile_features(hush_neural PUBLIC cxx_std_17)
    target_compile_options(hush_neural PRIVATE -Wall -Wextra )
    target_compile_definitions(hush_neural PUBLIC HUSH_HAS_ONNXRUNTIME=1)
    if(HUSH_NEURAL_WITH_CUDA)
        target_compile_definitions(hush_neural PUBLIC HUSH_NEURAL_WITH_CUDA=1)
    endif()

    add_library(hush::neural ALIAS hush_neural)

    add_executable(hush-neural src/main.cpp)
    target_include_directories(hush-neural PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../neural-lite/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(hush-neural PRIVATE
        hush_neural
        hush_composer_harmony
    )
    target_compile_options(hush-neural PRIVATE -Wall -Wextra )

    if(HUSH_NEURAL_REGENERATE_MODEL)
        find_package(Python3 COMPONENTS Interpreter QUIET)
        if(Python3_Interpreter_FOUND)
            add_custom_command(
                OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/model/model.onnx
                COMMAND ${Python3_EXECUTABLE}
                        ${CMAKE_CURRENT_SOURCE_DIR}/tools/train.py
                        --seed 42
                        --out  ${CMAKE_CURRENT_SOURCE_DIR}/model/model.onnx
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tools
                DEPENDS tools/train.py tools/corpus/seed.json
                COMMENT "Regenerating model.onnx"
            )
            add_custom_target(regenerate_model
                DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/model/model.onnx)
        else()
            message(WARNING "HUSH_NEURAL_REGENERATE_MODEL=ON but Python3 not found")
        endif()
    endif()

    enable_testing()
    add_executable(neural_tests tests/neural_tests.cpp)
    target_include_directories(neural_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../neural-lite/src
    )
    target_link_libraries(neural_tests PRIVATE
        hush_neural
        hush_composer_factory
    )
    target_compile_options(neural_tests PRIVATE -Wall -Wextra )
    add_test(NAME neural_tests COMMAND neural_tests)

    install(TARGETS hush_neural hush-neural
            ARCHIVE DESTINATION lib
            RUNTIME DESTINATION bin)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../include/hush
            DESTINATION include)
    install(FILES model/model.onnx
            DESTINATION share/hush/models
            OPTIONAL)

endif()
