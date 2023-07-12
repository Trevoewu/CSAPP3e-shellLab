/*
 * tsh - A tiny shell program with job control
 *
 * <Put your name and login ID here>
 * Trevor wu
 */
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_types/_pid_t.h>
#include <sys/_types/_sigset_t.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Misc manifest constants */
#define MAXLINE 1024   /* max line size */
#define MAXARGS 128    /* max args on a command line */
#define MAXJOBS 16     /* max jobs at any point in time */
#define MAXJID 1 << 16 /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/*
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;   /* defined in libc */
char prompt[] = "tsh> "; /* command line prompt (DO NOT CHANGE) */
int verbose = 0;         /* if true, print additional output */
int nextjid = 1;         /* next job ID to allocate */
char sbuf[MAXLINE];      /* for composing sprintf messages */

struct job_t {           /* The job struct */
  pid_t pid;             /* job PID */
  int jid;               /* job ID [1, 2, ...] */
  int state;             /* UNDEF, BG, FG, or ST */
  char cmdline[MAXLINE]; /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */

/* Function prototypes */
// extra function
void showJobByPid(struct job_t *, pid_t);
int isdigitstr(char *str);
void showJob(struct job_t *);

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv);
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs);
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid);
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid);
int pid2jid(pid_t pid);
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine
 */
int main(int argc, char **argv) {
  char c;
  char cmdline[MAXLINE];
  int emit_prompt = 1; /* emit prompt (default) */

  /* Redirect stderr to stdout (so that driver will get all output
   * on the pipe connected to stdout) */
  dup2(1, 2);

  /* Parse the command line */
  while ((c = getopt(argc, argv, "hvp")) != EOF) {
    switch (c) {
    case 'h': /* print help message */
      usage();
      break;
    case 'v': /* emit additional diagnostic info */
      verbose = 1;
      break;
    case 'p':          /* don't print a prompt */
      emit_prompt = 0; /* handy for automatic testing */
      break;
    default:
      usage();
    }
  }

  /* Install the signal handlers */

  /* These are the ones you will need to implement */
  Signal(SIGINT, sigint_handler);   /* ctrl-c */
  Signal(SIGTSTP, sigtstp_handler); /* ctrl-z */
  Signal(SIGCHLD, sigchld_handler); /* Terminated or stopped child */

  /* This one provides a clean way to kill the shell */
  Signal(SIGQUIT, sigquit_handler);

  /* Initialize the job list */
  initjobs(jobs);

  /* Execute the shell's read/eval loop */
  while (1) {

    /* Read command line */
    if (emit_prompt) {
      printf("%s", prompt); // print tsh >
      fflush(stdout);
    }
    if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
      app_error("fgets error");
    if (feof(stdin)) { /* End of file (ctrl-d) */
      fflush(stdout);
      exit(0);
    }

    /* Evaluate the command line */
    eval(cmdline);
    fflush(stdout);
    fflush(stdout);
  }

  exit(0); /* control never reaches here */
}

/*
 * eval - Evaluate the command line that the user has just typed in
 *
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.
 */
// eval: 解析和解释命令行的 Main 例程. [70 行]
// 在`eval`中，父进程在`fork`子进程之前必须使用`sigprocmask`阻塞`SIGCHLD`信号，
// 然后在将子进程添加到作业列表中时，通过调用`addjob`后再解除阻塞。
// 由于子进程继承了其父进程的阻塞向量，因此子进程在执行新程序之前必须确保解除阻塞`SIGCHLD`信号。
void eval(char *cmdline) {
  char *argv[MAXARGS];
  char buf[MAXLINE];
  int bg;
  pid_t pid;
  sigset_t mask, prev_mask;
  sigaddset(&mask, SIGCHLD);
  strcpy(buf, cmdline);
  bg = parseline(buf, argv);
  if (argv[0] == NULL)
    return;
  if (!builtin_cmd(argv)) {
    //阻塞`SIGCHLD`信号
    sigprocmask(SIG_BLOCK, &mask, &prev_mask);
    pid = fork();
    if (pid == 0) {
      // child process
      // 解除阻塞`SIGCHLD`信号。
      sigprocmask(SIG_SETMASK, &prev_mask, NULL);
      // 创建一个新的进程组，其进程组ID是p调用进程的pid，并把调用进程放到这个进程组中。
      setpgid(0, 0);
      if (execve(argv[0], argv, environ) < 0) {
        printf("command not found: %s\n", argv[0]);
        exit(0);
      }
    }
    // foreground
    if (!bg) {
      // add to job lists
      addjob(jobs, pid, FG, cmdline);
      // 解除阻塞`SIGCHLD`信号。
      sigprocmask(SIG_SETMASK, &prev_mask, NULL);
      waitfg(pid);
    } else {
      // background
      addjob(jobs, pid, BG, cmdline);
      // 解除阻塞`SIGCHLD`信号。
      sigprocmask(SIG_SETMASK, &prev_mask, NULL);
      showJobByPid(jobs, pid);
    }
  }
  return;
}

/*
 * parseline - Parse the command line and build the argv array.
 *
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.
 */
int parseline(const char *cmdline, char **argv) {
  static char array[MAXLINE]; /* holds local copy of command line */
  char *buf = array;          /* ptr that traverses command line */
  char *delim;                /* points to first space delimiter */
  int argc;                   /* number of args */
  int bg;                     /* background job? */

  strcpy(buf, cmdline);
  buf[strlen(buf) - 1] = ' ';   /* replace trailing '\n' with space */
  while (*buf && (*buf == ' ')) /* ignore leading spaces */
    buf++;

  /* Build the argv list */
  argc = 0;
  if (*buf == '\'') {
    buf++;
    delim = strchr(buf, '\'');
  } else {
    delim = strchr(buf, ' ');
  }

  while (delim) {
    argv[argc++] = buf;
    *delim = '\0';
    buf = delim + 1;
    while (*buf && (*buf == ' ')) /* ignore spaces */
      buf++;

    if (*buf == '\'') {
      buf++;
      delim = strchr(buf, '\'');
    } else {
      delim = strchr(buf, ' ');
    }
  }
  argv[argc] = NULL;

  if (argc == 0) /* ignore blank line */
    return 1;

  /* should the job run in the background? */
  if ((bg = (*argv[argc - 1] == '&')) != 0) {
    argv[--argc] = NULL;
  }
  return bg;
}

/*
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.
 */
//  builtin cmd: 识别和解释内置命令: quit，fg，bg，and jobs [25 行]
int builtin_cmd(char **argv) {
  if (!strcmp(argv[0], "quit"))
    exit(0);
  if (!strcmp(argv[0], "fg") | !strcmp(argv[0], "bg")) {
    if (argv[1] == NULL) {
      printf("%s command requires PID or %%jobid argument\n", argv[0]);
      return 1;
    }
    do_bgfg(argv);
    return 1;

  } else if (!strcmp(argv[0], "jobs")) {
    //    jobs 命令列出了所有的后台工作。
    listjobs(jobs);
    return 1;
  }
  return 0; /* not a builtin command */
}

/*
 * do_bgfg - Execute the builtin bg and fg commands
 */
// - fg < job > 命令通过发送 SIGCONT 信号重新启动 < job >
// ，然后在前台运行它。参数 < job > 可以是 PID 或 JID。
// Bg < job > 命令通过发送 SIGCONT 信号重启 < job > ，然后在后台运行。参数 <
// job > 可以是 PID 或 JID。
// do_bgfg: Implements the bg and fg built-in commands. [50 lines]
void do_bgfg(char **argv) {
  int jid;
  pid_t pid;
  struct job_t *stopJob;
  //判断是否为jid
  if (argv[1][0] == '%') {
    //判断jid输入是否合法——为数字
    if (!isdigitstr(&argv[1][1])) {
      printf("%s: argument must be a PID or %%jobid\n", argv[0]);
      return;
    }
    //转换为int类型
    jid = atoi(&argv[1][1]);
    if (jid > MAXJID)
      return;
    // jid存在，给这个进程组的所以进程发送SIGCONT信号
    stopJob = getjobjid(jobs, jid);
    if (stopJob != NULL)
      kill(-stopJob->pid, SIGCONT);
    else {
      printf("(%d): No such job\n", jid);
      return;
    }

  }
  //第二个参数为pid
  else {
    //判断是否为数字
    if (!isdigitstr(argv[1])) {
      printf("%s: argument must be a PID or %%jobid\n", argv[0]);
      return;
    }
    pid = atoi(argv[1]);
    stopJob = getjobpid(jobs, pid);
    if (stopJob != NULL)
      kill(-pid, SIGCONT);
    else {
      printf("(%d): No such process\n", pid);
      return;
    }
  }
  // bg命令 → 让进程在后台运行，更改state为BG
  if (!strcmp(argv[0], "bg")) {
    stopJob->state = BG;
    showJob(stopJob);
  }
  // fg, 让进程运行在前台，更改进程状态为FG，并等待前台进程结束
  else {
    stopJob->state = FG;
    // 等待前台进程结束
    waitfg(pid);
  }
  return;
}

/*
 * waitfg - Block until process pid is no longer the foreground process
 waitfg: 等待前台作业完成。[20 行]
 */
//  使用sleep（1）忙等循环检查前台作业，回收进程交给sigchld_handler处理函数。
void waitfg(pid_t pid) {
  while (1) {
    for (int i = 0; i < MAXJOBS; i++) {
      if (jobs[i].state == FG) {
        // 有前台作业，休眠一会，再继续检查
        sleep(1);
        i--;
      }
    }
    return;
  }
}

/*****************
 * Signal handlers
 *****************/

/*
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.
 */
//  sigchld handler: 捕捉 SIGCHILD 信号。[80 行]
void sigchld_handler(int sig) {
  int status;
  pid_t pid;
  struct job_t *waitJob;
  //立即返回，如果父进程创建的所以子进程变成已终止或者被停止返回进程pid，否则返回0
  if ((pid = waitpid(-1, &status, WUNTRACED | WNOHANG)) > 0) {
    waitJob = getjobpid(jobs, pid);
    // 进程被一个未被捕获的进程终止
    if (WIFSIGNALED(status)) {
      printf("Job [%d] (%d) terminated by signal %d\n", waitJob->jid,
             waitJob->pid, WTERMSIG(status));
      //  从作业中删除
      deletejob(jobs, pid);

    } //进程是正常中终止（return ｜ exit)
    else if (WIFEXITED(status)) {
      deletejob(jobs, pid);
    } else if (WIFSTOPPED(status)) {
      // child stop by ctrl-z ,
      //  signal numbers are OS-dependent, so SIGTSTP in Linux is 20 but in MAC
      //  OS it's 18.
      // change jobs state to STOP(ST)
      waitJob->state = ST;
      printf("Job [%d] (%d) stopped by signal %d\n", waitJob->jid, waitJob->pid,
             WSTOPSIG(status));
    } else {
      printf("child %d terminated abnormally \n", pid);
    }
  } else {
    printf("waitpid error: no child terminated\n");
  }
  return;
}

/*
 * sigint_handler - The kernel sends a SIGINT to the shell whenever the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.
 当您键入ctrl-c时，Shell应捕获生成的SIGINT信号，
 然后将其转发给相应的前台作业（更确切地说，包含前台作业的进程组）。
 1. sigint handler: 捕获SIGINT（ctrl-c）信号。[15行]
 */
void sigint_handler(int sig) {
  struct job_t *intjob;
  for (int i = 0; i < MAXJOBS; i++) {
    if (jobs[i].state == FG) {
      intjob = &jobs[i];
      kill(-intjob->pid, SIGINT);
      break;
    }
  }
  return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.
 */
void sigtstp_handler(int sig) {
  struct job_t *intjob;
  for (int i = 0; i < MAXJOBS; i++) {
    if (jobs[i].state == FG) {
      intjob = &jobs[i];
      intjob->state = ST;
      kill(-intjob->pid, SIGTSTP);

      break;
    }
  }
  return;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
  job->pid = 0;
  job->jid = 0;
  job->state = UNDEF;
  job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
  int i;

  for (i = 0; i < MAXJOBS; i++)
    clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs) {
  int i, max = 0;

  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].jid > max)
      max = jobs[i].jid;
  return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) {
  int i;

  if (pid < 1)
    return 0;

  for (i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid == 0) {
      jobs[i].pid = pid;
      jobs[i].state = state;
      jobs[i].jid = nextjid++;
      if (nextjid > MAXJOBS)
        nextjid = 1;
      strcpy(jobs[i].cmdline, cmdline);
      if (verbose) {
        printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid,
               jobs[i].cmdline);
      }
      return 1;
    }
  }
  printf("Tried to create too many jobs\n");
  return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid) {
  int i;

  if (pid < 1)
    return 0;

  for (i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid == pid) {
      clearjob(&jobs[i]);
      nextjid = maxjid(jobs) + 1;
      return 1;
    }
  }
  return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
  int i;

  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].state == FG)
      return jobs[i].pid;
  return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
  int i;

  if (pid < 1)
    return NULL;
  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].pid == pid)
      return &jobs[i];
  return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid) {
  int i;

  if (jid < 1)
    return NULL;
  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].jid == jid)
      return &jobs[i];
  return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) {
  int i;

  if (pid < 1)
    return 0;
  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].pid == pid) {
      return jobs[i].jid;
    }
  return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs) {
  int i;

  for (i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid != 0) {
      printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
      switch (jobs[i].state) {
      case BG:
        printf("Running ");
        break;
      case FG:
        printf("Foreground ");
        break;
      case ST:
        printf("Stopped ");
        break;
      default:
        printf("listjobs: Internal error: job[%d].state=%d ", i, jobs[i].state);
      }
      printf("%s", jobs[i].cmdline);
    }
  }
}
/******************************
 * end job list helper routines
 ******************************/

/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) {
  printf("Usage: shell [-hvp]\n");
  printf("   -h   print this message\n");
  printf("   -v   print additional diagnostic information\n");
  printf("   -p   do not emit a command prompt\n");
  exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg) {
  fprintf(stdout, "%s: %s\n", msg, strerror(errno));
  exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg) {
  fprintf(stdout, "%s\n", msg);
  exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) {
  struct sigaction action, old_action;

  action.sa_handler = handler;
  sigemptyset(&action.sa_mask); /* block sigs of type being handled */
  action.sa_flags = SA_RESTART; /* restart syscalls if possible */

  if (sigaction(signum, &action, &old_action) < 0)
    unix_error("Signal error");
  return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) {
  printf("Terminating after receipt of SIGQUIT signal\n");
  exit(1);
}
void showJobByPid(struct job_t *jobs, pid_t pid) {
  for (int i = 0; i < MAXJOBS; i++) {
    if (jobs[i].pid == pid) {
      printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
      printf("%s", jobs[i].cmdline);
      return;
    }
  }
}
// 判断字符串是否为数字
int isdigitstr(char *str) {
  int len = strlen(str);
  int i = 0;

  for (i = 0; i < len; i++) {
    if (!(isdigit(str[i])))
      return 0;
  }
  return 1;
}
void showJob(struct job_t *job) {
  printf("[%d] (%d) ", job->jid, job->pid);
  printf("%s", job->cmdline);
  return;
}
