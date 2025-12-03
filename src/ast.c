#include "ast.h"
#include "log.h"
#include "misc.h"
#include "tokenize.h"

#include <stdarg.h>
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
    case AST_IDENTIFIER:
        return "IDENTIFIER";
    case AST_CONSTANT:
        return "CONSTANT";
    case AST_DECLVAR:
        return "DECLVAR";
    case AST_ASSIGN:
        return "ASSIGN";
    case AST_BINOP:
        return "BINOP";
    default:
        return "UNKNOWN";
    }
}

ast* ast_new(ast_node_t type)
{
    ast* node = (ast*)malloc(sizeof(ast));
    node->type = type;
    return node;
}

void ast_fmt(char* buffer, ast* node)
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
            ast_fmt(temp, node->data.program.body[i]);
            bodies = strjoin(bodies, &cap, temp, i > 0);
            free(temp);
        }

        sprintf(buffer, "{\"type\": \"%s\", \"body\": [%s]}", get_node_type_string(node->type), bodies);
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
            ast_fmt(temp, node->data.body.statements[i]);
            stmts = strjoin(stmts, &cap, temp, i > 0);
            free(temp);
        }

        sprintf(buffer, "{\"type\": \"%s\", \"statements\": [%s]}", get_node_type_string(node->type), stmts);
        free(stmts);
        break;
    }
    case AST_IDENTIFIER:
        sprintf(buffer, "{\"type\": \"%s\", \"name\": \"%s\"}", get_node_type_string(node->type),
                node->data.identifier.name);
        break;
    case AST_CONSTANT:
        sprintf(buffer, "{\"type\": \"%s\", \"value\": %d}", get_node_type_string(node->type),
                node->data.constant.value);
        break;
    case AST_DECLVAR:
        char* ident_buffer = (char*)calloc(1, 512);
        ast_fmt(ident_buffer, node->data.declvar.identifier);
        sprintf(buffer, "{\"type\": \"%s\", \"ident\": %s, \"is_const\": %s}", get_node_type_string(node->type),
                ident_buffer, node->data.declvar.is_const ? "true" : "false");
        free(ident_buffer);
        break;
    case AST_ASSIGN:
    {
        char* lhs_buffer = (char*)malloc(512);
        char* rhs_buffer = (char*)malloc(512);
        if (!lhs_buffer || !rhs_buffer)
        {
            return;
        }
        ast_fmt(lhs_buffer, node->data.assign.lhs);
        ast_fmt(rhs_buffer, node->data.assign.rhs);
        sprintf(buffer, "{\"type\": \"%s\", \"lhs\": %s, \"rhs\": %s}", get_node_type_string(node->type), lhs_buffer,
                rhs_buffer);
        free(lhs_buffer);
        free(rhs_buffer);
        break;
    }
    case AST_BINOP:
        char* lhs_buffer = (char*)malloc(512);
        char* rhs_buffer = (char*)malloc(512);
        ast_fmt(lhs_buffer, node->data.binop.lhs);
        ast_fmt(rhs_buffer, node->data.binop.rhs);
        sprintf(buffer, "{\"type\": \"%s\", \"op\": \"%s\", \"lhs\": %s, \"rhs\": %s}",
                get_node_type_string(node->type), binop_to_string(node->data.binop.op), lhs_buffer, rhs_buffer);
        free(lhs_buffer);
        free(rhs_buffer);
        break;
    }
}

void ast_emit(char* buffer, ast* node)
{
    switch (node->type)
    {
    case AST_CONSTANT:
        sprintf(buffer, "    ldr r0, =%d\n", node->data.constant.value);
        break;
    case AST_PROGRAM:
        char* program_buffer = (char*)calloc(1, 512);
        strcat(program_buffer, "section .text\n");
        strcat(program_buffer, ".global main\n");
        strcat(program_buffer, "main:\n");
        strcat(program_buffer, "    int 0x80\n");
        sprintf(buffer, "%s", program_buffer);
        free(program_buffer);
        break;
    default:
        break;
    }
}

void ast_free(ast* node)
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
                ast_free(node->data.program.body[i]);
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
                ast_free(node->data.body.statements[i]);
            }
        }
        // Free the statement array itself
        free(node->data.body.statements);
        node->data.body.statements = NULL;
        break;
    case AST_DECLVAR:
        ast_free(node->data.declvar.identifier);
        break;
    case AST_IDENTIFIER:
        free(node->data.identifier.name);
        break;
    case AST_ASSIGN:
        ast_free(node->data.assign.lhs);
        ast_free(node->data.assign.rhs);
        break;
    case AST_BINOP:
        ast_free(node->data.binop.lhs);
        ast_free(node->data.binop.rhs);
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

void throw(const char* format, ...)
{
    va_list args;
    va_start(args, format);

    char* buffer = (char*)malloc(1024);
    vsprintf(buffer, format, args);
    log_error("%s", format);
    va_end(args);

    free(buffer);

    exit(1);
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
    log_debug("  Current token: start=%d, end=%d, type=%s, value='%s'", g_cur->start, g_cur->end,
              get_token_type_string(g_cur->type), g_cur->value);
}

ast* parse_constant()
{
    log_debug("Parsing constant...");
    ast* expr = ast_new(AST_CONSTANT);
    expr->data.constant.type = CONST_INT;
    expr->data.constant.value = atoi(g_cur->value);
    next();
    return expr;
}

ast* parse_identifier()
{
    log_debug("Parsing identifier...");
    ast* expr = ast_new(AST_IDENTIFIER);
    expr->data.identifier.name = strdup(g_cur->value);
    next();
    return expr;
}

ast* parse_binop()
{
    log_debug("Parsing binary operation...");
    ast* expr = ast_new(AST_BINOP);
    expr->data.binop.lhs = parse_identifier();

    switch (g_cur->type)
    {
    case TOK_ADD:
        expr->data.binop.op = BIN_ADD;
        break;
    case TOK_SUB:
        expr->data.binop.op = BIN_SUB;
        break;
    case TOK_MUL:
        expr->data.binop.op = BIN_MUL;
        break;
    case TOK_DIV:
        expr->data.binop.op = BIN_DIV;
        break;
    default:
        throw("Expected binary operator. Got %s", get_token_type_string(g_cur->type));
    }
    next();

    expr->data.binop.rhs = parse_expression();

    return expr;
}

ast* parse_expression()
{
    log_debug("Parsing expression...");

    // If the current token is a constant and the next token is a semicolon,
    // parse the constant and return.
    // e.g. const a = 5;
    //                ^^
    if (is_constant(g_cur->type) && expect_n(TOK_SEMICOLON, 1))
    {
        return parse_constant();
    }
    // If the current token is an identifier and the next token is a semicolon,
    // parse the identifier and return.
    // e.g. const a = 5;
    //      const b = a;
    //                ^^
    else if (g_cur->type == TOK_IDENTIFIER && expect_n(TOK_SEMICOLON, 1))
    {
        return parse_identifier();
    }
    else
    {
        return parse_binop();
    }
}

ast* parse_assignment()
{
    log_debug("Parsing assignment...");

    ast* expr = ast_new(AST_ASSIGN);

    // Require a valid identifier
    require(TOK_IDENTIFIER);
    ast* ident = ast_new(AST_IDENTIFIER);
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
    log_debug("Parsing new assignment...");

    ast* expr = ast_new(AST_ASSIGN);

    // Assume we're assigning to a new variable.
    require(TOK_DECLVAR);
    ast* declvar = ast_new(AST_DECLVAR);
    bool is_const = strcmp(g_cur->value, "const") == 0;
    declvar->data.declvar.is_const = is_const;
    next();

    // Require a valid identifier
    require(TOK_IDENTIFIER);
    ast* ident = ast_new(AST_IDENTIFIER);
    ident->data.identifier.name = strdup(g_cur->value);
    declvar->data.declvar.identifier = ident;
    expr->data.assign.lhs = declvar;
    next();

    // Require an assignment operator `=`
    require(TOK_ASSIGN);
    next();

    // Assume the only valid variable type is integer.
    expr->data.assign.rhs = parse_expression();

    return expr;
}

/* `stmt = assign | call | declvar | declfunc | declclass` */
ast* parse_statement()
{
    log_debug("Parsing statement...");
    // Parse new assignment if the next token is an
    // identifier and the second-next token is
    // an assignment operator (`=`).
    // <const|let>  <ident>  =
    // 0            1        2
    // ^ current    ^ next   ^ second-next
    if (expect(TOK_DECLVAR))
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

    log_error("Invalid token %s", get_token_type_string(g_cur->type));
    exit(1);
}

ast* parse_body()
{
    log_debug("Parsing body...");

    ast* expr = ast_new(AST_BODY);

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
    log_debug("Parsing program...");

    ast* expr = ast_new(AST_PROGRAM);

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
    log_debug("Tokenizing input...");
    token_t* tokens = calloc(TOKEN_COUNT, sizeof(token_t));
    size_t count = tokenize(buffer, tokens);
    log_debug("Found %d tokens.", count);

#ifdef DEBUG
    for (int i = 0; i < count; i++)
    {
        print_token(&tokens[i]);
    }
#endif

    g_cur = &tokens[0];

    ast* program = parse_program();
    char program_buffer[1024];
    ast_fmt(program_buffer, program);
    log_info("Program: %s\n", program_buffer);

    for (int i = 0; i < count; i++)
    {
        free_token(&tokens[i]);
    }
    free(tokens);

    return program;
}
