This folder is used for test spike-device with spike-driver in a baremetal mode.
After modification test.cpp, run:
```
export SPIKE_SRC_DIR=<path-to-spike-dir>
export SPIKE_TARGET_DIR=<path-to-spike-target-path>
mkdir build && cd build
cmake ..
make
./spike_test
```
if spike_main.so cannot be found, add `<path-to-spike-target-path>/lib` into LD_LIBRARY_PATH