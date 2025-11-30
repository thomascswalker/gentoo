#ifndef AST_H
#define AST_H

#include <string.h>

#include "lex.h"
#include "log.h"

static token_t* g_tcur = NULL;
static size_t g_tpos = 0;

/* Typedefs */

typedef struct ast ast;
typedef enum ast_tag ast_tag;

typedef struct ast_program ast_program;
typedef struct ast_body ast_body;
typedef struct ast_identifier ast_identifier;
typedef struct ast_int ast_int;

typedef struct ast_assign ast_assign;
typedef struct ast_add ast_add;
typedef struct ast_mul ast_mul;

// AST Node data types

enum ast_tag
{
    AST_PROGRAM,
    AST_BODY,
    AST_IDENTIFIER,

    // Primitives
    AST_INT,

    // Operators
    AST_ASSIGN,
    AST_ADD,
    AST_MUL,
};

// AST Node data structures

// Base AST Node

struct ast
{
    enum ast_tag type;

    union data
    {
        struct ast_program
        {
            ast* body;
        } ast_program;
        struct ast_body
        {
            ast* items; // Array of items
        } ast_body;
        struct ast_identifier
        {
            char* name;
        } ast_var;
        struct ast_int
        {
            int number;
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
};

void fmt_ast(char* buffer, ast* node)
{
    switch (node->type)
    {
    case AST_IDENTIFIER:
        /* Directly access the union member to avoid type/name shadowing */
        sprintf(buffer, "{\"type\": \"identifier\", \"name\": \"%s\"}", node->data.ast_var.name);
        break;
    case AST_INT:
        sprintf(buffer, "{\"type\": \"int\", \"number\": %d}", node->data.ast_int.number);
        break;
    case AST_ASSIGN:
        char* lhs_buffer;
        char* rhs_buffer;
        fmt_ast(lhs_buffer, node->data.ast_assign.lhs);
        fmt_ast(rhs_buffer, node->data.ast_assign.rhs);
        sprintf(buffer, "{\"type\": \"assign\", \"lhs\": %s, \"rhs\": %s}", lhs_buffer, rhs_buffer);
        break;
    case AST_ADD:
    case AST_MUL:
        break;
    }
}

ast* new_ast()
{
    return (ast*)malloc(sizeof(ast));
}

bool expect(token_type_t type)
{
    return g_tcur->type == type;
}

void require(token_type_t type)
{
    log_debug("Requiring %s...", get_token_type_string(type));
    if (!expect(type))
    {
        log_error("Expected token %s, got %s.\n", get_token_type_string(type), get_token_type_string(g_tcur->type));
        exit(1);
    }
}

bool peek(token_type_t type)
{
    return (g_tcur + 1)->type == type;
}

void next()
{
    g_tcur++;
    log_debug("  Current token: pos=%d, type=%s, value='%s'", g_tcur->pos, get_token_type_string(g_tcur->type),
              g_tcur->value);
}

ast* parse_assignment()
{
    ast* expr = new_ast();
    expr->type = AST_ASSIGN;

    // Ensure the name is present.
    require(TOK_NAME);

    // Parse and consume the name
    ast* lhs = new_ast();
    lhs->type = AST_IDENTIFIER;
    lhs->data.ast_var.name = g_tcur->value;
    expr->data.ast_assign.lhs = lhs;
    next();

    // Ensure the assignment operator is present.
    if (!expect(TOK_ASSIGN))
    {
        log_error("Expected assignment operator. Got %s.", get_token_type_string(g_tcur->type));
        exit(1);
    }
    next();

    // Consume the value.
    ast* rhs = new_ast();
    rhs->type = AST_INT;
    rhs->data.ast_int.number = atoi(g_tcur->value);
    expr->data.ast_assign.rhs = rhs;

    return expr;
}

ast* parse_expression()
{
    log_info("Parsing expression...");
    ast* expr = new_ast();

    // Expect either 'const' or 'let'
    if (!(expect(TOK_CONST) || expect(TOK_LET)))
    {
        log_error("Expected either 'const' or 'let'. Got %s.", get_token_type_string(g_tcur->type));
        exit(1);
    }

    // Consume 'const' or 'let'
    next();

    // Assume assignment
    return parse_assignment();
}

ast* parse_body()
{
    ast* expr = new_ast();
    expr->type = AST_BODY;
}

ast* parse_program()
{
    ast* expr = new_ast();
    expr->type = AST_PROGRAM;
}

ast* parse()
{
    // Lexically deconstruct the string buffer and construct a set of tokens.
    // Assuming a maximum of TOKEN_COUNT tokens.
    token_t tokens[TOKEN_COUNT];

    log_info("Lexing tokens...");
    size_t count = lex(tokens);

    log_info("Found %d tokens.", count);

    for (int i = 0; i < count; i++)
    {
        print_token(&tokens[i]);
    }

    g_tcur = &tokens[0];
    ast* expr = parse_expression();
    char expr_buffer[1024];
    fmt_ast(expr_buffer, expr);
    log_debug("%s\n", expr_buffer);
    return expr;
}

#endif