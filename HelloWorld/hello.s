.data
.align 8
fmt:
	.ascii "%d\n"
	.byte 0
/* end data */

.text
.globl main
main:
	pushq %rbp
	movq %rsp, %rbp
	movl $5, %eax
.Lbb2:
	imull %eax, %eax
	cmpl $200, %eax
	jl .Lbb2
	movl %eax, %esi
	leaq fmt(%rip), %rdi
	callq printf
	movl $0, %eax
	leave
	ret
/* end function main */

