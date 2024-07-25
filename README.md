# RegexCompiler

This is a compiler based on LLLVM (version 14) that compiles regular expressions, which gives a ~50% speedup over conventional regex tools, at the cost of requiring that they be precompiled.

The code isn't amazing, this project was mostly to get my toes into compilers again.

## Building

Ensure you have LLVM installed (or compile it yourself, though note that this takes several hours even on powerful hardware).

On Linux it may be preinstalled or available easily within your package manager. On Windows I recommend going to https://github.com/vovkos/llvm-package-windows, as the LLVM github repo only provides
LLVM-based tools like clang and binaries for the LLVM-C API, not the C++ API used by this project.

Once you have LLVM installed you should be able to run `cmake -B your_build_dir`, though on Windows you may need to set some environment variables to ensure that cmake can find your LLVM directory.
This cmake command will create build files for your configured buildsystem (defaults to Makefiles on Linux and Visual Studio solutions on Windows). This project is known to compile with both MSVC
and gcc.

## Use

Once you've built a binary, run `./RegexCompiler abc` to compile a regex. This will produce an `out.ll` LLVM IR file which can be compiled with `clang ./out.ll -x ir`, as well as any additional flags
you might want (e.g. `-O3` for optimization since the whole point of this project is to be faster than regex interpreters). This will produce an `a.out` or `a.exe` file that takes input through stdin.
It will return an exit code of 0 if the input matches the regex, and an exit code of 1 if it does not (and a different non-zero exit code in the case of error).

The LLVM IR can also be interpreted with `lli ./out.ll`. This is handy for tracking down bugs in codegen.

Currently the following metacharacters are supported:
- `^` for start of input
- `$` for end of input
- `\d` for any digit
- A preceding `\` for escaping metacharacters (and backslashes themselves)

