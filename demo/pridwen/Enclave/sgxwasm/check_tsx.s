	.text
	.global check_tsx
	.type	check_tsx, @function
check_tsx:
begin:
	xorq %rdi, %rdi
	xbegin begin
	movq $1, %rdi
	xend
	movq %rdi, %rax
	ret
