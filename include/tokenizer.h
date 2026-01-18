#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "common.h"

ErrorCode tokenize(TokenArray *tokens, const char *input);
void free_tokens(TokenArray *tokens);

#endif
