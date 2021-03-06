; Copyright (C) 2017, Isaac Woods.
; See LICENCE.md

; INPUT(rdi) - pointer to a string
; OUTPUT(rax) - length of the string
GetStringLength:
  push rbp
  push rdi

  xor rcx, rcx
  not rcx       ; Sets rcx to -1
  xor al, al    ; al=0 - specify that we're looking for a '\0' character
  cld
  repnz scasb   ; decrement rcx til we get to a '\0'
  not rcx       ; make the length positive
  dec rcx       ; don't include the null-terminator

  mov rax, rcx
  pop rdi
  pop rbp
  ret

global _R_Print

; INPUT(rdi) - pointer to a string
_R_Print:
  push rbp

  call GetStringLength
  mov rdx, rax
  mov rsi, rdi
  mov rdi, 1    ; Destination = STDOUT
  mov rax, 1    ; Syscall     = SYS_WRITE
  syscall

  pop rbp
  ret
