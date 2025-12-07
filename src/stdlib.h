#ifndef STDLIB_H
#define STDLIB_H

#include <stdio.h>
#include <stdlib.h>

#include <stdbool.h>

void swap(char* a, char* b)
{
    if (!a || !b)
    {
        return;
    }

    char temp = *(a);
    *(a) = *(b);
    *(b) = temp;
}

void reverse(char* str, int length)
{
    int start = 0;
    int end = length - 1;
    while (start < end)
    {
        swap((str + start), (str + end));
        start++;
        end--;
    }
}

char* itoa(int num, char* str, int base)
{
    int i = 0;
    bool is_signed = false;

    if (num == 0)
    {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    if (num < 0 && base == 10)
    {
        is_signed = true;
        num = -num;
    }

    while (num != 0)
    {
        int rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }

    if (is_signed == true)
    {
        str[i++] = '-';
    }

    str[i] = '\0';
    reverse(str, i);
    return str;
}

#endif