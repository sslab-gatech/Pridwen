	.text
	.global check_tsx
	.type	check_tsx, @function
check_tsx:
	#xorq %rdi, %rdi
	#xorq %rax, %rax
	movq $9, %rax
l0:
	xbegin l0
	#movq $1, %rdi
	xend
	#movq %rdi, %rax
	ret
