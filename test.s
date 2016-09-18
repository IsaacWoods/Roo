section .text

_R_MyFunction:
  push rbp
  mov rbp, rsp

  mov rax, 4
  leave
  ret

section .data
