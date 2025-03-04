cmake_policy(SET CMP0135 NEW)

include_directories(include)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

FetchContent_MakeAvailable(googletest)

INCLUDE(GoogleTest)

# Define KERNEL_ADDRESS if not already defined (default: 0x800000b8)
if(NOT DEFINED KERNEL_ADDRESS)
  message(STATUS "KERNEL_ADDRESS not specified, using default address 0x800000b8")
  set(KERNEL_ADDRESS "0x800000b8")
else()
  message(STATUS "Using specified KERNEL_ADDRESS: ${KERNEL_ADDRESS}") 
endif()

add_compile_definitions(KERNEL_ADDRESS=${KERNEL_ADDRESS})

set(PROJECT spike_auto_test)

set(SPIKE_SRC_DIR $ENV{SPIKE_SRC_DIR})
set(SPIKE_TARGET_DIR $ENV{SPIKE_TARGET_DIR})
set(VENTUS_INSTALL_PREFIX $ENV{VENTUS_INSTALL_PREFIX})

if(NOT DEFINED SPIKE_SRC_DIR)
  message(FATAL_ERROR "SPIKE_SRC_DIR must be defined")
endif()

if(NOT DEFINED SPIKE_TARGET_DIR)
  message(FATAL_ERROR "SPIKE_TARGET_DIR must be defined")
endif()

if(NOT DEFINED VENTUS_INSTALL_PREFIX)
  message(FATAL_ERROR "VENTUS_INSTALL_PREFIX must be defined")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS -lstdc++)

include_directories(${SPIKE_SRC_DIR})
include_directories(${SPIKE_SRC_DIR}/spike_main)
include_directories(${SPIKE_SRC_DIR}/riscv)
include_directories(${SPIKE_SRC_DIR}/build)
include_directories(${SPIKE_SRC_DIR}/softfloat)
include_directories(${SPIKE_SRC_DIR}/fesvr)

#set(CMAKE_POSITION_INDEPENDENT_CODE True)

set(SRCS 
  ${CMAKE_SOURCE_DIR}/src/ventus.cpp
  ${CMAKE_SOURCE_DIR}/src/utils.cpp
)

link_directories(${SPIKE_TARGET_DIR}/lib)
include_directories(${CMAKE_SOURCE_DIR}/include)

add_library(${PROJECT} SHARED ${SRCS})

target_link_libraries(${PROJECT} PUBLIC spike_main)

set_target_properties(${PROJECT} PROPERTIES OUTPUT_NAME "${PROJECT}")
set_target_properties(${PROJECT} PROPERTIES CLEAN_DIRECT_OUTPUT 1)


### >>> ADD YOUR TEST
add_spike_test(vecadd vectorAdd)
add_spike_test_opencl(vecadd_opencl vectorAdd)
add_spike_test_opencl(GEMM GEMM)
### <<<
