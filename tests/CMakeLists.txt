find_package(Catch2 CONFIG REQUIRED)

add_executable(
  tests
  asan.cpp
  guarded_data.cpp
  moved_flag.cpp
  no_unique_address.cpp
  non_copyable.cpp
  numeric_cast.cpp
  overload.cpp
  scope_exit.cpp
  unique_any.cpp
  unique_ptr.cpp
  version.cpp
)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain felly)
if (MSVC)
  target_compile_options(
    tests
    PRIVATE
    /W4 /WX
    /EHsc # Required for ASAN
  )
endif ()

include(CTest)
include(Catch)
catch_discover_tests(tests)