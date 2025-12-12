(Taken from phasar)

The files glibc_function_list_v1-04.05.17.conf and llvm_intrinsics_function_list_v1-04.05.17.conf contain a list of all functions defined in the GNU libc implementation and LLVM intrinsic functions, respectively.

When analyzing LLVM IR, it is often have call-sites that call a function contained in one of those lists. Following these calls is usually not desired as a user oftentimes do not want to analyze the next level of lowering (to libc). For pseudo functions like LLVM intrinsic functions there is no source code to analyze as these functions are only used to describe semantics (an orthogonal approch to adding a new instruction).

But default, we can  models calls to these functions as identity. But the specific algorithmic components could specify a different behavior if desired.