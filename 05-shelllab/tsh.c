#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */

/* Job states */
#define UNDEF  0  /* undefined */
#define FG     1  /* running in foreground */
#define BG     2  /* running in background */
#define ST     3  /* stopped */

typedef struct {
  pid_t pid;              /* job PID */
  int   jid;              /* job ID [1, 2, ...] */
  int   state;            /* UNDEF, BG, FG, or ST */
  char  cmdline[MAXLINE]; /* command line */
} job_t;

/* Global variables */
extern char **environ;

/* Function prototypes */
void eval(char *cmdline);
int  builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Job list helpers */
void clearjob(job_t *job);
void initjobs(job_t *jobs);
int  maxjid(job_t *jobs);
int  addjob(job_t *jobs, pid_t pid, int state, char *cmdline);
int  deletejob(job_t *jobs, pid_t pid);
pid_t fgpid(job_t *jobs);
job_t *getjobpid(job_t *jobs, pid_t pid);
job_t *getjobjid(job_t *jobs, int jid);
int  pid2jid(pid_t pid);
void listjobs(job_t *jobs);

/*
 * main - The shell's main routine
 */
int main(int argc, char **argv) {
  // TODO: install signal handlers, enter command loop
  printf("TODO: tiny shell\n");
  return 0;
}

/*
 * eval - Evaluate the command line
 */
void eval(char *cmdline) {
  // TODO: parse and execute command
}

/*
 * builtin_cmd - If first arg is a builtin command, run it and return 1
 */
int builtin_cmd(char **argv) {
  // TODO: quit, jobs, bg, fg
  return 0;
}

/*
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) {
  // TODO: bg/fg implementation
}

/*
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid) {
  // TODO: wait for foreground process
}

/*****************
 * Signal handlers
 *****************/

/*
 * sigchld_handler - Reap terminated/stopped children
 */
void sigchld_handler(int sig) {
  // TODO: reap child processes
}

/*
 * sigint_handler - Catch Ctrl-C (SIGINT)
 */
void sigint_handler(int sig) {
  // TODO: forward SIGINT to foreground job
}

/*
 * sigtstp_handler - Catch Ctrl-Z (SIGTSTP)
 */
void sigtstp_handler(int sig) {
  // TODO: forward SIGTSTP to foreground job
}
