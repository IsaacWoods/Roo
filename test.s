section .text

_R_SomeFunction:
  push rbp
  mov rbp, rsp

  mov rax, 4
  leave
  ret

_R_Main:
  push rbp
  mov rbp, rsp

  leave
  ret

section .data
