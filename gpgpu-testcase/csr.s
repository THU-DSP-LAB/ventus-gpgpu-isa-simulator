# read write test for custom csr
.section .data
.balign 4
test: 
.word 0x12,2,3,4,5,6,7
.section .text
.globl main
.type main, @function

main:
	la t0, test
	lw a0, 0(t0)
	lw a1, 4(t0)
	lw a2, 8(t0)
	lw a3, 12(t0)
	lw a4, 16(t0)
	csrrw x0, mscratch, sp
	csrrw x0, numw, a0
	csrrw x0, numt, a1
	csrrw x0, tid, a2
	csrrw x0, wid, a4
	csrrw x0, gds, a0
	csrrw x0, lds, a1
	csrrs a0, numw, x0
	ret
