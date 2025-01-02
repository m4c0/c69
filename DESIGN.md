# Design decisions

## Core values

Design decisions of C69 follows a list of simpler "values":

- **Source code should be fun for humans**
  Writing code should not be a chore. Everything a user type must be there for
  a reason only the user can explain.
- **Compilers should be clever**
  It is a compiler's responsibility to know everything about the code and its
  target environment.
- **No language is perfect**
  Therefore, even C69 is not perfect as well. We should be humble and accept
  our inefficiency so we can learn and improve.
- **All languages have something useful**
  Lessons can be taken from any existing/competing language. Same goes for C69
  so it will be a success even if its only outcome is a clever idea.
- **People are clever**
  Never assume someone is dumb. If they cannot understand something about C69,
  then it is because it is not clear enough or they got an idea that is better
  than yours.

## Specific "rules"

This section contain some specific design decisions.

They are presented in alphabetical order and indexed in a way you can read them
as needed, when you find a reference in examples.

### Meta-type prefixes

One reason why IDEs are so slow nowadays is the complexity required to compute
a list of symbols available. Usually this requires semantic-parsing source code
(or binary, like Java IDEs).

In such "modern" languages it is not possible to have a tool scan for `type X`
if you want to see the definition of a type named X.

Prefixing a symbol with its meta-type (`fn`, `type`, etc) brings a number of
benefits:

- Users only need to `grep -rn "fn .* main` to find the definition of a
  function named `main` or `grep -rn "type X"` to find the definition of a type
  named X
- Grammar loses ambiguity: when it reaches a `fn` then a function follows.
  Unlike C/C++ where a parser requires going further on the token stream to
  understand what the user typed.

### No main method

Once you understand how compilers, linkers and operating-systems coordinate
to run a process, you reach the only logical conclusion: there is no "main".
The real entry point in any modern operating system often is not the "main"
method defined by users.

Diving a bit deeper. Linux requires a `start` symbol, which calls `main`
eventually. C does not require much at this stage, but other languages goes
further. C++ uses this as to call global ctors before calling `main` - but
since it never actually defined the order of those global ctors, this also
leads to undefined behaviour if a global variable requires another one in their
ctors).

So, it makes more sense to follow Python's idea of module initialisation and
let the module load order define what is done.

This also enables some transparent integration with non-UNIX-like targets:
- WASM (via a exported global init method)
- Apple targets (which requires a boilerplate platform-specific main method)
- Windows (because of the main/WinMain shenanigans)

Also, it is simple to learn the requirement to add a single `main()`-like call
than adding support for non-UNIX-like platforms. If you ever had to add
"no-entry-point" to a WASM build, you understand what it means.

### Optional semi-colon

In many languages, semi-colons are useful to identify the end of a statement.

But if statement boundaries are unambiguous in the grammar, then they should
not be required.

