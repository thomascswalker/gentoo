#include <stdlib.h>
#include <string.h>

static char* strjoin(char* buf, size_t* cap, const char* piece, int prepend_comma)
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
