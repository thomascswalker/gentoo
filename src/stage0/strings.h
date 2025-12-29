#ifndef STRINGS_H
#define STRINGS_H

#include <stdlib.h>
#include <string.h>

static char* strjoin(char* buf, size_t* cap, const char* piece,
                     int prepend_comma)
{
    if (buf == NULL)
    {
        *cap = strlen(piece) + 64;
        buf = (char*)malloc(*cap);
        if (!buf)
        {
            return NULL;
        }
        buf[0] = '\0';
    }

    size_t cur_len = strlen(buf);
    size_t add_len = strlen(piece) + (prepend_comma ? 2 : 0) + 1;
    if (cur_len + add_len > *cap)
    {
        *cap = cur_len + add_len + 256;
        char* tmp = (char*)realloc(buf, *cap);
        if (!tmp)
        {
            return buf;
        }
        buf = tmp;
    }
    if (prepend_comma && cur_len > 0)
    {
        strcat(buf, ", ");
    }
    strcat(buf, piece);
    return buf;
}

// Convert a character to its respective hexadecimal value.
static int to_hex(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F')
    {
        return 10 + (c - 'A');
    }
    return -1;
}

// Decode C-style escape sequences in place.
static void stresc(char* str)
{
    if (!str)
    {
        return;
    }

    // Read position
    size_t rpos = 0;
    // Write position
    size_t wpos = 0;

    while (str[rpos] != '\0')
    {
        char c = str[rpos++];
        if (c == '\\')
        {
            char next = str[rpos];
            if (next == '\0')
            {
                str[wpos++] = '\\';
                break;
            }
            rpos++;
            switch (next)
            {
            case '\\':
            {
                str[wpos++] = '\\';
                break;
            }
            case '"':
            {
                str[wpos++] = '"';
                break;
            }
            case 'n':
            {
                str[wpos++] = '\n';
                break;
            }
            case 'r':
            {
                str[wpos++] = '\r';
                break;
            }
            case 't':
            {
                str[wpos++] = '\t';
                break;
            }
            case 'x':
            {
                int value = 0;
                int digits = 0;
                while (digits < 2)
                {
                    int h = to_hex(str[rpos]);
                    if (h < 0)
                    {
                        break;
                    }
                    value = (value << 4) | h;
                    rpos++;
                    digits++;
                }
                if (digits == 0)
                {
                    str[wpos++] = 'x';
                }
                else
                {
                    str[wpos++] = (char)value;
                }
                break;
            }
            default:
            {
                str[wpos++] = next;
                break;
            }
            }
        }
        else
        {
            str[wpos++] = c;
        }
    }

    str[wpos] = '\0';
}

#endif