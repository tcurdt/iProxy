/*
  main.c:
  $Id: main.c,v 1.14 2009/12/17 14:57:16 bulkstream Exp $

Copyright (C) 2001-2009 Tomo.M (author).
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. Neither the name of the author nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include <sys/stat.h>
#include "srelay.h"

/* prototypes */
void show_version  __P((void));
void usage         __P((void));

char *config = CONFIG;
char *ident = "srelay";
char *pidfile = PIDFILE;
char *pwdfile = NULL;
pid_t master_pid;

#if USE_THREAD
pthread_t main_thread;
int max_thread;
int threading;
#endif

int same_interface = 0;

#ifdef HAVE_LIBWRAP
int use_tcpwrap = 0;
#endif /* HAVE_LIBWRAP */

int max_child;
int cur_child;

int fg;        /* foreground operation */
int bind_restrict = 1; /* socks bind port is restricted */

/* authentication method priority table */
char method_tab[10];
int method_num;

void show_version()
{
  fprintf(stderr, "%s\n", version);
}

void usage()
{
  show_version();
  fprintf(stderr, "usage: %s [options]\n",
	  ident);
  fprintf(stderr, "options:\n"
	  "\t-c file\tconfig file\n"
	  "\t-i i/f\tlisten interface IP[:PORT]\n"
	  "\t-m num\tmax child/thread\n"
	  "\t-o min\tidle timeout minutes\n"
	  "\t-p file\tpid file\n"
	  "\t-a np\tauth methods n: no, p:pass\n"
	  "\t-u file\tsrelay password file\n"
	  "\t-f\trun into foreground\n"
	  "\t-r\tresolve client name in log\n"
	  "\t-s\tforce logging to syslog\n"
	  "\t-t\tdisable threading\n"
	  "\t-b\tavoid BIND port restriction\n"
	  "\t-g\tuse the same interface for outbound as inbound\n"
#ifdef HAVE_LIBWRAP
	  "\t-w\tuse tcp_wrapper access control\n"
#endif /* HAVE_LIBWRAP */
	  "\t-v\tshow version and exit\n"
	  "\t-h\tshow this help and exit\n");
  exit(1);
}

int srelay_main(int ac, char **av)
{
  int     ch, i=0;
//  pid_t   pid;
  FILE    *fp;
  uid_t   uid;
#ifdef USE_THREAD
  pthread_t tid;
  pthread_attr_t attr;
  struct rlimit rl;
  rlim_t max_fd = (rlim_t)MAX_FD;
  rlim_t save_fd = 0;
#endif

  /* try changing working directory */
  if ( chdir(WORKDIR0) != 0 )
    if ( chdir(WORKDIR1) != 0 )
      msg_out(norm, "giving up chdir to workdir");

#ifdef USE_THREAD
  threading = 1;
  max_thread = MAX_THREAD;
#endif

  max_child = MAX_CHILD;
  cur_child = 0;

  serv_init(NULL);

  proxy_tbl = NULL;
  proxy_tbl_ind = 0;

  method_num = 0;

  uid = getuid();

  while((ch = getopt(ac, av, "a:c:i:m:o:p:u:frstbwgvh?")) != -1)
    switch (ch) {
    case 'a':
      if (optarg != NULL) {
	for (i=0; i<sizeof method_tab; optarg++) {
	  if (*optarg == '\0')
	    break;
	  switch (*optarg) {
	  case 'p':
	    if ( uid != 0 ) {
	      /* process does not started by root */
	      msg_out(warn, "uid == %d (!=0),"
		      "user/pass auth will not work, ignored.\n",
		      uid);
	      break;
	    }
	    method_tab[i++] = S5AUSRPAS;
	    method_num++;
	    break;
	  case 'n':
	    method_tab[i++] = S5ANOAUTH;
	    method_num++;
	    break;
	  default:
	    break;
	  }
	}
      }
      break;

    case 'b':
      bind_restrict = 0;
      break;

    case 'c':
      if (optarg != NULL) {
        config = strdup(optarg);
      }
      break;

    case 'u':
      if (optarg != NULL) {
        pwdfile = strdup(optarg);
      }
      break;

    case 'i':
      if (optarg != NULL) {
	if (serv_init(optarg) < 0) {
	  msg_out(crit, "cannot init server socket(-i)\n");
	  exit(-1);
	}
      }
      break;

    case 'o':
      if (optarg != NULL) {
	idle_timeout = atol(optarg);
      }
      break;

    case 'p':
      if (optarg != NULL) {
	pidfile = strdup(optarg);
      }
      break;

    case 'm':
      if (optarg != NULL) {
#ifdef USE_THREAD
	max_thread = atoi(optarg);
#endif
	max_child = atoi(optarg);
      }
      break;

    case 't':
#ifdef USE_THREAD
      threading = 0;    /* threading disabled. */
#endif
      break;

    case 'g':
      same_interface = 1;
      break;

    case 'f':
      fg = 1;
      break;

    case 'r':
      resolv_client = 1;
      break;

    case 's':
      forcesyslog = 1;
      break;

    case 'w':
#ifdef HAVE_LIBWRAP
      use_tcpwrap = 1;
#endif /* HAVE_LIBWRAP */
      break;

    case 'v':
      show_version();
      exit(1);

    case 'h':
    case '?':
    default:
      usage();
    }

  ac -= optind;
  av += optind;

  fp = fopen(config, "r");
  if (readconf(fp) != 0) {
    /* readconf error */
    exit(1);
  }
  if (fp)
    fclose(fp);

  if (serv_sock_ind == 0) {   /* no valid ifs yet */
    if (serv_init(":") < 0) { /* use default */
      /* fatal */
      msg_out(crit, "cannot open server socket\n");
      exit(1);
    }
  }

#ifdef USE_THREAD
  if ( ! threading ) {
#endif
    if (queue_init() != 0) {
      msg_out(crit, "cannot init signal queue\n");
      exit(1);
    }
#ifdef USE_THREAD
  }
#endif

//  if (!fg) {
//    /* force stdin/out/err allocate to /dev/null */
//    fclose(stdin);
//    fp = fopen("/dev/null", "w+");
//    if (fileno(fp) != STDIN_FILENO) {
//      msg_out(crit, "fopen: %m");
//      exit(1);
//    }
//    if (dup2(STDIN_FILENO, STDOUT_FILENO) == -1) {
//      msg_out(crit, "dup2-1: %m");
//      exit(1);
//    }
//    if (dup2(STDIN_FILENO, STDERR_FILENO) == -1) {
//      msg_out(crit, "dup2-2: %m");
//      exit(1);
//    }
//
//    switch(fork()) {
//    case -1:
//      msg_out(crit, "fork failed: %m");
//      exit(1);
//    case 0:
//      /* child */
//      pid = setsid();
//      if (pid == -1) {
//	msg_out(crit, "setsid: %m");
////	exit(1);
//      }
//      break;
//    default:
//      /* parent */
//      return 0;
////      exit(0);
//    }
//  }
//
//  master_pid = getpid();
//  umask(S_IWGRP|S_IWOTH);
//  if ((fp = fopen(pidfile, "w")) != NULL) {
//    fprintf(fp, "%u\n", (unsigned)master_pid);
//    fchown(fileno(fp), PROCUID, PROCGID);
//    fclose(fp);
//  } else {
//    msg_out(warn, "cannot open pidfile %s", pidfile);
//  }

  setsignal(SIGHUP, reload);
  setsignal(SIGINT, SIG_IGN);
  setsignal(SIGQUIT, SIG_IGN);
  setsignal(SIGILL, SIG_IGN);
  setsignal(SIGTRAP, SIG_IGN);
  setsignal(SIGABRT, SIG_IGN);
#ifndef LINUX
  setsignal(SIGEMT, SIG_IGN);
#endif
  setsignal(SIGFPE, SIG_IGN);
  setsignal(SIGBUS, SIG_IGN);
  setsignal(SIGSEGV, SIG_IGN);
  setsignal(SIGSYS, SIG_IGN);
  setsignal(SIGPIPE, SIG_IGN);
  setsignal(SIGALRM, SIG_IGN);
  setsignal(SIGTERM, cleanup);
  setsignal(SIGUSR1, SIG_IGN);
  setsignal(SIGUSR2, SIG_IGN);
#if !defined(FREEBSD) && !defined(MACOSX)
  setsignal(SIGPOLL, SIG_IGN);
#endif
  setsignal(SIGVTALRM, SIG_IGN);
  setsignal(SIGPROF, SIG_IGN);
  setsignal(SIGXCPU, SIG_IGN);
  setsignal(SIGXFSZ, SIG_IGN);

#ifdef USE_THREAD
  if ( threading ) {
    if (max_thread <= 0 || max_thread > THREAD_LIMIT) {
      max_thread = THREAD_LIMIT;
    }
    /* resource limit is problem in threadig (e.g. Solaris:=64)*/
    memset((caddr_t)&rl, 0, sizeof rl);
    if (getrlimit(RLIMIT_NOFILE, &rl) != 0)
      msg_out(warn, "getrlimit: %m");
    else
      save_fd = rl.rlim_cur;
    if (rl.rlim_cur < (rlim_t)max_fd)
      rl.rlim_cur = max_fd;        /* willing to fix to max_fd */
    if ( rl.rlim_cur != save_fd )  /* if rlim_cur is changed   */
      if (setrlimit(RLIMIT_NOFILE, &rl) != 0)
        msg_out(warn, "cannot set rlimit(max_fd)");

    setregid(0, PROCGID);
    setreuid(0, PROCUID);

    pthread_mutex_init(&mutex_select, NULL);
    /*    pthread_mutex_init(&mutex_gh0, NULL); */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    msg_out(norm, "Starting: MAX_TH(%d)", max_thread);
    for (i=0; i<max_thread; i++) {
      if (pthread_create(&tid, &attr,
			 (void *)&serv_loop, (void *)NULL) != 0)
        exit(1);
    }
//    main_thread = pthread_self();   /* store main thread ID */
//    for (;;) {
//      pause();
//    }
  } else {
#endif
    setsignal(SIGCHLD, reapchild);
    setregid(0, PROCGID);
    setreuid(0, PROCUID);
    msg_out(norm, "Starting: MAX_CH(%d)", max_child);
    serv_loop();
#ifdef USE_THREAD
  }
#endif
  return(0);
}
