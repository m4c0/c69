# Trying some assembly code to understand per-platform requirements.
# It also shows how/if we can do our own calling conventions.
.intel_syntax

.p2align 4, 0x90 # Align with NOPs
.global  main    # Otherwise "main" isn't exported
main:
  lea  rax, [rip + .hello]
  call putz

  lea  rax, [rip + .world]
  call putz

  xor  eax, eax
  ret

# This function shows how we can isolate "externs" and use a different calling
# convention
putz:
  # On Windows, caller needs to reserve space on stack
  sub  rsp, 40   # TODO: check if needed in POSIX
  mov  rcx, rax
  call puts@PLT  # @PLT is a nice hack to avoid using .extern
  add  rsp, 40
  ret
  # clang use this trick to optimise, but it requires caller (i.e. "main" here)
  # to reserve the stack space
  # jmp puts@PLT

.section .rodata
.hello: .asciz "Hello"
.world: .asciz "World"

# Footnotes:
# - Win64 calling conventions: https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention?view=msvc-170
#   rax = fn(rcx, rdx, r8, r9, stack...)
# 
# Personal notes:
# - Looks like most BS of calling conventions have three categories:
#   1. Order of registers when calling other functions (which, of course, is
#      not standard)
#   2. Which registers should be saved if modified
#   3. How much shite can be stashed in hidden areas using special assembly
#      directives (CFIs plus the implicit call return address).
