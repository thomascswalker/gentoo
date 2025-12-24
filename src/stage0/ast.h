#ifndef AST_H
#define AST_H

#include <stddef.h>

#include "tokenize.h"

#define ERROR_SPAN 10

/* Forward declarations */

typedef enum codegen_type_t codegen_type_t;
typedef struct ast ast;

/* AST enums */

/* Enumeration for the explicit AST Node types. */
typedef enum ast_node_t
{
    // Top level
    AST_PROGRAM,
    AST_BODY,
    AST_BLOCK,

    // Statements
    AST_DECLVAR,
    AST_DECLFN,
    AST_ASSIGN,
    AST_RETURN,

    // Expressions
    AST_IDENTIFIER,
    AST_BINOP,
    AST_CONSTANT,
    AST_STRING,
    AST_CALL,
} ast_node_t;

typedef enum ast_constant_t
{
    CONST_INT,
    CONST_STRING,
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

char* ast_to_string(ast_node_t type);
char* binop_to_string(ast_binop_t op);

typedef enum ast_ident_t
{
    IDENT_VAR,
    IDENT_FN,
} ast_ident_t;

/* AST Node definitions
 *
 * Adding a new node definition requires 4 steps:
 *
 * 1. Declare the new node using the macros below.
 * 2. Add the node type to `ast_fmt`.
 * 3. Add the node type to `ast_free`.
 * 4. Add the node type to `ast_to_string`.
 */

#define AST_PROP(type, name) type name;
#define AST_NODE(name, ...)                                                    \
    typedef struct ast_##name                                                  \
    {                                                                          \
        __VA_ARGS__                                                            \
    } ast_##name

AST_NODE(program, AST_PROP(ast**, body) AST_PROP(int, count));
AST_NODE(body, AST_PROP(ast**, statements) AST_PROP(int, count));
AST_NODE(block, AST_PROP(ast**, statements) AST_PROP(int, count));
AST_NODE(declvar, AST_PROP(ast*, identifier) AST_PROP(bool, is_const));
AST_NODE(declfn, AST_PROP(ast*, identifier) AST_PROP(ast*, block));
AST_NODE(identifier, AST_PROP(char*, name));
AST_NODE(string, AST_PROP(char*, value));
AST_NODE(constant, AST_PROP(int, value) AST_PROP(ast_constant_t, type));
AST_NODE(call, AST_PROP(ast*, identifier) AST_PROP(ast**, args)
                   AST_PROP(size_t, count));
AST_NODE(assign, AST_PROP(ast*, lhs) AST_PROP(ast*, rhs));
AST_NODE(binop,
         AST_PROP(ast*, lhs) AST_PROP(ast*, rhs) AST_PROP(ast_binop_t, op));
AST_NODE(ret, AST_PROP(ast*, node));

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
        struct ast_block block;
        struct ast_declvar declvar;
        struct ast_declfn declfn;
        struct ast_identifier identifier;
        struct ast_constant constant;
        struct ast_string string;
        struct ast_call call;
        struct ast_assign assign;
        struct ast_binop binop;
        struct ast_ret ret;
    } data;
    size_t start;
    size_t end;
};

/* AST functions */

ast* ast_new(ast_node_t type);
void ast_free(ast* node);
void ast_fmt(char* buffer, ast* node);
char* ast_codegen(ast* node, codegen_type_t type);
void log_context();

/* Parsing functions for each AST Node type */

ast* parse_constant();
ast* parse_identifier();
ast* parse_factor();
ast* parse_term();
ast* parse_string();
ast* parse_expression();
ast* parse_assignment();
ast* parse_call();
ast* parse_declvar();
ast* parse_declfn();
ast* parse_ret();
ast* parse_statement();
ast* parse_block();
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
