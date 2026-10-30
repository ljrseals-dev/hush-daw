add_library(hush_forensic STATIC
  src/ForensicRack.cpp
  src/ForensicReport.cpp
)
target_include_directories(hush_forensic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(hush_forensic PUBLIC cxx_std_20)
add_library(hush::forensic ALIAS hush_forensic)

if(HUSH_BUILD_TESTS)
  add_executable(hush_forensic_tests tests/forensic_tests.cpp)
  target_link_libraries(hush_forensic_tests PRIVATE hush_forensic)
  add_test(NAME hush_forensic_tests COMMAND hush_forensic_tests)
endif()
