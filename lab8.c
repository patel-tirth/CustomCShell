#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;
int main(void) {
  int child_status;
  posix_spawn_file_actions_t actions1, actions2;
  char* const cmd1[] = {"/bin/ls", NULL};
  char* const cmd2[] = {"wc", NULL};
  int pipe_fds[2];
  int pid1, pid2;

  //Initialize spawn file actions object for both the processes
  posix_spawn_file_actions_init(&actions1);
  posix_spawn_file_actions_init(&actions2);

  //Create a unidirectional pipe for interprocess communication with a 
  // read and write end.
  pipe(pipe_fds);

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

  // Create the first child process 
  if (0 != posix_spawnp(&pid1, cmd1[0], &actions1, NULL, cmd1, environ)) {
    perror("spawn failed");
    exit(1);
  }

  //Create the second child process
  if (0 != posix_spawnp(&pid2, cmd2[0], &actions2, NULL, cmd2, environ)) {
    perror("spawn failed");
    exit(1);
  }

  // Close the read end in the parent process
  close(pipe_fds[0]);

  // Close the write end in the parent process
  close(pipe_fds[1]);

  // Wait for the first child to complete
  waitpid(pid1, &child_status, 0);

  // Wait for the second child to complete
  waitpid(pid2, &child_status, 0);
}