section .text

_R_SomeFunction:
  push rbp
  mov rbp, rsp

  mov rdi, 4
  add rdi, 7
  mov rax, rdi
  leave
  ret

_R_Main:
  push rbp
  mov rbp, rsp

  mov rdi, 11
  call SomeFunction
  mov rax, rax
  leave
  ret

section .data
