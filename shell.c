// shell.c
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <ctype.h>

#define BUFSIZE 64
#define DELIIMITER " \n\t\r\a"

// function declarations
char *read_line(void);
char **split_line(char *line);
int execute(char **args);
int sh_launch(char **args);
char ***split_pipe(char *line, int *count);
int pipe_execute(char ***pipe_args, int count);

int sh_cd(char **args);
int sh_help(char **args);
int sh_exit(char **args);

/* ---------- BUILTIN LOGIC (OLD STYLE) ---------- */

char *built_in[] = { "cd", "help", "exit" };

int builtin_size() {
    return sizeof(built_in) / sizeof(char *);
}

int (*built_in_func[])(char **args) = {
    &sh_cd,
    &sh_help,
    &sh_exit
};

/* ---------- BUILTINS ---------- */

int sh_cd(char **args)
{
    if (args[1] == NULL) {
        if (chdir(getenv("HOME")) != 0) {
            perror("sh");
        }
    } else {
        if (chdir(args[1]) != 0) {
            perror("sh");
        }
    }
    return 1;
}

int sh_help(char **args)
{
    printf("\nWelcome to sh shell!!!! (ik its such a generic name lmaooo)");
    printf("\nAvailable built in commands:\n");

    for (int i = 0; i < builtin_size(); i++) {
        printf(" %s\n", built_in[i]);
    }
    return 1;
}

int sh_exit(char **args)
{
    return 0;
}

/* ---------- INPUT ---------- */

char *read_line(void)
{
    char *line = NULL;
    size_t buffer_size = 0;

    if (getline(&line, &buffer_size, stdin) == -1) {
        if (feof(stdin)) {
            exit(EXIT_SUCCESS);
        } else {
            perror("sh");
            exit(EXIT_FAILURE);
        }
    }
    return line;
}

/* ---------- TOKENIZATION ---------- */

char **split_line(char *line)
{
    int buf_size = BUFSIZE;
    int position = 0;
    char **tokens = malloc(buf_size * sizeof(char *));
    char *token;
    char **tokens_backup;

    if (!tokens) {
        fprintf(stderr, "sh: allocation failed\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, DELIIMITER);

    while (token != NULL) {
        tokens[position++] = strdup(token);

        if (position >= buf_size) {
            buf_size += BUFSIZE;
            tokens_backup = tokens;
            tokens = realloc(tokens, buf_size * sizeof(char *));
            if (!tokens) {
                for (int i = 0; i < position; i++) {
                    free(tokens_backup[i]);
                }
                free(tokens_backup);
                fprintf(stderr, "sh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, DELIIMITER);
    }

    tokens[position] = NULL;
    return tokens;
}

/* ---------- EXECUTION ---------- */

int execute(char **args)
{
    if (args[0] == NULL) {
        return 1;
    }

    for (int i = 0; i < builtin_size(); i++) {
        if (strcmp(args[0], built_in[i]) == 0) {
            return (*built_in_func[i])(args);
        }
    }

    return sh_launch(args);
}

int sh_launch(char **args)
{
    pid_t pid;
    int status;

    pid = fork();

    if (pid == 0) {
        if (execvp(args[0], args) == -1) {
            perror("sh");
            exit(EXIT_FAILURE);
        }
    } else if (pid < 0) {
        perror("sh");
    } else {
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

/* ---------- PIPE HANDLING ---------- */

char *trim(char *str)
{
    while (*str && isspace(*str)) str++;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace(*end)) *end-- = '\0';
    return str;
}

char ***split_pipe(char *line, int *count)
{
    char **pipe_cmds = NULL;
    char **tmp;
    int n = 0;

    char *cmd = strtok(line, "|");
    while (cmd != NULL) {
        tmp = realloc(pipe_cmds, (n + 1) * sizeof(char *));
        if (!tmp) {
            perror("sh");
            exit(EXIT_FAILURE);
        }
        pipe_cmds = tmp;
        pipe_cmds[n++] = strdup(trim(cmd));
        cmd = strtok(NULL, "|");
    }

    char ***pipe_tokens = malloc(n * sizeof(char **));
    for (int i = 0; i < n; i++) {
        pipe_tokens[i] = split_line(pipe_cmds[i]);
        free(pipe_cmds[i]);
    }
    free(pipe_cmds);

    *count = n;
    return pipe_tokens;
}

int pipe_execute(char ***pipe_args, int count)
{
    int pipes[count - 1][2];
    pid_t pids[count];

    for (int i = 0; i < count - 1; i++) {
        pipe(pipes[i]);
    }

    for (int i = 0; i < count; i++) {
        pids[i] = fork();

        if (pids[i] == 0) {
            if (i > 0) {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }
            if (i < count - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            for (int j = 0; j < count - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            execvp(pipe_args[i][0], pipe_args[i]);
            perror("sh");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < count - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    for (int i = 0; i < count; i++) {
        waitpid(pids[i], NULL, 0);
    }

    return 1;
}

/* ---------- LOOP ---------- */

void loop(void)
{
    char *line;
    char **args;
    char ***pipe_args;
    int count;
    int status;

    do {
        printf(">");
        line = read_line();

        if (strchr(line, '|')) {
            pipe_args = split_pipe(line, &count);
            status = pipe_execute(pipe_args, count);

            for (int i = 0; i < count; i++) {
                for (int j = 0; pipe_args[i][j] != NULL; j++) {
                    free(pipe_args[i][j]);
                }
                free(pipe_args[i]);
            }
            free(pipe_args);
        } else {
            args = split_line(line);
            status = execute(args);
            for (int i = 0; args[i] != NULL; i++) {
                free(args[i]);
            }
            free(args);
        }

        free(line);
    } while (status);
}

int main(void)
{
    loop();
    return EXIT_SUCCESS;
}
