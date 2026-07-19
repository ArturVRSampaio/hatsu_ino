cmake_minimum_required(VERSION 3.14)
project(hatsu_ino_v1_tests CXX)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.5.4
)
FetchContent_MakeAvailable(Catch2)

add_executable(tests test_logic.cpp)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../hatsu_v1)

option(COVERAGE "Enable coverage instrumentation" OFF)
if(COVERAGE)
  target_compile_options(tests PRIVATE --coverage -O0)
  target_link_options(tests PRIVATE --coverage)
endif()
