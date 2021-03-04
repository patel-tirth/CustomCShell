/* $begin shellmain */
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <spawn.h>
#include <signal.h>

#define MAXARGS 128
#define MAXLINE 8192 /* Max text line length */
#define MSGLEN1 23
#define MSGLEN2 24

// static volatile sig_atomic_t keep_running = 0;
extern char **environ; /* Defined by libc */

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);
void parse_redirections(char **argv );
void parse_pipe(char **argv, int i);
void parse_semicolon(char **argv, int i, posix_spawn_file_actions_t my_file_actions);
void parse_input_output(char ** argv, int i,posix_spawn_file_actions_t my_file_actions);
int globalPid = 0 ;    // to keep track of the pid of last process ran 
int globalStatus = 0 ;  // to keep track of the status of the last process ran 
void unix_error(char *msg) /* Unix-style error */
{
  fprintf(stderr, "%s: %s\n", msg, strerror(errno));
  exit(EXIT_FAILURE);
}
// handle sigint and sigtstp signals
//source: yt lecture signal handling
static void sig_handler(int this_signal)
{
  if(this_signal == SIGINT){
    write(STDOUT_FILENO,"\ncaught sigint\nCS361 >",MSGLEN1);
  }
  else if (this_signal == SIGTSTP)
  {
     write(STDOUT_FILENO,"\ncaught sigtstp\nCS361 >",MSGLEN2);
  }
  
}
int main() {
  char cmdline[MAXLINE]; /* Command line */
 
  signal(SIGINT,sig_handler);
  signal(SIGTSTP,sig_handler);
  
  while (1) {
    char *result;
    /* Read */
    printf("CS361 >");
   
    result = fgets(cmdline, MAXLINE, stdin);
    if (result == NULL && ferror(stdin)) {
      fprintf(stderr, "fatal fgets error\n");
      exit(EXIT_FAILURE);
    }

    if (feof(stdin)) exit(0);

    /* Evaluate */
    eval(cmdline);
  }
}
/* $end shellmain */

/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline) {
  char *argv[MAXARGS]; /* Argument list execve() */
  char buf[MAXLINE];   /* Holds modified command line */
  int bg;              /* Should the job run in bg or fg? */
  pid_t pid;    /* Process id */
                 

  strcpy(buf, cmdline);

  bg = parseline(buf, argv);
  if (argv[0] == NULL) return; /* Ignore empty lines */

  if (!builtin_command(argv)) {
    if (0 != posix_spawnp(&pid, argv[0], NULL, NULL, argv, environ)) {
    perror("spawn failed");
    exit(1);
  } 
  globalPid = pid;
    // printf("not yet implemented :(\n");
    /* Parent waits for foreground job to terminate */
    if (!bg) {
      int status;
      if (waitpid(pid, &status, 0) < 0) unix_error("waitfg: waitpid error");
      globalPid = pid;
      globalStatus = status;
    } else
      printf("%d %s", pid, cmdline); 
  }
  return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) {
  
  if (!strcmp(argv[0], "exit")) /* exit command */
    exit(0);
  if (!strcmp(argv[0], "&")) /* Ignore singleton & */
    return 1;
  if(!strcmp(argv[0], "?"))
  {
   printf("pid:%d status:%d\n", globalPid, globalStatus); 
    return 1;
  }
  return 0; /* Not a builtin command */
}
/* $end eval */
void parse_pipe(char** argv, int i)
{
  // source: Lab 8
  int child_status;
      posix_spawn_file_actions_t actions1, actions2; 
      posix_spawn_file_actions_init(&actions1);
      posix_spawn_file_actions_init(&actions2);

       int pipe_fds[2];
      int pid1, pid2;

      pipe(pipe_fds);

      argv[i] = '\0';
      // Add duplication action of copying the write end of the pipe to the 
     // standard out file descriptor
      posix_spawn_file_actions_adddup2(&actions1, pipe_fds[1], STDOUT_FILENO);
      // Add action of closing the read end of the pipe
      posix_spawn_file_actions_addclose(&actions1, pipe_fds[0]);

      // Add duplication action of copying the read end of the pipe to the 
  // standard in file descriptor
      posix_spawn_file_actions_adddup2(&actions2, pipe_fds[0], STDIN_FILENO);
    // Add the action of closing the write end of the pipe
     posix_spawn_file_actions_addclose(&actions2, pipe_fds[1]);

     if (0 != posix_spawnp(&pid1, argv[0], &actions1, NULL, argv, environ)) {
      perror("spawn failed");
      exit(1);
     }
    if (0 != posix_spawnp(&pid2, argv[i+1], &actions2, NULL, &argv[i+1], environ)) {   //ls -lh | wc -l
    perror("spawn failed");
    exit(1);
    }
    close(pipe_fds[0]);

    close(pipe_fds[1]);

    waitpid(pid1, &child_status, 0);
    globalPid = pid1;
    globalStatus = child_status;
    waitpid(pid2, &child_status, 0);
    globalStatus = child_status;
    globalPid = pid2;
    
      argv[0] = '\0';   

}

// parse " ; " command
void parse_semicolon(char** argv , int i, posix_spawn_file_actions_t my_file_actions )
{
  posix_spawn_file_actions_init(&my_file_actions);
  
  int pid,child_status;
   argv[i] = '\0';
      if (0 != posix_spawnp(&pid, argv[0], &my_file_actions, NULL, argv, environ) ) 
      {
       perror("spawn failed");
       exit(1);
      }
      wait(&child_status);
    globalPid = pid;
    globalStatus = child_status;

      if (0 != posix_spawnp(&pid, argv[i+1], &my_file_actions, NULL, &argv[i+1], environ) ) 
      {
       perror("spawn failed");
       exit(1);
      }
      wait(&child_status);
     globalPid = pid;
     globalStatus = child_status;

      argv[0] = '\0';
     
}
// parse input and output redirection // cat < output > output2
void parse_input_output(char ** argv, int i ,posix_spawn_file_actions_t my_file_actions)
{
  //  posix_spawn_file_actions_t my_file_actions;
  posix_spawn_file_actions_init(&my_file_actions);
  int child_status,pid;
      argv[i] = '\0';
      posix_spawn_file_actions_addopen(&my_file_actions, STDIN_FILENO, argv[i+1],O_RDONLY,S_IRUSR | S_IWUSR);
    // posix_spawn_file_actions_adddup2(&my_file_actions, STDIN_FILENO, STDIN_FILENO);
    // redirect STDOUT_FILENO to null
    // source stackoverflow: https://stackoverflow.com/questions/32049807/how-to-redirect-posix-spawn-stdout-to-dev-null
    // to prevent the input redirection from being printed to the terminal 
      posix_spawn_file_actions_addopen(&my_file_actions, 1, "/dev/null",O_WRONLY,0); 
      //
      if (0 != posix_spawnp(&pid, argv[0], &my_file_actions, NULL, argv, environ) ) 
      {
       perror("spawn failed");
       exit(1);
      }
        wait(&child_status);
        globalPid = pid;
        globalStatus = child_status;

    argv[i+2] = '\0';   // >  = '\0'
         // open i + 3 ..example, open output2 in case of " cat < output > output2"
      posix_spawn_file_actions_addopen(&my_file_actions, STDOUT_FILENO, argv[i+3],O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);

      if (0 != posix_spawnp(&pid, argv[0], &my_file_actions, NULL, argv, environ) ) 
      {
      perror("spawn failed");
      exit(1);
      }
       wait(&child_status);
       globalPid = pid;
       globalStatus = child_status;
 
    argv[0] = '\0';
    // return;s
}

void parse_redirections(char **argv)
{
  posix_spawn_file_actions_t my_file_actions;
  posix_spawn_file_actions_init(&my_file_actions);
 
 int i,pid,child_status;
  for(i = 0 ; argv[i] != NULL; ++i)
  {
    if((strcmp(argv[i], ">")) == 0){
      
       argv[i] = '\0';
     
      posix_spawn_file_actions_addopen(&my_file_actions, STDOUT_FILENO, argv[i+1],O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
     
      if (0 != posix_spawnp(&pid, argv[0], &my_file_actions, NULL, argv, environ) ) 
      {
      perror("spawn failed");
      exit(1);
      }
       wait(&child_status);
       globalPid = pid;
       globalStatus = child_status;
    
    argv[0] = '\0';
   
    }
    else if ((strcmp(argv[i], "<")) == 0)
    {
      if(argv[i+2] != NULL && strcmp(argv[i+2] , ">") == 0 ) // check if 3 rd argv is >.. then call parse input output
      {
        parse_input_output(argv, i , my_file_actions); // parse input and output redirection
      }
      else {
      argv[i] = '\0';
      posix_spawn_file_actions_addopen(&my_file_actions, STDIN_FILENO, argv[i+1],O_RDONLY,S_IRUSR | S_IWUSR);
      if (0 != posix_spawnp(&pid, argv[0], &my_file_actions, NULL, argv, environ) ) 
      {
       perror("spawn failed");
       exit(1);
      }
        wait(&child_status);
        globalPid = pid;
        globalStatus = child_status;
        argv[0] = '\0';
      }
    }
    // parse pipe 
    else if ((strcmp(argv[i], "|")) == 0)
    {
      parse_pipe(argv , i);
    }
    else if ((strcmp(argv[i], ";")) == 0)
    {
      parse_semicolon(argv , i, my_file_actions);
    }
 
  }
} 


/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) {
  char *delim; /* Points to first space delimiter */
  int argc;    /* Number of args */
  int bg;      /* Background job? */
 
  buf[strlen(buf) - 1] = ' ';   /* Replace trailing '\n' with space */
  while (*buf && (*buf == ' ')) /* Ignore leading spaces */
    buf++;

  /* Build the argv list */
  argc = 0;
  while ((delim = strchr(buf, ' '))) {
    argv[argc++] = buf;
    *delim = '\0';
    buf = delim + 1;
    while (*buf && (*buf == ' ')) /* Ignore spaces */
      buf++;
  }
  argv[argc] = NULL;

  if (argc == 0) /* Ignore blank line */
    return 1;

  /* Should the job run in the background? */
  if ((bg = (*argv[argc - 1] == '&')) != 0) argv[--argc] = NULL;
  parse_redirections(argv);
  return bg;
}
/* $end parseline */
