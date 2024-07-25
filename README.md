# RegexCompiler

This is a compiler based on LLLVM (version 14.0.6, from June 2022) that compiles regular expressions, which gives a ~70% speedup over conventional regex tools, at the cost of requiring that they be precompiled.
I have not tested this project on newer versions of LLVM.

The code isn't amazing, this project was mostly to get my toes into compilers again.

## Building

Ensure you have LLVM installed (or compile it yourself, though note that this takes several hours even on powerful hardware). If you go down the route of compiling LLVM yourself I highly recommend
you ensure that Ninja is your configured build system in cmake, it will make compilation significantly faster (though still quite slow).

On Linux it may be preinstalled or available easily within your package manager. On Windows I recommend going to https://github.com/vovkos/llvm-package-windows, as the LLVM github repo only provides
LLVM-based tools like clang and binaries for the LLVM-C API, not the C++ API used by this project. I have not tried building this on MacOS but I don't anticipate any problems.

Once you have LLVM installed you should be able to run `cmake -B your_build_dir`, though on Windows you may need to set some environment variables to ensure that cmake can find your LLVM directory.
This cmake command will create build files for your configured build system (defaults to Makefiles on Linux and Visual Studio solutions on Windows). This project is known to compile with both MSVC
and gcc.

cmake is configured to set the following flags:
- `-Wall -Wextra -Wpedantic` on non-MSVC compilers
- `/W4` on MSVC

I target zero warnings with `-Wall -Wextra -Wpedantic`, as I'm mostly developing this on Linux. MSVC seems to be stricter with these warning settings, I'm not particularly familiar with MSVC warning
levels.

Further, the `ENV` cmake variable (defaults to `DEBUG`) adds these flags based on its value:
- `ENV=DEBUG` sets `-g` on most compilers and `/DEBUG` on MSVC
- `ENV=RELEASE` sets `-O3` on most compilers and `/O2` on MSVC

If you're using Makefiles I strongly recommend passing `-jN` to run N separate jobs in parallel, significantly improving compile times. They're not too bad, but it's nearing 9 seconds singlethreaded,
likely due to all the LLVM header files that need to be included. This can be reduced to about 2.5 seconds. This should be handled for you if you use Ninja, and I don't know enough to comment on
other build systems.

Note that Ninja will try to use as many threads as you have, which can cause it to run out of memory and fail when building LLVM itself. You might want to set it to manually set `-jN` to a more
conservative value if you run into issues. Fortunately you will be able to restart it from where it left off, but it's unfortunate that it needs some babysitting.

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

