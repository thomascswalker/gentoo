#include "ast.h"
#include "log.h"
#include "misc.h"
#include "tokenize.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Track the current token
static token_t* g_cur = NULL;

char* get_node_type_string(ast_node_t type)
{
    switch (type)
    {
    case AST_PROGRAM:
        return "PROGRAM";
    case AST_BODY:
        return "BODY";
    case AST_STMT:
        return "STATEMENT";
    case AST_EXPR:
        return "EXPRESSION";
    case AST_IDENTIFIER:
        return "IDENTIFIER";
    case AST_CONSTANT:
        return "CONSTANT";
    case AST_VARDECL:
        return "VARDECL";
    case AST_ASSIGN:
        return "ASSIGN";
    case AST_BINOP:
        return "BINOP";
    default:
        return "UNKNOWN";
    }
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

        for (int i = 0; i < node->data.program.count; i++)
        {
            char* temp = (char*)malloc(1024);
            if (!temp)
            {
                continue;
            }
            fmt_ast(temp, node->data.program.body[i]);
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

        for (int i = 0; i < node->data.body.count; i++)
        {
            char* temp = (char*)malloc(512);
            if (!temp)
            {
                continue;
            }
            fmt_ast(temp, node->data.body.statements[i]);
            stmts = strjoin(stmts, &cap, temp, i > 0);
            free(temp);
        }

        sprintf(buffer, "{\"type\": \"body\", \"statements\": [%s]}", stmts);
        free(stmts);
        break;
    }
    case AST_IDENTIFIER:
        sprintf(buffer, "{\"type\": \"identifier\", \"name\": \"%s\"}", node->data.identifier.name);
        break;
    case AST_CONSTANT:
        sprintf(buffer, "{\"type\": \"constant\", \"value\": %d}", node->data.constant.value);
        break;
    case AST_VARDECL:
        char* ident_buffer = (char*)calloc(1, 512);
        fmt_ast(ident_buffer, node->data.vardecl.identifier);
        sprintf(buffer, "{\"type\": \"vardecl\", \"ident\": %s, \"is_const\": %s}", ident_buffer,
                node->data.vardecl.is_const ? "true" : "false");
        free(ident_buffer);
        break;
    case AST_ASSIGN:
    {
        char* lhs_buffer = (char*)calloc(1, 512);
        char* rhs_buffer = (char*)calloc(1, 512);
        if (!lhs_buffer || !rhs_buffer)
        {
            return;
        }
        fmt_ast(lhs_buffer, node->data.assign.lhs);
        fmt_ast(rhs_buffer, node->data.assign.rhs);
        sprintf(buffer, "{\"type\": \"assign\", \"lhs\": %s, \"rhs\": %s}", lhs_buffer, rhs_buffer);
        free(lhs_buffer);
        free(rhs_buffer);
        break;
    }
    case AST_BINOP:
        sprintf(buffer, "{\"type\": \"op\"}");
        break;
    }
}

ast* new_ast(ast_node_t type)
{
    ast* node = (ast*)malloc(sizeof(ast));
    node->type = type;
    return node;
}

void free_ast(ast* node)
{
    if (!node)
    {
        return;
    }

    log_debug("Freeing %s", get_node_type_string(node->type));

    switch (node->type)
    {
    case AST_PROGRAM:
        if (node->data.program.body)
        {
            // Free each body in the array
            for (int i = 0; i < node->data.program.count; i++)
            {
                free_ast(node->data.program.body[i]);
            }
        }
        // Free the body array itself
        free(node->data.program.body);
        node->data.program.body = NULL;
        break;
    case AST_BODY:
        if (node->data.body.statements)
        {
            // Free each statement in the array
            for (int i = 0; i < node->data.body.count; i++)
            {
                free_ast(node->data.body.statements[i]);
            }
        }
        // Free the statement array itself
        free(node->data.body.statements);
        node->data.body.statements = NULL;
        break;
    case AST_VARDECL:
        free_ast(node->data.vardecl.identifier);
        break;
    case AST_IDENTIFIER:
        free(node->data.identifier.name);
        break;
    case AST_ASSIGN:
        free_ast(node->data.assign.lhs);
        free_ast(node->data.assign.rhs);
        break;
    default:
        break;
    }

    free(node);
}

bool expect(token_type_t type)
{
    return g_cur->type == type;
}

bool expect_n(token_type_t type, size_t offset)
{
    return (g_cur + offset)->type == type;
}

void require(token_type_t type)
{
    log_debug("Requiring %s...", get_token_type_string(type));
    if (!expect(type))
    {
        log_error("Expected token %s, got %s.", get_token_type_string(type), get_token_type_string(g_cur->type));
        exit(1);
    }
    log_debug("Found %s", get_token_type_string(g_cur->type));
}

void require_n(token_type_t type, size_t offset)
{
    log_debug("Requiring %s at offset %d...", get_token_type_string(type), offset);
    if (!expect_n(type, offset))
    {
        log_error("Expected token %s at offset %d, got %s.", get_token_type_string(type), offset,
                  get_token_type_string((g_cur + offset)->type));
        exit(1);
    }
    log_debug("Found %s", get_token_type_string((g_cur + offset)->type));
}

bool can_continue()
{
    return g_cur != NULL && g_cur->type != 0;
}

void next()
{
    g_cur++;
    log_debug("  Current token: pos=%d, type=%s, value='%s'", g_cur->pos, get_token_type_string(g_cur->type),
              g_cur->value);
}

ast* parse_expression()
{
    log_info("Parsing expression...");
    ast* expr = new_ast(AST_CONSTANT);
    expr->data.constant.value = atoi(g_cur->value);
    next();

    return expr;
}

ast* parse_assignment()
{
    log_info("Parsing assignment...");

    ast* expr = new_ast(AST_ASSIGN);

    // Require a valid identifier
    require(TOK_IDENTIFIER);
    ast* ident = new_ast(AST_IDENTIFIER);
    ident->data.identifier.name = strdup(g_cur->value);
    expr->data.assign.lhs = ident;
    next();

    // Require an assignment operator `=`
    require(TOK_ASSIGN);
    next();

    // Assume the only valid variable type is integer.
    expr->data.assign.rhs = parse_expression();

    return expr;
}

ast* parse_new_assignment()
{
    log_info("Parsing new assignment...");

    ast* expr = new_ast(AST_ASSIGN);

    // Assume we're assigning to a new variable.
    require(TOK_VARDECL);
    ast* vardecl = new_ast(AST_VARDECL);
    bool is_const = strcmp(g_cur->value, "const") == 0;
    vardecl->data.vardecl.is_const = is_const;
    next();

    // Require a valid identifier
    require(TOK_IDENTIFIER);
    ast* ident = new_ast(AST_IDENTIFIER);
    ident->data.identifier.name = strdup(g_cur->value);
    vardecl->data.vardecl.identifier = ident;
    expr->data.assign.lhs = vardecl;
    next();

    // Require an assignment operator `=`
    require(TOK_ASSIGN);
    next();

    // Assume the only valid variable type is integer.
    expr->data.assign.rhs = parse_expression();

    return expr;
}

ast* parse_statement()
{
    log_info("Parsing statement...");

    // Parse new assignment if the next token is an
    // identifier and the second-next token is
    // an assignment operator (`=`).
    // <const|let>  <ident>  =
    // 0            1        2
    // ^ current    ^ next   ^ second-next
    if (expect(TOK_VARDECL))
    {
        require_n(TOK_IDENTIFIER, 1);
        require_n(TOK_ASSIGN, 2);
        return parse_new_assignment();
    }

    // Parse existing assignment if the next token is
    // an assignment operator (`=`).
    // <ident>     =
    // 0           1
    // ^ current   ^ next
    if (expect(TOK_IDENTIFIER))
    {
        require_n(TOK_ASSIGN, 1);
        return parse_assignment();
    }
    log_error("Invalid token: %s", get_token_type_string(g_cur->type));
    exit(1);
}

ast* parse_body()
{
    log_info("Parsing body...");

    ast* expr = new_ast(AST_BODY);

    struct ast_body* body = &expr->data.body;

    // Assume a maximum of 32 statements.
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

    ast* expr = new_ast(AST_PROGRAM);

    // Assume a maximum of 32 bodies.
    struct ast_program* program = &expr->data.program;
    program->body = calloc(32, sizeof(ast*));

    program->count = 0;
    while (can_continue())
    {
        program->body[program->count] = parse_body();
        program->count++;
    }

    return expr;
}

ast* parse(char* buffer)
{
    log_info("Tokenizing input...");
    token_t* tokens = calloc(TOKEN_COUNT, sizeof(token_t));
    size_t count = tokenize(buffer, tokens);
    log_info("Found %d tokens.", count);

#ifdef DEBUG
    for (int i = 0; i < count; i++)
    {
        print_token(&tokens[i]);
    }
#endif

    g_cur = &tokens[0];

    ast* program = parse_program();
    char program_buffer[1024];
    fmt_ast(program_buffer, program);
    log_info("Program: %s\n", program_buffer);

    for (int i = 0; i < count; i++)
    {
        free_token(&tokens[i]);
    }
    free(tokens);

    return program;
}
