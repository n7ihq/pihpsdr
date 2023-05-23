Here are some "style" guidelines for adding code to piHPSDR:

- regularly use "make cppcheck" to check the code
- do not use local variables that shadow outer/global ones
- declare local variables with the smallest possible scope
- do not uses tabs, use spaces instead
- do not use fprintf to stderr, use g_print instead
- do not use alloca(), use variable-length arrays instead
- avoid code duplications
