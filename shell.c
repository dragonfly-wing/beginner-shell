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

//Function declarations
ErrorCode read_line(char **line);
ErrorCode split_line(char *line, TokenArray *tok);
ErrorCode execute(char **args);
ErrorCode sh_launch(char **args);
ErrorCode split_pipe(char *line, char ***pipe_args, int *count_args);
ErrorCode pipe_execute(char ***pipe_args, int count);

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

    else {
      int start = i;
      
      while(message[i] && !isspace(message[i]) && message[i]!='|'){
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

//------------------ 
//Function for cd 
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

char *trim(char *str) {
    while(*str && isspace(*str)) str++; // skip leading spaces
    char *end = str + strlen(str) - 1;
    while(end > str && isspace(*end)) *end-- = '\0'; // remove trailing spaces
    return str;
}

ErrorCode split_pipe(char *line, char ***pipe_commands, int *count_args) {
    char **pipe_cmds = NULL;    //to prevent the wastage the space, we dont malloc with fixed size 
    int count = 0;

    char *cmd = strtok(line , "|");
    while(cmd!=NULL)
        {   
        
            char **tmp= realloc(pipe_cmds , (count+2) * sizeof(char*));
            if(tmp==NULL)
                {
                    for(int i=0;i<count;i++)
                        {
                            free(pipe_cmds[i]);
                        }
                    free(pipe_cmds);
                    return E_ALLOC;
                }

            pipe_cmds=tmp;
            pipe_cmds[count] = strdup(trim(cmd));
            if (!pipe_cmds[count]) {
                for (int i = 0; i < count; i++) {
                    free(pipe_cmds[i]);
                }
                free(pipe_cmds);
                return E_ALLOC;
            }
      
            count++;
            cmd = strtok(NULL , "|");
        }
    pipe_cmds[count] = NULL;
    *count_args=count;
    *pipe_commands = pipe_cmds;
    return E_OK;
}

//to execute pipe.
ErrorCode pipe_execute(char ***pipe_args, int count)
{
    int pipes[count-1][2];
    pid_t pids[count];

    //creating the pipes 
    for(int i=0;i<count-1;i++)      //we only need n-1 pipes because the last command writes to the terminal and does not need a pipe , and hence count-1
        {
            if(pipe(pipes[i])==-1)
                {
                    for (int j = 0; j < i; j++) {
                        close(pipes[j][0]);
                        close(pipes[j][1]);
                    }
                    return E_SYSCALL;
                }
        }

    //fork processes 
    for(int i=0;i<count;i++)    //count, because we need child process for all the commands 
        {
            pids[i]=fork();
            if (pids[i] == -1) {
                for (int j = 0; j < i; j++) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }
                return E_FORK;
            }

            if(pids[i]==0)          //child process 
                {
                    //if not the first command, get the input from the previous pipe ie make the stdin point to the read end
                    if(i>0)
                        {
                            if (dup2(pipes[i - 1][0], STDIN_FILENO) < 0) {
                                perror("sh");
                                _exit(EXIT_FAILURE);
                            }
                        }
            
                    //if not last command, output to the next pipe  
                    if(i<count-1)
                        {
                            if (dup2(pipes[i][1], STDOUT_FILENO) < 0) {
                                perror("sh");
                                _exit(EXIT_FAILURE);
                            }    //you are connecting the stdout to the write end 
                        }

                    for (int j=0;j<count-1;j++)             //all processes which access the fd has to close them for an eof to be sent. otherwise, the read operation is never performed.
                        {
                            close(pipes[j][0]);
                            close(pipes[j][1]);
                        }

                    execvp(pipe_args[i][0], pipe_args[i]);  //the format of execvp is like ls, ls -l -a.
                    perror("sh");
                    _exit(EXIT_FAILURE);
                }
        }
    for(int i=0;i<count-1;i++)
        {
            close(pipes[i][0]);
            close(pipes[i][1]);
        }
    
    for(int i=0;i<count;i++)
        {
            waitpid(pids[i], NULL, 0);
        }

    return E_OK;
}

//to parse the inputs into tokens
#define BUFSIZE 64                  //the initial size of the array
#define DELIIMITER " \n\t\a\r"      //the delimiters to look for when parsing

ErrorCode split_line(char *line, TokenArray *tok)
{   
    int buf_size = BUFSIZE;
    int position=0;
    char **tokens = malloc(buf_size * sizeof(char*));
    char *token;
    
    if(!tokens)
        {
            return E_ALLOC;
        }

    token = strtok(line, DELIIMITER);   //this takes the first token, and when it finds a delimiter, it adds \0 after the token it read.

    while(token != NULL)
        {
      
            if(position+1>=buf_size)
                {   
                    buf_size += BUFSIZE;
                    char **tmp = realloc(tokens, buf_size * sizeof(char*));

                    if(tmp==NULL)
                        {
                            for(int i=0;i<position;i++)
                                {
                                    free(tokens[i]);
                                }
                            free(tokens);
                            return E_ALLOC;
                        }
                    tokens=tmp;
                }
            tokens[position] = strdup(token);
            if (!tokens[position]) {
                for (int i = 0; i < position; i++) {
                    free(tokens[i]);
                }
                free(tokens);
                return E_ALLOC;
            }
            //here, the strtok returns a pointer to the original string, and we can directly do this because we dont modify the original command. strdup does malloc and then strcpy so that it is an independant copy and not pointers.
            position++;

            token = strtok(NULL, DELIIMITER);   //since the last parsed token is followed by a \0, ie NULL, we have to starting splitting from that. hence the NULL.
        }

    tokens[position] = NULL; // indicating the end of the tokens.
    *args = tokens;
    return E_OK;
}

ErrorCode execute(char **args)
{   
    CommandEntry *entry;

    if(args[0]==NULL)
        {   
            //empty command.
            return E_OK;
        }

    for(entry=commands ; entry->name!=NULL; entry++){
        if(strcmp(args[0], entry->name)==0){
            return entry->built_in_func(args);
        }
    }
    
    return sh_launch(args);     //if it is not a built in command, then execute it normally using fork.
}

ErrorCode sh_launch(char **args)
{
    pid_t pid;
    int status;

    pid = fork();
    if (pid == 0) // child process
        {
            if(execvp(args[0] , args )==-1)
                {
                    perror("sh");
                    _exit(EXIT_FAILURE);
                }
        }

    else if (pid < 0) // error forking
        {
            return E_FORK;
        }

    else            //parent process   
        {
            do 
                {
                    waitpid(pid, &status , WUNTRACED);
                }while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }
    return E_OK;   //here, the return value is always 1,to continue the execution of the shell. this is then used as status in the loop function.
}

typedef enum { REDIR_OUTPUT, REDIR_INPUT, REDIR_APPEND } RedirType;

typedef struct {
    char *cmd;
    char *file;
    char **args;
    RedirType type;
} Redir;

int operator_length(RedirType type) {
    switch (type) {
    case REDIR_APPEND:
        return 2;
    case REDIR_INPUT:
        return 1;
    case REDIR_OUTPUT:
        return 1;
    default:
        return 0;
    }
}

// this version only works for simple cmd>file type redirections.
// parsing functionality for append >> has been added.
ErrorCode parse_redir(char *line, Redir *r) {
    Redir r_op = {0};
    char *op = NULL;
    char *file_part = NULL;

    if ((op=strstr(line, ">>"))) {
        r_op.type = REDIR_APPEND;

    } else if ((op=strchr(line, '>'))) {
        r_op.type = REDIR_OUTPUT;
    } else if ((op=strchr(line, '<'))) {
        r_op.type = REDIR_INPUT;
    }

    if (!op) {
        return E_SYNTAX;
    }

    *op = '\0';
    r_op.cmd = strdup(trim(line));
    if (!r_op.cmd) {
        return E_ALLOC;
    }
    file_part = trim(op + operator_length(r_op.type));

    if (*file_part == '\0') {
        free(r_op.cmd);
        return E_SYNTAX;
    }
    r_op.file = strdup(file_part);
    if (!r_op.file) {
        free(r_op.cmd);
        return E_ALLOC;
    }

    *r = r_op;
    return E_OK;
}

ErrorCode redir_execution(Redir r) {
    pid_t pid;
    pid = fork();
  
    //open() has to be used only in the child.
    // int fd = open(r.file, O_WRONLY | O_CREAT | O_TRUNC , 0644);

    if (pid == -1) {
        return E_FORK;
    }
  
    if (pid == 0) {
        if (r.type == REDIR_OUTPUT) {
            int fd = open(r.file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("sh");
                _exit(EXIT_FAILURE);
            }
            if (dup2(fd, STDOUT_FILENO) < 0) {
                perror("sh");
                _exit(EXIT_FAILURE);
            }
            // close the fds after opening
            close(fd);

            execvp(r.args[0], r.args);
            perror("sh");
            _exit(EXIT_FAILURE);
      
        } else if(r.type==REDIR_INPUT) {
            int fd = open(r.file, O_RDONLY);
            if (fd < 0) {
                perror("sh");
                _exit(EXIT_FAILURE);
            }
      
            if (dup2(fd, STDIN_FILENO)<0) {
                perror("sh");
                _exit(EXIT_FAILURE);
            }
            close(fd);

            execvp(r.args[0], r.args);
            perror("sh");
            _exit(EXIT_FAILURE);
        } else if (r.type == REDIR_APPEND) {
            int fd = open(r.file, O_WRONLY | O_CREAT | O_APPEND, 0644);    //always pass modes when using O_CREAT (like 0644)
            if (fd < 0) {
                perror("sh");
                _exit(EXIT_FAILURE);
            }
      
            if (dup2(fd, STDOUT_FILENO)<0) {
                perror("sh");
                _exit(EXIT_FAILURE);
            }
            close(fd);
      
            execvp(r.args[0], r.args);
            perror("sh");
            _exit(EXIT_FAILURE);
        }
    }

    if (pid > 0) {
        waitpid(pid,NULL,0);
    }
    return E_OK;
}

void free_args(char **args) {
    if (!args) {
        return;
    }
    for (int i = 0; args[i] != NULL; i++) {
        free(args[i]);
    }
    free(args);

}

void free_pipe_args(char ***pipe_args) {
    if (!pipe_args) {
        return;
    }
    for (int i = 0; pipe_args[i] != NULL; i++) {
        for (int j = 0; pipe_args[i][j] != NULL; j++) {
            free(pipe_args[i][j]);
        }
        free(pipe_args[i]);
    }
    free(pipe_args);
}

void loop(void) {
  Redir r = {0};
  TokenArray tokens ={0};
  ErrorCode err;
  
  char *line=NULL;
  char **args = NULL;

  char **pipe_commands = NULL;
  char ***pipe_args = NULL;
  int count;

  int status=1;
    
  do {
    printf(">");
    err = read_line(&line);

    if (error_handler(err) == LOOP_EXIT) {
      goto cleanup;
    }
    if (!line) {
      continue;
    }
    
    //for handling pipe commands
    if (strchr(line, '|')) {
      err = split_pipe(line, &pipe_commands,&count);
      if (err != E_OK) {
        if (error_handler(err) == LOOP_EXIT) {
          goto cleanup;
        }
        continue;
      }

      pipe_args = malloc((count + 1) * sizeof(char **));
      if (!pipe_args) {
        goto cleanup;
      }
      pipe_args[count] = NULL;
      
      for (int i = 0; pipe_commands[i] != NULL; i++) {
        err = split_line(pipe_commands[i], &pipe_args[i]);
        if (err != E_OK) {
          if (error_handler(err) == LOOP_EXIT) {
            goto cleanup;
          }
          goto pipe_fail;
        }
      }

      err = pipe_execute(pipe_args, count);
      if(err!=E_OK){
        if (error_handler(err) == LOOP_EXIT) {
          goto cleanup;
        }
        goto pipe_fail; 
      }

    pipe_fail:
      free_pipe_args(pipe_args);
      free_args(pipe_commands);
      pipe_commands=NULL;
      pipe_args = NULL;
      continue;
    }

    // for simple cmd > file type redirection 
    else if ((strchr(line, '>')) || (strchr(line, '<')) ) {
      err = parse_redir(line, &r);
      if (err != E_OK) {
        if (error_handler(err) == LOOP_EXIT) {
          goto cleanup;
        }
        continue;
      }

      err = split_line(r.cmd, &r.args);
      if(err!=E_OK){
        if (error_handler(err) == LOOP_EXIT) {
          goto cleanup;
        }
        continue;
      }

      err = redir_execution(r);
      if(err!=E_OK){
        if (error_handler(err) == LOOP_EXIT) {
          goto cleanup;
        }
        continue;
      }
            
      for (int i = 0; r.args[i] != NULL; i++) {
        free(r.args[i]);
      }
      free_args(r.args);
      free(r.cmd);
      free(r.file);
    }

    // for handling normal commands
    else {
      err = split_line(line, &tokens);  
      if(err!=E_OK){

        if (error_handler(err) == LOOP_EXIT){
          goto cleanup;
        }
        continue;
      }
      
      err = execute(args);
      if(err!=E_OK){
        if (error_handler(err) == LOOP_EXIT){
          goto cleanup;
        }
        continue;
      }
      free_args(args);
      args=NULL;
    }
    
    free(line);
    line = NULL; // defensive style: setting unused pointers to NULL.
  } while (status);
  
 cleanup:
  free(line);
  free_args(args);
  free_args(pipe_commands);
  free_pipe_args(pipe_args);
  free(r.cmd);
  free_args(r.args);
  free(r.file);
  free_tokens(&tokens);
  return;
}

int main(int argc, char **argv)
{
  //run the command loop
  loop();
  return EXIT_SUCCESS;
}

