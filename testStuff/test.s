section .text
global _entry

_entry:
  push rbp
  mov rbp, rsp

  mov rax, 4
  imul rax, 2

  leave
  ret
