#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include "string.h"
#include "executor.h"
#include "builtins.h"

ErrorCode ast_executor(ASTnode *ast) {
  int n = ast->pipe->count;
  int pipes[n-1][2];
  pid_t pids[n];

  if (n == 1) {
    for (Builtin *b = builtins; b->name; b++)
      if (!strcmp(ast->pipe->commands[0]->argv[0], b->name))
        return b->func(ast->pipe->commands[0]->argv);
  }

  for (int i = 0; i < n-1; i++)
    if (pipe(pipes[i]) < 0) return E_SYSCALL;

  for (int i = 0; i < n; i++) {
    if ((pids[i] = fork()) == 0) {
      if (i > 0) dup2(pipes[i-1][0], STDIN_FILENO);
      if (i < n-1) dup2(pipes[i][1], STDOUT_FILENO);

      for (int j = 0; j < ast->pipe->commands[i]->redir_count; j++) {
        Redirect r = ast->pipe->commands[i]->redirs[j];
        int fd = open(r.filename,
          r.type == REDIR_IN ? O_RDONLY :
          r.type == REDIR_APPEND ? O_WRONLY|O_CREAT|O_APPEND :
          O_WRONLY|O_CREAT|O_TRUNC, 0644);
        dup2(fd, r.type == REDIR_IN ? STDIN_FILENO : STDOUT_FILENO);
        close(fd);
      }

      for (int j = 0; j < n-1; j++) close(pipes[j][0]), close(pipes[j][1]);
      execvp(ast->pipe->commands[i]->argv[0],
             ast->pipe->commands[i]->argv);
      perror("exec");
      _exit(1);
    }
  }

  for (int i = 0; i < n-1; i++) close(pipes[i][0]), close(pipes[i][1]);
  for (int i = 0; i < n; i++) waitpid(pids[i], NULL, 0);
  return E_OK;
}
