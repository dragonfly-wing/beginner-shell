#include <stdlib.h>
#include <string.h>
#include "parser.h"

typedef struct {
  Token *tokens;
  int count;
  int pos;
} Parser;

/* --- helpers --- */

static ErrorCode cmd_init(Command **cmd) {
  *cmd = calloc(1, sizeof(Command));
  return *cmd ? E_OK : E_ALLOC;
}

static ErrorCode add_arg(Command *cmd, Parser *p) {
  char **tmp = realloc(cmd->argv, sizeof(char*) * (cmd->argc + 2));
  if (!tmp) return E_ALLOC;
  cmd->argv = tmp;

  cmd->argv[cmd->argc] =
    strndup(p->tokens[p->pos].start, p->tokens[p->pos].length);
  if (!cmd->argv[cmd->argc]) return E_ALLOC;

  cmd->argv[++cmd->argc] = NULL;
  return E_OK;
}

static ErrorCode add_redir(Command *cmd, Parser *p, RedirType r) {
  Redirect *tmp = realloc(cmd->redirs,
      sizeof(Redirect) * (cmd->redir_count + 1));
  if (!tmp) return E_ALLOC;
  cmd->redirs = tmp;

  cmd->redirs[cmd->redir_count].type = r;
  cmd->redirs[cmd->redir_count].filename =
    strndup(p->tokens[p->pos].start, p->tokens[p->pos].length);
  if (!cmd->redirs[cmd->redir_count].filename) return E_ALLOC;

  cmd->redir_count++;
  return E_OK;
}

static void free_cmd(Command *cmd) {
  for (int i = 0; i < cmd->argc; i++) free(cmd->argv[i]);
  for (int i = 0; i < cmd->redir_count; i++)
    free(cmd->redirs[i].filename);
  free(cmd->argv);
  free(cmd->redirs);
  free(cmd);
}

/* --- parsing --- */

static RedirType tok_to_redir(TokenType t) {
  if (t == TOK_REDIR_IN) return REDIR_IN;
  if (t == TOK_REDIR_OUT) return REDIR_OUT;
  if (t == TOK_REDIR_APPEND) return REDIR_APPEND;
  return REDIR_INVALID;
}

static ErrorCode parse_simple(Command **out, Parser *p) {
  if (p->tokens[p->pos].type != TOK_WORD) return E_SYNTAX;
  Command *cmd;
  if (cmd_init(&cmd) != E_OK) return E_ALLOC;

  while (p->tokens[p->pos].type == TOK_WORD) {
    if (add_arg(cmd, p) != E_OK) { free_cmd(cmd); return E_ALLOC; }
    p->pos++;
  }
  *out = cmd;
  return E_OK;
}

static ErrorCode parse_redir(Command **out, Parser *p) {
  Command *cmd;
  if (parse_simple(&cmd, p) != E_OK) return E_SYNTAX;

  while (1) {
    RedirType r = tok_to_redir(p->tokens[p->pos].type);
    if (r == REDIR_INVALID) break;
    p->pos++;
    if (p->tokens[p->pos].type != TOK_WORD) {
      free_cmd(cmd); return E_SYNTAX;
    }
    if (add_redir(cmd, p, r) != E_OK) {
      free_cmd(cmd); return E_ALLOC;
    }
    p->pos++;
  }
  *out = cmd;
  return E_OK;
}

ErrorCode parser(ASTnode **ast, TokenArray tokens) {
  Parser p = { tokens.data, tokens.count, 0 };
  Pipeline *pipe = calloc(1, sizeof(Pipeline));
  if (!pipe) return E_ALLOC;

  Command *cmd;
  if (parse_redir(&cmd, &p) != E_OK) goto fail;
  pipe->commands = malloc(sizeof(Command*));
  pipe->commands[pipe->count++] = cmd;

  while (p.tokens[p.pos].type == TOK_PIPE) {
    p.pos++;
    if (parse_redir(&cmd, &p) != E_OK) goto fail;
    pipe->commands =
      realloc(pipe->commands, sizeof(Command*) * (pipe->count + 1));
    pipe->commands[pipe->count++] = cmd;
  }

  *ast = malloc(sizeof(ASTnode));
  (*ast)->pipe = pipe;
  return E_OK;

fail:
  for (int i = 0; i < pipe->count; i++) free_cmd(pipe->commands[i]);
  free(pipe->commands);
  free(pipe);
  return E_SYNTAX;
}

void free_ast(ASTnode *ast) {
  Pipeline *p = ast->pipe;
  for (int i = 0; i < p->count; i++) free_cmd(p->commands[i]);
  free(p->commands);
  free(p);
  free(ast);
}
