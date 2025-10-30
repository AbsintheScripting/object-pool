cmake_minimum_required(VERSION 3.20)

project(ObjectPool)

message(STATUS "Preparing Tests for ObjectPool...")
message(STATUS "Using CMake ${CMAKE_VERSION} & C++ compiler ${CMAKE_CXX_COMPILER}")

# ------------------ compiler settings ------------------

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(WARNINGS_ACTIVATE
    -Wall
    -Wextra
    -Werror=return-type)
set(WARNINGS_IGNORE
    -fno-strict-aliasing
    -Wno-reorder
    -Wno-unused-variable
    -Wno-unused-function
    -Wno-unknown-pragmas
    -Wno-parentheses)
set(COMPILER_WARNINGS ${WARNINGS_ACTIVATE} ${WARNINGS_IGNORE})

# ------------------ GTest ------------------

include(FetchContent)
# configure build of googletest
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_Declare(gtest
    QUIET
    URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(gtest)

# ------------------ Test for ObjectPool ------------------

add_executable(object_pool_tests
    "tests/ObjectPool.cpp"
)

target_include_directories(object_pool_tests
    PRIVATE ${CMAKE_SOURCE_DIR}/tests
    PRIVATE ${CMAKE_SOURCE_DIR}/include
)

target_compile_options(object_pool_tests PRIVATE
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O2>
    ${COMPILER_WARNINGS}
)

target_compile_features(object_pool_tests PRIVATE cxx_std_23)

target_link_libraries(object_pool_tests
    gtest gtest_main
)

# ------------------ GTest settings for ObjectPool ------------------

enable_testing()
include(GoogleTest)
gtest_discover_tests(object_pool_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES LABELS "object_pool"
    DISCOVERY_TIMEOUT 240  # how long to wait (in seconds) before crashing
)
