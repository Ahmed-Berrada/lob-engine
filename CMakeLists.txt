cmake_minimum_required(VERSION 3.20)
project(lob_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optimization flags for Release
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")

# Library
add_library(lob_engine
    src/order_book.cpp
    src/matching_engine.cpp
)
target_include_directories(lob_engine PUBLIC include)

# Benchmark executable
add_executable(bench bench/benchmark.cpp)
target_link_libraries(bench PRIVATE lob_engine)

# Test executable
add_executable(tests tests/test_orderbook.cpp)
target_link_libraries(tests PRIVATE lob_engine)
