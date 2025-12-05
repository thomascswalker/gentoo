#include <stdio.h>

#define STACK_SIZE 32

#define STRINGIFY(x) #x
#define EXPAND(...) __VA_ARGS__

#define _ASM(text) buffer_puts(b, text "\n");

#define BITS(size) _ASM("bits " STRINGIFY(size) "\n")
#define EXTERN(symbol) _ASM("extern " STRINGIFY(symbol))
#define SECTION(name) _ASM("\nsection ." STRINGIFY(name))
#define GLOBAL(symbol) _ASM("\tglobal " STRINGIFY(symbol))
#define FUNC_START(symbol) _ASM("\n" STRINGIFY(symbol) ":")
#define PUSH(reg) _ASM("\tpush " STRINGIFY(reg))
#define POP(reg) _ASM("\tpop " STRINGIFY(reg))
#define MOV(a, b) _ASM("\tmov " STRINGIFY(a) ", " STRINGIFY(b))
#define ADD(reg, value) _ASM("\tadd " STRINGIFY(reg) ", " STRINGIFY(value))
#define SUB(reg, value) _ASM("\tsub " STRINGIFY(reg) ", " STRINGIFY(value))
#define CALL(symbol) _ASM("\tcall " STRINGIFY(symbol))
#define RET() _ASM("\tret")

#define STRV(...) #__VA_ARGS__
#define DB(symbol, ...) _ASM("\t" #symbol " db " STRV(__VA_ARGS__))
#define LDR(value) buffer_printf(b, "\tldr r0, =%d\n", value)