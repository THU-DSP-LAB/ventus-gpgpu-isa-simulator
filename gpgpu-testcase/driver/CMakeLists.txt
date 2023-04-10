cmake_minimum_required(VERSION 3.21)
set(PROJECT spike_driver)
project(${PROJECT})

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS -lstdc++)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories($ENV{SPIKE_SRC_DIR})
include_directories($ENV{SPIKE_SRC_DIR}/spike_main)
include_directories($ENV{SPIKE_SRC_DIR}/riscv)
include_directories($ENV{SPIKE_SRC_DIR}/build)
include_directories($ENV{SPIKE_SRC_DIR}/softfloat)
include_directories($ENV{SPIKE_SRC_DIR}/fesvr)

#set(CMAKE_POSITION_INDEPENDENT_CODE True)

file(GLOB_RECURSE SRCS ./ventus.cpp)

link_directories($ENV{SPIKE_TARGET_DIR}/lib)

add_library(${PROJECT} SHARED ${SRCS})

target_link_libraries(${PROJECT} PUBLIC spike_main)


set_target_properties(${PROJECT} PROPERTIES OUTPUT_NAME "${PROJECT}")
set_target_properties(${PROJECT} PROPERTIES CLEAN_DIRECT_OUTPUT 1)


add_executable(spike_test ./test.cpp)
add_dependencies(spike_test ${PROJECT})
target_link_libraries(spike_test PUBLIC ${PROJECT})
