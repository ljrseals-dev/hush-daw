add_library(hush_engine_core STATIC
  src/bus_graph.cpp
  src/preallocated_bus_processor.cpp
  src/realtime_bus_engine.cpp
  src/path_sandbox.cpp
)
target_include_directories(hush_engine_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(hush_engine_core PUBLIC cxx_std_20)
if(NOT MSVC)
  target_compile_options(hush_engine_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(HUSH_BUILD_TESTS)
  add_executable(hush_bus_graph_tests tests/bus_graph_tests.cpp)
  target_link_libraries(hush_bus_graph_tests PRIVATE hush_engine_core)
  add_test(NAME hush_bus_graph_tests COMMAND hush_bus_graph_tests)

  add_executable(hush_parameter_queue_tests tests/parameter_queue_tests.cpp)
  target_include_directories(hush_parameter_queue_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_compile_features(hush_parameter_queue_tests PRIVATE cxx_std_20)
  add_test(NAME hush_parameter_queue_tests COMMAND hush_parameter_queue_tests)

  add_executable(hush_preallocated_bus_processor_tests tests/preallocated_bus_processor_tests.cpp)
  target_link_libraries(hush_preallocated_bus_processor_tests PRIVATE hush_engine_core)
  add_test(NAME hush_preallocated_bus_processor_tests COMMAND hush_preallocated_bus_processor_tests)

  add_executable(hush_realtime_bus_engine_tests tests/realtime_bus_engine_tests.cpp)
  target_link_libraries(hush_realtime_bus_engine_tests PRIVATE hush_engine_core)
  add_test(NAME hush_realtime_bus_engine_tests COMMAND hush_realtime_bus_engine_tests)

  add_executable(hush_stereo_adapter_tests tests/stereo_adapter_tests.cpp)
  target_include_directories(hush_stereo_adapter_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_compile_features(hush_stereo_adapter_tests PRIVATE cxx_std_20)
  add_test(NAME hush_stereo_adapter_tests COMMAND hush_stereo_adapter_tests)

  add_executable(hush_path_sandbox_tests tests/path_sandbox_tests.cpp)
  target_link_libraries(hush_path_sandbox_tests PRIVATE hush_engine_core)
  add_test(NAME hush_path_sandbox_tests COMMAND hush_path_sandbox_tests)
endif()
