#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "tokenizer.h"
#include "parser.h"
#include "executor.h"

LoopVariable error_handler(ErrorCode code)
{
    switch (code) {
    case E_OK:
        return LOOP_CONTINUE;
    case E_EOF:
    case E_EXIT:
        return LOOP_EXIT;
    case E_SYSCALL:
    case E_ALLOC:
    case E_SYNTAX:
    default:
        return LOOP_CONTINUE;
    }
}

ErrorCode read_line(char **line)
{
    char *input_line = NULL;
    size_t buffer_size = 0;

    if (getline(&input_line, &buffer_size, stdin) == -1) {
        free(input_line);
        if (feof(stdin)) {
            return E_EOF;
        } else {
            return E_SYSCALL;
        }
    }

    *line = input_line;
    return E_OK;
}

int main()
{
    ErrorCode err;
    char *line = NULL;

    TokenArray tokens = {0};
    ASTnode *ast = NULL;

    do {
        printf(">");

        err = read_line(&line);
        if (error_handler(err) == LOOP_EXIT) {
            goto cleanup;
        }
        if (!line) {
            continue;
        }

        err = tokenize(&tokens, line);
        if (error_handler(err) == LOOP_EXIT) {
            goto cleanup;
        }

        err = parser(&ast, tokens);
        if (error_handler(err) == LOOP_EXIT) {
            goto cleanup;
        }

        err = ast_executor(ast);

        free_ast(ast);
        ast = NULL;

        if (error_handler(err) == LOOP_EXIT) {
            goto cleanup;
        }

        free(line);
        free_tokens(&tokens);

    } while (1);

cleanup:
    free(line);
    free_tokens(&tokens);
    if (ast) {
        free_ast(ast);
    }
    return EXIT_SUCCESS;
}
