# Trying some assembly code to understand per-platform requirements
.intel_syntax

.p2align 4, 0x90 # Align with NOPs
.global  main    # Otherwise "main" isn't exported
main:
  push rbp
  mov  rbp, rsp
  lea  rcx, [rip + .msg]
  call puts@PLT

  xor  eax, eax
  pop  rbp
  ret

.section .rodata
.msg: .asciz "Hello World"

