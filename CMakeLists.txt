cmake_minimum_required(VERSION 3.25..4.1 FATAL_ERROR)
project(felly VERSION 0.0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(
  felly
  INTERFACE
  include/felly/guarded_data.hpp
  include/felly/moved_flag.hpp
  include/felly/numeric_cast.hpp
  include/felly/non_copyable.hpp
  include/felly/overload.hpp
  include/felly/scope_exit.hpp
  include/felly/unique_any.hpp
)
target_include_directories(
  felly
  INTERFACE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
)

include(CTest)
if (BUILD_TESTING)
  add_subdirectory(tests)
endif ()