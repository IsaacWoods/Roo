section .text
global _start

main:
  push rbp
  mov rbp, rsp

  mov rax, 4
  mov rbx, 'a'
  int 0x80

  leave
  ret

_start:
  xor rbp, rbp

  call main

  ; Issue a SYS_exit system call
  mov rax, 1
  xor rbx, rbx
  int 0x80
