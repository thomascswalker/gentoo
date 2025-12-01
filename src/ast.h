#ifndef AST_H
#define AST_H

#include <string.h>

#include "log.h"
#include "tokenize.h"

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

// Base AST Node

struct ast
{
    enum ast_tag type;

    // AST Node data structures

    union data
    {
        struct ast_program
        {
            ast** body; // Array of pointers to ast
            int count;
        } ast_program;
        struct ast_body
        {
            ast** statements; // Array of pointers to ast
            int count;
        } ast_body;
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

static char* strjoin(char* buf, size_t* cap, const char* piece, int prepend_comma)
{
    if (buf == NULL)
    {
        *cap = strlen(piece) + 64;
        buf = (char*)malloc(*cap);
        if (!buf)
        {
            return NULL;
        }
        buf[0] = '\0';
    }

    size_t cur_len = strlen(buf);
    size_t add_len = strlen(piece) + (prepend_comma ? 2 : 0) + 1;
    if (cur_len + add_len > *cap)
    {
        *cap = cur_len + add_len + 256;
        char* tmp = (char*)realloc(buf, *cap);
        if (!tmp)
        {
            return buf;
        }
        buf = tmp;
    }
    if (prepend_comma && cur_len > 0)
    {
        strcat(buf, ", ");
    }
    strcat(buf, piece);
    return buf;
}

void fmt_ast(char* buffer, ast* node)
{
    switch (node->type)
    {
    case AST_PROGRAM:
    {
        size_t cap = 1024;
        char* bodies = (char*)malloc(cap);
        if (!bodies)
        {
            return;
        }
        bodies[0] = '\0';

        for (int i = 0; i < node->data.ast_program.count; i++)
        {
            char* temp = (char*)malloc(1024);
            if (!temp)
            {
                continue;
            }
            fmt_ast(temp, node->data.ast_program.body[i]);
            bodies = strjoin(bodies, &cap, temp, i > 0);
            free(temp);
        }

        sprintf(buffer, "{\"type\": \"program\", \"body\": [%s]}", bodies);
        free(bodies);
        break;
    }
    case AST_BODY:
    {
        size_t cap = 512;
        char* stmts = (char*)malloc(cap);
        if (!stmts)
        {
            return;
        }
        stmts[0] = '\0';

        for (int i = 0; i < node->data.ast_body.count; i++)
        {
            char* temp = (char*)malloc(512);
            if (!temp)
            {
                continue;
            }
            fmt_ast(temp, node->data.ast_body.statements[i]);
            stmts = strjoin(stmts, &cap, temp, i > 0);
            free(temp);
        }

        sprintf(buffer, "{\"type\": \"body\", \"statements\": [%s]}", stmts);
        free(stmts);
        break;
    }
    case AST_IDENTIFIER:
        sprintf(buffer, "{\"type\": \"identifier\", \"name\": \"%s\"}", node->data.ast_identifier.name);
        break;
    case AST_INT:
        sprintf(buffer, "{\"type\": \"int\", \"number\": %d}", node->data.ast_int.number);
        break;
    case AST_ASSIGN:
    {
        char* lhs_buffer = (char*)calloc(1, 512);
        char* rhs_buffer = (char*)calloc(1, 512);
        if (!lhs_buffer || !rhs_buffer)
            return;
        fmt_ast(lhs_buffer, node->data.ast_assign.lhs);
        fmt_ast(rhs_buffer, node->data.ast_assign.rhs);
        sprintf(buffer, "{\"type\": \"assign\", \"lhs\": %s, \"rhs\": %s}", lhs_buffer, rhs_buffer);
        free(lhs_buffer);
        free(rhs_buffer);
        break;
    }
    case AST_ADD:
    case AST_MUL:
        sprintf(buffer, "{\"type\": \"op\"}");
        break;
    }
}

ast* new_ast()
{
    return (ast*)calloc(1, sizeof(ast));
}

void free_ast(ast* node)
{
    if (!node)
    {
        return;
    }

    // Free sub-nodes
    switch (node->type)
    {
    case AST_PROGRAM:
    {
        if (node->data.ast_program.body)
        {
            for (int i = 0; i < node->data.ast_program.count; i++)
            {
                free_ast(node->data.ast_program.body[i]);
            }
            free(node->data.ast_program.body);
        }
        break;
    }
    case AST_BODY:
    {
        if (node->data.ast_body.statements)
        {
            for (int i = 0; i < node->data.ast_body.count; i++)
            {
                free_ast(node->data.ast_body.statements[i]);
            }
            free(node->data.ast_body.statements);
        }
        break;
    }
    case AST_IDENTIFIER:
    {
        free(node->data.ast_identifier.name);
        break;
    }
    case AST_ASSIGN:
    {
        free_ast(node->data.ast_assign.lhs);
        free_ast(node->data.ast_assign.rhs);
        break;
    }
    default:
        break;
    }

    // Free the node itself
    free(node);
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

bool can_continue()
{
    return g_tcur != NULL && g_tcur->type != 0;
}

void next()
{
    g_tcur++;
    log_debug("  Current token: pos=%d, type=%s, value='%s'", g_tcur->pos, get_token_type_string(g_tcur->type),
              g_tcur->value);
}

ast* parse_assignment()
{
    log_info("Parsing assignment...");

    ast* expr = new_ast();
    expr->type = AST_ASSIGN;

    // Ensure the name is present.
    require(TOK_NAME);

    // Parse and consume the name
    ast* lhs = new_ast();
    lhs->type = AST_IDENTIFIER;
    // Duplicate the token value so the AST owns its own copy
    lhs->data.ast_identifier.name = strdup(g_tcur->value);
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

    next();

    return expr;
}

ast* parse_statement()
{
    log_info("Parsing statement...");

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
    log_info("Parsing body...");

    ast* expr = new_ast();
    expr->type = AST_BODY;

    ast_body* body = &expr->data.ast_body;
    body->statements = calloc(32, sizeof(ast*));

    body->count = 0;
    while (can_continue())
    {
        body->statements[body->count] = parse_statement();
        body->count++;

        require(TOK_SEMICOLON);
        next();
    }

    return expr;
}

ast* parse_program()
{
    log_info("Parsing program...");

    ast* expr = new_ast();
    expr->type = AST_PROGRAM;

    ast_program* program = &expr->data.ast_program;
    program->body = calloc(32, sizeof(ast*));

    // Parse each top-level body
    program->count = 0;
    while (can_continue())
    {
        program->body[program->count] = parse_body();
        program->count++;
    }

    return expr;
}

ast* parse()
{
    // Tokenize the string buffer, returning an array of tokens.
    // - Assuming `g_lbuf` and `g_lpos` are initialized elsewhere.
    // - Assuming a maximum of `TOKEN_COUNT` tokens.
    log_info("Tokenizing input...");
    token_t* tokens = calloc(TOKEN_COUNT, sizeof(token_t));
    size_t count = tokenize(tokens);
    log_info("Found %d tokens.", count);

#ifdef DEBUG
    for (int i = 0; i < count; i++)
    {
        print_token(&tokens[i]);
    }
#endif

    // Start with the first token.
    g_tcur = &tokens[0];

    // Construct an abstract syntax tree from the token array.
    ast* program = parse_program();
    char program_buffer[1024];
    fmt_ast(program_buffer, program);
    log_info("Program: %s\n", program_buffer);

    // Once all AST nodes are constructed, free per-token value buffers
    // and then free the token array itself.
    for (int i = 0; i < count; i++)
    {
        free_token(&tokens[i]);
    }
    free(tokens);

    // Return the root-level program AST node.
    return program;
}

#endif