
//shell
#include <string.h>
#include<unistd.h>   //this is for fork pipe and stuff
#include<sys/wait.h> //this for functions like waitpid (which is a library function)
#include<stdio.h>    //for perror and printf 
#include<stdlib.h>  //for exit()
#include<sys/types.h> //for datatypes such as ssize_t and pid_t 
#include<ctype.h>

//function declarations
char *read_line(void);
char **split_line(char *line);
int execute(char **args);
int sh_launch(char **args);
char ***split_pipe(char *line, int *count);

int sh_cd(char **args);
int sh_help(char **args);
int sh_exit(char **args);

/* array of the builtins 
char *built_in[] = {"cd", "help", "exit"};

to get the size of the built_ins array
int builtin_size()
{
    return sizeof(built_in) / sizeof(char*);
};

//to call the approprotiate functions for the built_ins.
int (*built_in_func[])(char **args)  = { &sh_cd ,  &sh_help , &sh_exit};  //here, this is an array of pointers to functions of parameter type char **args, and return type int.
*/

//a struct to represent the built-in commands 
typedef struct {
    char *name;
    int (*built_in_func)(char **args);
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

//function for cd 
int sh_cd(char **args)
{
    if(args[1]==NULL)
    {
        if(chdir(getenv("HOME"))!=0)
        {
            perror("sh"); 
        }
    }
    
    else 
    {
        if(chdir(args[1])!=0)   //chdir changes the directory.
        {
            perror("sh");
        }
    }
    return 1;
}

//function to display the help mennu
int sh_help(char **args)
{   
    CommandEntry *entry;
    printf("\nWelcome to sh shell!!!! (ik its such a generic name lmaooo)");
    printf("\nAvailable built in commands:\n");
    
    for(entry=commands ; entry->name!=NULL; entry++)
    {
        printf(" %s\n" , entry->name);
    }
     return 1;
}

//function to exit.
int sh_exit(char **args)
{
    return 0;
}

//read input 
char *read_line(void)
{
    char *line = NULL;
    ssize_t buffer_size = 0; //ssize_t has larger capacity than int, and can also hold -1 for errors. initialising it to zero tells the compiler to dynamically allocate memory.

    if(getline(&line, &buffer_size , stdin)==-1)
    {
       if(feof(stdin))
       {
           exit(EXIT_SUCCESS);  //received an eof -  user interuppt
       }
       else 
       {
           perror("sh:");
           exit(EXIT_FAILURE);  //in case of errors during processes, exit is used to terminate the process.
       }
    }

    return line;
}

char *trim(char *str) {
    while(*str && isspace(*str)) str++; // skip leading spaces
    char *end = str + strlen(str) - 1;
    while(end > str && isspace(*end)) *end-- = '\0'; // remove trailing spaces
    return str;
}


char ***split_pipe(char *line,  int *count_arg)
{
    char **pipe_cmds = NULL;    //to prevent the wastage the space, we dont malloc with fixed size 
    char **pipe_cmds_temp=NULL;
    int count = 0;

    char *cmd = strtok(line , "|");
    while(cmd!=NULL)
    {   
        
        pipe_cmds_temp= realloc(pipe_cmds , (count+1) * sizeof(char*));
        if(!pipe_cmds_temp)
        {
            perror("sh: realloc error");
            for(int i=0;i<count;i++)
            {
                free(pipe_cmds[i]);
            }
            exit(EXIT_FAILURE);
        }

        pipe_cmds=pipe_cmds_temp;
        pipe_cmds[count] = strdup(trim(cmd));        
        count++;
        cmd = strtok(NULL , "|");
    }
    
    char ***pipe_token = malloc(count * sizeof(char**));
    if(!pipe_token)
    {
        perror("sh");

        for(int i=0;i<count;i++)
        {
           free(pipe_cmds[i]); 
        }

        free(pipe_cmds);
        exit(EXIT_FAILURE);
    }

    for(int i=0; i<count;i++)
    {
        pipe_token[i] = split_line(pipe_cmds[i]);   //split each command into tokens 
        free(pipe_cmds[i]);     //free temporary command string
    }
    free(pipe_cmds);        //free the array itself

    *count_arg  =count;
    return pipe_token;
}

//to execute pipe.
int pipe_execute(char ***pipe_args, int count)
{
    int pipes[count-1][2];
    pid_t pids[count];

    //creating the pipes 
    for(int i=0;i<count-1;i++)      //we only need n-1 pipes because the last command writes to the terminal and does not need a pipe , and hence count-1
    {
        if(pipe(pipes[i])==-1)
        {
            perror("sh");
            exit(EXIT_FAILURE);
        }
    }

    //fork processes 
    for(int i=0;i<count;i++)    //count, because we need child process for all the commands 
    {
        pids[i]=fork();
        if(pids[i]==-1)
        {
            perror("sh");
            exit(EXIT_FAILURE);
        }

        if(pids[i]==0)          //child process 
        {
            //if not the first command, get the input from the previous pipe ie make the stdin point to the read end
            if(i>0)
            {
                dup2(pipes[i-1][0], STDIN_FILENO);
            }
            
            //if not last command, output to the next pipe  
            if(i<count-1)
            {
                dup2(pipes[i][1],STDOUT_FILENO);    //you are connecting the stdout to the write end 
            }

            for (int j=0;j<count-1;j++)             //all processes which access the fd has to close them for an eof to be sent. otherwise, the read operation is never performed.
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            execvp(pipe_args[i][0], pipe_args[i]);  //the format of execvp is like ls, ls -l -a.
            perror("sh");
            exit(EXIT_FAILURE);
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

    return 1;
}
//to parse the inputs into tokens

#define BUFSIZE 64                  //the initial size of the array
#define DELIIMITER " \n\t\a\r"      //the delimiters to look for when parsing

char **split_line(char *line)
{   
    int buf_size = BUFSIZE;
    int position=0;
    char **tokens = malloc(buf_size * sizeof(char*));
    char *token , **tokens_backup;
    
    if(!tokens)
    {
        fprintf(stderr , "sh: allocation failed.\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, DELIIMITER);   //this takes the first token, and when it finds a delimiter, it adds \0 after the token it read.

    while(token != NULL)
    {
        tokens[position] = strdup(token);
        //here, the strtok returns a pointer to the original string, and we can directly do this because we dont modify the original command. strdup does malloc and then strcpy so that it is an independant copy and not pointers.
        position++;

        if(position>=buf_size)
        {   
            buf_size += BUFSIZE;
            tokens_backup = tokens;
            tokens = realloc(tokens, buf_size * sizeof(char*));

            if(tokens==NULL)
            {
                for(int i=0;i<position;i++)
                {
                    free(tokens_backup[i]);
                }
                free(tokens_backup);
                fprintf(stderr, "sh: allocation error.\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, DELIIMITER);   //since the last parsed token is followed by a \0, ie NULL, we have to starting splitting from that. hence the NULL.
    }

    tokens[position]=NULL;      //indicating the end of the tokens.
    return tokens;
}

int execute(char **args)
{   
    CommandEntry *entry;

    if(args[0]==NULL)
    {   
        //empty command.
        return 1;
    }

    for(entry=commands ; entry->name!=NULL; entry++){
        if(strcmp(args[0], entry->name)==0){
            return entry->built_in_func(args);
        }
    }
    

    return sh_launch(args);     //if it is not a built in command, then execute it normally using fork.
}

int sh_launch(char **args)
{
    pid_t pid;
    int status;

    pid = fork();

    if(pid==0)      //child process 
    {
        if(execvp(args[0] , args )==-1)
        {
            perror("sh");
            exit(EXIT_FAILURE);
        }
    }

    else if(pid<0)      //error forking
    {
        perror("sh");
        exit(EXIT_FAILURE);
    }

    else            //parent process   
    {
        do 
        {
            waitpid(pid, &status , WUNTRACED);
        }while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    return 1;   //here, the return value is always 1,to continue the execution of the shell. this is then used as status in the loop function.
}

void redirection(char *line) {
  printf("hi");
}


void loop(void)
{
    char *line;
    char **args;
    char ***pipe_args;
    int count;
    int status;

    do {
        printf(">");
        line = read_line();
        //for handling pipe commands
        if(strchr(line, '|'))
        {

            pipe_args = split_pipe(line,&count);
            status = pipe_execute(pipe_args,count);
            for(int i=0;i<count;i++)
            {
                for(int j=0;pipe_args[i][j]!=NULL;j++)
                {
                    free(pipe_args[i][j]);
                }
                free(pipe_args[i]);
            }
            free(pipe_args);
        } else if ( (strchr(line, '>')) || (strchr(line, '<')))  {
	  redirection(line);
        }

        //for handling normal commands
        else 
        {
            args = split_line(line);
            status = execute(args);
            for(int i=0;args[i]!=NULL;i++)
            {
                free(args[i]);
            }
            free(args);
        }
        free(line);

    }while (status);
}

int main(int argc, char **argv)
{
    //run the command loop 
    loop();

    return EXIT_SUCCESS;
}

