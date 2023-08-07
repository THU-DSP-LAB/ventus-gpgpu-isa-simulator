

__kernel void vectorAdd(__global float* A, __global float* B) {
  unsigned tid = get_global_id(0);
  A[tid] += B[tid];
  printf("ddd%d 1", 2);
}