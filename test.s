section .text
global _entry

_entry:
  push rbp
  mov rbp, rsp

  mov rsp, rbp
  pop rbp
