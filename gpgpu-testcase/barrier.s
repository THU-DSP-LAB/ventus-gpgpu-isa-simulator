
# each warp iterate wid times and finally sync with barrier

.section .text
main:
    addi    sp,sp,-16
    sd      s0,8(sp)
    addi    s0,sp,16
	j       BARRIER
    .globl main

main_end:
	li      a5,0
    mv      a0,a5
    ld      s0,8(sp)
    addi    sp,sp,16
    ret
    .globl main_end

BARRIER:
    li      t0, 4       # 写死了，测的时候记得改，由硬件分配wid
    csrw    wid, t0     # 写死了，测的时候记得改，由硬件分配wid
    csrr    t0, wid
    li      t1, 0
    
LOOP:
    addi    t1, t1, 1
    blt     t1, t0, LOOP

LOOP_END:
    barrier x0, x0, x0
    j main_end

    
