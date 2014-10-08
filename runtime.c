/***************************************************************************
 *  Title: Runtime environment 
 * -------------------------------------------------------------------------
 *    Purpose: Runs commands
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.1 $
 *    Last Modification: $Date: 2005/10/13 05:24:59 $
 *    File: $RCSfile: runtime.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: runtime.c,v $
 *    Revision 1.1  2005/10/13 05:24:59  sbirrer
 *    - added the skeleton files
 *
 *    Revision 1.6  2002/10/24 21:32:47  sempi
 *    final release
 *
 *    Revision 1.5  2002/10/23 21:54:27  sempi
 *    beta release
 *
 *    Revision 1.4  2002/10/21 04:49:35  sempi
 *    minor correction
 *
 *    Revision 1.3  2002/10/21 04:47:05  sempi
 *    Milestone 2 beta
 *
 *    Revision 1.2  2002/10/15 20:37:26  sempi
 *    Comments updated
 *
 *    Revision 1.1  2002/10/15 20:20:56  sempi
 *    Milestone 1
 *
 ***************************************************************************/
#define __RUNTIME_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

/************Private include**********************************************/
#include "runtime.h"
#include "io.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

/************Global Variables*********************************************/

#define NBUILTINCOMMANDS (sizeof BuiltInCommands / sizeof(char*))

typedef struct bgjob_l {
  pid_t pid;
  int jobNum;
  bool running;
  bool completed;
  bool stopped;
  int status;
  commandT* command;
  struct bgjob_l* next;
} bgjobL;

/* the pids of the background processes */
//bgjobL *bgjobs = NULL;
bgjobL *bgjobs2 = NULL;
//extern that ^
int numJobs = 1;
/************Function Prototypes******************************************/
/* run command */
static void RunCmdFork(commandT*, bool);
/* runs an external program command after some checks */
static void RunExternalCmd(commandT*, bool);
/* resolves the path and checks for exutable flag */
static bool ResolveExternalCmd(commandT*);
/* forks and runs a external program */
static void Exec(commandT*, bool);
/* runs a builtin command */
static void RunBuiltInCmd(commandT*);
/* checks whether a command is a builtin command */
static bool IsBuiltIn(char*);
static void fg();
static void bg();
static void jobs();
static void cd(char* path);
void handleSigChld();
void addJob(pid_t child, int jobN, commandT* cmd);
bgjobL * removeJob(pid_t pid);
//int update_status (pid_t pid, int status);
/************External Declaration*****************************************/

/**************Implementation***********************************************/
int total_task;
void RunCmd(commandT** cmd, int n)
{
  int i;
  total_task = n;
  if(n == 1)
    RunCmdFork(cmd[0], TRUE);
  else{
    RunCmdPipe(cmd[0], cmd[1]);
    for(i = 0; i < n; i++)
      ReleaseCmdT(&cmd[i]);
  }
}

void RunCmdFork(commandT* cmd, bool fork)
{
  if (cmd->argc<=0)
    return;
  if (IsBuiltIn(cmd->argv[0]))
  {
    RunBuiltInCmd(cmd);
  }
  else
  {
    RunExternalCmd(cmd, fork);
  }
}

/* Take the given cmd and run it in the background, tracking its pid */
void RunCmdBg(commandT* cmd)
{
  //TO DO
}

void RunCmdPipe(commandT* cmd1, commandT* cmd2)
{
}

void RunCmdRedirOut(commandT* cmd, char* file)
{
}

void RunCmdRedirIn(commandT* cmd, char* file)
{
}


/*Try to run an external command*/
static void RunExternalCmd(commandT* cmd, bool fork)
{
  if (ResolveExternalCmd(cmd)){
    Exec(cmd, fork);
  }
  else {
    printf("%s: command not found\n", cmd->argv[0]);
    fflush(stdout);
    ReleaseCmdT(&cmd);
  }
}

/*Find the executable based on search list provided by environment variable PATH*/
static bool ResolveExternalCmd(commandT* cmd)
{
  char *pathlist, *c;
  char buf[1024];
  int i, j;
  struct stat fs;

  if(strchr(cmd->argv[0],'/') != NULL){
    if(stat(cmd->argv[0], &fs) >= 0){
      if(S_ISDIR(fs.st_mode) == 0)
        if(access(cmd->argv[0],X_OK) == 0){/*Whether it's an executable or the user has required permisson to run it*/
          cmd->name = strdup(cmd->argv[0]);
          return TRUE;
        }
    }
    return FALSE;
  }
  pathlist = getenv("PATH");
  if(pathlist == NULL) return FALSE;
  i = 0;
  while(i<strlen(pathlist)){
    c = strchr(&(pathlist[i]),':');
    if(c != NULL){
      for(j = 0; c != &(pathlist[i]); i++, j++)
        buf[j] = pathlist[i];
      i++;
    }
    else{
      for(j = 0; i < strlen(pathlist); i++, j++)
        buf[j] = pathlist[i];
    }
    buf[j] = '\0';
    strcat(buf, "/");
    strcat(buf,cmd->argv[0]);
    if(stat(buf, &fs) >= 0){
      if(S_ISDIR(fs.st_mode) == 0)
        if(access(buf,X_OK) == 0){/*Whether it's an executable or the user has required permisson to run it*/
          cmd->name = strdup(buf); 
          return TRUE;
        }
    }
  }
  return FALSE; /*The command is not found or the user don't have enough priority to run.*/
}

static void Exec(commandT* cmd, bool forceFork)
{
  //pid_t parent = getpid();
  pid_t child;
  int status;

  sigset_t signal_set;
  sigemptyset(&signal_set);
  if(sigaddset(&signal_set, SIGCHLD) < 0) fprintf(stderr, "Error sigaddset");
  if(sigprocmask(SIG_BLOCK, &signal_set, NULL) < 0) fprintf(stderr, "Error sigprocmask block");


  child = fork();


  if(child == 0) { //Child process
    setpgid(0,0);
    if(sigprocmask(SIG_UNBLOCK, &signal_set, NULL) < 0) fprintf(stderr, "Error sigprocmask unblock");
    execv(cmd->name, cmd->argv);
    exit(0);
  } else {
    if(cmd->bg == 1) {
      //Background Command
      addJob(child, numJobs, cmd);
      if(sigprocmask(SIG_UNBLOCK, &signal_set, NULL) < 0) fprintf(stderr, "Error sigprocmask unblock");
      //printf("[%d] %d",mNewJob->jobNum,mNewJob->pid);
    } else {
      //Foreground Command
      if(sigprocmask(SIG_UNBLOCK, &signal_set, NULL) < 0) fprintf(stderr, "Error sigprocmask unblock");
      waitpid(child, &status, 0);
      kill(child, SIGKILL);
    }
    return;
  }

  
}

void addJob(pid_t child, int jobN, commandT* cmd) {
 //Add the new job
  numJobs++; //increment total number of jobs
  //fprintf(stdout, "\nSet: %p", bgjobs);
  bgjobL * mNewJob = (bgjobL *)malloc(sizeof(bgjobL));
  mNewJob->jobNum = jobN;
  mNewJob->pid = child;
  mNewJob->running = TRUE;
  mNewJob->completed = FALSE;
  mNewJob->stopped = FALSE;
  mNewJob->command = cmd;

  bgjobL * temp = bgjobs2;
  if(bgjobs2 == NULL) {
    bgjobs2 = (bgjobL *)malloc(sizeof(bgjobL));
    mNewJob->next = NULL;
    //mNewJob->next=bgjobs2;
    bgjobs2->next = mNewJob;
  } else {
    while(temp->next) {
      temp = temp->next;
    }
    temp->next = mNewJob;
  }
}

bgjobL * removeJob(pid_t pid) {
  
  if(bgjobs2->next == NULL)
    return NULL;
  else {
    bgjobL * prev = bgjobs2;
    bgjobL * deleteMe = bgjobs2->next
    ;
    while(deleteMe->next && (deleteMe->pid != pid)) {
      deleteMe = deleteMe->next;
      prev = prev->next;
    }
    if(deleteMe->pid == pid) {
      //delete node
      
      //fprintf(stdout, "DELETE: deleteMe: %p %d, prev: %p %d", deleteMe, deleteMe->pid, prev, prev->pid);
      prev->next = deleteMe->next;
      //fprintf(stdout, "DELETE: deleteMe: %p %d, prev: %p %d prev next: %p", deleteMe, deleteMe->pid, prev, prev->pid, prev->next);
      free(deleteMe->command);
      free(deleteMe);
      return prev;
      
    } else {
      fprintf(stdout, "Error: Pid not found for deletion");
      return NULL;
    }

  }

}

static bool IsBuiltIn(char* cmd)
{
  return (strcmp(cmd,"fg") == 0 || strcmp(cmd,"bg") == 0 || strcmp(cmd, "jobs") ==0 || strcmp(cmd, "cd") == 0);
}


static void RunBuiltInCmd(commandT* cmd)
{
  //printf("Built In Command\n");

  cmd->name = cmd->argv[0];
  if(strcmp(cmd->name, "fg")==0)
    fg();
  else if (strcmp(cmd->name, "bg")== 0)
    bg();
  else if(strcmp(cmd->name, "jobs")== 0)
    jobs();
  else if(strcmp(cmd->name, "cd") == 0)
    cd(cmd->argv[1]);
  else
    printf("ERROR command not found");
}

static void fg(){
  printf("You just ran fg\n");
}
static void bg(){
  printf("You just ran bg\n");
}

/* Print out the current jobs */
static void jobs(){
  if(bgjobs2 == NULL || bgjobs2->next == NULL)
    return;

  bgjobL * iterJobs2;
  
  for(iterJobs2 = bgjobs2; iterJobs2; iterJobs2=iterJobs2->next) 
  {    
    if(iterJobs2->running) {
      printf("[%d]\tRunning\t\t\t%s\n", iterJobs2->jobNum, iterJobs2->command->cmdline);
    } else if(iterJobs2->stopped) {
      printf("[%d]\tStopped\t\t\t%s\n", iterJobs2->jobNum, iterJobs2->command->cmdline);
    } else if(iterJobs2->completed) {
      printf("[%d]\tDone\t\t\t%s\n", iterJobs2->jobNum, iterJobs2->command->cmdline);
    }
  }
  fflush(stdout);
}

/* Handle the SIGCHLD signal from the kernel */
void handleSigChld()
{
  printf("signal");
  if(bgjobs2 == NULL) {
    return;
  }
  int status;
  pid_t deleteMe;

  deleteMe = waitpid(-1, &status, WNOHANG|WUNTRACED);

  if(!WIFEXITED(status))
    return;
  if(WIFCONTINUED(status))
    return;

  bgjobL * iterJobs2;// = bgjobs2->next;
  for(iterJobs2 = bgjobs2->next; iterJobs2; iterJobs2=iterJobs2->next) {
    if(iterJobs2->pid == deleteMe) {
      iterJobs2->running = FALSE;
      iterJobs2->completed = TRUE;
    }
  }

  /*
  bgjobL * iterJobs2 = bgjobs2;
  for(iterJobs2 = bgjobs2; iterJobs2; iterJobs2=iterJobs2->next) {
    waitpid(iterJobs2->pid, &status, WNOHANG|WUNTRACED);


    if(WIFSIGNALED(status)) {
      iterJobs2->completed = TRUE;
      iterJobs2->running = FALSE;
      //printf("[%d]\tDone\t\t\t%s %s\n", iterJobs2->jobNum, iterJobs2->command->name, iterJobs2->command->argv[1]);
      //iterJobs2 = removeJob(iterJobs2->pid);
      //fprintf(stdout, "\n HERE %d\n", iterJobs2->pid);
    }
  }
  */
  
}

void CheckJobs()
{
  if(bgjobs2 == NULL)
    return;

  bgjobL* temp;
  for(temp=bgjobs2->next; temp; temp=temp->next) {
    if(temp->completed == TRUE)
    {
      printf("[%d]\tDone\t\t\t%s\n", temp->jobNum, temp->command->cmdline);
      fflush(stdout);
      //printf("%d %d\n", temp->pid, temp->next->pid);
      temp = removeJob(temp->pid);
      //printf("%d %d\n", temp->pid, temp->next->pid);
    }
  }
}

commandT* CreateCmdT(int n)
{
  int i;
  commandT * cd = malloc(sizeof(commandT) + sizeof(char *) * (n + 1));
  cd -> name = NULL;
  cd -> cmdline = NULL;
  cd -> is_redirect_in = cd -> is_redirect_out = 0;
  cd -> redirect_in = cd -> redirect_out = NULL;
  cd -> argc = n;
  for(i = 0; i <=n; i++)
    cd -> argv[i] = NULL;
  return cd;
}

/*Release and collect the space of a commandT struct*/
void ReleaseCmdT(commandT **cmd){
  int i;
  if((*cmd)->name != NULL) free((*cmd)->name);
  if((*cmd)->cmdline != NULL) free((*cmd)->cmdline);
  if((*cmd)->redirect_in != NULL) free((*cmd)->redirect_in);
  if((*cmd)->redirect_out != NULL) free((*cmd)->redirect_out);
  for(i = 0; i < (*cmd)->argc; i++)
    if((*cmd)->argv[i] != NULL) free((*cmd)->argv[i]);
  free(*cmd);
}

static void cd(char* path){
  
  if(path && chdir(path) !=0){
    printf("cd:  %s:  No such file or directory\n", path);
  }   
}