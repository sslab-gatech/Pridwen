  .text
  .global test_tsx
  .type   test_tsx, @function
test_tsx:
L0:
  xbegin L0
  xorq %rax, %rax
  xend
  ret
