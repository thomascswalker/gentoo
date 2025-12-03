/*
 * Bare-minimum compiler to bootstrap.
 *
 * Features:
 *   - Assignment
 *   - Arrays
 *   - Functions
 *   - File I/O
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "ast.h"
#include "log.h"

char* read_file(const char* filename)
{
    FILE* fp = NULL;
    long file_size = 0;
    char* buffer = NULL;

    fp = fopen(filename, "r");
    if (fp == NULL)
    {
        perror("Error opening file");
        return NULL;
    }

    // Determine file size
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    rewind(fp); // Go back to the beginning

    // Allocate memory for the buffer
    buffer = (char*)malloc(file_size + 1); // +1 for null terminator
    if (buffer == NULL)
    {
        perror("Error allocating memory");
        fclose(fp);
        return NULL;
    }

    // Read the file content into the buffer
    size_t bytes_read = fread(buffer, 1, file_size, fp);
    if (bytes_read != file_size)
    {
        perror("Error reading file");
        free(buffer);
        fclose(fp);
        return NULL;
    }

    // Add null terminator
    buffer[file_size] = '\0';

    fclose(fp);
    return buffer;
}

void write_file(const char* filename, char* buffer)
{
    FILE* fp = NULL;
    fp = fopen(filename, "w");
    if (fp == NULL)
    {
        perror("Error opening file");
        return;
    }

    fputs(buffer, fp);
    fclose(fp);
}

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
    struct stat stat_buffer;
    if (stat(file_name, &stat_buffer) != 0)
    {
        fprintf(stderr, "File %s does not exist.", file_name);
        return 1;
    }

    // Read the contents of the file.
    // 1. Open the file.
    // 2. Get the file size.
    // 3. Read the file data into a buffer.
    // 4. Close the file.
    char* buf = read_file(file_name);

    log_debug("Parsing:\n%s", buf);
    ast* root_node = parse(buf);

    char* asm_buffer = malloc(512);
    ast_emit(asm_buffer, root_node);
    log_info("Assembly:\n%s", asm_buffer);

    // Output to asm file
    write_file("program.asm", asm_buffer);

    free(asm_buffer);

    ast_free(root_node);

    // Free the file content buffer
    free(buf);

    system("bash compile.sh program");

    return 0;
}