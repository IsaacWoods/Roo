section .text

_R_SomeFunction:
  push rbp
  mov rbp, rsp

  mov rcx, 4
  add rcx, 7
  mov rax, rcx
  leave
  ret

_R_Main:
  push rbp
  mov rbp, rsp

  mov rcx, 11
  not rcx
  mov rdi, rcx
  call SomeFunction
  mov rax, rax
  leave
  ret

section .data
