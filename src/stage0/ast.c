#include "ast.h"
#include "asm.h"
#include "buffer.h"
#include "codegen.h"
#include "log.h"
#include "strings.h"
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
    case AST_IF:
        return "IF";
    case AST_TYPE:
        return "TYPE";
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
    case BIN_GT:
        return "GT";
    case BIN_LT:
        return "LT";
    default:
        return "UNKNOWN";
    }
}

char* ast_value_type_to_string(ast_value_type_t type)
{
    switch (type)
    {
    case TYPE_VOID:
        return "void";
    case TYPE_BOOL:
        return "bool";
    case TYPE_INT:
        return "int";
    case TYPE_STRING:
        return "string";
    }
    return NULL;
}

ast* ast_new(ast_node_t type)
{
    ast* node = (ast*)malloc(sizeof(ast));
    node->type = type;
    return node;
}

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
        if (n->data.constant.type == TYPE_STRING)
        {
            const char* value = n->data.constant.string_value
                                    ? n->data.constant.string_value
                                    : "null";
            buffer_printf(out, "{\"type\": \"%s\", \"value\": %s}",
                          ast_to_string(n->type), value);
        }
        else
        {
            buffer_printf(out, "{\"type\": \"%s\", \"value\": %d}",
                          ast_to_string(n->type), n->data.constant.value);
        }
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
        buffer_printf(out, ", \"ret_type\": ");
        ast_fmt_buf(n->data.declfn.ret_type, out);
        buffer_printf(out, ", \"block\": ");
        ast_fmt_buf(n->data.declfn.block, out);
        buffer_puts(out, "}");
        break;
    case AST_TYPE:
        buffer_printf(out, "{\"type\": \"%s\", \"kind\": \"%s\"}",
                      ast_to_string(n->type),
                      ast_value_type_to_string(n->data.type.type));
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
        if (n->data.ret.node)
        {
            ast_fmt_buf(n->data.ret.node, out);
        }
        else
        {
            buffer_puts(out, "null");
        }
        buffer_puts(out, "}");
        break;
    case AST_IF:
        buffer_puts(out, "{\"type\": \"IF\", \"cond\": ");
        ast_fmt_buf(n->data.if_stmt.condition, out);
        buffer_puts(out, ", \"then\": ");
        ast_fmt_buf(n->data.if_stmt.then_branch, out);
        buffer_puts(out, ", \"else\": ");
        if (n->data.if_stmt.else_branch)
        {
            ast_fmt_buf(n->data.if_stmt.else_branch, out);
        }
        else
        {
            buffer_puts(out, "null");
        }
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
    case AST_CONSTANT:
        if (node->data.constant.type == TYPE_STRING &&
            node->data.constant.string_value)
        {
            free(node->data.constant.string_value);
            node->data.constant.string_value = NULL;
        }
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
    case AST_IF:
        ast_free(node->data.if_stmt.condition);
        ast_free(node->data.if_stmt.then_branch);
        if (node->data.if_stmt.else_branch)
        {
            ast_free(node->data.if_stmt.else_branch);
        }
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

bool expect_either(token_type_t type_a, token_type_t type_b)
{
    return g_cur->type == type_a || g_cur->type == type_b;
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

void require_either(token_type_t type_a, token_type_t type_b)
{
    log_debug("Requiring either %s or %s...", get_token_type_string(type_a),
              get_token_type_string(type_b));
    if (!expect_either(type_a, type_b))
    {
        g_error_token = g_cur;
        log_error("Expected token %s or %s, got %s.",
                  get_token_type_string(type_a), get_token_type_string(type_b),
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

/* Move to the next token to parse. */
void next()
{
    g_cur++;
    log_debug("  Current token: start=%d, end=%d, type=%s, value='%s'",
              g_cur->start, g_cur->end, get_token_type_string(g_cur->type),
              g_cur->value);
}

/**
 * Require the specified token type, consuming it and moving to the next
 * token to parse.
 */
void consume(token_type_t type)
{
    require(type);
    next();
}

/**
 * Can we continue parsing? Is the current token either NULL or the current
 * token's type is NULL?
 */
bool can_continue()
{
    return g_cur != NULL && g_cur->type != 0;
}

/* Parse a constant (literal) value (5, "string", 4.3234, etc.). */
ast* parse_constant()
{
    log_debug("Parsing constant...");
    ast* expr = ast_new(AST_CONSTANT);
    expr->data.constant.value = 0;
    expr->data.constant.string_value = NULL;
    expr->start = g_cur->start;
    expr->end = g_cur->end;

    if (expect(TOK_NUMBER))
    {
        expr->data.constant.type = TYPE_INT;
        expr->data.constant.value = atoi(g_cur->value);
    }
    else if (expect(TOK_TRUE) || expect(TOK_FALSE))
    {
        expr->data.constant.type = TYPE_BOOL;
        expr->data.constant.value = (expect(TOK_TRUE)) ? 1 : 0;
    }
    else if (expect(TOK_STRING))
    {
        expr->data.constant.type = TYPE_STRING;
        expr->data.constant.string_value = strdup(g_cur->value);
        if (!expr->data.constant.string_value)
        {
            log_error("Out of memory duplicating string literal.");
            exit(1);
        }
    }
    else
    {
        log_error("Unsupported constant token: %s",
                  get_token_type_string(g_cur->type));
        exit(1);
    }
    next();
    return expr;
}

/* Parse an identifier: `let name <== ...` or `... 5 * name <== ...`*/
ast* parse_identifier()
{
    log_debug("Parsing identifier...");
    require(TOK_IDENTIFIER);
    ast* expr = ast_new(AST_IDENTIFIER);
    expr->data.identifier.name = strdup(g_cur->value);
    next();
    return expr;
}

/* Parses a type, matching exactly a value within `TYPES`.*/
ast* parse_type()
{
    log_debug("Parsing type...");

    for (size_t i = 0; i < TYPE_COUNT; i++)
    {
        if (streq(g_cur->value, TYPES[i]))
        {
            next();
            ast* type = ast_new(AST_TYPE);
            type->data.type.type = (ast_value_type_t)i;
            return type;
        }
    }
    size_t capacity = 0;
    char* types_list = NULL;
    for (size_t i = 0; i < TYPE_COUNT; i++)
    {
        types_list = strjoin(types_list, &capacity, TYPES[i], i > 0);
    }
    log_error("Invalid type '%s', wanted one of %s.", g_cur->value,
              types_list ? types_list : "<unknown>");
    free(types_list);
    exit(1);
    return 0;
}

/**
 * Parse one of: constants (literals), calls, identifiers, parenthesis
 * expressions.
 */
ast* parse_factor()
{
    // Parse a constant, e.g. `1` or `true` or `"string"`
    if (is_constant(g_cur->type))
    {
        return parse_constant();
    }
    // Parse a function call: `func()`
    if (expect(TOK_IDENTIFIER) && expect_n(TOK_L_PAREN, 1))
    {
        return parse_call();
    }
    // Parse a normal identifier: `name`
    if (expect(TOK_IDENTIFIER))
    {
        return parse_identifier();
    }
    // Parse a parenthesis-wrapped expression: `( ... )`
    if (expect(TOK_L_PAREN))
    {
        next();
        ast* expr = parse_expression();
        consume(TOK_R_PAREN);
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

static ast* parse_addition_chain()
{
    ast* node = parse_term();
    while (g_cur->type == TOK_ADD || g_cur->type == TOK_SUB)
    {
        ast* bin = ast_new(AST_BINOP);
        bin->data.binop.lhs = node;
        bin->data.binop.op = (g_cur->type == TOK_ADD) ? BIN_ADD : BIN_SUB;
        next();
        bin->data.binop.rhs = parse_term();
        node = bin;
    }
    return node;
}

static ast* parse_comparison_chain()
{
    ast* node = parse_addition_chain();
    while (g_cur->type == TOK_GT || g_cur->type == TOK_LT)
    {
        ast* bin = ast_new(AST_BINOP);
        bin->data.binop.lhs = node;
        bin->data.binop.op = (g_cur->type == TOK_GT) ? BIN_GT : BIN_LT;
        next();
        bin->data.binop.rhs = parse_addition_chain();
        node = bin;
    }
    return node;
}

static ast* parse_equality_chain()
{
    ast* node = parse_comparison_chain();
    while (g_cur->type == TOK_EQ)
    {
        ast* bin = ast_new(AST_BINOP);
        bin->data.binop.lhs = node;
        bin->data.binop.op = BIN_EQ;
        next();
        bin->data.binop.rhs = parse_comparison_chain();
        node = bin;
    }
    return node;
}

ast* parse_expression()
{
    log_debug("Parsing expression...");
    return parse_equality_chain();
}

ast* parse_assignment()
{
    log_debug("Parsing assignment...");

    ast* expr = ast_new(AST_ASSIGN);

    // Require a valid identifier
    expr->data.assign.lhs = parse_identifier();

    // Require an assignment operator `=`
    consume(TOK_ASSIGN);

    // Assume the only valid variable type is integer.
    expr->data.assign.rhs = parse_expression();

    consume(TOK_SEMICOLON);

    return expr;
}

ast* parse_call()
{
    log_debug("Parsing call...");

    ast* expr = ast_new(AST_CALL);
    expr->data.call.args = NULL;
    expr->data.call.count = 0;
    expr->data.call.identifier = parse_identifier();

    consume(TOK_L_PAREN);

    size_t capacity = 0;

    // Parse arguments if the parenthesis are not immediately closed
    // e.g. ( ... ) as opposed to ()
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

            if (expect(TOK_COMMA))
            {
                next();
                continue;
            }
            break;
        }
    }

    consume(TOK_R_PAREN);

    return expr;
}

/* */
ast* parse_declvar()
{
    log_debug("Parsing new assignment...");

    ast* expr = ast_new(AST_ASSIGN);

    // Assume we're assigning to a new variable.
    consume(TOK_DECLVAR);
    ast* declvar = ast_new(AST_DECLVAR);
    declvar->data.declvar.is_const = false;
    expr->data.assign.lhs = declvar;

    // Require a valid identifier
    ast* ident = parse_identifier();
    declvar->data.declvar.identifier = ident;

    // Require an assignment operator `=`
    consume(TOK_ASSIGN);

    // Assume the only valid variable type is integer.
    expr->data.assign.rhs = parse_expression();

    consume(TOK_SEMICOLON);

    return expr;
}

/* `fn identifier(arguments...): type => { ... }` */
ast* parse_declfn()
{
    log_debug("Parsing declfn...");

    consume(TOK_DECLFN);

    ast* expr = ast_new(AST_DECLFN);
    expr->data.declfn.args = NULL;
    expr->data.declfn.count = 0;
    expr->data.declfn.identifier = parse_identifier();

    consume(TOK_L_PAREN);

    // If the next token is not a closing parenthesis `)`,
    // parse arguments until we hit a closing parenthesis.
    size_t capacity = 0;
    if (!expect(TOK_R_PAREN))
    {
        capacity = 4;
        expr->data.declfn.args = (ast**)calloc(capacity, sizeof(ast*));
        while (true)
        {
            if (expr->data.declfn.count >= capacity)
            {
                capacity *= 2;
                expr->data.declfn.args = (ast**)realloc(
                    expr->data.declfn.args, capacity * sizeof(ast*));
            }
            expr->data.declfn.args[expr->data.declfn.count++] =
                parse_identifier();

            if (expect(TOK_COMMA))
            {
                next();
                continue;
            }
            break;
        }
    }

    consume(TOK_R_PAREN);
    consume(TOK_COLON);
    require(TOK_IDENTIFIER);
    expr->data.declfn.ret_type = parse_type();
    consume(TOK_ARROW);

    expr->data.declfn.block = parse_block();

    return expr;
}

/* `if (condition) { ... } else { ... }` */
ast* parse_if()
{
    log_debug("Parsing if statement...");

    consume(TOK_IF);

    ast* stmt = ast_new(AST_IF);
    consume(TOK_L_PAREN);
    stmt->data.if_stmt.condition = parse_expression();
    consume(TOK_R_PAREN);

    // Always parse THEN
    stmt->data.if_stmt.then_branch = parse_block();
    // ELSE is optional
    stmt->data.if_stmt.else_branch = NULL;

    if (expect(TOK_ELSE))
    {
        next();
        if (expect(TOK_IF))
        {
            stmt->data.if_stmt.else_branch = parse_if();
        }
        else
        {
            stmt->data.if_stmt.else_branch = parse_block();
        }
    }

    return stmt;
}

/**
 * `for (identifier in expression) { ... }`
 */
ast* parse_for()
{
    log_debug("Parsing for statement...");

    consume(TOK_FOR);

    ast* stmt = ast_new(AST_FOR);
    consume(TOK_L_PAREN);
    stmt->data.for_stmt.identifier = parse_identifier();
    consume(TOK_IN);
    stmt->data.for_stmt.expr = parse_expression();
    consume(TOK_R_PAREN);
    stmt->data.for_stmt.block = parse_block();

    return stmt;
}

ast* parse_ret()
{
    consume(TOK_RETURN);

    ast* expr = ast_new(AST_RETURN);
    if (!expect(TOK_SEMICOLON))
    {
        expr->data.ret.node = parse_expression();
    }
    else
    {
        expr->data.ret.node = NULL;
    }
    consume(TOK_SEMICOLON);

    return expr;
}

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

    if (expect(TOK_IF))
    {
        return parse_if();
    }

    if (expect(TOK_FOR))
    {
        return parse_for();
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
            consume(TOK_SEMICOLON);
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

    consume(TOK_L_BRACKET);

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

    consume(TOK_R_BRACKET);

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
