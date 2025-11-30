#ifndef LEX_H
#define LEX_H

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "log.h"

static int g_lpos = 0;
static char* g_lbuf = NULL;

#define TOKEN_COUNT 1024

#define KW_CONST "const"
#define KW_LET "let"
#define KW_DEF "def"
#define KW_RETURN "return"
#define KW_IF "if"
#define KW_ELSE "else"
#define KW_FOR "for"
#define KW_WHILE "while"

enum token_type_t
{
    TOK_EOF = 0,
    TOK_INT,
    TOK_STRING,

    TOK_NAME = 128,
    TOK_CONST,
    TOK_LET,
    TOK_DEF,
    TOK_RETURN,
    TOK_IF,
    TOK_ELSE,
    TOK_FOR,
    TOK_WHILE,

    // Operators
    TOK_ASSIGN = '=',

    // Whitespace
    TOK_SPACE = ' ',     // Space
    TOK_TAB = '\t',      // Tab
    TOK_NEWLINE = '\n',  // New line, 0x0A
    TOK_CARRIAGE = '\r', // Carriage return, 0x0D
    TOK_COLON = ':',     // Colon
    TOK_SEMICOLON = ';', // Semicolon
    TOK_QUOTE = '"',     // Quote
    TOK_L_PAREN = '(',
    TOK_R_PAREN = ')',
    TOK_L_SQUARE = '[',  // Left square bracket
    TOK_R_SQUARE = ']',  // Right square bracket
    TOK_L_BRACKET = '{', // Left curly bracket
    TOK_R_BRACKET = '}', // Right curly bracket

    TOK_UNKNOWN = 255,
};

struct token_t
{
    enum token_type_t type;
    char* value;
    int pos;
};

typedef struct token_t token_t;
typedef enum token_type_t token_type_t;

#define CASE(t)                                                                                                        \
    case t:                                                                                                            \
        return #t;
static char* get_token_type_string(token_type_t type)
{
    switch (type)
    {
        CASE(TOK_EOF)
        CASE(TOK_INT)
        CASE(TOK_STRING)
        CASE(TOK_NAME)
        CASE(TOK_CONST)
        CASE(TOK_LET)
        CASE(TOK_DEF)
        CASE(TOK_RETURN)
        CASE(TOK_IF)
        CASE(TOK_ELSE)
        CASE(TOK_FOR)
        CASE(TOK_WHILE)
        CASE(TOK_ASSIGN)
        CASE(TOK_SPACE)
        CASE(TOK_TAB)
        CASE(TOK_NEWLINE)
        CASE(TOK_CARRIAGE)
        CASE(TOK_COLON)
        CASE(TOK_SEMICOLON)
        CASE(TOK_QUOTE)
        CASE(TOK_L_PAREN)
        CASE(TOK_R_PAREN)
        CASE(TOK_L_SQUARE)
        CASE(TOK_R_SQUARE)
        CASE(TOK_L_BRACKET)
        CASE(TOK_R_BRACKET)
        CASE(TOK_UNKNOWN)
    }
    return "";
}

void print_token(token_t* token)
{
    if (token->type == 0 || token->value == "")
    {
        return;
    }
    log_debug("  [%s, %d] -> %s", get_token_type_string(token->type), token->pos, token->value);
}

bool is_whitespace(char c)
{
    return c == TOK_SPACE || c == TOK_TAB || c == TOK_NEWLINE || c == TOK_CARRIAGE;
}

bool is_keyword(char c)
{
    return isalnum(c) || c == '_';
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

token_t lex_number()
{
    token_t token;
    token.pos = g_lpos;
    token.type = TOK_INT;

    // Count the number of digits in this number until we hit a
    // non-digit char.
    int count = 0;
    while (isdigit(g_lbuf[g_lpos + count]))
    {
        count++;
    }

    token.value = (char*)malloc(count + 1);
    memcpy(token.value, &g_lbuf[g_lpos], count);
    token.value[count] = 0;

    g_lpos += count;
    return token;
}

#define KW(name)                                                                                                       \
    (memcmp(token.value, KW_##name, count))                                                                            \
    {                                                                                                                  \
        token.type = TOK_##name;                                                                                       \
    }

token_t lex_keyword()
{
    token_t token;
    token.pos = g_lpos;

    int count = 0;
    while (is_keyword(g_lbuf[g_lpos + count]))
    {
        count++;
    }
    token.value = (char*)malloc(count + 1);
    memcpy(token.value, &g_lbuf[g_lpos], count);
    token.value[count] = 0;

    log_debug("Lexing keyword: %s", token.value);

    if (strcmp(token.value, "const") == 0)
    {
        token.type = TOK_CONST;
    }
    else if (strcmp(token.value, "let") == 0)
    {
        token.type = TOK_LET;
    }
    else if (strcmp(token.value, "def") == 0)
    {
        token.type = TOK_DEF;
    }
    else if (strcmp(token.value, "return") == 0)
    {
        token.type = TOK_RETURN;
    }
    else if (strcmp(token.value, "if") == 0)
    {
        token.type = TOK_IF;
    }
    else if (strcmp(token.value, "else") == 0)
    {
        token.type = TOK_ELSE;
    }
    else if (strcmp(token.value, "for") == 0)
    {
        token.type = TOK_FOR;
    }
    else if (strcmp(token.value, "while") == 0)
    {
        token.type = TOK_WHILE;
    }
    else
    {
        token.type = TOK_NAME;
    }
    g_lpos += count;

    return token;
}

token_t lex_string()
{
    token_t token;
    token.type = TOK_STRING;
    g_lpos++;
    return token;
}

token_t lex_operator()
{
    token_t token;

    // Store single char in 2-byte value
    token.value = (char*)malloc(2);
    token.value[0] = g_lbuf[g_lpos];
    token.value[1] = 0;

    // Store type from the value itself
    token.type = (token_type_t)token.value[0];

    // Store and increment position
    token.pos = g_lpos;
    g_lpos++;
    return token;
}

token_t lex_semicolon()
{
    token_t token;
    token.type = TOK_SEMICOLON;
    token.value = (char*)";";
    token.pos = g_lpos;
    g_lpos++;
    return token;
}

token_t lex_next()
{
    // Skip whitespace
    while (is_whitespace(g_lbuf[g_lpos]))
    {
        g_lpos++;
    }

    char c = g_lbuf[g_lpos];

    // Integers
    if (isdigit(c))
    {
        return lex_number();
    }
    // Keywords
    if (isalpha(c))
    {
        return lex_keyword();
    }
    // Strings (within quotes)
    if (is_string(c))
    {
        return lex_string();
    }
    // Operators
    if (is_operator(c))
    {
        return lex_operator();
    }
    // Semicolons
    if (is_semicolon(c))
    {
        return lex_semicolon();
    }

    token_t token;
    token.type = c;
    token.value = (char*)malloc(2);
    token.value[0] = c;
    token.value[1] = 0;
    token.pos = g_lpos;
    g_lpos++;
    return token;
}

size_t lex(token_t* tokens)
{
    memset(tokens, 0, sizeof(token_t) * TOKEN_COUNT);
    token_t* current_token = tokens;
    size_t token_count = 0;
    while (g_lbuf[g_lpos] != TOK_EOF)
    {
        token_t token = lex_next();
        *current_token = token;
        current_token++;
        token_count++;
    }
    return token_count;
}

#endif