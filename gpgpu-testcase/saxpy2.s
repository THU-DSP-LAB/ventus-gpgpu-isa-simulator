.equ CSR_NUMW,  0x801
.equ CSR_NUMT,  0x802
.equ CSR_TID,   0x800
.equ CSR_WID,   0x805
.equ CSR_GDS,   0x807
.equ CSR_LDS,   0x806
# register arguments:
#     a0      n
#     a3      a
#     a1      x
#     a2      y
.section .text 
.globl main, saxpy2, saxpy
main:
    addi            sp,sp,-16
    sd              s0,8(sp)
    addi            s0,sp,16
    j               saxpy2
main_end:
    li              a5,0
    mv              a0,a5
    ld              s0,8(sp)
    addi            sp,sp,16
    ret
saxpy2:
    csrr            t1, CSR_GDS
    la              t1, global_data
    csrr            a4, vl
    lw              a0, 0(t1)
    lw              a3, 4(t1)
    addi            t2, a0, 31
    srli            t2, t2, 5
    slli            t2, t2, 7
    addi            a1, t1, 128
    add             a2, a1, t2

saxpy:
    li              t4, 32
	vsetvli         a4, t4, e32, ta, ma
    li              t4, 0
    vle32.v         v1, (a1)
    sub             a0, a0, a4
    slli            a4, a4, 2
    add             a1, a1, a4
    vle32.v         v8, (a2)
    vfmacc.vf       v8, fa3, v1
    vse32.v         v8, (a2)
    add             a2, a2, a4
    bnez            a0, saxpy
    endprg          x0, x0, x0
    endprg          x0, x0, x0
    endprg          x0, x0, x0
    j               main_end
.section .data
global_data:
    .word 0x00000040
    .word 0x40000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x00000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x42000000
    .word 0x00000000
    .word 0x40000000
    .word 0x40800000
    .word 0x40c00000
    .word 0x41000000
    .word 0x41200000
    .word 0x41400000
    .word 0x41600000
    .word 0x41800000
    .word 0x41900000
    .word 0x41a00000
    .word 0x41b00000
    .word 0x41c00000
    .word 0x41d00000
    .word 0x41e00000
    .word 0x41f00000
    .word 0x42000000
    .word 0x42080000
    .word 0x42100000
    .word 0x42180000
    .word 0x42200000
    .word 0x42280000
    .word 0x42300000
    .word 0x42380000
    .word 0x42400000
    .word 0x42480000
    .word 0x42500000
    .word 0x42580000
    .word 0x42600000
    .word 0x42680000
    .word 0x42700000
    .word 0x42780000
    .word 0x00000000
    .word 0x40000000
    .word 0x40800000
    .word 0x40c00000
    .word 0x41000000
    .word 0x41200000
    .word 0x41400000
    .word 0x41600000
    .word 0x41800000
    .word 0x41900000
    .word 0x41a00000
    .word 0x41b00000
    .word 0x41c00000
    .word 0x41d00000
    .word 0x41e00000
    .word 0x41f00000
    .word 0x42000000
    .word 0x42080000
    .word 0x42100000
    .word 0x42180000
    .word 0x42200000
    .word 0x42280000
    .word 0x42300000
    .word 0x42380000
    .word 0x42400000
    .word 0x42480000
    .word 0x42500000
    .word 0x42580000
    .word 0x42600000
    .word 0x42680000
    .word 0x42700000
    .word 0x42780000
