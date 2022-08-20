# test for branch, include
# 1. normal
# 2. if mask is all zero
# 3. else mask is all zero

.section .data
base:
	.word 1,2,3,4,5,6,7,8
normal:
	.word 1,6,1,4,5,1,2,6
all_one:
	.word 1,2,3,4,5,6,7,8
all_zero:
	.word 3,4,5,2,3,5,2,3

.section .text
.globl main, NORMAL, ELSE_NORMAL, NORMAL_JOIN
.globl main, ALL_ONE, ELSE_ALL_ONE, ALL_ONE_JOIN
.globl main, ALL_ZERO, ELSE_ALL_ZERO, ALL_ZERO_JOIN
main:
	addi    sp,sp,-16
    sd      s0,8(sp)
    addi    s0,sp,16
	li t4, 32
	vsetvli t4, t4, e32, ta, ma
	j NORMAL
main_end:
	li      a5,0
    mv      a0,a5
    ld      s0,8(sp)
    addi    sp,sp,16
    ret

NORMAL:
	la t0, base
	la t1, normal
	vle32.v v1, (t0)
	vle32.v v2, (t1)
	vbeq v1, v2, ELSE_NORMAL
	vadd.vv v0, v1, v2
	join v0, v0, NORMAL_JOIN
	
ELSE_NORMAL:
	vadd.vv v0, v1, v2
	join v0, v0, NORMAL_JOIN

NORMAL_JOIN:
	j ALL_ONE

ALL_ONE:
	la t0, base
	la t1, all_one
	vle32.v v1, (t0)
	vle32.v v2, (t1)
	vbeq v1, v2, ELSE_ALL_ONE
	vadd.vv v0, v1, v2
	join v0, v0, ALL_ONE_JOIN
	
ELSE_ALL_ONE:
	vadd.vv v0, v1, v2
	join v0, v0, ALL_ONE_JOIN

ALL_ONE_JOIN:
	j ALL_ZERO

ALL_ZERO:
	la t0, base
	la t1, all_zero
	vle32.v v1, (t0)
	vle32.v v2, (t1)
	vbeq v1, v2, ELSE_ALL_ZERO
	vadd.vv v0, v1, v2
	join v0, v0, ALL_ZERO_JOIN
	
ELSE_ALL_ZERO:
	vadd.vv v0, v1, v2
	join v0, v0, ALL_ZERO_JOIN

ALL_ZERO_JOIN:
	endprg x0, x0, x0
	j main_end




