section .rodata
otherMessage db "Potato",0
message db "Hello, World!",0xa,0

section .text
global _start

_start:
  xor rbp, rbp

  mov rdi, 1
  mov rax, 1
  mov rsi, message
  mov rdx, 14
  syscall

  ; Issue a SYS_exit system call
  mov rax, 1
  xor rbx, rbx
  int 0x80
