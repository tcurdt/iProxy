/*
  util.c:
  $Id: util.c,v 1.8 2009/12/09 04:07:53 bulkstream Exp $

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

#include <syslog.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "srelay.h"

#ifdef LOCAL_FAC
# define LOCALFAC  LOCAL_FAC
#else
# define LOCALFAC  0
#endif
#ifdef SYSLOG_FAC
#  define SYSLOGFAC (SYSLOG_FAC|LOCALFAC)
#else
#  define SYSLOGFAC (LOG_DAEMON|LOCALFAC)
#endif

int forcesyslog = 0;

void msg_out(int severity, const char *fmt, ...)
{
  if (severity == norm) return;

  va_list ap;
//  int priority;

//  switch (severity) {
//  case crit:
//    priority = SYSLOGFAC|LOG_ERR;
//    break;
//  case warn:
//    priority = SYSLOGFAC|LOG_WARNING;
//    break;
//  case norm:
//  default:
//    priority = SYSLOGFAC|LOG_INFO;
//    break;
//  }

  va_start(ap, fmt);
//  if (!forcesyslog && isatty(fileno(stderr))) {
//    vfprintf(stderr, fmt, ap);
//  } else {
//    vsyslog(priority, fmt, ap);
//  }
  vfprintf(stdout, fmt, ap);

  printf("\n");

  va_end(ap);
}

/*
  addr_comp:
      compare bin_addr
*/
int addr_comp(struct bin_addr *a1, struct bin_addr *a2, int mask)
{
  int    ret = -1;
  u_int32_t smask;
  struct in_addr sin1, sin2;

  u_int16_t  f, r, smask16;
  int      i;
  struct in6_addr sin61, sin62;

  struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
  struct in_addr  inaddr_any;

  inaddr_any.s_addr = INADDR_ANY;

  if (a1->atype != a2->atype)
    return -1;             /* address type mismatched */

  /*
    if a2 entry is wildcard, everything is matched.
    if mask == 0, the mask could not be set in conf or,
    meaning-less setting. I'd rather guess former.

  */
    
  switch (a1->atype) {

  case S5ATIPV4:
    if (memcmp(a2->v4_addr,
	       &inaddr_any, sizeof inaddr_any) == 0) { /* wild card */
      ret = 0;
      break;
    }

    if (mask == 0 || mask == 32) { /* no need to process mask */
      ret = memcmp(a2->v4_addr, a1->v4_addr, sizeof(struct in_addr));

    } else if (mask > 0 && mask < 32) { /* process address mask */
      smask = ( 0xffffffff << (32-mask) ) & 0xffffffff;
      memcpy(&sin1, a1->v4_addr, sizeof(struct in_addr));
      memcpy(&sin2, a2->v4_addr, sizeof(struct in_addr));
      sin1.s_addr &= htonl(smask);
      sin2.s_addr &= htonl(smask);
      ret = memcmp(&sin1, &sin2, sizeof(struct in_addr));
    }
    break;
  
  case S5ATIPV6:
    if (memcmp(a2->v6_addr,
	       &in6addr_any, sizeof in6addr_any) == 0) { /* wild card */
      ret = 0;
      break;
    }

    if (a2->v6_scope != a1->v6_scope) {
      ret = -1;
      break;
    }

    if (mask == 0 || mask == 128) { /* no need to process mask */
      ret = memcmp(a2->v6_addr, a1->v6_addr, sizeof(struct in6_addr));

    } else if (mask > 0 && mask < 128) { /* process address mask */
      f = mask / 8;
      r = mask % 8;
      if ( f > 16 ) { /* ??? why ??? */
	f = 16; r = 0;
      }
      memcpy(&sin61, a1->v6_addr, sizeof(struct in6_addr));
      memcpy(&sin62, a2->v6_addr, sizeof(struct in6_addr));
      ret = 0;
      for (i=0; i<f; i++) {
	if (sin61.s6_addr[i] != sin62.s6_addr[i]) {
	  ret = -1;
	  break;
	}
      }
      if (ret == 0) {
	if (f < 16 && r > 0) {
	  smask16 = (0xff << (8-r)) & 0xff;
	  sin61.s6_addr[f] &= smask16;
	  sin62.s6_addr[f] &= smask16;
	  ret = memcmp(&sin61, &sin62, sizeof(struct in6_addr));
	}
      }
    }
    break;

  case S5ATFQDN:
    if (strncmp((char *)a2->fqdn, "*", strlen("*")) == 0) { /* wild card */
      ret = 0;
      break;
    }
    if ( a1->len_fqdn >= a2->len_fqdn ) {
      ret = strncasecmp((char *)a2->fqdn,
			(char *)(&(a1->fqdn[a1->len_fqdn - a2->len_fqdn])),
			a2->len_fqdn);
    }
    break;

  default:
    ret = -1;

  }
  return ret;
}

/*
  set_blocking:
          i/o mode to descriptor
 */
void set_blocking(int s)
{
  int flags;

  flags = fcntl(s, F_GETFL);
  if (flags >= 0) {
    fcntl(s, F_SETFL, flags & ~O_NONBLOCK);
  }
}

int settimer(int sec)
{
  struct itimerval it, ot;

  it.it_interval.tv_sec = 0;
  it.it_interval.tv_usec = 0;
  it.it_value.tv_sec = sec;
  it.it_value.tv_usec = 0;
  if (setitimer(ITIMER_REAL, &it, &ot) == 0) {
    return(ot.it_value.tv_sec);
  } else {
    return(-1);
  }
}

void feed_sig(char c)
{
  if (write(sig_queue[1], &c, 1) != 1) {
    msg_out(crit, "write: %m");
    exit(-1);
  }
}

/**  SIGALRM handling **/
void timeout(int signo)
{
  return;
}

void do_sigchld(int signo)
{
  feed_sig('C');
}

void do_sighup(int signo)
{
  feed_sig('H');
}

void do_sigterm(int signo)
{
  feed_sig('T');
}

void reapchild()
{
  int olderrno = errno;
  pid_t pid;
  int status, count;

  count=0;
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    proclist_drop(pid);
    if (count++ > 1000) {
      /* reapchild: waitpid loop */
      break;
    }
  }
  errno = olderrno;
}

void cleanup()
{
  int i;
  for ( i = 0; i < serv_sock_ind; i++ ) {
    close(serv_sock[i]);
  }
  /* unlink PID file */
  if (pidfile != NULL) {
    setreuid(PROCUID, 0);
    unlink(pidfile);
    setreuid(0, PROCUID);
  }
  msg_out(norm, "sig TERM received. exitting...");
  // exit(0);
}

void reload()
{
  FILE *fp;

#ifdef USE_THREAD
  if (threading) {
    if (! pthread_equal(pthread_self(), main_thread)) {
      /* do nothing */
      return;
    }
  }
#endif
  msg_out(norm, "reloading initiated ...");
  if ((fp = fopen(config, "r")) != NULL) {
    if (readconf(fp) != 0)
      msg_out(warn, "reloading failed.");
    fclose(fp);
  }
  msg_out(norm, "reloading done.");
}

sigfunc_t setsignal(int sig, sigfunc_t handler)
{
  struct sigaction n, o;

  memset(&n, 0, sizeof n);
  n.sa_handler = handler;

  if (sig != SIGALRM) {
    n.sa_flags = SA_RESTART;
  }

  if (sigaction(sig, &n, &o) < 0)
    return SIG_ERR;
  return o.sa_handler;
}

int blocksignal(int sig)
{
  sigset_t sset, oset;

  sigemptyset(&sset);
  sigaddset(&sset, sig);
  if (sigprocmask(SIG_BLOCK, &sset, &oset) < 0)
    return -1;
  else
    return sigismember(&oset, sig);
}

int releasesignal(int sig)
{
  sigset_t sset, oset;

  sigemptyset(&sset);
  sigaddset(&sset, sig);
  if (sigprocmask(SIG_UNBLOCK, &sset, &oset) < 0)
    return -1;
  else
    return sigismember(&oset, sig);
}

static pid_t    *proclist = NULL;
static int      proclist_size = 0;
#define PROCLIST_SEG      32

void proclist_add(pid_t pid)
{
  int i;
  void proclist_probe(void);

  for (i = 0; i < proclist_size; i++) {
    if (proclist[i] == (pid_t)0)
      break;
  }
  if (i >= proclist_size) {
    proclist_probe();
    for (i = 0; i < proclist_size; i++) {
      if (proclist[i] == 0)
	break;
    }
  }
  if (i >= proclist_size) {
    pid_t *new_proclist;

    new_proclist = (pid_t *)malloc(sizeof(pid_t)
				   * (proclist_size + PROCLIST_SEG));
    if (new_proclist == (pid_t *)NULL) {
      msg_out(warn, "could not malloc proclist");
      /* XXX */
      return;
    }
    if (proclist_size > 0) {
      memcpy(proclist, new_proclist, proclist_size * sizeof(pid_t));
      free(proclist);
    }
    for (i = proclist_size; i < proclist_size + PROCLIST_SEG; i++) {
      new_proclist[i] = 0;
    }
    i = proclist_size;
    proclist_size += PROCLIST_SEG;
    proclist = new_proclist;
  }
  proclist[i] = pid;
  cur_child++;
}

void proclist_drop(pid_t pid)
{
  int i;

  for (i = 0; i < proclist_size; i++) {
    if (proclist[i] == pid) {
      proclist[i] = 0;
      break;
    }
  }
  if (cur_child > 0)
    cur_child--;
}

void proclist_probe()
{
  int i;

  for (i = 0; i < proclist_size; i++) {
    if (proclist[i] == 0)
      continue;
    if (kill(proclist[i], 0) < 0) {
      /* lost child pid */
      proclist[i] = 0;
      cur_child--;
    }
  }
  if (cur_child < 0)
    cur_child = 0;
}

// ----------------

void srelay_exit()
{
  feed_sig('T');
}