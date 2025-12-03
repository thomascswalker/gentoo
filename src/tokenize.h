#ifndef TOKENIZE_H
#define TOKENIZE_H

#include <stdbool.h>
#include <stddef.h>

#define TOKEN_COUNT 1024

typedef enum token_type_t
{
    TOK_EOF = 0,

    TOK_NUMBER,

    TOK_IDENTIFIER = 128,
    TOK_DECLVAR,
    TOK_DEF,
    TOK_RETURN,
    TOK_IF,
    TOK_ELSE,
    TOK_FOR,
    TOK_WHILE,

    TOK_ASSIGN = '=',
    TOK_ADD = '+',
    TOK_SUB = '-',
    TOK_MUL = '*',
    TOK_DIV = '/',

    TOK_SPACE = ' ',
    TOK_TAB = '\t',
    TOK_NEWLINE = '\n',
    TOK_CARRIAGE = '\r',
    TOK_COLON = ':',
    TOK_SEMICOLON = ';',
    TOK_QUOTE = '"',
    TOK_L_PAREN = '(',
    TOK_R_PAREN = ')',
    TOK_L_SQUARE = '[',
    TOK_R_SQUARE = ']',
    TOK_L_BRACKET = '{',
    TOK_R_BRACKET = '}',

    TOK_UNKNOWN = 255,
} token_type_t;

typedef struct token_t
{
    enum token_type_t type;
    char* value;
    size_t start;
    size_t end;
} token_t;

static bool is_binop(token_type_t type)
{
    return type == TOK_ADD || type == TOK_SUB || type == TOK_MUL || type == TOK_DIV;
}

static bool is_constant(token_type_t type)
{
    return type == TOK_NUMBER;
}

// Tokenizer API
size_t tokenize(char* buffer, token_t* tokens);
token_t* new_token(void);
void alloc_token(token_t* token, size_t size);
void free_token(token_t* token);
char* get_token_type_string(enum token_type_t type);
void print_token(token_t* token);

#endif