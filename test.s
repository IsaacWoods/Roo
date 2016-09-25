section .text

_R_SomeFunction:
  push rbp
  mov rbp, rsp

  mov rdi, 4
  add rdi, 7
  mov rax, rdi
  leave
  ret

section .data
