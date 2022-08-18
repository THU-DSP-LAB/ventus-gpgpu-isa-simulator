
# v0 - 0
# v1 - iter
# v2 - all 1
# v3 - res

.section .data
loop_bound:
    .word 12,11,10,8,5,4,3,0
loop_sum:
    .word 1,1,1,1,1,1,1,1



.section .text

.globl main, main_end, start, LOOP, LOOP_COND_EVAL, LOOP_END
main:
    addi    sp,sp,-16
    sd      s0,8(sp)
    addi    s0,sp,16
	li      t4, 32
	vsetvli t4, t4, e32, ta, ma
    la      t0, loop_bound
    la      t1, loop_sum
    vle32.v v1, (t0)
    vle32.v v2, (t1)
	j       start

main_end:
	li      a5,0
    mv      a0,a5
    ld      s0,8(sp)
    addi    sp,sp,16
    ret

start:
    vblt    v0, v1, LOOP
    join    v0, v0, LOOP_END
LOOP:
    vadd.vi v1, v1, -1
    join    v0, v0, LOOP_COND_EVAL
LOOP_COND_EVAL:
    vblt    v0, v1, LOOP
    join    v0, v0, LOOP_END
LOOP_END:
    endprg  x0, x0, x0
    j main_end

