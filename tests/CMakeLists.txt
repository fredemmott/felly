find_package(Catch2 CONFIG REQUIRED)

add_executable(
  tests
  moved_flag.cpp
  overload.cpp
  scope_exit.cpp
)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain felly)
if (MSVC)
  target_compile_options(
    tests
    PRIVATE
    /W4 /WX
  )
endif ()

include(CTest)
include(Catch)
catch_discover_tests(tests)