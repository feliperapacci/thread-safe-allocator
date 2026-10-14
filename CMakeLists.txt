cmake_minimum_required(VERSION 3.20)
project(tsmm_allocator C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(TSMM_BACKEND "tlcache" CACHE STRING "Global Lock vs Thread-Local Cache")
set_property(CACHE TSMM_BACKEND PROPERTY STRINGS lock tlcache)

if (TSMM_BACKEND STREQUAL "lock")
    set(TSMM_BACKEND_SRC src/global_alloc.c)
elseif (TSMM_BACKEND STREQUAL "tlcache")
    set(TSMM_BACKEND_SRC src/tlcache_alloc.c)
else ()
    message(FATAL_ERROR "TSMM_BACKEND should be 'lock' or 'tlcache'. Not '${TSMM_BACKEND}'")
endif()

# Create Thread Safe Memory Manager (tsmm) static library
add_library(tsmm_lib STATIC
    ${TSMM_BACKEND_SRC}
    src/core.c
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
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(bench PRIVATE -O0 -g -Wall -Wextra)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(bench PRIVATE -O2 -Wall -Wextra)
endif()

# Simpler fixed-8-thread benchmark
add_executable(bench_simple benchmarks/simple_benchmark.c)
target_link_libraries(bench_simple tsmm_lib)
target_compile_definitions(bench_simple PRIVATE TSMM_BACKEND_NAME="${TSMM_BACKEND}")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(bench_simple PRIVATE -O0 -g -Wall -Wextra)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(bench_simple PRIVATE -O2 -Wall -Wextra)
endif()

# Testing Binary
add_executable(tests
    tests/test.c 
    tests/st_sanity.c
    tests/st_invariants.c
    tests/mt_invariants.c)
target_include_directories(tests PRIVATE 
    tests
    src)
target_link_libraries(tests tsmm_lib)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(tests PRIVATE -O0 -g -Wall -Wextra)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(tests PRIVATE -O2 -Wall -Wextra)
endif()

