/*
 * Bare-minimum compiler to bootstrap.
 *
 * Features:
 *   - Assignment
 *   - Arrays
 *   - Functions
 *   - File I/O
 */

#include "lex.h"

int main(int argc, char** argv)
{
    // Ensure exaclty one argument (the file's name)
    if (argc != 2)
    {
        fprintf(stderr, "Missing required input file: %d\n", 0);
        return 1;
    }

    // Ensure the file exists
    const char* file_name = argv[1];
    struct stat buffer;
    if (stat(file_name, &buffer) != 0)
    {
        fprintf(stderr, "File %s does not exist.", file_name);
        return 1;
    }

    // Read the contents of the file.
    // 1. Open the file.
    // 2. Get the file size.
    // 3. Read the file data into a buffer.
    // 4. Close the file.
    FILE* handle = fopen(file_name, "r");
    fseek(handle, 0L, SEEK_END);
    int file_size = ftell(handle) + 1;
    fseek(handle, 0L, SEEK_SET);
    fprintf(stdout, "Compiling file: %s (%db)\n", file_name, file_size);
    char* file_data = (char*)malloc(file_size);
    fgets(file_data, file_size, handle);
    fclose(handle);

    // Copy the file data to a temporary lex buffer
    g_buffer = (char*)malloc(file_size);
    memcpy(g_buffer, file_data, file_size);
    // Free the file data.
    free(file_data);

    fprintf(stdout, "Lexing: %s\n\n", g_buffer);

    // Lexically deconstruct the string buffer and construct a set of tokens.
    TOKEN* tokens = NULL;
    const int lex_err = lex(&tokens);

    // Free the lex buffer
    free(g_buffer);

    return 0;
}