cmake_minimum_required(VERSION 3.16)
project(matching_engine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # generates compile_commands.json for clangd/IntelliSense

# Release build with optimizations by default -- latency work is meaningless in Debug
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")

find_package(PkgConfig REQUIRED)
pkg_check_modules(URING REQUIRED liburing)

# --- Google Test via FetchContent ---
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

# --- Main library ---
add_library(engine_core
  src/order_book.cpp
)
target_include_directories(engine_core PUBLIC include ${URING_INCLUDE_DIRS})
target_link_libraries(engine_core PUBLIC ${URING_LIBRARIES})

# --- Tests ---
enable_testing()
add_executable(engine_tests tests/order_book_test.cpp)
target_link_libraries(engine_tests PRIVATE engine_core gtest_main)
include(GoogleTest)
gtest_discover_tests(engine_tests)