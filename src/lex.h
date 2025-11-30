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

    TOK_NAME,
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

#define TOKEN struct token_t
#define TYPE enum token_type_t

void print_token(TOKEN* token)
{
    fprintf(stdout, "[%d, %d] -> %s\n", token->type, token->pos, token->value);
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

#define KW(name)                                                                                                       \
    (memcmp(token.value, KW_##name, count))                                                                            \
    {                                                                                                                  \
        token.type = TOK_##name;                                                                                       \
    }

TOKEN lex_keyword()
{
    TOKEN token;
    token.pos = g_pos;

    int count = 0;
    while (is_keyword(g_buffer[g_pos + count]))
    {
        count++;
    }
    token.value = (char*)malloc(count + 1);
    memcpy(token.value, &g_buffer[g_pos], count);
    token.value[count] = 0;

    if (memcmp(token.value, "const", count))
    {
        token.type = TOK_CONST;
    }
    else if (memcmp(token.value, "let", count))
    {
        token.type = TOK_LET;
    }
    else if (memcmp(token.value, "def", count))
    {
        token.type = TOK_DEF;
    }
    else if (memcmp(token.value, "return", count))
    {
        token.type = TOK_RETURN;
    }
    else if (memcmp(token.value, "if", count))
    {
        token.type = TOK_IF;
    }
    else if (memcmp(token.value, "else", count))
    {
        token.type = TOK_ELSE;
    }
    else if (memcmp(token.value, "for", count))
    {
        token.type = TOK_FOR;
    }
    else if (memcmp(token.value, "while", count))
    {
        token.type = TOK_WHILE;
    }
    else
    {
        token.type = TOK_NAME;
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

    char c = g_buffer[g_pos];

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

    TOKEN token;
    token.type = c;
    token.value = (char*)malloc(2);
    token.value[0] = c;
    token.value[1] = 0;
    token.pos = g_pos;
    g_pos++;
    return token;
}

int lex(TOKEN* tokens)
{
    TOKEN* current_token = tokens;
    while (g_buffer[g_pos] != TOK_EOF)
    {
        TOKEN token = lex_next();
        *current_token = token;
        current_token++;
    }
    return 0;
}

#endif