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
void parse_redirections(char **argv);

void unix_error(char *msg) /* Unix-style error */
{
  fprintf(stderr, "%s: %s\n", msg, strerror(errno));
  exit(EXIT_FAILURE);
}
// handle sigint and sigtstp signals
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
    printf("CS361 > ");
   
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
  pid_t pid;           /* Process id */
  //  posix_spawn_file_actions_t my_file_actions;
 
  
  strcpy(buf, cmdline);
  bg = parseline(buf, argv);
  if (argv[0] == NULL) return; /* Ignore empty lines */

  if (!builtin_command(argv)) {
    if (0 != posix_spawnp(&pid, argv[0], NULL, NULL, argv, environ)) {
    perror("spawn failed");
    exit(1);
  } 
  
    // TODO: run the command argv using posix_spawnp.
    // printf("not yet implemented :(\n");
    /* Parent waits for foreground job to terminate */
    if (!bg) {
      int status;
      if (waitpid(pid, &status, 0) < 0) unix_error("waitfg: waitpid error");
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
  return 0; /* Not a builtin command */
}
/* $end eval */

void parse_redirections(char **argv)
{
 posix_spawn_file_actions_t my_file_actions;
 posix_spawn_file_actions_init(&my_file_actions);
  pid_t pid;
 int i;
  for(i = 0 ; argv[i] != NULL; ++i)
  {
    if((strcmp(argv[i], ">")) == 0){
      argv[i] = NULL;
      
     posix_spawn_file_actions_addopen(&my_file_actions, STDOUT_FILENO, argv[i + 1], O_WRONLY | O_CREAT,S_IRUSR| S_IWUSR) ;
  
      if (0 != posix_spawnp(&pid, argv[i-1], &my_file_actions, NULL, argv, environ)) 
      {
      perror("spawn failed");
      exit(1);
    }
    // waitpid(pid,NULL,0);
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
