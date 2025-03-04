# Auto-test

Auto-Test is an efficient testing framework built on GoogleTest, designed to provide rapid validation for Ventus GPGPU kernel functions. The framework emulates the OpenCL style in its wrapper for common operations(still in progress). Some portions of the code are derived from drivers folder.

The initial motivation for creating this project was to test the correctness of my MLIR lowering pipeline. Therefore, the framework is not limited to testing OpenCL code alone.

# Usage

## OpenCL

To add a new test, such as validating your custom `vectorAdd` OpenCL code, follow these steps:

1. First, create a new folder named `vecadd` within the `test` directory.

2. Add the following files to your new folder:
   - `vecadd.cl`: The OpenCL code you need to validate (note that this filename must match your folder name)
   - `test.cpp`: The C++ code that calls your OpenCL kernel function (this filename is mandatory)

> [!WARNING]
> Currently, folder names cannot contain the characters `-`, `+`, or `.` as these will cause test failures for reasons that remain unclear.

3. Add the following line to the bottom of the `CMakeLists.txt` file in the `test` directory:
```
add_spike_test(vecadd vectorAdd)
```
Where `vecadd` is your test directory folder name and `vectorAdd` is the kernel function name in your OpenCL code.

4. Build your test with the following commands. Like in drivers, you'll need to set up certain environment variables first:
```
export SPIKE_SRC_DIR=...
export SPIKE_TARGET_DIR=...
export VENTUS_INSTALL_PREFIX=...

mkdir build
cd build
cmake ..
cmake --build .
```

> [!WARNING]
> The `ctest` functionality is currently unavailable, requiring manual execution of each program individually. This issue may be related to the execution directory context of `ctest`.

## MLIR
