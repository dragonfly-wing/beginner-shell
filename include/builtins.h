#ifndef BUILTINS_H
#define BUILTINS_H

#include "common.h"

typedef struct {
  char *name;
  ErrorCode (*func)(char **argv);
} Builtin;

extern Builtin builtins[];

ErrorCode sh_cd(char **args);
ErrorCode sh_help(char **args);
ErrorCode sh_exit(char **args);

#endif
