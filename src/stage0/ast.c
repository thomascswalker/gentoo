#include "ast.h"
#include "asm.h"
#include "buffer.h"
#include "codegen.h"
#include "log.h"
#include "misc.h"
#include "tokenize.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Track the current token
static char* g_raw = NULL;
static token_t* g_cur = NULL;
static token_t* g_error_token = NULL;
static buffer_t* ast_buffer;

char* ast_to_string(ast_node_t type)
{
    switch (type)
    {
    case AST_PROGRAM:
        return "PROGRAM";
    case AST_BODY:
        return "BODY";
    case AST_BLOCK:
        return "AST_BLOCK";
    case AST_IDENTIFIER:
        return "IDENTIFIER";
    case AST_CONSTANT:
        return "CONSTANT";
    case AST_STRING:
        return "STRING";
    case AST_DECLVAR:
        return "DECLVAR";
    case AST_DECLFN:
        return "DECLFN";
    case AST_CALL:
        return "CALL";
    case AST_ASSIGN:
        return "ASSIGN";
    case AST_BINOP:
        return "BINOP";
    case AST_RETURN:
        return "RETURN";
    default:
        return "UNKNOWN";
    }
}

char* binop_to_string(ast_binop_t op)
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

ast* ast_new(ast_node_t type)
{
    ast* node = (ast*)malloc(sizeof(ast));
    node->type = type;
    return node;
}

// Implementation
void ast_fmt_buf(ast* n, buffer_t* out)
{
    switch (n->type)
    {
    case AST_PROGRAM:
    {
        buffer_printf(out, "{\"type\": \"%s\", \"body\": [",
                      ast_to_string(n->type));
        for (int i = 0; i < n->data.program.count; i++)
        {
            if (i > 0)
                buffer_puts(out, ", ");
            ast_fmt_buf(n->data.program.body[i], out);
        }
        buffer_puts(out, "]}");
        break;
    }
    case AST_BODY:
    case AST_BLOCK:
    {
        buffer_printf(out, "{\"type\": \"%s\", \"statements\": [",
                      ast_to_string(n->type));
        int count =
            (n->type == AST_BODY) ? n->data.body.count : n->data.block.count;
        ast** stmts = (n->type == AST_BODY) ? n->data.body.statements
                                            : n->data.block.statements;
        for (int i = 0; i < count; i++)
        {
            if (i > 0)
                buffer_puts(out, ", ");
            ast_fmt_buf(stmts[i], out);
        }
        buffer_puts(out, "]}");
        break;
    }
    case AST_IDENTIFIER:
        buffer_printf(out, "{\"type\": \"%s\", \"name\": \"%s\"}",
                      ast_to_string(n->type), n->data.identifier.name);
        break;
    case AST_CONSTANT:
        buffer_printf(out, "{\"type\": \"%s\", \"value\": %d}",
                      ast_to_string(n->type), n->data.constant.value);
        break;
    case AST_STRING:
        buffer_printf(out, "{\"type\": \"%s\", \"value\": %s}",
                      ast_to_string(n->type), n->data.string.value);
        break;
    case AST_DECLVAR:
        buffer_printf(out,
                      "{\"type\": \"%s\", \"ident\": ", ast_to_string(n->type));
        ast_fmt_buf(n->data.declvar.identifier, out);
        buffer_printf(out, ", \"is_const\": %s}",
                      n->data.declvar.is_const ? "true" : "false");
        break;
    case AST_DECLFN:
        buffer_printf(out,
                      "{\"type\": \"%s\", \"ident\": ", ast_to_string(n->type));
        ast_fmt_buf(n->data.declfn.identifier, out);
        buffer_printf(out, ", \"block\": ");
        ast_fmt_buf(n->data.declfn.block, out);
        buffer_puts(out, "}");
        break;
    case AST_CALL:
    {
        buffer_printf(out, "{\"type\": \"%s\", \"ident\": \"%s\", \"args\": [",
                      ast_to_string(n->type),
                      n->data.call.identifier->data.identifier.name);
        for (size_t i = 0; i < n->data.call.count; i++)
        {
            if (i > 0)
            {
                buffer_puts(out, ", ");
            }
            ast_fmt_buf(n->data.call.args[i], out);
        }
        buffer_puts(out, "]}");
        break;
    }
    case AST_ASSIGN:
        buffer_puts(out, "{\"type\": \"ASSIGN\", \"lhs\": ");
        ast_fmt_buf(n->data.assign.lhs, out);
        buffer_puts(out, ", \"rhs\": ");
        ast_fmt_buf(n->data.assign.rhs, out);
        buffer_puts(out, "}");
        break;
    case AST_BINOP:
        buffer_printf(out, "{\"type\": \"BINOP\", \"op\": \"%s\", \"lhs\": ",
                      binop_to_string(n->data.binop.op));
        ast_fmt_buf(n->data.binop.lhs, out);
        buffer_puts(out, ", \"rhs\": ");
        ast_fmt_buf(n->data.binop.rhs, out);
        buffer_puts(out, "}");
        break;
    case AST_RETURN:
        buffer_puts(out, "{\"type\": \"RETURN\", \"expr\": ");
        ast_fmt_buf(n->data.ret.node, out);
        buffer_puts(out, "}");
        break;
    default:
        buffer_printf(out, "{\"type\": \"%s\"}", ast_to_string(n->type));
        break;
    }
}

void ast_fmt(char* buffer, ast* node)
{
    // Helper that writes into a temporary buffer_t and copies result.
    buffer_t* buf = buffer_new();

    // Build into buf and copy to caller-provided buffer
    ast_fmt_buf(node, buf);
    // Ensure null-terminated and copy up to caller's buffer size (assume 4096)
    size_t copy_len = buf->size < 4095 ? buf->size : 4095;
    memcpy(buffer, buf->data, copy_len);
    buffer[copy_len] = '\0';
    buffer_free(buf);
}

char* ast_codegen(ast* node, codegen_type_t type)
{
    if (node->type != AST_PROGRAM)
    {
        log_error("Expected AST Program Node, got %d.", node->type);
        log_context();
        exit(1);
    }

    // Get the emitter for the specified architecture
    g_codegen = codegen_new(type);
    if (g_codegen == NULL)
    {
        codegen_free(g_codegen);
        log_error("Codegen is not valid.");
        log_context();
        exit(1);
    }
    log_info("Generating %s assembly...",
             codegen_type_to_string(g_codegen->type));

    g_codegen->ops.program(node);
    log_info("Completed emission.");

    buffer_t* code_buffer = buffer_new();
    if (g_codegen->global && g_codegen->global->size > 0)
    {
        buffer_puts(code_buffer, g_codegen->global->data);
    }
    if (g_codegen->data && g_codegen->data->size > 0)
    {
        buffer_puts(code_buffer, g_codegen->data->data);
    }
    if (g_codegen->bss && g_codegen->bss->size > 0)
    {
        buffer_puts(code_buffer, g_codegen->bss->data);
    }
    if (g_codegen->text && g_codegen->text->size > 0)
    {
        buffer_puts(code_buffer, g_codegen->text->data);
    }

    char* code = strdup(code_buffer->data);
    buffer_free(code_buffer);
    codegen_free(g_codegen);
    return code;
}

void ast_free(ast* node)
{
    if (!node)
    {
        return;
    }

    log_debug("Freeing %s", ast_to_string(node->type));

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
    case AST_BLOCK:
        if (node->data.block.statements)
        {
            for (int i = 0; i < node->data.block.count; i++)
            {
                ast_free(node->data.block.statements[i]);
            }
        }
        free(node->data.block.statements);
        node->data.block.statements = NULL;
        break;
    case AST_DECLVAR:
        ast_free(node->data.declvar.identifier);
        break;
    case AST_DECLFN:
        ast_free(node->data.declfn.identifier);
        ast_free(node->data.declfn.block);
        break;
    case AST_IDENTIFIER:
        free(node->data.identifier.name);
        break;
    case AST_STRING:
        free(node->data.string.value);
        break;
    case AST_CALL:
        if (node->data.call.args)
        {
            for (size_t i = 0; i < node->data.call.count; i++)
            {
                ast_free(node->data.call.args[i]);
            }
            free(node->data.call.args);
            node->data.call.args = NULL;
        }
        ast_free(node->data.call.identifier);
        node->data.call.identifier = NULL;
        break;
    case AST_ASSIGN:
        ast_free(node->data.assign.lhs);
        ast_free(node->data.assign.rhs);
        break;
    case AST_BINOP:
        ast_free(node->data.binop.lhs);
        ast_free(node->data.binop.rhs);
        break;
    case AST_RETURN:
        ast_free(node->data.ret.node);
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

bool expect_2(token_type_t type_a, token_type_t type_b)
{
    return g_cur->type == type_a && g_cur->type == type_b;
}

bool expect_n(token_type_t type, size_t offset)
{
    return (g_cur + offset)->type == type;
}

void log_context()
{
    token_t* token = g_error_token ? g_error_token : g_cur;
    if (!token || !g_raw)
    {
        return;
    }

    size_t buffer_len = strlen(g_raw);
    size_t line_start = token->start;
    while (line_start > 0)
    {
        char ch = g_raw[line_start - 1];
        if (ch == '\n' || ch == '\r')
        {
            break;
        }
        line_start--;
    }

    size_t line_end = token->start;
    while (line_end < buffer_len)
    {
        char ch = g_raw[line_end];
        if (ch == '\n' || ch == '\r' || ch == '\0')
        {
            break;
        }
        line_end++;
    }

    size_t len = (line_end > line_start) ? (line_end - line_start) : 0;
    char* line_buf = (char*)calloc(len + 1, 1);
    memcpy(line_buf, g_raw + line_start, len);
    line_buf[len] = '\0';

    size_t caret_column = token->start - line_start;
    size_t prefix_len = strlen("[ERR] - ");
    log_error("%s\n%*c^", line_buf, (int)(caret_column + prefix_len), ' ');
    free(line_buf);
    g_error_token = NULL;
}

void require(token_type_t type)
{
    log_debug("Requiring %s...", get_token_type_string(type));
    if (!expect(type))
    {
        g_error_token = g_cur;
        log_error("Expected token %s, got %s.", get_token_type_string(type),
                  get_token_type_string(g_cur->type));
        log_context();
        exit(1);
    }
    log_debug("Found %s", get_token_type_string(g_cur->type));
}

void require_n(token_type_t type, size_t offset)
{
    log_debug("Requiring %s at offset %d...", get_token_type_string(type),
              offset);
    if (!expect_n(type, offset))
    {
        g_error_token = g_cur + offset;
        log_error("Expected token %s at offset %d, got %s.",
                  get_token_type_string(type), offset,
                  get_token_type_string((g_cur + offset)->type));
        log_context();
        exit(1);
    }
    log_debug("Found %s", get_token_type_string((g_cur + offset)->type));
}

bool can_continue()
{
    return g_cur != NULL && g_cur->type != 0;
}

/* Move to the next token to parse. */
void next()
{
    g_cur++;
    log_debug("  Current token: start=%d, end=%d, type=%s, value='%s'",
              g_cur->start, g_cur->end, get_token_type_string(g_cur->type),
              g_cur->value);
}

/* Parse a constant (literal) value (5, "string", 4.3234, etc.). */
ast* parse_constant()
{
    log_debug("Parsing constant...");
    ast* expr = ast_new(AST_CONSTANT);
    expr->data.constant.type = CONST_INT;
    expr->data.constant.value = atoi(g_cur->value);
    next();
    return expr;
}

/* Parse an identifier: `let name <== ...` or `... 5 * name <== ...`*/
ast* parse_identifier()
{
    log_debug("Parsing identifier...");
    ast* expr = ast_new(AST_IDENTIFIER);
    expr->data.identifier.name = strdup(g_cur->value);
    next();
    return expr;
}

/**
 * Parse a one of the below types.
 *
 *   - Literal (number, string, boolean, etc.)
 *   - Identifier (variable or constant)
 *   - Parenthesis expression: '(' expression ')'
 *   - Unary/prefix operator applied to a factor (e.g., +, -, !)
 */
ast* parse_factor()
{
    if (is_constant(g_cur->type))
    {
        return parse_constant();
    }
    if (g_cur->type == TOK_STRING)
    {
        return parse_string();
    }
    else if (g_cur->type == TOK_IDENTIFIER)
    {
        return parse_identifier();
    }
    else if (g_cur->type == TOK_L_PAREN)
    {
        next();
        ast* expr = parse_expression();
        require(TOK_R_PAREN);
        next();
        return expr;
    }
    log_context();
    log_error("Unexpected token in factor: %s",
              get_token_type_string(g_cur->type));
    exit(1);
    return NULL;
}

ast* parse_term()
{
    ast* node = parse_factor();
    while (g_cur->type == TOK_MUL || g_cur->type == TOK_DIV)
    {
        ast* bin = ast_new(AST_BINOP);
        bin->data.binop.lhs = node;
        if (g_cur->type == TOK_MUL)
        {
            bin->data.binop.op = BIN_MUL;
        }
        else
        {
            bin->data.binop.op = BIN_DIV;
        }
        next();
        bin->data.binop.rhs = parse_factor();
        node = bin;
    }
    return node;
}

ast* parse_string()
{
    ast* str = ast_new(AST_STRING);
    str->data.string.value = strdup(g_cur->value);
    str->start = g_cur->start;
    str->end = g_cur->end;
    next();
    return str;
}

ast* parse_expression()
{
    log_debug("Parsing expression...");

    if (g_cur->type == TOK_IDENTIFIER && expect_n(TOK_L_PAREN, 1))
    {
        return parse_call();
    }
    // Otherwise assume the next token is a term.
    else
    {
        ast* node = parse_term();
        while (g_cur->type == TOK_ADD || g_cur->type == TOK_SUB)
        {
            ast* bin = ast_new(AST_BINOP);
            bin->data.binop.lhs = node;
            if (g_cur->type == TOK_ADD)
            {
                bin->data.binop.op = BIN_ADD;
            }
            else
            {
                bin->data.binop.op = BIN_SUB;
            }
            next();
            bin->data.binop.rhs = parse_term();
            node = bin;
        }
        return node;
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

    require(TOK_SEMICOLON);
    next();

    return expr;
}

ast* parse_call()
{
    log_info("Parsing call...");

    ast* expr = ast_new(AST_CALL);
    expr->data.call.args = NULL;
    expr->data.call.count = 0;

    require(TOK_IDENTIFIER);
    ast* ident = ast_new(AST_IDENTIFIER);
    ident->data.identifier.name = strdup(g_cur->value);
    expr->data.call.identifier = ident;
    next();

    require(TOK_L_PAREN);
    next();

    size_t capacity = 0;
    if (!expect(TOK_R_PAREN))
    {
        capacity = 4;
        expr->data.call.args = (ast**)calloc(capacity, sizeof(ast*));
        while (true)
        {
            if (expr->data.call.count >= capacity)
            {
                capacity *= 2;
                expr->data.call.args = (ast**)realloc(expr->data.call.args,
                                                      capacity * sizeof(ast*));
            }
            expr->data.call.args[expr->data.call.count++] = parse_expression();

            if (g_cur->type == TOK_COMMA)
            {
                next();
                continue;
            }
            break;
        }
    }

    require(TOK_R_PAREN);
    next();

    return expr;
}

ast* parse_declvar()
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

    require(TOK_SEMICOLON);
    next();

    return expr;
}

ast* parse_declfn()
{
    log_debug("Parsing declfn...");
    next(); // Skip `fn`

    ast* expr = ast_new(AST_DECLFN);

    // Parse the function mame
    require(TOK_IDENTIFIER);
    expr->data.declfn.identifier = parse_identifier();

    // // Parse arguments
    require(TOK_L_PAREN);
    next();
    require(TOK_R_PAREN);
    next();

    // // Parse return type
    // require(TOK_COLON);
    // next();

    // // Return type
    // require(TOK_IDENTIFIER);
    // next();

    // Arrow
    require(TOK_ARROW);
    next();

    expr->data.declfn.block = parse_block();

    return expr;
}

/* return = "return" expr*/
ast* parse_ret()
{
    ast* expr = ast_new(AST_RETURN);
    next();
    expr->data.ret.node = parse_expression();

    require(TOK_SEMICOLON);
    next();

    return expr;
}

/* `stmt = assign | call | declvar | declfunc | declclass` */
ast* parse_statement()
{
    log_debug("Parsing statement...");

    if (expect(TOK_DECLFN))
    {
        return parse_declfn();
    }

    // Parse return statements.
    if (expect(TOK_RETURN))
    {
        return parse_ret();
    }

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
        return parse_declvar();
    }

    // Parse existing assignment if the next token is
    // an assignment operator (`=`).
    // <ident>     =
    // 0           1
    // ^ current   ^ next
    if (expect(TOK_IDENTIFIER))
    {
        if (expect_n(TOK_L_PAREN, 1))
        {
            ast* call = parse_call();
            require(TOK_SEMICOLON);
            next();
            return call;
        }

        require_n(TOK_ASSIGN, 1);
        return parse_assignment();
    }

    log_error("Invalid token %s", get_token_type_string(g_cur->type));
    exit(1);
}

ast* parse_block()
{
    log_debug("Parsing block...");

    require(TOK_L_BRACKET);
    next();

    ast* expr = ast_new(AST_BLOCK);
    struct ast_block* block = &expr->data.block;

    // Assume a maximum of 32 statements.
    block->statements = calloc(32, sizeof(ast*));

    block->count = 0;
    while (!expect(TOK_R_BRACKET))
    {
        block->statements[block->count] = parse_statement();
        block->count++;
    }

    require(TOK_R_BRACKET);
    next();

    return expr;
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
    }

    return expr;
}

ast* parse_program()
{
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
    g_raw = strdup(buffer);

    log_debug("Tokenizing input...");
    token_t* tokens = calloc(TOKEN_COUNT, sizeof(token_t));
    size_t count = tokenize(buffer, tokens);
    log_debug("Found %d tokens.", count);

#ifdef _DEBUG
    for (int i = 0; i < count; i++)
    {
        print_token(&tokens[i]);
    }
#endif

    g_cur = &tokens[0];

    ast* program = parse_program();
    char* ast_buffer = (char*)calloc(1, 4096);
    ast_fmt(ast_buffer, program);
    log_debug("%s", ast_buffer);
    free(ast_buffer);

    for (int i = 0; i < count; i++)
    {
        free_token(&tokens[i]);
    }
    free(tokens);

    free(g_raw);
    g_raw = NULL;
    g_cur = NULL;
    g_error_token = NULL;

    return program;
}
