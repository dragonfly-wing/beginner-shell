#include <ctype.h>
#include <stdlib.h>
#include "tokenizer.h"

static ErrorCode check_capacity(TokenArray *tok) {
  if (tok->count < tok->capacity) return E_OK;
  tok->capacity *= 2;
  Token *tmp = realloc(tok->data, tok->capacity * sizeof(Token));
  if (!tmp) return E_ALLOC;
  tok->data = tmp;
  return E_OK;
}

void free_tokens(TokenArray *tok) {
  free(tok->data);
  tok->data = NULL;
  tok->count = tok->capacity = 0;
}

ErrorCode tokenize(TokenArray *tokens, const char *input) {
  tokens->count = 0;
  tokens->capacity = 8;
  tokens->data = malloc(tokens->capacity * sizeof(Token));
  if (!tokens->data) return E_ALLOC;

  for (int i = 0; input[i]; ) {
    if (isspace(input[i])) { i++; continue; }

    if (check_capacity(tokens) != E_OK) goto fail;

    if (input[i] == '|') {
      tokens->data[tokens->count++] = (Token){TOK_PIPE, &input[i++], 1};
    } else if (input[i] == '>' && input[i+1] == '>') {
      tokens->data[tokens->count++] = (Token){TOK_REDIR_APPEND, &input[i], 2};
      i += 2;
    } else if (input[i] == '>') {
      tokens->data[tokens->count++] = (Token){TOK_REDIR_OUT, &input[i++], 1};
    } else if (input[i] == '<') {
      tokens->data[tokens->count++] = (Token){TOK_REDIR_IN, &input[i++], 1};
    } else {
      int start = i;
      while (input[i] && !isspace(input[i]) &&
             input[i] != '|' && input[i] != '<' && input[i] != '>') i++;
      tokens->data[tokens->count++] =
        (Token){TOK_WORD, &input[start], i - start};
    }
  }

  tokens->data[tokens->count++] = (Token){TOK_EOF, NULL, 0};
  return E_OK;

fail:
  free_tokens(tokens);
  return E_ALLOC;
}
