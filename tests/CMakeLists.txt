find_package(Catch2 CONFIG REQUIRED)

add_executable(
  tests
  moved_flag.cpp
  overload.cpp
)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain felly)

include(CTest)
include(Catch)
catch_discover_tests(tests)