.text
.globl sieve
sieve:
	pushq %rbp
	movq %rsp, %rbp
	cmpl $0, %edi
	jle .Lbb2
	movb $0, (%rsi)
.Lbb2:
	cmpl $1, %edi
	jle .Lbb4
	movb $0, 1(%rsi)
.Lbb4:
	movq %rsi, %rcx
	addq $2, %rcx
	movl $2, %eax
.Lbb6:
	cmpl %edi, %eax
	jge .Lbb8
	movb $1, (%rcx)
	addl $1, %eax
	addq $1, %rcx
	jmp .Lbb6
.Lbb8:
	movq %rsi, %r9
	addq $2, %r9
	movl $2, %edx
.Lbb10:
	cmpl %edi, %edx
	jge .Lbb15
	movl %edx, %eax
	imull %edx, %eax
	movslq %edx, %r8
	movslq %eax, %rcx
	addq %rsi, %rcx
.Lbb12:
	cmpl %edi, %eax
	jge .Lbb14
	movb $0, (%rcx)
	addl %edx, %eax
	addq %r8, %rcx
	jmp .Lbb12
.Lbb14:
	addl $1, %edx
	addq $1, %r9
	jmp .Lbb10
.Lbb15:
	movl $0, %eax
	leave
	ret
/* end function sieve */

