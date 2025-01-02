# Design decisions

This document contain some design decisions. They are indexed in a way you can
read them as needed, when you find a reference in examples.

## No main method

Once you understand how compilers, linkers and operating-systems coordinate
to run a process, you reach the only logical conclusion: there is no "main".

So, it makes more sense to follow Python's idea of module initialisation and
let the module load order define what is done.

This also enables some transparent integration with non-UNIX-like targets:
- WASM (via a exported global init method)
- Apple targets (which requires a boilerplate platform-specific main method)
- Windows (because of the main/WinMain shenanigans)

Also, it is simple to learn the requirement to add a single `main()`-like call
than adding support for non-UNIX-like platforms. If you ever had to add
"no-entry-point" to a WASM build, you understand what it means.
