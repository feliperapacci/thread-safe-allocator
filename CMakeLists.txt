cmake_minimum_required(VERSION 3.20)
project(tsmm_allocator C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Create Thread Safe Memory Manager (tsmm) static library
add_library(tsmm_lib STATIC
    src/mm.c
    src/mmhelper.c
)
target_include_directories(tsmm_lib 
    PUBLIC include
    PRIVATE src
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(tsmm_lib PRIVATE -O0 -g -Wall -Wextra)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(tsmm_lib PRIVATE -O2 -Wall -Wextra)
endif()

target_link_libraries(tsmm_lib pthread)

# Create Benchmark Program
add_executable(bench benchmarks/benchmark.c)
target_link_libraries(bench tsmm_lib)

# Testing Binary
add_executable(tests
    tests/test.c 
    tests/sanity.c)
target_include_directories(tests PRIVATE tests)
target_link_libraries(tests tsmm_lib)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(tests PRIVATE -O0 -g -Wall -Wextra)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(tests PRIVATE -O2 -Wall -Wextra)
endif()

