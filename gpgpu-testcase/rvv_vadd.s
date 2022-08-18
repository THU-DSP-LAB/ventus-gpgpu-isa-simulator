.section .data
vec_a :
  .word 1,  2,  3,  4,  5,  6,  7,  8,  9,  10
  .word 1,  2,  3,  4,  5,  6,  7,  8,  9,  10
  .word 1,  2,  3,  4,  5,  6,  7,  8,  9,  10
  .word 1,  2
vec_b :
  .word 1,  2,  3,  4,  5,  6,  7,  8,  9,  10
  .word 1,  2,  3,  4,  5,  6,  7,  8,  9,  10
  .word 1,  2,  3,  4,  5,  6,  7,  8,  9,  10
  .word 1,  2
vec_c :
  .word 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
  .word 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
  .word 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
  .word 0, 0
        
.section .text
.globl main
.type main @function

main:
    addi    sp,sp,-16
    sd      s0,8(sp)
    addi    s0,sp,16
    
    la a1, vec_a
    la a2, vec_b
    la a3, vec_c
    li t0, 32
    vsetvli t0, t0, e32, ta, ma  # Set vector length based on 32-bit vectors
    vle32.v v0, (a1)         # Get first vector
    vle32.v v1, (a2)         # Get second vector
    vadd.vv v2, v0, v1       # Sum vectors
    vse32.v v2, (a3)         # Store result
    
    li      a5,0
    mv      a0,a5
    ld      s0,8(sp)
    addi    sp,sp,16
    ret


