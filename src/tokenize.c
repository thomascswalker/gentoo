#include "tokenize.h"
#include "log.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int g_pos = 0;
static char* g_buf = NULL;

token_t* new_token()
{
    token_t* token = calloc(1, sizeof(token_t));
    token->value = NULL;
    return token;
}

void alloc_token(token_t* token, size_t size)
{
    if (!token)
    {
        return;
    }
    if (token->value)
    {
        free(token->value);
    }
    token->value = (char*)calloc(1, size);
}

void free_token(token_t* token)
{
    if (token && token->value)
    {
        free(token->value);
        token->value = NULL;
    }
}

#define CASE(t)                                                                \
    case TOK_##t:                                                              \
        return #t;
char* get_token_type_string(enum token_type_t type)
{
    switch (type)
    {
        CASE(EOF)
        CASE(NUMBER)
        CASE(IDENTIFIER)
        CASE(DECLVAR)
        CASE(DECLFN)
        CASE(RETURN)
        CASE(IF)
        CASE(ELSE)
        CASE(FOR)
        CASE(WHILE)
        CASE(ASSIGN)
        CASE(ADD)
        CASE(SUB)
        CASE(MUL)
        CASE(DIV)
        CASE(ARROW)
        CASE(SPACE)
        CASE(TAB)
        CASE(NEWLINE)
        CASE(CARRIAGE)
        CASE(COLON)
        CASE(SEMICOLON)
        CASE(QUOTE)
        CASE(L_PAREN)
        CASE(R_PAREN)
        CASE(L_SQUARE)
        CASE(R_SQUARE)
        CASE(L_BRACKET)
        CASE(R_BRACKET)
    default:
        return "UNKNOWN";
    }
}
#undef CASE

void print_token(token_t* token)
{
    if (!token || token->type == TOK_EOF || !token->value)
    {
        return;
    }
    log_debug("  [%s, %d, %d] -> %s", get_token_type_string(token->type),
              token->start, token->end, token->value);
}

bool is_whitespace(char c)
{
    return c == TOK_SPACE || c == TOK_TAB || c == TOK_NEWLINE ||
           c == TOK_CARRIAGE;
}

bool is_keyword(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

bool is_string(char c)
{
    return c == '"';
}

bool is_operator(char c)
{
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '=';
}

bool is_semicolon(char c)
{
    return c == ';';
}

bool is_compound_op(char* str)
{
    if (is_operator(str[0]) && str[1] == '=')
    {
        return true;
    }
    if (str[0] == '=' && str[1] == '>')
    {
        return true;
    }
    return false;
}

token_t* tokenize_number()
{
    token_t* token = new_token();
    token->start = g_pos;
    token->type = TOK_NUMBER;

    int count = 0;
    while (isdigit((unsigned char)g_buf[g_pos + count]))
    {
        count++;
    }

    alloc_token(token, count + 1);
    memcpy(token->value, &g_buf[g_pos], count);
    token->value[count] = 0;
    g_pos += count;

    token->end = g_pos;
    return token;
}

token_t* tokenize_keyword()
{
    token_t* token = new_token();
    token->start = g_pos;

    int count = 0;
    while (is_keyword(g_buf[g_pos + count]))
    {
        count++;
    }
    alloc_token(token, count + 1);
    memcpy(token->value, &g_buf[g_pos], count);
    token->value[count] = 0;

    if (strcmp(token->value, "const") == 0)
    {
        token->type = TOK_DECLVAR;
    }
    else if (strcmp(token->value, "let") == 0)
    {
        token->type = TOK_DECLVAR;
    }
    else if (strcmp(token->value, "fn") == 0)
    {
        token->type = TOK_DECLFN;
    }
    else if (strcmp(token->value, "return") == 0)
    {
        token->type = TOK_RETURN;
    }
    else if (strcmp(token->value, "if") == 0)
    {
        token->type = TOK_IF;
    }
    else if (strcmp(token->value, "else") == 0)
    {
        token->type = TOK_ELSE;
    }
    else if (strcmp(token->value, "for") == 0)
    {
        token->type = TOK_FOR;
    }
    else if (strcmp(token->value, "while") == 0)
    {
        token->type = TOK_WHILE;
    }
    else
    {
        token->type = TOK_IDENTIFIER;
    }
    g_pos += count;

    token->end = g_pos;

    return token;
}

token_t* tokenize_string()
{
    token_t* token = new_token();
    token->type = TOK_NUMBER;
    g_pos++;
    return token;
}

token_t* tokenize_operator()
{
    token_t* token = new_token();

    // Parse compound (2 character) operators
    if (is_compound_op(&g_buf[g_pos]))
    {
        alloc_token(token, 3);
        token->value[0] = g_buf[g_pos];
        token->value[1] = g_buf[g_pos + 1];
        token->value[2] = 0;
        token->start = g_pos;
        if (strcmp(token->value, "=>") == 0)
        {
            token->type = TOK_ARROW;
        }
        g_pos += 2;
        token->end = g_pos;
    }
    // Parse simple (1 character) operators
    else
    {
        alloc_token(token, 2);
        token->value[0] = g_buf[g_pos];
        token->value[1] = 0;
        token->type = (token_type_t)token->value[0];
        token->start = g_pos;
        g_pos++;
        token->end = g_pos;
    }
    return token;
}

token_t* tokenize_semicolon()
{
    token_t* token = new_token();
    token->type = TOK_SEMICOLON;
    alloc_token(token, 2);
    token->value[0] = ';';
    token->value[1] = 0;
    token->start = g_pos;
    g_pos++;
    token->end = g_pos;
    return token;
}

token_t* tokenize_next()
{
    while (is_whitespace(g_buf[g_pos]))
    {
        g_pos++;
    }

    char c = g_buf[g_pos];

    if (isdigit((unsigned char)c))
    {
        return tokenize_number();
    }
    if (isalpha((unsigned char)c))
    {
        return tokenize_keyword();
    }
    if (is_string(c))
    {
        return tokenize_string();
    }
    if (is_operator(c))
    {
        return tokenize_operator();
    }
    if (is_semicolon(c))
    {
        return tokenize_semicolon();
    }

    token_t* token = new_token();
    alloc_token(token, 2);
    token->type = (token_type_t)c;
    token->value[0] = c;
    token->value[1] = 0;
    token->start = g_pos;
    g_pos++;
    return token;
}

size_t tokenize(char* buffer, token_t* tokens)
{
    // Copy the input string buffer into a global buffer. This is freed at the
    // end of this function.
    g_buf = strdup(buffer);
    g_pos = 0;

    size_t token_count = 0;

    // While we're not at the end of the buffer, keep constructing tokens.
    while (g_buf[g_pos] != '\0')
    {
        // Get the next token.
        token_t* t = tokenize_next();

        // If the token is invalid, break.
        if (!t)
        {
            break;
        }

        // Copy the token into the token buffer.
        tokens[token_count] = *t;

        // Free the temporary token we had allocated.
        free(t);

        // Increement the token count.
        token_count++;

        // If we've exceeded the max token count, break.
        if (token_count >= TOKEN_COUNT - 1)
        {
            break;
        }
    }

    // Construct an EOF token at the end.
    tokens[token_count].type = TOK_EOF;
    tokens[token_count].value = NULL;
    tokens[token_count].start = g_pos;
    token_count++;

    // Free the temporary buffer of the input string
    free(g_buf);

    // Return the token count
    return token_count;
}

bool is_binop(token_type_t type)
{
    return type == TOK_ADD || type == TOK_SUB || type == TOK_MUL ||
           type == TOK_DIV;
}

bool is_constant(token_type_t type)
{
    return type == TOK_NUMBER;
}