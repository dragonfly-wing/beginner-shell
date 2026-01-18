#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "builtins.h"

ErrorCode sh_cd(char **args) {
  if (!args[1]) {
    if (chdir(getenv("HOME")) < 0) return E_SYSCALL;
  } else {
    if (chdir(args[1]) < 0) return E_SYSCALL;
  }
  return E_OK;
}

ErrorCode sh_help(char **args) {
  (void)args;
  printf("Built-in commands:\n");
  for (Builtin *b = builtins; b->name; b++)
    printf("  %s\n", b->name);
  return E_OK;
}

ErrorCode sh_exit(char **args) {
  (void)args;
  return E_EXIT;
}

Builtin builtins[] = {
  {"cd", sh_cd},
  {"help", sh_help},
  {"exit", sh_exit},
  {NULL, NULL}
};
