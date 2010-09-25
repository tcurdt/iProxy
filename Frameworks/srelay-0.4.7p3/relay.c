/*
  relay.c:
  $Id: relay.c,v 1.11 2009/12/17 14:49:50 bulkstream Exp $

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

#include "srelay.h"

#ifdef HAVE_LIBWRAP
# include <tcpd.h>
# ifdef LINUX
#  include <syslog.h>
int    allow_severity = LOG_AUTH|LOG_INFO;
int    deny_severity  = LOG_AUTH|LOG_NOTICE;
# endif /* LINUX */
extern int hosts_ctl __P((char *, char *, char *, char *));
#endif /* HAVE_LIBWRAP */

#define TIMEOUTSEC   30

typedef struct {
  int from, to;
  size_t nr, nw;
  ssize_t nread, nwritten;
  int oob;     /* flag OOB */
  char buf[BUFSIZE];
} rlyinfo;

typedef struct {
  struct sockaddr_storage mys;
  socklen_t  mys_len;
  struct sockaddr_storage myc;
  socklen_t  myc_len;
  struct sockaddr_storage prs;
  socklen_t  prs_len;
  struct sockaddr_storage prc;
  socklen_t  prc_len;
  u_long upl;
  u_long dnl;
  u_long bc;
  struct timeval elp;
} loginfo;
  
int resolv_client;

/* proto types */
void readn __P((rlyinfo *));
void writen __P((rlyinfo *));
ssize_t forward __P((rlyinfo *));
int validate_access __P((char *, char *));
int set_sock_info __P((loginfo *, int, int));
void relay __P((int, int));
int log_transfer __P((loginfo *));

void readn(rlyinfo *ri)
{
  ri->nread = 0;
  if (ri->oob == 0) {
    ri->nread = read(ri->from, ri->buf, ri->nr);
  } else {
    ri->nread = recvfrom(ri->from, ri->buf, ri->nr, MSG_OOB, NULL, 0);
  }
  if (ri->nread < 0) {
    msg_out(warn, "read: %m");
  }
}

void writen(rlyinfo *ri)
{
  ri->nwritten = 0;
  if (ri->oob == 0) {
    ri->nwritten = write(ri->to, ri->buf, ri->nw);
  } else {
    ri->nwritten = sendto(ri->to, ri->buf, ri->nw, MSG_OOB, NULL, 0);
  }
  if (ri->nwritten <= 0) {
    msg_out(warn, "write: %m");
  }
}

ssize_t forward(rlyinfo *ri)
{
  settimer(TIMEOUTSEC);
  readn(ri);
  if (ri->nread > 0) {
    ri->nw = ri->nread;
    writen(ri);
  }
  settimer(0);
  if (ri->nread == 0)
    return(0);           /* EOF */
  if (ri->nread < 0)
    return(-1);
  return(ri->nwritten);
}

int validate_access(char *client_addr, char *client_name)
{
  int stat = 0;
#ifdef HAVE_LIBWRAP
  int i;

  if ( use_tcpwrap ) {
    /* proc ident pattern */
    stat = hosts_ctl(ident, client_name, client_addr, STRING_UNKNOWN);
    /* IP.PORT pattern */
    for (i = 0; i < serv_sock_ind; i++) {
      if (str_serv_sock[i] != NULL && str_serv_sock[i][0] != 0) {
	stat |= hosts_ctl(str_serv_sock[i],
			  client_name, client_addr, STRING_UNKNOWN);
      }
    }
  } else {
#endif /* HAVE_LIBWRAP */
    stat = 1;  /* allow access un-conditionaly */
#ifdef HAVE_LIBWRAP
  }
#endif /* HAVE_LIBWRAP */

  if (stat < 1) {
    msg_out(warn, "%s[%s] access denied.", client_name, client_addr);
  }

  return stat;
}

int set_sock_info(loginfo *li, int cs, int ss)
{
  /* prepare sockaddr info for logging */
  int len;

  len = sizeof(struct sockaddr_storage);
  /* get socket name of upstream side */
  getsockname(ss, (struct sockaddr *)&li->mys, (socklen_t *)&len);
  li->mys_len = len;

  len = sizeof(struct sockaddr_storage);
  /* get socket name of downstream side */
  getsockname(cs, (struct sockaddr *)&li->myc, (socklen_t *)&len);
  li->myc_len = len;

  len = sizeof(struct sockaddr_storage);
  /* get socket ss peer name */
  getpeername(ss, (struct sockaddr *)&li->prs, (socklen_t *)&len);
  li->prs_len = len;

  len = sizeof(struct sockaddr_storage);
  /* get socket cs peer name */
  getpeername(cs, (struct sockaddr *)&li->prc, (socklen_t *)&len);
  li->prc_len = len;
  return 0;
}


u_long idle_timeout = IDLE_TIMEOUT;

void relay(int cs, int ss)
{
  fd_set   rfds, xfds;
  int      nfds, sfd;
  struct   timeval tv, ts, ots, elp;
  struct   timezone tz;
  ssize_t  wc;
  rlyinfo  ri;
  int      done;
  u_long   max_count = idle_timeout;
  u_long   timeout_count;
  loginfo  li;

  memset(&ri, 0, sizeof(ri));
  ri.nr = BUFSIZE;

  memset(&li, 0, sizeof(li));
  set_sock_info(&li, cs, ss);

  nfds = (ss > cs ? ss : cs);
  setsignal(SIGALRM, timeout);
  gettimeofday(&ots, &tz);
  li.bc = li.upl = li.dnl = 0; ri.oob = 0; timeout_count = 0;
  for (;;) {
    FD_ZERO(&rfds);
    FD_SET(cs, &rfds); FD_SET(ss, &rfds);
    if (ri.oob == 0) {
      FD_ZERO(&xfds);
      FD_SET(cs, &xfds); FD_SET(ss, &xfds);
    }
    done = 0;
    /* idle timeout related setting. */
    tv.tv_sec = 60; tv.tv_usec = 0;   /* unit = 1 minute. */
    tz.tz_minuteswest = 0; tz.tz_dsttime = 0;
    sfd = select(nfds+1, &rfds, 0, &xfds, &tv);
    if (sfd > 0) {
      if (FD_ISSET(ss, &rfds)) {
	ri.from = ss; ri.to = cs; ri.oob = 0;
	if ((wc = forward(&ri)) <= 0)
	  done++;
	else
	  li.bc += wc; li.dnl += wc;

	FD_CLR(ss, &rfds);
      }
      if (FD_ISSET(ss, &xfds)) {
	ri.from = ss; ri.to = cs; ri.oob = 1;
	if ((wc = forward(&ri)) <= 0)
	  done++;
	else
	  li.bc += wc; li.dnl += wc;
	FD_CLR(ss, &xfds);
      }
      if (FD_ISSET(cs, &rfds)) {
	ri.from = cs; ri.to = ss; ri.oob = 0;
	if ((wc = forward(&ri)) <= 0)
	  done++;
	else
	  li.bc += wc; li.upl += wc;
	FD_CLR(cs, &rfds);
      }
      if (FD_ISSET(cs, &xfds)) {
	ri.from = cs; ri.to = ss; ri.oob = 1;
	if ((wc = forward(&ri)) <= 0)
	  done++;
	else
	  li.bc += wc; li.upl += wc;
	FD_CLR(cs, &xfds);
      }
      if (done > 0)
	break;
    } else if (sfd < 0) {
      if (errno != EINTR)
	break;
    } else { /* sfd == 0 */
      if (max_count != 0) {
	timeout_count++;
	if (timeout_count > max_count)
	  break;
      }
    }
  }
  gettimeofday(&ts, &tz);
  if (ts.tv_usec < ots.tv_usec) {
    ts.tv_sec--; ts.tv_usec += 1000000;
  }
  elp.tv_sec = ts.tv_sec - ots.tv_sec;
  elp.tv_usec = ts.tv_usec - ots.tv_usec;
  li.elp = elp;

  log_transfer(&li);

  close(ss);
  close(cs);
}

#ifdef USE_THREAD
pthread_mutex_t mutex_select;
pthread_mutex_t mutex_gh0;
#endif

int serv_loop()
{
  int    cs, ss = 0;
  struct sockaddr_storage cl;
  fd_set readable;
  int    i, n, len;
  char   cl_addr[NI_MAXHOST];
  char   cl_name[NI_MAXHOST];
  int    error;
  pid_t  pid;

#ifdef USE_THREAD
  if (threading) {
    blocksignal(SIGHUP);
    blocksignal(SIGINT);
    blocksignal(SIGUSR1);
  }
#endif

  for (;;) {
    readable = allsock;

    MUTEX_LOCK(mutex_select);
    n = select(maxsock+1, &readable, 0, 0, 0);
    if (n <= 0) {
      if (n < 0 && errno != EINTR) {
        msg_out(warn, "select: %m");
      }
      MUTEX_UNLOCK(mutex_select);
      continue;
    }

#ifdef USE_THREAD
    if ( ! threading ) {
#endif
      /* handle any queued signal flags */
      if (FD_ISSET(sig_queue[0], &readable)) {
        if (ioctl(sig_queue[0], FIONREAD, &i) != 0) {
          msg_out(crit, "ioctl: %m");
          exit(-1);
        }
        while (--i >= 0) {
          char c;
          if (read(sig_queue[0], &c, 1) != 1) {
            msg_out(crit, "read: %m");
            exit(-1);
          }
          switch(c) {
          case 'H': /* sighup */
            reload();
            break;
          case 'C': /* sigchld */
            reapchild();
            break;
          case 'T': /* sigterm */
            cleanup();
            return 0;
            break;
          default:
            break;
          }
        }
      }
#ifdef USE_THREAD
    }
#endif

    for ( i = 0; i < serv_sock_ind; i++ ) {
      if (FD_ISSET(serv_sock[i], &readable)) {
	n--;
	break;
      }
    }
    if ( n < 0 || i >= serv_sock_ind ) {
      MUTEX_UNLOCK(mutex_select);
      continue;
    }

    len = sizeof(struct sockaddr_storage);
    cs = accept(serv_sock[i], (struct sockaddr *)&cl, (socklen_t *)&len);
    if (cs < 0) {
      if (errno == EINTR
#ifdef SOLARIS
	  || errno == EPROTO
#endif
	  || errno == EWOULDBLOCK
	  || errno == ECONNABORTED) {
	; /* ignore */
      } else {
	/* real accept error */
	msg_out(warn, "accept: %m");
      }
      MUTEX_UNLOCK(mutex_select);
      continue;
    }
    MUTEX_UNLOCK(mutex_select);

#ifdef USE_THREAD
    if ( !threading ) {
#endif
      if (max_child > 0 && cur_child >= max_child) {
	msg_out(warn, "child: cur %d; exeedeing max(%d)",
		          cur_child, max_child);
	close(cs);
	continue;
      }
#ifdef USE_THREAD
    }
#endif

    error = getnameinfo((struct sockaddr *)&cl, len,
			cl_addr, sizeof(cl_addr),
			NULL, 0,
			NI_NUMERICHOST);
    if (resolv_client) {
      error = getnameinfo((struct sockaddr *)&cl, len,
			  cl_name, sizeof(cl_name),
			  NULL, 0, 0);
      msg_out(norm, "%s[%s] connected", cl_name, cl_addr);
    } else {
      msg_out(norm, "%s connected", cl_addr);
      strncpy(cl_name, cl_addr, sizeof(cl_name));
    }

    i = validate_access(cl_addr, cl_name);
    if (i < 1) {
      /* access denied */
      close(cs);
      continue;
    }

    set_blocking(cs);

#ifdef USE_THREAD
    if (!threading ) {
#endif
      blocksignal(SIGHUP);
      blocksignal(SIGCHLD);
      pid = fork();
      switch (pid) {
        case -1:  /* fork child failed */
            printf("\nfork failed\n");
            break;
        case 0:   /* i am child */
            for ( i = 0; i < serv_sock_ind; i++ ) {
              close(serv_sock[i]);
            }
            setsignal(SIGCHLD, SIG_DFL);
                setsignal(SIGHUP, SIG_DFL);
                releasesignal(SIGCHLD);
                releasesignal(SIGHUP);
            ss = proto_socks(cs);
            if ( ss == -1 ) {
              close(cs);  /* may already be closed */
              exit(1);
            }
            // printf("\nrelaying\n");
            relay(cs, ss);
            exit(0);
      default: /* may be parent */
            // printf("\nadding proc\n");
            proclist_add(pid);
            break;
      }
      close(cs);
      releasesignal(SIGHUP);
      releasesignal(SIGCHLD);
#ifdef USE_THREAD
    } else {
      ss = proto_socks(cs);
      if ( ss == -1 ) {
        close(cs);  /* may already be closed */
        continue;
      }
      relay(cs, ss);
    }
#endif
  }
}

int log_transfer(loginfo *li)
{

  char    prc_ip[NI_MAXHOST], prs_ip[NI_MAXHOST];
  char    myc_port[NI_MAXSERV], mys_port[NI_MAXSERV];
  char    prc_port[NI_MAXSERV], prs_port[NI_MAXSERV];
  int     error = 0;

  error = getnameinfo((struct sockaddr *)&li->myc, li->myc_len,
		      NULL, 0,
		      myc_port, sizeof(myc_port),
		      NI_NUMERICHOST|NI_NUMERICSERV);
  error = getnameinfo((struct sockaddr *)&li->mys, li->mys_len,
		      NULL, 0,
		      mys_port, sizeof(mys_port),
		      NI_NUMERICHOST|NI_NUMERICSERV);
  error = getnameinfo((struct sockaddr *)&li->prc, li->prc_len,
		      prc_ip, sizeof(prc_ip),
		      prc_port, sizeof(prc_port),
		      NI_NUMERICHOST|NI_NUMERICSERV);
  error = getnameinfo((struct sockaddr *)&li->prs, li->prs_len,
		      prs_ip, sizeof(prs_ip),
		      prs_port, sizeof(prs_port),
		      NI_NUMERICHOST|NI_NUMERICSERV);

  msg_out(norm, "%s:%s-%s/%s-%s:%s %u(%u/%u) %u.%06u",
	  prc_ip, prc_port, myc_port,
	  mys_port, prs_ip, prs_port,
	  li->bc, li->upl, li->dnl,
	  li->elp.tv_sec, li->elp.tv_usec);

  return(0);
}
