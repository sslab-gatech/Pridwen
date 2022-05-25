	.text
	.global _check_tsx
_check_tsx:
Lbegin:
	xorq %rdi, %rdi
	xbegin Lbegin
	movq $1, %rdi
	xend
	movq %rdi, %rax
	ret
