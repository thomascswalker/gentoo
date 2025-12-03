#ifndef AST_H
#define AST_H

#include <stddef.h>

#include "tokenize.h"

/* Forward declarations */

typedef struct ast ast;

/* AST enums */

/* Enumeration for the explicit AST Node types. */
typedef enum ast_node_t
{
    // Top level
    AST_PROGRAM,
    AST_BODY,

    // Statements
    AST_DECLVAR,
    AST_RETURN,
    AST_ASSIGN,

    // Expressions
    AST_IDENTIFIER,
    AST_BINOP,
    AST_CONSTANT,
} ast_node_t;

typedef enum ast_constant_t
{
    CONST_INT
} ast_constant_t;

/* Enumeration for the binary operation types. */
typedef enum ast_binop_t
{
    BIN_ADD,
    BIN_SUB,
    BIN_MUL,
    BIN_DIV,
    BIN_EQ,
} ast_binop_t;

static char* binop_to_string(ast_binop_t op)
{
    switch (op)
    {
    case BIN_ADD:
        return "ADD";
    case BIN_SUB:
        return "SUB";
    case BIN_MUL:
        return "MUL";
    case BIN_DIV:
        return "DIV";
    case BIN_EQ:
        return "EQ";
    default:
        return "UNKNOWN";
    }
}

/* AST Node definitions
 *
 * Adding a new node definition requires 4 steps:
 *
 * 1. Declare the new node using the macros below.
 * 2. Add the node type to `fmt_ast`.
 * 3. Add the node type to `free_ast`.
 * 4. Add the node type to `get_node_type_string`.
 */

#define PAD(n) char __padding[n]
#define AST_PROP(type, name) type name;
#define AST_NODE(name, pad, ...)                                                                                       \
    typedef struct ast_##name                                                                                          \
    {                                                                                                                  \
        __VA_ARGS__                                                                                                    \
        PAD(pad);                                                                                                      \
    } ast_##name

AST_NODE(program, 8, AST_PROP(ast**, body) AST_PROP(int, count));
AST_NODE(body, 8, AST_PROP(ast**, statements) AST_PROP(int, count));
AST_NODE(stmt, 12, AST_PROP(ast*, stmt));
AST_NODE(expr, 12, AST_PROP(ast*, expr));
AST_NODE(declvar, 8, AST_PROP(ast*, identifier) AST_PROP(bool, is_const));
AST_NODE(identifier, 12, AST_PROP(char*, name));
AST_NODE(constant, 12, AST_PROP(int, value) AST_PROP(ast_constant_t, type));
AST_NODE(assign, 8, AST_PROP(ast*, lhs) AST_PROP(ast*, rhs));
AST_NODE(binop, 4, AST_PROP(ast*, lhs) AST_PROP(ast*, rhs) AST_PROP(ast_binop_t, op));

#undef PAD
#undef AST_PROP
#undef AST_NODE

/* Root AST Node definition */

struct ast
{
    ast_node_t type;

    // Union of all AST node types. All structs
    // are aligned to 16 bytes.
    union data
    {
        struct ast_program program;
        struct ast_body body;
        struct ast_expr expr;
        struct ast_stmt stmt;
        struct ast_declvar declvar;
        struct ast_identifier identifier;
        struct ast_constant constant;
        struct ast_assign assign;
        struct ast_binop binop;
    } data;
    size_t start;
    size_t end;
};

/* AST functions */

ast* new_ast(ast_node_t type);
void free_ast(ast* node);
void fmt_ast(char* buffer, ast* node);

/* Parsing functions for each AST Node type */

ast* parse_constant();
ast* parse_binop();
ast* parse_expression();
ast* parse_assignment();
ast* parse_new_assignment();
ast* parse_statement();
ast* parse_body();
ast* parse_program();

/* @brief Parses the incoming `buffer` string in two passes:
 *
 * 1. Tokenizes the buffer, constructing an array of tokens from the raw text.
 *
 * 2. Parses the token array into an abstract syntax tree.
 *
 */

ast* parse(char* buffer);

#endif
