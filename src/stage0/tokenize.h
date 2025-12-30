#ifndef TOKENIZE_H
#define TOKENIZE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TOKEN_COUNT 4096

typedef enum token_type_t
{
    TOK_EOF = 0,

    TOK_NUMBER,

    TOK_IDENTIFIER = 128,
    TOK_DECLVAR,
    TOK_DECLFN,
    TOK_RETURN,
    TOK_IF,
    TOK_ELSE,
    TOK_FOR,
    TOK_WHILE,
    TOK_TRUE,
    TOK_FALSE,
    TOK_EQ,
    TOK_GT,
    TOK_LT,

    TOK_ASSIGN = '=',
    TOK_ADD = '+',
    TOK_SUB = '-',
    TOK_MUL = '*',
    TOK_DIV = '/',
    TOK_ARROW,

    TOK_SPACE = ' ',
    TOK_TAB = '\t',
    TOK_NEWLINE = '\n',
    TOK_CARRIAGE = '\r',
    TOK_COLON = ':',
    TOK_SEMICOLON = ';',
    TOK_STRING = '"',
    TOK_COMMA = ',',
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

bool is_binop(token_type_t type);
bool is_constant(token_type_t type);

// Tokenization
size_t tokenize(char* buffer, token_t* tokens);
token_t* new_token(void);
void alloc_token(token_t* token, size_t size);
void free_token(token_t* token);
char* get_token_type_string(enum token_type_t type);
void print_token(token_t* token);

#endif