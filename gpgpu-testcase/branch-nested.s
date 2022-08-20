# nest branch test, with high level code as below
# if(if1_vs1 == if1_vs2) {
#   if1_ture;
#   if(if2_vs1 == if2_vs2) {
#       if2_true;
#   } else {
#       if2_false;
#       if(if3_vs1 != if3_vs2) {
#           if3_true;
#       } else {
#           if3_false;
#       }
#   }
# } else {
#   if1_false;
# }

.section .data
if1_vs1:
	.word 1,2,3,4,5,6,7,8
if1_vs2:
	.word 1,6,1,4,5,1,2,6
if2_vs1:
	.word 1,2,3,4,5,6,7,8
if2_vs2:
	.word 2,3,4,5,5,6,3,4
if3_vs1:
    .word 1,2,3,4,5,6,7,8
if3_vs2:
    .word 1,2,2,4,2,2,4,8

.section .text

.globl main, main_end, 
.globl NESTED, IF1_FALSE, IF1_JOIN, IF2_FALSE, IF2_JOIN
.globl if3_FALSE, if3_JOIN

main:
    addi    sp,sp,-16
    sd      s0,8(sp)
    addi    s0,sp,16
	li      t4, 32
	vsetvli t4, t4, e32, ta, ma
	j       NESTED

main_end:
	li      a5,0
    mv      a0,a5
    ld      s0,8(sp)
    addi    sp,sp,16
    ret

NESTED:
    la      t0, if1_vs1
    la      t1, if1_vs2
    vle32.v v1, (t0)
    vle32.v v2, (t1)
    vbne    v1, v2, IF1_FALSE   # if1 start
    vadd.vv v0, v1, v2          # if1 true branch
    la      t0, if2_vs1
    la      t1, if2_vs2
    vle32.v v1, (t0)
    vle32.v v2, (t1)
    vbne    v1, v2, IF2_FALSE   # if2 start
    vadd.vv v0, v1, v2          # if2 true branch
    join    v0, v0, IF2_JOIN    # if2 true branch end

IF1_FALSE:
    vand.vv v0, v1, v2          # if1 false branch
    join    v0, v0, IF1_JOIN            # if1 false branch end

IF1_JOIN:
    endprg  x0, x0, x0          # if1 join point - end prog and jump to main
    j       main_end

IF2_FALSE:
    vand.vv v0, v1, v2          # if2 false branch
    la      t0, if3_vs1
    la      t1, if3_vs2
    vle32.v v1, (t0)
    vle32.v v2, (t1)
    vbne    v1, v1, IF3_FALSE   # if3 start
    vadd.vv v0, v1, v2          # if3 true branch
    join    v0, v0, IF3_JOIN    # if3 true branch end

IF2_JOIN:
    join    v0, v0, IF1_JOIN    # if2 join point - if1 true branch end

IF3_FALSE:
    vand.vv v0, v1, v2          # if3 false branch
    join    v0, v0, IF3_JOIN    # if3 false branch end

IF3_JOIN:
    join    v0, v0, IF2_JOIN    # if3 join point - if2 false branch end

