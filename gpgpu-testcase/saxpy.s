.equ CSR_NUMW,  0x801
.equ CSR_NUMT,  0x802
.equ CSR_TID,   0x800
.equ CSR_WID,   0x805
.equ CSR_GDS,   0x807
.equ CSR_LDS,   0x806

# data
#   vector X                x10
#   vector Y                x11
# value
#   size                    x7
#   fp scalar A             x20

.section .text
.globl main, saxpy, L_END, L_ENDPRG
main:
    addi        sp,sp,-16
    sd          s0,8(sp)
    addi        s0,sp,16
    jal         saxpy
    li          a5,0
    mv          a0,a5
    ld          s0,8(sp)
    addi        sp,sp,16
    ret
saxpy:
    csrr        x5, CSR_GDS
    la          x5, global_data
    csrr        x6, CSR_TID
    vid.v       v7
    vadd.vx     v8, v7, x6
    lw          x7, 0(x5)               # size
    vmv.v.x     v9, x7
    vbge        v8, v9, L_END
    lw          x20, 4(x5)              # A
    addi        x7, x7, 31
    srli        x7, x7, 5
    slli        x7, x7, 7
    slli        x6, x6, 2
    addi        x10, x5, 128            
    add         x10, x10, x6            # X
    vle32.v     v10, (x10)
    add         x11, x10, x7            # Y
    vle32.v     v11, (x11)
    vfmacc.vf   v11, f20, v10
    vse32.v     v11, (x11)
L_END:
    ; join        v0, v0, L_ENDPRG
    join        v0, v0, L_ENDPRG
L_ENDPRG:
    barrier     x0, x0, x0
    endprg      x0, x0, x0
    endprg      x0, x0, x0
    endprg      x0, x0, x0
    ret

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
