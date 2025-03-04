__kernel void GEMM(
    __global float* A,
    __global float* B,
    __global float* C,
    __constant int* M, __constant int* N, __constant int* K) {
    
    int test_id = get_global_id(0);
    
    // Thread identifiers
    const int globalRow = get_global_id(0) / (*N); // Row ID of C (0..M)
    const int globalCol = get_global_id(0) % (*N); // Col ID of C (0..N)
 
    // Compute a single element (loop over K)

    float acc = 0.0f;
    for (int k=0; k<(*K); k++) {
        acc += A[k + globalRow*(*K)] * B[globalCol + k*(*N)];
    }
 
    // Store the result
    C[globalRow*(*N) + globalCol] = acc;
}
