; Copyright (C) 2016, Isaac Woods. All rights reserved.

section .text
global _start
extern _R_Main

_start:
  ; Clearly mark the outermost stack frame
  xor rbp, rbp

  ; Call into Roo!
  call _R_Main

  ; Call the SYS_EXIT system call
  mov rax, 1
  xor rbx, rbx
  int 0x80
