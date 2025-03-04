cmake_minimum_required(VERSION 3.13.4)

project( auto_test )

cmake_policy(SET CMP0116 NEW)

enable_testing()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include(cmake/AddSpikeTest.cmake)

add_subdirectory(test)
