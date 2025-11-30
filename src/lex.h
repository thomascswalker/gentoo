#ifndef LEX_H
#define LEX_H

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int g_pos = 0;
static char* g_buffer = NULL;

static const char* CONST = "const";
static const char* LET = "let";

enum token_type_t
{
    TOK_EOF = 0,
    TOK_INT,
    TOK_STRING,

    TOK_NAME,
    TOK_CONST,
    TOK_LET,

    // Operators
    TOK_ASSIGN = '=',

    // Whitespace
    TOK_SPACE = ' ',     // Space
    TOK_TAB = '\t',      // Tab
    TOK_NEWLINE = '\n',  // New line
    TOK_RETURN = '\r',   // Carriage return
    TOK_SEMICOLON = ';', // Semicolon
    TOK_QUOTE = '"',     // Quote

    TOK_UNKNOWN = 255,
};

struct token_t
{
    enum token_type_t type;
    char* value;
    int pos;
};

#define TOKEN struct token_t
#define TYPE enum token_type_t

void print_token(TOKEN* token)
{
    fprintf(stdout, "[%d, %d] -> %s\n", token->type, token->pos, token->value);
}

bool is_whitespace(char c)
{
    return c == TOK_SPACE || c == TOK_TAB || c == TOK_NEWLINE || c == TOK_RETURN;
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

TOKEN lex_number()
{
    TOKEN token;
    token.pos = g_pos;
    token.type = TOK_INT;

    // Count the number of digits in this number until we hit a
    // non-digit char.
    int count = 0;
    while (isdigit(g_buffer[g_pos + count]))
    {
        count++;
    }

    token.value = (char*)malloc(count + 1);
    memcpy(token.value, &g_buffer[g_pos], count);
    token.value[count] = 0;

    g_pos += count;
    return token;
}

TOKEN lex_keyword()
{
    TOKEN token;
    token.pos = g_pos;

    int count = 0;
    while (isalpha(g_buffer[g_pos + count]))
    {
        count++;
    }
    token.value = (char*)malloc(count + 1);
    memcpy(token.value, &g_buffer[g_pos], count);
    token.value[count] = 0;

    if (memcmp(token.value, CONST, count))
    {
        token.type = TOK_CONST;
    }
    else if (memcmp(token.value, LET, count))
    {
        token.type = TOK_LET;
    }
    g_pos += count;

    return token;
}

TOKEN lex_string()
{
    TOKEN token;
    token.type = TOK_STRING;
    g_pos++;
    return token;
}

TOKEN lex_operator()
{
    TOKEN token;

    // Store single char in 2-byte value
    token.value = (char*)malloc(2);
    token.value[0] = g_buffer[g_pos];
    token.value[1] = 0;

    // Store type from the value itself
    token.type = (TYPE)token.value[0];

    // Store and increment position
    token.pos = g_pos;
    g_pos++;
    return token;
}

TOKEN lex_semicolon()
{
    TOKEN token;
    token.type = TOK_SEMICOLON;
    token.value = (char*)";";
    token.pos = g_pos;
    g_pos++;
    return token;
}

TOKEN lex_next()
{
    // Skip whitespace
    while (is_whitespace(g_buffer[g_pos]))
    {
        g_pos++;
    }

    // Integers

    char c = g_buffer[g_pos];
    if (isdigit(c))
    {
        return lex_number();
    }
    if (isalpha(c))
    {
        return lex_keyword();
    }
    if (is_string(c))
    {
        return lex_string();
    }
    if (is_operator(c))
    {
        return lex_operator();
    }
    if (is_semicolon(c))
    {
        return lex_semicolon();
    }

    TOKEN token;
    token.type = TOK_UNKNOWN;
    token.value = (char*)"UNKNOWN";
    token.pos = g_pos;
    g_pos++;
    return token;
}

int lex(TOKEN** tokens)
{
    while (g_buffer[g_pos] != TOK_EOF)
    {
        TOKEN token = lex_next();
        print_token(&token);
    }
    return 0;
}

#endif