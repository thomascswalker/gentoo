#ifndef X86_64_H
#define X86_64_H

typedef struct ast ast;

void x86_binop(ast* node);
void x86_statement(ast* node);
void x86_body(ast* node);
void target_x86(ast* node);

#endif