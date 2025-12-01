#ifndef AST_H
#define AST_H

#include <stddef.h>

#include "tokenize.h"

typedef struct ast ast;
typedef enum ast_node_t ast_node_t;

typedef struct ast_program ast_program;
typedef struct ast_body ast_body;
typedef struct ast_identifier ast_identifier;
typedef struct ast_int ast_int;

typedef struct ast_assign ast_assign;
typedef struct ast_add ast_add;
typedef struct ast_mul ast_mul;

typedef enum ast_node_t
{
    AST_PROGRAM,
    AST_BODY,
    AST_IDENTIFIER,
    AST_INT,
    AST_VARDECL,
    AST_ASSIGN,
    AST_ADD,
    AST_MUL,
} ast_node_t;

struct ast
{
    ast_node_t type;
    union data
    {
        struct ast_program
        {
            ast** body;
            int count;
        } ast_program;
        struct ast_body
        {
            ast** statements;
            int count;
        } ast_body;
        struct ast_vardecl
        {
            ast* identifier;
            bool is_const;
        } ast_vardecl;
        struct ast_identifier
        {
            char* name;
            int __unused0;
        } ast_identifier;
        struct ast_int
        {
            int number;
            int __unused0;
        } ast_int;
        struct ast_assign
        {
            ast* lhs;
            ast* rhs;
        } ast_assign;
        struct ast_add
        {
            ast* lhs;
            ast* rhs;
        } ast_add;
        struct ast_mul
        {
            ast* lhs;
            ast* rhs;
        } ast_mul;
    } data;
    size_t start;
    size_t end;
};

ast* new_ast(ast_node_t type);
void free_ast(ast* node);
void fmt_ast(char* buffer, ast* node);

ast* parse(char* buffer);

#endif
