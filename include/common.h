#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>

typedef enum {
    E_OK = 0,
    E_EOF,
    E_SYSCALL,
    E_FORK,
    E_ALLOC,
    E_EXIT,
    E_SYNTAX,
    E_MAX
} ErrorCode;

typedef enum {
  TOK_WORD,
  TOK_PIPE,
  TOK_REDIR_OUT,
  TOK_REDIR_IN,
  TOK_REDIR_APPEND,
  TOK_EOF
} TokenType;

typedef enum {
  REDIR_IN,
  REDIR_OUT,
  REDIR_APPEND,
  REDIR_INVALID
} RedirType;

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

typedef struct {
  RedirType type;
  char *filename;
} Redirect;

typedef struct {
  char **argv;
  int argc;
  Redirect *redirs;
  int redir_count;
} Command;

typedef struct {
  Command **commands;
  int count;
} Pipeline;

typedef struct {
  Pipeline *pipe;
} ASTnode;

typedef enum {
  LOOP_CONTINUE,
  LOOP_EXIT
} LoopVariable;

/* error handling */
LoopVariable error_handler(ErrorCode code);


#endif
