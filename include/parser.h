#ifndef PARSER_H
#define PARSER_H

#include "common.h"

ErrorCode parser(ASTnode **ast, TokenArray tokens);
void free_ast(ASTnode *ast);

#endif
