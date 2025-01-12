# Trying some assembly code to understand per-platform requirements.
# It also shows how/if we can do our own calling conventions.
.intel_syntax

.p2align 4, 0x90 # Align with NOPs
.global  main    # Otherwise "main" isn't exported
main:
  lea  rax, [rip + .hello]
  call putz

  # try { thrower() } catch (???) { ??? }
  call thrower

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

thrower:
  .seh_proc thrower
  sub  rsp, 40
  # Easier to use the C handler than dealing with its shenanigans. Example from ReactOS:
  # https://github.com/reactos/reactos/blob/0520c75aaf3a9334d209e6b5ec04f15cb9b773c4/sdk/lib/crt/except/amd64/ehandler.c
  .seh_handler __C_specific_handler, @except
  .seh_handlerdata
  .long 1
  .long (try_s)@IMGREL
  .long (try_e)@IMGREL
  .long (ex_filter)@IMGREL
  .long (catcher)@IMGREL
  .text
try_s:
  mov  qword ptr [rbp - 16], -2
  call throwz
try_e:
  add  rsp, 40
  ret
catcher:
  lea  rax, [rip + .cruel]
  call putz
  jmp  try_e
  .seh_endproc

throwz:
  sub  rsp, 40
  mov  rcx, 1    # Exception code
  xor  rdx, rdx  # Exception flags
  xor  r8,  r8   # Number of arguments
  xor  r9,  r9   # Arguments
  call RaiseException@PLT
  int3

ex_filter:
  # EXCEPTION_EXECUTE_HANDLER 1
  # EXCEPTION_CONTINUE_SEARCH 0
  # EXCEPTION_CONTINUE_EXECUTION -1
  mov  eax, 1
  ret

.section .rodata
.hello: .asciz "Hello"
.cruel: .asciz "Cruel"
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
