function(add_spike_test target kernel_func )
  add_spike_test_makefile(
    ${target} 
    ${kernel_func}  
    ${CMAKE_CURRENT_SOURCE_DIR}/${target}/Makefile 
  )
endfunction()

function(add_spike_test_opencl target kernel_func)
  add_spike_test_makefile(
    ${target} 
    ${kernel_func} 
    ${CMAKE_SOURCE_DIR}/cmake/OpenCL.mk
  )
endfunction()

function(add_spike_test_makefile target kernel_func makefile_path)
  file(GLOB TEST_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/${target}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/${target}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/${target}/*.hpp"
  )

  add_executable( test_${target} 
      ${TEST_SRCS}
  )

  add_dependencies( test_${target} ${PROJECT} )
  target_link_libraries( test_${target}  
    PUBLIC ${PROJECT}
    PRIVATE GTest::gtest_main 
  )

  GTEST_DISCOVER_TESTS( test_${target} )

  add_custom_target(
    ${target}.riscv
    COMMAND make -f ${makefile_path}
      VENTUS_INSTALL_PREFIX=${VENTUS_INSTALL_PREFIX}
      KERNEL_FUNC=${kernel_func}
      TARGET=${target}
      SOURCE_PATH=${CMAKE_CURRENT_SOURCE_DIR}/${target}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )

  add_dependencies(test_${target} ${target}.riscv)
endfunction()
