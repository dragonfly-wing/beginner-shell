//shell
#include <string.h>
#include <unistd.h>     //this is for fork pipe and stuff
#include <sys/wait.h>   //this for functions like waitpid (which is a library function)
#include <stdio.h>      //for perror and printf
#include <stdlib.h>     //for exit()
#include <sys/types.h>  //for datatypes such as ssize_t and pid_t
#include <ctype.h>
#include <sys/stat.h>   //for open() for files.
#include <fcntl.h>

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

typedef enum { LOOP_CONTINUE, LOOP_EXIT } LoopVariable;

//enum for token types
typedef enum {
  TOK_WORD,
  TOK_PIPE,
  TOK_REDIR_OUT,
  TOK_REDIR_IN,
  TOK_REDIR_APPEND,
  TOK_EOF
} TokenType;

//structure to represent the token
typedef struct {
  TokenType type;
  const char *start;
  int length;
} Token;

//structure to represent the array which stores the tokens.
typedef struct {
  Token *data;
  int count;
  int capacity;
} TokenArray;

//-------------------------
// parser utilities
//-------------------------

typedef enum { REDIR_IN, REDIR_OUT, REDIR_APPEND } RedirType;

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

typedef struct {
  Token *tokens;
  int count;
  int pos;
} Parser;


//Function declarations

ErrorCode sh_cd(char **args);
ErrorCode sh_help(char **args);
ErrorCode sh_exit(char **args);

//a struct to represent the built-in commands 
typedef struct {
    char *name;
    ErrorCode (*built_in_func)(char **args);
}CommandEntry;

//an array of structurs for the built-in commands
CommandEntry commands[] =  {
    {
        "cd",
        &sh_cd 
    },
    {
        "help",
        &sh_help 
    },
    {
        "exit",
        &sh_exit 
    },
    {
        NULL,
        0    //this is to represent the end
    }
};

typedef struct {
    const char *message;
} ErrorDesc;

const ErrorDesc error_table[E_MAX] = {
    [E_SYSCALL] = {"System call error"},
    [E_OK] = {"Success"},
    [E_EOF] = {"End of file."},
    [E_FORK] = {"Fork failed"},
    [E_ALLOC] = {"Allocation failed"},
    [E_SYNTAX]={"Syntax error"}
};


const char *error_lookup(ErrorCode code) {
    if (code < 0 || code >= E_MAX) {
        return "Invalid error code";
    }
    return error_table[code].message;
}

LoopVariable error_handler(ErrorCode code) {
    switch (code) {
    case E_OK:
        // fprintf(stderr,"sh%s\n",error_lookup(code));
        return LOOP_CONTINUE;
    case E_EOF:
        return LOOP_EXIT;
    case E_SYSCALL:
        fprintf(stderr,"sh:%s\n",error_lookup(code));
        return LOOP_CONTINUE;
    case E_ALLOC:
        fprintf(stderr, "sh:%s\n", error_lookup(code));
        return LOOP_CONTINUE;
    case E_FORK:
        fprintf(stderr, "sh:%s\n", error_lookup(code));
        return LOOP_EXIT;
    case E_EXIT:
        return LOOP_EXIT;
    case E_SYNTAX:
        fprintf(stderr,"sh:%s\n", error_lookup(code));
        return  LOOP_CONTINUE;
    default:
        fprintf(stderr, "sh:%s\n", error_lookup(code));
        return LOOP_CONTINUE;
    }
}



//-----------------------------------------------
// tokenizer
//-----------------------------------------------

ErrorCode check_capacity(TokenArray *tok) {
  if (tok->count < tok->capacity) {
    return E_OK;
  }
  tok->capacity *= 2;
  Token *tmp = realloc(tok->data, tok->capacity * sizeof(Token));
  if (!tmp) {
    return E_ALLOC;
  }
  tok->data = tmp;
  return E_OK;
}

void free_tokens(TokenArray *tok) {
  free(tok->data);
  tok->data = NULL;
  tok->capacity = 0;
  tok->count=0;
}

ErrorCode tokenize(TokenArray *tokens, const char *message) {
  ErrorCode err;
  tokens->count = 0;
  tokens->capacity = 8;
  tokens->data = malloc(tokens->capacity * sizeof(Token));
  
  if (!tokens->data) {
    return E_ALLOC;
  }

  int i = 0;
  while (message[i] != '\0') {
    if (isspace(message[i])) {
      i++;
      continue;
    }

    err = check_capacity(tokens);
    if (err != E_OK) {
      goto fail;
    }
    
    if (message[i] == '|') {
      tokens->data[tokens->count] = (Token){TOK_PIPE, &message[i], 1};
      i++;
    }

    else if (message[i] == '>' && message[i + 1] == '>') {
      tokens->data[tokens->count] = (Token){TOK_REDIR_APPEND, &message[i], 2};
      i += 2;
    } else if (message[i] == '>') {
      tokens->data[tokens->count] = (Token){TOK_REDIR_OUT, &message[i], 1};
      i++;
    } else if (message[i] == '<') {
      tokens->data[tokens->count] = (Token){TOK_REDIR_IN, &message[i], 1};
      i++;
    } else {
      int start = i;
      
      while(message[i] && !isspace(message[i]) && message[i]!='|' && message[i]!='>' && message[i]!='<'){
        i++;
      }
      int length = i - start;
      tokens->data[tokens->count]=(Token){TOK_WORD , &message[start],length};
    }
    tokens->count++;
  }

  err = check_capacity(tokens);
  if (err != E_OK) {
    goto fail;
  }
 
  tokens->data[tokens->count++]=(Token){TOK_EOF,NULL,0};
  return E_OK;

 fail:
  free_tokens(tokens);
  return err;
}

//===============================
// Parser
//===============================

ErrorCode cmd_init(Command **cmd) {
  Command *command = malloc(sizeof(Command));
  if (!command) {
    return E_ALLOC;
  }
  command->argv = NULL;
  command->argc = 0;
  command->redir_count = 0;
  command->redirs=NULL;

  *cmd = command;
  return E_OK;
}

ErrorCode add_arg(Command *cmd, Parser *p) {
  char **new_argv = realloc(cmd->argv, sizeof(char *) * (cmd->argc + 2));
  if (!new_argv) {
    return  E_ALLOC;
  }

  cmd->argv = new_argv;
  cmd->argv[cmd->argc] =
      strndup(p->tokens[p->pos].start, p->tokens[p->pos].length);
  if (!cmd->argv[cmd->argc]) {;
    return E_ALLOC;
  }
  cmd->argv[cmd->argc + 1] = NULL;
  cmd->argc++;
  return E_OK;
}

void free_cmd(Command *cmd) {
  for (int i = 0; i < cmd->argc; i++) {
    free(cmd->argv[i]);
  }
  free(cmd->argv);
  cmd->argv = NULL;
  
  for (int i = 0; i < cmd->redir_count; i++) {
    free(cmd->redirs[i].filename);
  }
  free(cmd->redirs);
  cmd->redirs = NULL;

  free(cmd);
}

ErrorCode parse_simple(Command **command,Parser *p) {
  ErrorCode err;
  if (p->pos >= p->count || p->tokens[p->pos].type != TOK_WORD) {
    return E_SYNTAX;
  }

  Command *cmd;
  err = cmd_init(&cmd);
  if (err != E_OK) {
    goto fail;
  }

  while (p->pos < p->count && p->tokens[p->pos].type == TOK_WORD) {
    err = add_arg(cmd, p);
    if (err != E_OK) {
      free_cmd(cmd);
      goto fail;
    }
    p->pos++;
  }
  *command=cmd;

fail:
  return err;
}

RedirType TokenType_to_RedirType(TokenType type) {
  switch (type) {
  case TOK_REDIR_APPEND:
    return REDIR_APPEND;
  case TOK_REDIR_IN:
    return REDIR_IN;
  case TOK_REDIR_OUT:
    return REDIR_OUT;
  default:
    return -1;
  }
}

ErrorCode add_redir(Command *cmd, Parser *p, RedirType r) {
  Redirect *new_redirs =
      realloc(cmd->redirs, sizeof(Redirect) * (cmd->redir_count + 1));
  if (!new_redirs) {
    return E_ALLOC;
  }

  cmd->redirs = new_redirs;
  cmd->redirs[cmd->redir_count].filename =
      strndup(p->tokens[p->pos].start, p->tokens[p->pos].length);
  if (!cmd->redirs[cmd->redir_count].filename) {
    return E_ALLOC;
  }
  cmd->redirs[cmd->redir_count].type = r;
  cmd->redir_count++;
  return E_OK;
}

ErrorCode parse_redir(Command **command,Parser *p) {
  Command *cmd;
  ErrorCode err;
  err = parse_simple(&cmd, p);
  if (err != E_OK) {
    goto fail;
  }
  while (p->pos < p->count && (p->tokens[p->pos].type == TOK_REDIR_APPEND ||
                               p->tokens[p->pos].type == TOK_REDIR_IN ||
                               p->tokens[p->pos].type == TOK_REDIR_OUT)) {
    RedirType r = TokenType_to_RedirType(p->tokens[p->pos].type);
    p->pos++;

    if (p->pos >= p->count || p->tokens[p->pos].type != TOK_WORD) {
      free_cmd(cmd);
      return E_SYNTAX;
    }

    err = add_redir(cmd, p, r);
    if (err != E_OK) {
      free_cmd(cmd);
      goto fail;
    }
    p->pos++;
  }
  
  *command = cmd;
  return E_OK;
  
fail:
  return err;
}

ErrorCode pipeline_init(Pipeline **pipeline) {
  Pipeline *pipe = malloc(sizeof(Pipeline));
  if (!pipe) {
    return E_ALLOC;
  }
  pipe->commands = NULL;
  pipe->count = 0;

  *pipeline = pipe;
  return E_OK;
}

ErrorCode add_pipe(Command *cmd, Pipeline *pipe) {
  Command **new_cmds =
      realloc(pipe->commands, sizeof(Command *) * (pipe->count + 1));
  if (!new_cmds) {
    return E_ALLOC;
  }
  pipe->commands = new_cmds;
  pipe->commands[pipe->count] = cmd;
  pipe->count++;
  return E_OK;
}

void free_pipe(Pipeline *pipe) {
  for (int i = 0; i < pipe->count; i++) {
    free_cmd(pipe->commands[i]);
  }
  free(pipe->commands);
  free(pipe);
}

void free_ast(ASTnode *ast) { free_pipe(ast->pipe); }

ErrorCode parse_pipe(Pipeline **pipeline , Parser *p) {
  Pipeline *pipe;
  ErrorCode err;
  err = pipeline_init(&pipe);
  if (err!=E_OK) {
    goto fail;
  }
  Command *cmd;
  err = parse_redir(&cmd, p);
  if (err != E_OK) {
    free_pipe(pipe);
    goto fail;
  }
  err = add_pipe(cmd, pipe);
  if (err != E_OK) {
    free_cmd(cmd);
    free_pipe(pipe);
    goto fail;
  }
  while (p->pos < p->count && p->tokens[p->pos].type == TOK_PIPE) {
    p->pos++;

    if (p->pos >= p->count || p->tokens[p->pos].type != TOK_WORD) {
      free_cmd(cmd);
      free_pipe(pipe);
      return E_SYNTAX;
    }

    err = parse_redir(&cmd, p);
    if (err != E_OK) {
      free_pipe(pipe);
      goto fail;
    }

    err = add_pipe(cmd, pipe);
    if (err != E_OK) {
      free_cmd(cmd);
      free_pipe(pipe);
      goto fail;
    }
  }
  *pipeline = pipe;
  return E_OK;
fail:
  return err;
}

ErrorCode parser(ASTnode **AstNode,TokenArray tokens) {
  Parser p;
  p.tokens = tokens.data;
  p.pos = 0;
  p.count = tokens.count;

  Pipeline *pipe;
  ErrorCode err;
  err = parse_pipe(&pipe, &p);
  if (err != E_OK) {
    goto fail;
  }
  ASTnode *ast = malloc(sizeof(ASTnode));
  if (!ast) {
    return E_ALLOC;
  }
  ast->pipe = pipe;
  *AstNode = ast;
  return E_OK;
  
fail:
  return err;
}

//------------------
// Function for cd
//------------------

ErrorCode sh_cd(char **args)
{
  if(args[1]==NULL)
    {
      if(chdir(getenv("HOME"))!=0)
        {
          return E_SYSCALL;
        }
    }
  else 
    {
      if(chdir(args[1])!=0)   //chdir changes the directory.
        {
          return E_SYSCALL;
        }
    }
  return E_OK;
}

//function to display the help mennu
ErrorCode sh_help(char **args)
{   
    CommandEntry *entry;
    printf("\nWelcome to sh shell!!!! (ik its such a generic name lmaooo)");
    printf("\nAvailable built in commands:\n");
    
    for(entry=commands ; entry->name!=NULL; entry++)
        {
            printf(" %s\n" , entry->name);
        }
    return E_OK;
}

//function to exit.
ErrorCode sh_exit(char **args)
{
    return E_EXIT;
}

//read input 
ErrorCode read_line(char **line)
{
    char *input_line = NULL;
    size_t buffer_size = 0; //ssize_t has larger capacity than int, and can also hold -1 for errors. initialising it to zero tells the compiler to dynamically allocate memory.

    if (getline(&input_line, &buffer_size, stdin) == -1) {
        free(input_line);
        if(feof(stdin))
            {
                return E_EOF;
            } else {
            return E_SYSCALL;
            // perror("sh:");
            //exit(EXIT_FAILURE);  //in case of errors during processes, exit is used to terminate the process.
        }
    }
    *line = input_line;
    return E_OK;
}

ErrorCode ast_executor(ASTnode *ast) {
  int count = ast->pipe->count;
  int pipes[count-1][2];
  pid_t pids[ast->pipe->count];

  CommandEntry *entry;

  if(count==1){
    for (entry = commands; entry->name != NULL; entry++) {
      if (strcmp(ast->pipe->commands[0]->argv[0], entry->name) == 0) {
        return entry->built_in_func(ast->pipe->commands[0]->argv);
      }
    }
  }
  
  
  for (int i = 0; i < count - 1; i++) {
    
    if (pipe(pipes[i]) == -1) {
 for (int j = 0; j < i; j++) {
        close(pipes[j][0]);
        close(pipes[j][1]);
      }
      return E_SYSCALL;
    }
  }

  for (int i = 0; i < count; i++) {
    pids[i] = fork();
    if (pids[i] == -1) {
      for (int j = 0; j < i; j++) {
        close(pipes[j][0]);
        close(pipes[j][1]);
      }
      return E_FORK;
    }

    if (pids[i] == 0) {
      if (i > 0) {
        if (dup2(pipes[i - 1][0], STDIN_FILENO) < 0) {
          perror("sh");
          _exit(EXIT_FAILURE);
        }
      }

      if (i < count - 1) {
        if (dup2(pipes[i][1], STDOUT_FILENO) < 0) {
          perror("sh");
          _exit(EXIT_FAILURE);
        }
      }

      for (int j = 0; j < ast->pipe->commands[i]->redir_count;  j++) {
        
        if (ast->pipe->commands[i]->redirs[j].type == REDIR_OUT) {
          int fd = open(ast->pipe->commands[i]->redirs[j].filename,
                        O_WRONLY | O_CREAT | O_TRUNC, 0644);
          if (fd < 0) {
            perror("sh");
            _exit(EXIT_FAILURE);
          }
          if (dup2(fd, STDOUT_FILENO) < 0) {
            perror("sh");
            _exit(EXIT_FAILURE);
          }
          close(fd);
        } else if (ast->pipe->commands[i]->redirs[j].type == REDIR_IN) {
          int fd = open(ast->pipe->commands[i]->redirs[j].filename, O_RDONLY);
          if (fd < 0) {
            perror("sh");
            _exit(EXIT_FAILURE);
          }
          if(dup2(fd, STDIN_FILENO)<0){
            perror("sh");
            _exit(EXIT_FAILURE);
          }
          close(fd);
        } else if (ast->pipe->commands[i]->redirs[j].type == REDIR_APPEND) {
          int fd = open(ast->pipe->commands[i]->redirs[j].filename,
                        O_WRONLY | O_CREAT | O_APPEND, 0644);
          if (fd < 0) {
            perror("sh");
            _exit(EXIT_FAILURE);
          }
          if (dup2(fd, STDOUT_FILENO) < 0) {
            perror("sh");
            _exit(EXIT_FAILURE);
          }
          close(fd);
        }
      }
      
      for (int j = 0; j < count - 1; j++) {
        close(pipes[j][0]);
        close(pipes[j][1]);
      }
      
      for (entry = commands; entry->name != NULL; entry++) {
        if (strcmp(ast->pipe->commands[i]->argv[0], entry->name) == 0) {
          entry->built_in_func(ast->pipe->commands[i]->argv);
          _exit(EXIT_SUCCESS);
        }
      }
      execvp(ast->pipe->commands[i]->argv[0], ast->pipe->commands[i]->argv);
      perror("sh");
      _exit(EXIT_FAILURE);
    }
  }

  for (int i = 0; i < count - 1; i++) {
    close(pipes[i][0]);
    close(pipes[i][1]);
  }

  for (int i = 0; i < count; i++) {
    waitpid(pids[i] , NULL,0);
  }
  
  return E_OK;
}

void loop(void) {
  ErrorCode err;
  char *line=NULL;

  // new logic with tokenizer and parser.
  TokenArray tokens = {0};
  ASTnode *ast = NULL;
  
  do {
    printf(">");
    err = read_line(&line);
    if (error_handler(err) == LOOP_EXIT) {
      goto cleanup;
    }
    if (!line) {
      continue;
    }
    err = tokenize(&tokens, line);
    if (error_handler(err) == LOOP_EXIT) {
      goto cleanup;
    }
    
    err = parser(&ast, tokens);
    if (error_handler(err) == LOOP_EXIT) {
      goto cleanup;
    }

    err = ast_executor(ast);
    free_ast(ast);
    ast=NULL;
    if (error_handler(err) == LOOP_EXIT) {
      goto cleanup;
    }
    
  }while(1);
  
 cleanup:
  free(line);
  free_tokens(&tokens);
  if (ast) {
    free_ast(ast);
  }
  return;
}

int main(int argc, char **argv)
{
  //run the command loop
  loop();
  return EXIT_SUCCESS;
}
