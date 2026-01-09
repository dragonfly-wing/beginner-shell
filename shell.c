// ---------------- shell.c ----------------
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

/* =======================
   TOKENIZER (UNCHANGED)
   ======================= */

typedef enum {
  TOK_WORD,
  TOK_PIPE,
  TOK_REDIR_OUT,
  TOK_REDIR_IN,
  TOK_REDIR_APPEND,
  TOK_EOF
} TokenType;

typedef struct {
  TokenType type;
  const char *start;
  int length;
} Token;

typedef struct {
  Token *data;
  int count;
  int capacity;
} TokenArray;

TokenArray tokenize(char *message) {
  TokenArray tokens;
  tokens.count = 0;
  tokens.capacity = 8;
  tokens.data = malloc(tokens.capacity * sizeof(Token));

  int i = 0;
  while (message[i]) {
    if (isspace(message[i])) {
      i++;
      continue;
    }

    if (message[i] == '|') {
      tokens.data[tokens.count++] = (Token){TOK_PIPE, &message[i++], 1};
    }
    else if (message[i] == '>' && message[i+1] == '>') {
      tokens.data[tokens.count++] = (Token){TOK_REDIR_APPEND, &message[i], 2};
      i += 2;
    }
    else if (message[i] == '>') {
      tokens.data[tokens.count++] = (Token){TOK_REDIR_OUT, &message[i++], 1};
    }
    else if (message[i] == '<') {
      tokens.data[tokens.count++] = (Token){TOK_REDIR_IN, &message[i++], 1};
    }
    else {
      int start = i;
      while (message[i] &&
             !isspace(message[i]) &&
             message[i] != '|' &&
             message[i] != '<' &&
             message[i] != '>') {
        i++;
      }
      tokens.data[tokens.count++] =
          (Token){TOK_WORD, &message[start], i - start};
    }

    if (tokens.count == tokens.capacity) {
      tokens.capacity *= 2;
      tokens.data = realloc(tokens.data, tokens.capacity * sizeof(Token));
    }
  }

  tokens.data[tokens.count++] = (Token){TOK_EOF, NULL, 0};
  return tokens;
}

/* =======================
   AST STRUCTURES (UNCHANGED)
   ======================= */

typedef struct ASTnode ASTnode;

typedef enum {
  CMD_SIMPLE,
  CMD_PIPELINE,
  CMD_REDIRECT_IN,
  CMD_REDIRECT_OUT,
  CMD_REDIRECT_APPEND
} CommandType;

typedef struct {
  char *name;
  char **args;
} Command;

typedef struct {
  ASTnode **stages;
  int count;
} Pipeline;

typedef enum {
  REDIR_IN,
  REDIR_OUT,
  REDIR_APPEND
} RedirectType;

typedef struct {
  RedirectType type;
  ASTnode *child;
  char *file;
} Redirect;

typedef union {
  Command *simple;
  Pipeline *pipeline;
  Redirect *redirect;
} CommandData;

struct ASTnode {
  CommandType type;
  CommandData data;
};

/* =======================
   AST HELPERS (UNCHANGED)
   ======================= */

Command *cmd_init(void) {
  Command *cmd = malloc(sizeof(Command));
  cmd->name = NULL;
  cmd->args = NULL;
  return cmd;
}

Pipeline *pipeline_init(void) {
  Pipeline *p = malloc(sizeof(Pipeline));
  p->stages = NULL;
  p->count = 0;
  return p;
}

RedirectType token_to_redir_type(TokenType t) {
  if (t == TOK_REDIR_IN) return REDIR_IN;
  if (t == TOK_REDIR_OUT) return REDIR_OUT;
  return REDIR_APPEND;
}

CommandType redir_to_cmd_type(RedirectType t) {
  if (t == REDIR_IN) return CMD_REDIRECT_IN;
  if (t == REDIR_OUT) return CMD_REDIRECT_OUT;
  return CMD_REDIRECT_APPEND;
}

ASTnode *ast_simple(Command *c) {
  ASTnode *n = malloc(sizeof(ASTnode));
  n->type = CMD_SIMPLE;
  n->data.simple = c;
  return n;
}

ASTnode *ast_pipeline(Pipeline *p) {
  ASTnode *n = malloc(sizeof(ASTnode));
  n->type = CMD_PIPELINE;
  n->data.pipeline = p;
  return n;
}

ASTnode *ast_redir(Redirect *r) {
  ASTnode *n = malloc(sizeof(ASTnode));
  n->type = redir_to_cmd_type(r->type);
  n->data.redirect = r;
  return n;
}

void add_arg(Command *cmd, Token t) {
  int argc = 0;
  if (cmd->args)
    while (cmd->args[argc]) argc++;

  cmd->args = realloc(cmd->args, sizeof(char*) * (argc + 2));
  cmd->args[argc] = strndup(t.start, t.length);
  cmd->args[argc + 1] = NULL;
}

void pipeline_add(Pipeline *p, ASTnode *n) {
  p->stages = realloc(p->stages, sizeof(ASTnode*) * (p->count + 1));
  p->stages[p->count++] = n;
}

/* =======================
   FIXED PARSER (CORE FIX)
   ======================= */

ASTnode *parser(TokenArray tokens) {
  ASTnode *current = NULL;
  Pipeline *pipe = NULL;

  for (int i = 0; i < tokens.count; i++) {
    Token t = tokens.data[i];

    if (t.type == TOK_WORD) {
      if (!current || current->type != CMD_SIMPLE) {
        Command *cmd = cmd_init();
        cmd->name = strndup(t.start, t.length);
        current = ast_simple(cmd);
      } else {
        add_arg(current->data.simple, t);
      }
    }

    else if (t.type == TOK_PIPE) {
      if (!current) return NULL;
      if (!pipe) pipe = pipeline_init();
      pipeline_add(pipe, current);
      current = NULL;
    }

    else if (t.type == TOK_REDIR_IN ||
             t.type == TOK_REDIR_OUT ||
             t.type == TOK_REDIR_APPEND) {

      if (!current) return NULL;
      i++;

      if (tokens.data[i].type != TOK_WORD) return NULL;

      Redirect *r = malloc(sizeof(Redirect));
      r->type = token_to_redir_type(t.type);
      r->child = current;
      r->file = strndup(tokens.data[i].start,
                        tokens.data[i].length);

      current = ast_redir(r);
    }

    else if (t.type == TOK_EOF) break;
  }

  if (pipe) {
    if (!current) return NULL;
    pipeline_add(pipe, current);
    return ast_pipeline(pipe);
  }

  return current;
}

/* =======================
   AST EXECUTION
   ======================= */

void exec_ast(ASTnode *n) {
  if (!n) return;

  if (n->type == CMD_SIMPLE) {
    execvp(n->data.simple->name, n->data.simple->args);
    perror("exec");
    _exit(1);
  }

  if (n->type == CMD_PIPELINE) {
    int in = STDIN_FILENO;
    for (int i = 0; i < n->data.pipeline->count; i++) {
      int fd[2];
      pipe(fd);

      if (!fork()) {
        dup2(in, STDIN_FILENO);
        if (i < n->data.pipeline->count - 1)
          dup2(fd[1], STDOUT_FILENO);
        close(fd[0]);
        exec_ast(n->data.pipeline->stages[i]);
      }

      close(fd[1]);
      in = fd[0];
    }
    while (wait(NULL) > 0);
  }

  if (n->type == CMD_REDIRECT_IN ||
      n->type == CMD_REDIRECT_OUT ||
      n->type == CMD_REDIRECT_APPEND) {

    Redirect *r = n->data.redirect;
    int fd;

    if (n->type == CMD_REDIRECT_IN)
      fd = open(r->file, O_RDONLY);
    else if (n->type == CMD_REDIRECT_OUT)
      fd = open(r->file, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    else
      fd = open(r->file, O_WRONLY|O_CREAT|O_APPEND, 0644);

    dup2(fd,
          n->type == CMD_REDIRECT_IN ? STDIN_FILENO : STDOUT_FILENO);
    close(fd);
    exec_ast(r->child);
  }
}

/* =======================
   MAIN LOOP
   ======================= */

int main(void) {
  char *line = NULL;
  size_t n = 0;

  while (1) {
    printf("> ");
    if (getline(&line, &n, stdin) == -1) break;

    TokenArray tokens = tokenize(line);
    ASTnode *ast = parser(tokens);

    if (!ast) {
      fprintf(stderr, "syntax error\n");
      free(tokens.data);
      continue;
    }

    if (!fork())
      exec_ast(ast);
    wait(NULL);

    free(tokens.data);
  }
}
