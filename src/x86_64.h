#ifndef X86_64_H
#define X86_64_H

typedef struct ast ast;

void emit_x86_64_binop(ast* node);
void emit_x86_64_statement(ast* node);
void emit_x86_64_body(ast* node);
void target_x86_64(ast* node);

#endif