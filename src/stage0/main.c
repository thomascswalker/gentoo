/*
 * Bare-minimum compiler to bootstrap.
 *
 * Features:
 *   - Assignment
 *   - Arrays
 *   - Functions
 *   - File I/O
 */

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef _WIN32
#include <sys/wait.h>
#else
#include <direct.h>
#endif

#include "ast.h"
#include "buffer.h"
#include "codegen.h"
#include "log.h"

static int ensure_directory_exists(const char* path)
{
    struct stat info;
    if (stat(path, &info) == 0)
    {
        if (S_ISDIR(info.st_mode))
        {
            return 0;
        }

        log_error("Path %s exists but is not a directory.", path);
        return -1;
    }

    if (errno != ENOENT)
    {
        perror("stat");
        return -1;
    }

#ifndef _WIN32
    if (mkdir(path, 0777) != 0 && errno != EEXIST)
#else
    if (_mkdir(path) != 0 && errno != EEXIST)
#endif
    {
        perror("mkdir");
        return -1;
    }

    return 0;
}

static const char* find_last_dot(const char* base, size_t len)
{
    const char* p = base + len;
    while (p != base)
    {
        --p;
        if (*p == '.')
        {
            return p;
        }
    }
    return NULL;
}

static void derive_output_name(const char* input, char* out, size_t out_sz)
{
    const char* base = strrchr(input, '/');
    const char* alt = strrchr(input, '\\');
    if (alt && (!base || alt > base))
        base = alt;
    base = base ? base + 1 : input;

    size_t len = strnlen(base, out_sz);
    const char* dot = len ? find_last_dot(base, len) : NULL;
    if (!dot)
        dot = base + len;

    size_t copy = (size_t)(dot - base);
    if (copy >= out_sz)
        copy = out_sz - 1;
    memcpy(out, base, copy);
    out[copy] = '\0';
}

static int run_command_fmt(const char* format, ...)
{
    char stack_cmd[512];
    char* heap_cmd = NULL;
    va_list args;

    va_start(args, format);
    int needed = vsnprintf(stack_cmd, sizeof(stack_cmd), format, args);
    va_end(args);
    if (needed < 0)
    {
        return -1;
    }

    const char* cmd = stack_cmd;
    if ((size_t)needed >= sizeof(stack_cmd))
    {
        heap_cmd = (char*)malloc((size_t)needed + 1);
        if (heap_cmd == NULL)
        {
            return -1;
        }

        va_start(args, format);
        vsnprintf(heap_cmd, (size_t)needed + 1, format, args);
        va_end(args);

        cmd = heap_cmd;
    }

    log_info("Running: %s", cmd);
    int status = system(cmd);
    free(heap_cmd);
    if (status == -1)
    {
        perror("system");
        return -1;
    }

#ifndef _WIN32
    if (WIFEXITED(status))
    {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status))
    {
        return 128 + WTERMSIG(status);
    }
#endif

    return status;
}

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
    // Ensure exactly one argument (the file's name)
    if (argc < 2)
    {
        fprintf(stderr, "Missing required input file: %d\n", 0);
        return 1;
    }

    // Parse 'exec' option
    bool exec = false;
    if (argc >= 3)
    {
        exec = streq(argv[2], "--exec");
    }
    log_info("Exec: %s", exec ? "true" : "false");

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
    log_info("Compiling %s...", file_name);
    char* buf = read_file(file_name);

    // Parse the file content into an AST
    log_info("Parsing file...");
    log_debug("%s", buf);
    ast* root_node = parse(buf);
    free(buf);

    // Generate assembly code
    log_info("Generating assembly...");
    char* code = ast_codegen(root_node, X86_64);
    ast_free(root_node);

    log_debug("%s", code);

    const char* build_dir = "./build";
    if (ensure_directory_exists(build_dir) != 0)
    {
        free(code);
        return 1;
    }

    char output_name[512];
    derive_output_name(file_name, output_name, sizeof(output_name));

    char asm_filepath[1024];
    char obj_filepath[1024];
    char bin_filepath[1024];
    snprintf(asm_filepath, sizeof(asm_filepath), "%s/%s.asm", build_dir,
             output_name);
    snprintf(obj_filepath, sizeof(obj_filepath), "%s/%s.o", build_dir,
             output_name);
    snprintf(bin_filepath, sizeof(bin_filepath), "%s/%s", build_dir,
             output_name);

    // Output to asm file
    write_file(asm_filepath, code);
    free(code);

    run_command_fmt("nasm -f elf64 %s -o %s", asm_filepath, obj_filepath);
    run_command_fmt("gcc %s -o %s -z noexecstack -no-pie", obj_filepath,
                    bin_filepath);
    if (exec)
    {
        run_command_fmt("%s", bin_filepath);
    }

    return 0;
}