#include <stdio.h>

#define STACK_SIZE 32
#define STRV(...) #__VA_ARGS__

#define BUF_GLOBAL(...) buffer_printf(g_global, __VA_ARGS__);
#define BUF_DATA(...) buffer_printf(g_data, __VA_ARGS__);
#define BUF_BSS(...) buffer_printf(g_bss, __VA_ARGS__);
#define BUF_TEXT(...) buffer_printf(g_text, __VA_ARGS__);

#define STRINGIFY(x) #x
#define EXPAND(...) __VA_ARGS__

#define EXTERN(symbol) BUF_TEXT("extern %s\n", STRINGIFY(symbol))
#define GLOBAL(symbol) BUF_TEXT("\tglobal %s\n", STRINGIFY(symbol))
#define FUNC_START(symbol) BUF_TEXT("%s:\n", STRINGIFY(symbol))
#define PUSH(reg) BUF_TEXT("\tpush %s\n", STRINGIFY(reg))
#define POP(reg) BUF_TEXT("\tpop %s\n", STRINGIFY(reg))
#define MOV(a, b) BUF_TEXT("\tmov %s, %s\n", STRINGIFY(a), STRINGIFY(b))
#define ADD(reg, value) BUF_TEXT("\tadd %s, %s\n", STRINGIFY(reg), STRINGIFY(value))
#define SUB(reg, value) BUF_TEXT("\tsub %s, %s\n", STRINGIFY(reg), STRINGIFY(value))
#define CALL(symbol) BUF_TEXT("\tcall %s\n", STRINGIFY(symbol))
#define RET() BUF_TEXT("\tret\n")
#define INT(syscall) BUF_TEXT("\tint %s\n", STRINGIFY(syscall))

#define DB(symbol, ...) BUF_DATA("\t%s: db %s\n", STRINGIFY(symbol), STRV(__VA_ARGS__))
#define EQU(a, b) BUF_DATA("\t%s: equ $-%s\n", STRINGIFY(a), STRINGIFY(b))
#define LDR(value) BUF_TEXT("\tldr r0, =%d\n", value)