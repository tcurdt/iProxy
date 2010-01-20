/*
  socks.c:
  $Id: socks.c,v 1.18 2009/12/17 15:47:36 bulkstream Exp $

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

#define S5REQ_CONN    1
#define S5REQ_BIND    2
#define S5REQ_UDPA    3

#define S5AGRANTED    0
#define S5EGENERAL    1
#define S5ENOTALOW    2
#define S5ENETURCH    3 
#define S5EHOSURCH    4
#define S5ECREFUSE    5
#define S5ETTLEXPR    6
#define S5EUNSUPRT    7
#define S5EUSATYPE    8
#define S5EINVADDR    9

#define S4REQ_CONN    1
#define S4REQ_BIND    2

#define S4AGRANTED    90
#define S4EGENERAL    91
#define S4ECNIDENT    92
#define S4EIVUSRID    93

#define TIMEOUTSEC    30

#define GEN_ERR_REP(s, v) \
    switch ((v)) { \
    case 0x04:\
      socks_rep((s), (v), S4EGENERAL, 0);\
      close((s));\
      break;\
    case 0x05:\
      socks_rep((s), (v), S5EGENERAL, 0);\
      close((s));\
      break;\
    case -1:\
      break;\
    default:\
      break;\
    }\


#define POSITIVE_REP(s, v, a) \
    switch ((v)) { \
    case 0x04:\
      error = socks_rep((s), (v), S4AGRANTED, (a));\
      break;\
    case 0x05:\
      error = socks_rep((s), (v), S5AGRANTED, (a));\
      break;\
    case -1:\
      error = 0;\
      break;\
    default:\
      error = -1;\
      break;\
    }\


struct host_info {
  char    host[NI_MAXHOST];
  char    port[NI_MAXSERV];
};

struct req_host_info {
  struct  host_info dest;
  struct  host_info proxy;
};

/* prototypes */
int proto_socks4 __P((struct socks_req *));
int proto_socks5 __P((struct socks_req *));
int socks_direct_conn __P((struct socks_req *));
int proxy_connect __P((struct socks_req *));
int build_socks_request __P((struct socks_req *, u_char *, int));
int connect_to_socks __P((struct socks_req *, int));
int socks_proxy_reply __P((int, struct socks_req *));
int socks_rep __P((int , int , int , struct sockaddr *));
int build_socks_reply __P((int, int, struct sockaddr *, u_char *));
int s5auth_s __P((int));
int s5auth_s_rep __P((int, int));
int s5auth_c __P((int, struct socks_req *));
int connect_to_http __P((struct socks_req *));
int forward_connect __P((struct socks_req *, int *));
int bind_sock __P((int, struct socks_req *, struct addrinfo *));
int do_bind __P((int, struct addrinfo *, u_int16_t));

int get_line __P((int, char *, size_t));
int lookup_tbl __P((struct socks_req *));
int resolv_host __P((struct bin_addr *, u_int16_t, struct host_info *));
int log_request __P((struct socks_req *));


/*
  proto_socks:
               handle socks protocol.
*/
int proto_socks(int sock)
{
  u_char buf[128];
  int r;
  int on = 1;
  struct  socks_req req;
  size_t len;

  memset(&req, 0, sizeof(req));
  req.s = sock;
  req.r = -1;   /* forward socket not connected. */

  /* find the interface that the socket is connected */
  if (same_interface) {
    len = sizeof(req.inaddr);
    r = getsockname(sock, (struct sockaddr*)&req.inaddr, (socklen_t *)&len);
    if (r == 0) {
      switch (req.inaddr.ss_family) {
      case AF_INET:
	((struct sockaddr_in*) &req.inaddr)->sin_port = 0;
	break;
      case AF_INET6:
	((struct sockaddr_in6*) &req.inaddr)->sin6_port = 0;
	break;
      }
    }
  }

  r = timerd_read(sock, buf, sizeof(buf), TIMEOUTSEC, MSG_PEEK);
  if ( r <= 0 ) {
    close(sock);
    return(-1);
  }

  req.ver = buf[0];
  switch (req.ver) {
  case 4:
    if (method_num > 0) {
      /* this implies this server is working in V5 mode */
      GEN_ERR_REP(sock, 4);
      msg_out(warn, "V4 request is not accepted.");
      r = -1;
    } else {
      r = proto_socks4(&req);
    }
    break;
  case 5:
    if ((r = s5auth_s(sock)) == 0) {
      r = proto_socks5(&req);
    }
    break;
  default:
    r = -1;
  }

  if (r >= 0) {
    req.tbl_ind = lookup_tbl(&req);
    log_request(&req);

    if (req.rtbl.rl_meth == DIRECT) {
      r = socks_direct_conn(&req);
    } else {
      r = proxy_connect(&req);
    }
  }

  if (r >= 0) {
    setsockopt(req.r, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof on);
#if defined(FREEBSD) || defined(MACOSX)
    setsockopt(req.r, SOL_SOCKET, SO_REUSEPORT, (char *)&on, sizeof on);
#endif
    setsockopt(req.r, IPPROTO_TCP, TCP_NODELAY, (char *)&on, sizeof on);

    return(req.r);   /* connected forwarding socket descriptor */
  }
  if (req.r >= 0) {
    close(req.r);
  }
  req.r = -1;
  return(-1);  /* error */
}

/*  socks4 protocol functions */
/*
  proto_socks4:
           handle socks v4/v4a protocol.
	   get socks v4/v4a request from client.
*/
int proto_socks4(struct socks_req *req)
{
  u_char  buf[512];
  int     r, len;

  r = timerd_read(req->s, buf, 1+1+2+4, TIMEOUTSEC, 0);
  if (r < 1+1+2+4) {    /* cannot read request */
    GEN_ERR_REP(req->s, 4);
    return(-1);
  }
  if ( buf[0] != 0x04 ) {
    /* wrong version request (why ?) */
    GEN_ERR_REP(req->s, 4);
    return(-1);
  }
  req->req = buf[1];

  /* check if request has socks4-a domain name format */
  if ( buf[4] == 0 && buf[5] == 0 &&
       buf[6] == 0 && buf[7] != 0 ) {
    req->dest.atype = S4ATFQDN;
  } else {
    req->dest.atype = S4ATIPV4;
  }

  req->port = buf[2] * 0x100 + buf[3];

  if (req->dest.atype == S4ATIPV4) {
    memcpy(req->dest.v4_addr, &buf[4], 4);
  }
  
  /* read client user name in request */
  r = timerd_read(req->s, buf, sizeof(buf), TIMEOUTSEC, MSG_PEEK);
  if ( r < 1 ) {
    /* error or client sends EOF */
    GEN_ERR_REP(req->s, 4);
    return(-1);
  }
  /* buf could contains
          username '\0'
      or,
          username '\0' hostname '\0'
  */
  r = strlen((char *)buf);        /* r should be 0 <= r <= 255 */
  if (r < 0 || r > 255) {
    /* invalid username length */
    GEN_ERR_REP(req->s, 4);
    return(-1);
  }

  r = timerd_read(req->s, buf, r+1, TIMEOUTSEC, 0);
  if ( r > 0 && r <= 255 ) {    /* r should be 1 <= r <= 255 */
    len = r - 1;
    req->u_len = len;
    memcpy(req->user, buf, len);
  } else {
    /* read error or something */
    GEN_ERR_REP(req->s, 4);
    return(-1);
  }

  if ( req->dest.atype == S4ATFQDN ) {
    /* request is socks4-A specific */
    r = timerd_read(req->s, buf, sizeof buf, TIMEOUTSEC, 0);
    if ( r > 0 && r <= 256 ) {   /* r should be 1 <= r <= 256 */
      len = r - 1;
      req->dest.len_fqdn = len;
      memcpy(req->dest.fqdn, buf, len);
    } else {
      /* read error or something */
      GEN_ERR_REP(req->s, 4);
      return(-1);
    }
  }
  return(0);
}


/* socks5 protocol functions */
/*
  proto_socks5:
           handle socks v5 protocol.
	   get socks v5 request from client.
*/
int proto_socks5(struct socks_req *req)
{
  u_char    buf[512];
  int     r, len;

  /* peek first 5 bytes of request. */
  r = timerd_read(req->s, buf, sizeof(buf), TIMEOUTSEC, MSG_PEEK);
  if ( r < 5 ) {
    /* cannot read client request */
    close(req->s);
    return(-1);
  }

  if ( buf[0] != 0x05 ) {
    /* wrong version request */
    GEN_ERR_REP(req->s, 5);
    return(-1);
  }

  req->req = buf[1];
  req->dest.atype = buf[3];  /* address type field */

  switch(req->dest.atype) {
  case S5ATIPV4:  /* IPv4 address */
    r = timerd_read(req->s, buf, 4+4+2, TIMEOUTSEC, 0);
    if (r < 4+4+2) {     /* cannot read request (why?) */
      GEN_ERR_REP(req->s, 5);
      return(-1);
    }
    memcpy(req->dest.v4_addr, &buf[4], sizeof(struct in_addr));
    req->port = buf[8] * 0x100 + buf[9];
    break;

  case S5ATIPV6:
    r = timerd_read(req->s, buf, 4+16+2, TIMEOUTSEC, 0);
    if (r < 4+16+2) {     /* cannot read request (why?) */
      GEN_ERR_REP(req->s, 5);
      return(-1);
    }
    memcpy(req->dest.v6_addr, &buf[4], sizeof(struct in6_addr));
    req->port = buf[20] * 0x100 + buf[21];
    break;

  case S5ATFQDN:  /* string or FQDN */
    if ((len = buf[4]) < 0 || len > 255) {
      /* invalid length */
      socks_rep(req->s, 5, S5EINVADDR, 0);
      close(req->s);
      return(-1);
    }
    r = timerd_read(req->s, buf, 4+1+len+2, TIMEOUTSEC, 0);
    if ( r < 4+1+len+2 ) {  /* cannot read request (why?) */
      GEN_ERR_REP(req->s, 5);
      return(-1);
    }
    memcpy(req->dest.fqdn, &buf[5], len);
    req->dest.len_fqdn = len;
    req->port = buf[4+1+len] * 0x100 + buf[4+1+len+1];
    break;

  default:
    /* unsupported address */
    socks_rep(req->s, 5, S5EUSATYPE, 0);
    close(req->s);
    return(-1);
  }
  return(0);
}


/*
  socks_direct_conn:
      behave as socks server to connect/bind.
 */
int socks_direct_conn(struct socks_req *req)
{
  int    cs, acs = 0;
  int    len;
  struct addrinfo ba;
  struct sockaddr_storage ss;
  int    error = 0;
  int    save_errno = 0;

  /* process direct connect/bind to destination */

  /* process by_command request */
  switch (req->req) {   /* request */
  case S5REQ_CONN:
    cs = forward_connect(req, &save_errno);
    if (cs >= 0) {
      len = sizeof(ss);
      if (getsockname(cs, (struct sockaddr *)&ss, (socklen_t *)&len) < 0) {
	save_errno = errno;
	close(cs);
	cs = -1;
      }
    }
    if (cs < 0 || save_errno != 0) {
      /* any socket error */
      switch (req->ver) {
      case 0x04:
	GEN_ERR_REP(req->s, 4);
	break;
      case 0x05:
	switch(save_errno) {
	case ENETUNREACH:  socks_rep(req->s, 5, S5ENETURCH, 0); break;
	case ECONNREFUSED: socks_rep(req->s, 5, S5ECREFUSE, 0); break;
#ifndef _POSIX_SOURCE
	case EHOSTUNREACH: socks_rep(req->s, 5, S5EHOSURCH, 0); break;
#endif
	case ETIMEDOUT:    socks_rep(req->s, 5, S5ETTLEXPR, 0); break; /* ??? */
	default:           socks_rep(req->s, 5, S5EGENERAL, 0); break;
	}
	break;
      default:
	break;
      }
      close(req->s);
      return(-1);
    }
    break;

  case S5REQ_BIND:
    memset(&ba, 0, sizeof(ba));
    memset(&ss, 0, sizeof(ss));
    ba.ai_addr = (struct sockaddr *)&ss;
    ba.ai_addrlen = sizeof(ss);
    /* just one address can be stored */
    error = get_bind_addr(req, &ba);
    if (error) {
      GEN_ERR_REP(req->s, req->ver);
      return(-1);
    }
    acs = -1;
    acs = socket(ba.ai_family, ba.ai_socktype, ba.ai_protocol);
    if (acs < 0) {
      /* socket error */
      GEN_ERR_REP(req->s, req->ver);
      return(-1);
    }

    if (bind_sock(acs, req, &ba) != 0) {
      GEN_ERR_REP(req->s, req->ver);
      return(-1);
    }

    listen(acs, 64);
    /* get my socket name again to acquire an
       actual listen port number */
    len = sizeof(ss);
    if (getsockname(acs, (struct sockaddr *)&ss, (socklen_t *)&len) == -1) {
      /* getsockname failed */
      GEN_ERR_REP(req->s, req->ver);
      close(acs);
      return(-1);
    }

    /* first reply for bind request */
    POSITIVE_REP(req->s, req->ver, (struct sockaddr *)&ss);
    if ( error < 0 ) {
      /* could not reply */
      close(req->s);
      close(acs);
      return(-1);
    }
    if (wait_for_read(acs, TIMEOUTSEC) <= 0) {
      GEN_ERR_REP(req->s, req->ver);
      close(acs);
      return(-1);
    }
      
    len = sizeof(ss);
    if ((cs = accept(acs, (struct sockaddr *)&ss, (socklen_t *)&len)) < 0) {
      GEN_ERR_REP(req->s, req->ver);
      close(acs);
      return(-1);
    }
    close(acs); /* accept socket is not needed
		   any more, for current socks spec. */
    /* sock name is in ss */
    /* TODO:
     *  we must check ss against req->dest here for security reason
     */
    /* XXXXX */
    break;

  default:
    /* unsupported request */
    GEN_ERR_REP(req->s, req->ver);
    close(req->s);
    return(-1);
  }

  POSITIVE_REP(req->s, req->ver, (struct sockaddr *)&ss);
  if ( error < 0 ) {
    /* could not reply */
    close(req->s);
    close(cs);
    return(-1);
  }
  req->r = cs;   /* set forwarding socket */
  return(0);
}

/*   proxy socks functions  */
/*
  proxy_connect:
	   connect to next hop socks/ server.
           indirect connection to destination.
*/
int proxy_connect(struct socks_req *req)
{
  int     save_errno = 0;
  int     r = 0;
  struct socks_req cp_req;

  /* sanity check */
  /* relay method must not be DIRECT */
  /* forward socket should not be connected yet */
  if (req->rtbl.rl_meth < 1 || req->r >= 0) {
    /* shoud not be here */
    GEN_ERR_REP(req->s, req->ver);
    return(-1);
  }

  req->r = forward_connect(req, &save_errno);
  if (req->r < 0 || save_errno != 0) {
    GEN_ERR_REP(req->s, req->ver);
    req->r = -1;
    return(-1);
  }

  switch(req->rtbl.rl_meth) {
  case PROXY1:
    memcpy(&cp_req, req, sizeof(struct socks_req));
    cp_req.ver = -1; /* fake req ver to suppress resp to client */
    cp_req.req = S5REQ_CONN;
    memcpy(&cp_req.dest, &req->rtbl.prx[0].proxy, sizeof(struct bin_addr));
    cp_req.port = req->rtbl.prx[0].pport;
    if (req->rtbl.prx[1].pproto == HTTP) {
      r = connect_to_http(&cp_req);
    } else { /* SOCKS, SOCKSv4, SOCKSv5 */
      r = connect_to_socks(&cp_req, req->rtbl.prx[1].pproto);
    }
    if (r < 0) {
      GEN_ERR_REP(req->s, req->ver);
      req->r = -1;
      return(-1);
    }
    /* not break, just continue */
  case PROXY:
    if (req->rtbl.prx[0].pproto == HTTP) {
      /* limitation: cannot handle bind operation */
      return(connect_to_http(req));
    } else { /* SOCKS, SOCKSv4, SOCKSv5 */
      return(connect_to_socks(req, req->rtbl.prx[0].pproto));
    }
  default:
    break;
  }
  return(-1);

}

int connect_to_socks(struct socks_req *req, int pproto)
{
  int     r, len = 0;
  u_char  buf[640];

  if (req->r < 0) {
    GEN_ERR_REP(req->s, req->ver);
    return(-1);
  }

  /* process proxy request to next hop socks */
  /* first try socks5 server */
  if (pproto == SOCKS || pproto == SOCKSv5) {
    if ((len = build_socks_request(req, buf, 5)) > 0) {
      if (s5auth_c(req->r, req) == 0) {
	/* socks5 auth nego to next hop success */
	r = timerd_write(req->r, buf, len, TIMEOUTSEC);
	if ( r == len ) {
	  /* send request success */
	  r = socks_proxy_reply(5, req);
	  if (r == 0) {
	    return(req->r);
	  }
	}
      }
    }
  }

  /* if an error, second try socks4 server */
  if (pproto == SOCKS || pproto == SOCKSv4) {
    if ((len = build_socks_request(req, buf, 4)) > 0) {
      r = timerd_write(req->r, buf, len, TIMEOUTSEC);
      if ( r == len ) {
	/* send request success */
	r = socks_proxy_reply(4, req);
	if (r == 0) {
	  return(req->r);
	}
      }
    }
  }

  /* still be an error, give it up. */
  GEN_ERR_REP(req->s, req->ver);
  return(-1);
}

/*
  build_socks_request:
      build socks request on buf
      return buf length
 */
int build_socks_request(struct socks_req *req, u_char *buf, int ver)
{
  int     r, len = 0;
  char    *user;
  /* buf must be at least 640 bytes long */

  switch (ver) {   /* next hop socks server version */
  case 0x04:
    /* build v4 request */
    buf[0] = 0x04;
    buf[1] = req->req;
    if ( req->u_len == 0 ) {
      user = S4DEFUSR;
      r = strlen(user);
    } else {
      user = req->user;
      r = req->u_len;
    }
    if (r < 0 || r > 255) {
      return(-1);
    }
    buf[2] = (req->port / 256);
    buf[3] = (req->port % 256);
    memcpy(&buf[8], user, r);
    len = 8+r;
    buf[len++] = 0x00;
    switch (req->dest.atype) {
    case S4ATIPV4:
      memcpy(&buf[4], req->dest.v4_addr, sizeof(struct in_addr));
      break;
    case S4ATFQDN:
      buf[4] = buf[5] = buf[6] = 0; buf[7] = 1;
      r = req->dest.len_fqdn;
      if (r <= 0 || r > 255) {
	return(-1);
      }
      memcpy(&buf[len], req->dest.fqdn, r);
      len += r;
      buf[len++] = 0x00;
      break;
    default:
      return(-1);
    }
    break;

  case 0x05:
    /* build v5 request */
    buf[0] = 0x05;
    buf[1] = req->req;
    buf[2] = 0;
    buf[3] = req->dest.atype;
    switch (req->dest.atype) {
    case S5ATIPV4:
      memcpy(&buf[4], req->dest.v4_addr, 4);
      buf[8] = (req->port / 256);
      buf[9] = (req->port % 256);
      len = 10;
      break;
    case S5ATIPV6:
      memcpy(&buf[4], req->dest.v6_addr, 16);
      buf[20] = (req->port / 256);
      buf[21] = (req->port % 256);
      len = 22;
      break;
    case S5ATFQDN:
      len = req->dest.len_fqdn;
      buf[4] = len;
      memcpy(&buf[5], req->dest.fqdn, len);
      buf[5+len]   = (req->port / 256);
      buf[5+len+1] = (req->port % 256);
      len = 5+len+2;
      break;
    default:
      return(-1);
    }
    break;
  default:
    return(-1);   /* unknown version */
  }
  return(len); /* OK */
}


/*
  socks_proxy_reply:
       v: server socks version.
       read server response and
       write converted reply to client if needed.
       note: req->ver == -1 means DEEP indirect proxy.
*/
int socks_proxy_reply(int v, struct socks_req *req)
{
  int     r, c, len;
  u_char  buf[512];
  struct  addrinfo hints, *res, *res0;
  int     error;
  int found = 0;
  struct sockaddr_storage ss;
  struct sockaddr_in *sa;
  struct sockaddr_in6 *sa6;

  sa = (struct sockaddr_in *)&ss;
  sa6 = (struct sockaddr_in6 *)&ss;

  switch (req->req) {
  case S5REQ_CONN:
    c = 1;
    break;

  case S5REQ_BIND:
    c = 2;
    break;

  default:   /* i don't know what to do */
    c = 1;
    break;
  }

  while (c-- > 0) {
    memset(&ss, 0, sizeof(ss));
    /* read server reply */
    r = timerd_read(req->r, buf, sizeof buf, TIMEOUTSEC, 0);

    if (req->ver == -1) {  /* special case */
      if ((v == 5 && buf[1] == S5AGRANTED)
	  || (v == 4 && buf[1] == S4AGRANTED)) {
	return(0);
      }
      return(-1);
    }

    switch (v) { /* server socks version */

    case 4: /* server v:4 */
      if ( r < 8 ) {  /* from v4 spec, r should be 8 */
	/* cannot read server reply */
	r = -1;
	break;
      }
      switch (req->ver) {  /* client ver */
      case 4: /* same version */
	r = timerd_write(req->s, buf, r, TIMEOUTSEC);
	break;

      case 5:
	if ( buf[1] == S4AGRANTED ) {
	  /* translate reply v4->v5 */
	  sa->sin_family = AF_INET;
	  memcpy(&(sa->sin_addr), &buf[4], 4);
	  memcpy(&(sa->sin_port), &buf[2], 2);
	  r = socks_rep(req->s, 5, S5AGRANTED, (struct sockaddr *)&ss);
	} else {
	  r = -1;
	}
	break;
      default:
	r = -1;
	break;
      }
      break;

    case 5: /* server v:5 */
      if ( r < 7 ) {   /* should be 10 or more */
	/* cannot read server reply */
	r = -1;
	break;
      }
      switch (req->ver) { /* client ver */
      case 4:
	/* translate reply v5->v4 */
	if ( buf[1] == S5AGRANTED ) {
	  switch (buf[3]) {   /* address type */
	  case S5ATIPV4:
	    sa->sin_family = AF_INET;
	    memcpy(&(sa->sin_addr), &buf[4], 4);
	    memcpy(&(sa->sin_port), &buf[8], 2);
	    break;
	  case S5ATIPV6:
	    /* basically v4 cannot handle IPv6 address */
	    /* make fake IPv4 to forcing reply */
	    sa->sin_family = AF_INET;
	    memcpy(&(sa->sin_addr), &buf[16], 4);
	    memcpy(&(sa->sin_port), &buf[20], 2);
	    break;
	  case S5ATFQDN:
	  default:
	    sa->sin_family = AF_INET;
	    len = buf[4] & 0xff;
	    memcpy(&(sa->sin_port), &buf[5+len], 2);
	    buf[5+len] = '\0';
	    memset(&hints, 0, sizeof(hints));
	    hints.ai_socktype = SOCK_STREAM;
	    hints.ai_family = AF_INET;
	    error = getaddrinfo((char *)&buf[5], NULL, &hints, &res0);
	    if (!error) {
	      for (res = res0; res; res = res->ai_next) {
		if (res->ai_socktype != AF_INET)
		  continue;
		memcpy(&(sa->sin_addr),
		       &(((struct sockaddr_in *)res->ai_addr)->sin_addr),
		       sizeof(struct in_addr));
		found++; break;
	      }
	      freeaddrinfo(res0);
	    }
	    if (!found) {
	      /* set fake ip */
	      memset(&(sa->sin_addr), 0, 4);
	    }
	    break;
	  }
	  r = socks_rep(req->s, 4, S4AGRANTED, (struct sockaddr *)&ss);
	} else {
	  r = -1;
	}
	break;
      case 5: /* same version */
	r = timerd_write(req->s, buf, r, TIMEOUTSEC);
	break;
      default:
	r = -1;
	break;
      }
      break;

    default:
      /* parameter error */
      r = -1;
      break;
    }

    if (r < 0) {
      return(r);
    }
  }
  return(0);
}


int socks_rep(int s, int ver, int code, struct sockaddr *addr)
{
  u_char     buf[512];
  int        len = 0, r;

  /* check */
  if (ver == -1) {
    return(0); /* special case */
  }

  memset(buf, 0, sizeof(buf));
  len = build_socks_reply(ver, code, addr, buf);

  if (len > 0)
    r = timerd_write(s, buf, len, TIMEOUTSEC);
  else
    r = -1;

  if (r < len)
    return -1;

  return 0;
}

int build_socks_reply(int ver, int code, struct sockaddr *addr, u_char *buf)
{

  int len = 0;

  switch (ver) {
  case 0x04:
    switch (code) {
    case S4AGRANTED:
      buf[0] = 0;
      buf[1] = code;   /* succeeded */
      if (addr) {
	memcpy(&buf[2], &(((struct sockaddr_in *)addr)->sin_port), 2);
	memcpy(&buf[4], &(((struct sockaddr_in *)addr)->sin_addr), 4);
      }
      len = 8;
      break;

    default:  /* error cases */
      buf[0] = ver;
      buf[1] = code;   /* error code */
      len = 8;
      break;
    }
    break;

  case 0x05:
    switch (code) {
    case S5AGRANTED:
      buf[0] = ver;
      buf[1] = code;   /* succeeded */
      buf[2] = 0;
      if (addr) {
	switch (addr->sa_family) {
	case AF_INET:
	  buf[3] = S5ATIPV4;
	  memcpy(&buf[4], &(((struct sockaddr_in *)addr)->sin_addr), 4);
	  memcpy(&buf[8], &(((struct sockaddr_in *)addr)->sin_port), 2);
	  len = 4+4+2;
	  break;
	case AF_INET6:
	  buf[3] = S5ATIPV6;
	  memcpy(&buf[4], &(((struct sockaddr_in6 *)addr)->sin6_addr), 16);
	  memcpy(&buf[20], &(((struct sockaddr_in6 *)addr)->sin6_port), 2);
	  len = 4+16+2;
	  break;
	default:
	  buf[1] = S5EUSATYPE;
	  buf[3] = S5ATIPV4;
	  len = 4+4+2;
	  break;
	}
      }
      break;

    default:  /* error cases */
      buf[0] = ver;
      buf[1] = code & 0xff;   /* error code */
      buf[2] = 0;
      buf[3] = S5ATIPV4;  /* addr type fixed to IPv4 */
      len = 10;
      break;
    }
    break;

  default:
    /* unsupported socks version */
    len = 0;
    break;
  }
  return(len);
}

/*
  socks5 auth negotiation as server.
*/
int s5auth_s(int s)
{
  u_char buf[512];
  int r, i, j, len;
  int method=0, done=0;

  /* auth method negotiation */
  r = timerd_read(s, buf, 2, TIMEOUTSEC, 0);
  if ( r < 2 ) {
    /* cannot read */
    s5auth_s_rep(s, S5ANOTACC);
    return(-1);
  }

  len = buf[1];
  if ( len < 0 || len > 255 ) {
    /* invalid number of methods */
    s5auth_s_rep(s, S5ANOTACC);
    return(-1);
  }

  r = timerd_read(s, buf, len, TIMEOUTSEC, 0);
  if (method_num == 0) {
    for (i = 0; i < r; i++) {
      if (buf[i] == S5ANOAUTH) {
	method = S5ANOAUTH;
	done = 1;
	break;
      }
    }
  } else {
    for (i = 0; i < method_num; i++) {
      for (j = 0; j < r; j++) {
	if (buf[j] == method_tab[i]){
	  method = method_tab[i];
	  done = 1;
	  break;
	}
      }
      if (done)
	break;
    }
  }
  if (!done) {
    /* no suitable method found */
    method = S5ANOTACC;
  }

  if (s5auth_s_rep(s, method) < 0)
    return(-1);

  switch (method) {
  case S5ANOAUTH:
    /* heh, do nothing */
    break;
  case S5AUSRPAS:
    if (auth_pwd_server(s) == 0) {
      break;
    } else {
      close(s);
      return(-1);
    }
  default:
    /* other methods are unknown or not implemented */
    close(s);
    return(-1);
  }
  return(0);
}

/*
  Auth method negotiation reply
*/
int s5auth_s_rep(int s, int method)
{
  u_char buf[2];
  int r;

  /* reply to client */
  buf[0] = 0x05;   /* socks version */
  buf[1] = method & 0xff;   /* authentication method */
  r = timerd_write(s, buf, 2, TIMEOUTSEC);
  if (r < 2) {
    /* write error */
    close(s);
    return(-1);
  }
  return(0);
}

/*
  socks5 auth negotiation as client.
*/
int s5auth_c(int s, struct socks_req *req)
{
  u_char buf[512];
  int r, num=0;

  /* auth method negotiation */
  buf[0] = 0x05;
  buf[1] = 1;           /* number of methods.*/
  buf[2] = S5ANOAUTH;   /* no authentication */
  num = 3;

  if ( pwdfile != NULL ) {
    buf[1] = 2;
    buf[3] = S5AUSRPAS;   /* username/passwd authentication */
    num++;
  }
  r = timerd_write(s, buf, num, TIMEOUTSEC);
  if ( r < num ) {
    /* cannot write */
    close(s);
    return(-1);
  }

  r = timerd_read(s, buf, 2, TIMEOUTSEC, 0);
  if ( r < 2 ) {
    /* cannot read */
    close(s);
    return(-1);
  }
  if (buf[0] == 0x05 && buf[1] == 0) {
    /* no auth method is accepted */
    return(0);
  }
  if (buf[0] == 0x05 && buf[1] == 2) {
    /* do username/passwd authentication */
    return(auth_pwd_client(s, req));
  }
  /* auth negotiation failed */
  return(-1);
}

int connect_to_http(struct socks_req *req)
{
  struct host_info dest;
  char   buf[1024];
  int    error = 0;
  int    c, r, len;
  struct sockaddr_storage ss;
  char *p;

  p = buf;
  if (req->req != S5REQ_CONN) {
    /* cannot handle request */
    GEN_ERR_REP(req->s, req->ver);
    return(-1);
  }

  error = resolv_host(&req->dest, req->port, &dest);

  snprintf(buf, sizeof(buf), "CONNECT %s:%s HTTP/1.0\r\n\r\n",
	   dest.host, dest.port);
  /* http/proxy auth not supported. */

  /* debug */
  msg_out(norm, ">>HTTP CONN %s:%s", dest.host, dest.port);

  len = strlen(buf);
  r = timerd_write(req->r, (u_char *)buf, len, TIMEOUTSEC);

  if ( r == len ) {
    /* get resp */
    r = get_line(req->r, buf, sizeof(buf));
    if (r >= 12) {
      while (r>0 && ((c=*p) != ' ' && c != '\t'))
	p++; r--;
      while (r>0 && ((c=*p) == ' ' || c == '\t'))
	p++; r--;
      if (strncmp(p, "200", 3) == 0) {
	/* redirection not supported */
	do { /* skip resp headers */
	  r = get_line(req->r, buf, sizeof(buf));
	  if (r <= 0) {
	    GEN_ERR_REP(req->s, req->ver);
	    return(-1);
	  }
	} while (strcmp(buf, "\r\n") != 0);
	len = sizeof(ss);
	getsockname(req->r, (struct sockaddr *)&ss, (socklen_t *)&len);
	/* is this required ?? */
	POSITIVE_REP(req->s, req->ver, (struct sockaddr *)&ss);
	return(req->r);
      }
    }
  }
  GEN_ERR_REP(req->s, req->ver);
  return(-1);
}

/*
  forward_connect:
      just resolve host and connect to her.
      return connected socket/error(-1);
 */

int forward_connect(struct socks_req *req, int *err)
{
  int    cs;
  struct host_info dest;
  struct addrinfo hints, *res, *res0;
  int    error = 0;

  cs = -1;

  switch(req->rtbl.rl_meth) {
  case DIRECT:
    error = resolv_host(&req->dest, req->port, &dest);
    break;
  case PROXY:
    error = resolv_host(&req->rtbl.prx[0].proxy,
			req->rtbl.prx[0].pport, &dest);
    break;
  case PROXY1:
    error = resolv_host(&req->rtbl.prx[1].proxy,
			req->rtbl.prx[1].pport, &dest);
    break;
  default:
    return(-1);
  }

  /* string addr => addrinfo */
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;
  error = getaddrinfo(dest.host, dest.port, &hints, &res0);
  if (error) { /* getaddrinfo returns error>0 when error */
    return -1;
  }

  for (res = res0; res; res = res->ai_next) {
    error = 0;
    cs = socket(res->ai_family,
		res->ai_socktype, res->ai_protocol);
    if ( cs < 0 ) {
      /* socket error */
      continue;
    }

    if (same_interface) {
      /* bind the outgoing socket to the same interface
	 as the inbound client */
      if (bind(cs, (struct sockaddr*)&req->inaddr, sizeof(req->inaddr)) <0) {
	/* bind error */
	error = errno;
	close(cs);
	continue;
      }
    }

    if (connect(cs, res->ai_addr, res->ai_addrlen) < 0) {
      /* connect fail */
      error = errno;
      close(cs);
      continue;
    }
    break;
  }

  freeaddrinfo(res0);

  msg_out(norm, "Forward connect to %s:%s : %d",
	  dest.host, dest.port, error);
  if (err != NULL)
    *err = error;

  return(cs);
}

int bind_sock(int s, struct socks_req *req, struct addrinfo *ai)
{
  /*
    BIND port selection priority.
    1. requested port. (assuming dest->sin_port as requested port)
    2. clients src port.
    3. free port.
  */
  struct sockaddr_storage ss;
  u_int16_t  port;
  size_t     len;

  /* try requested port */
  if (do_bind(s, ai, req->port) == 0)
    return 0;

  /* try same port as client's */
  len = sizeof(ss);
  memset(&ss, 0, len);
  if (getpeername(req->s, (struct sockaddr *)&ss, (socklen_t *)&len) != 0)
    port = 0;
  else {
    switch (ss.ss_family) {
    case AF_INET:
      port = ntohs(((struct sockaddr_in *)&ss)->sin_port);
      break;
    case AF_INET6:
      port = ntohs(((struct sockaddr_in6 *)&ss)->sin6_port);
      break;
    default:
      port = 0;
    }
  }
  if (do_bind(s, ai, port) == 0)
    return 0;

  /*  bind free port */
  return(do_bind(s, ai, 0));
}

int do_bind(int s, struct addrinfo *ai, u_int16_t p)
{
  u_int16_t port = p;  /* Host Byte Order */
  int       r;

  if ( bind_restrict && port < IPPORT_RESERVEDSTART)
    port = 0;

  switch (ai->ai_family) {
  case AF_INET:
    ((struct sockaddr_in *)ai->ai_addr)->sin_port = htons(port);
    break;
  case AF_INET6:
    ((struct sockaddr_in6 *)ai->ai_addr)->sin6_port = htons(port);
    break;
  default:
    /* unsupported */
    return(-1);
  }

#ifdef IPV6_V6ONLY
  {
    int    on = 1;
    if (ai->ai_family == AF_INET6 &&
	setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY,
		   &on, sizeof(on)) < 0)
      return -1;
  }
#endif

  if (port > 0 && port < IPPORT_RESERVED)
    setreuid(PROCUID, 0);
  r = bind(s, ai->ai_addr, ai->ai_addrlen);
  setreuid(0, PROCUID);
  return(r);
}

/*
  wait_for_read:
          wait for readable status.
	  descriptor 's' must be in blocking i/o mode.
 */
int wait_for_read(int s, long sec)
{
  fd_set fds;
  int n, nfd;
  struct timeval tv;

  tv.tv_sec = sec;
  tv.tv_usec = 0;

  nfd = s;
  FD_ZERO(&fds); FD_SET(s, &fds);
  n = select(nfd+1, &fds, 0, 0, &tv);
  switch (n) {
  case -1:            /* error */
    return(-1);
  case 0:             /* timed out */
    return(0);
  default:            /* ok */
    if (FD_ISSET(s, &fds))
      return(s);
    else
      return(-1);
  }
}

ssize_t timerd_read(int s, u_char *buf, size_t len, int sec, int flags)
{
  ssize_t r = -1;
  settimer(sec);
  r = recvfrom(s, buf, len, flags, 0, 0);
  settimer(0);
  return(r);
}

ssize_t timerd_write(int s, u_char *buf, size_t len, int sec)
{
  ssize_t r = -1;
  settimer(sec);
  r = sendto(s, buf, len, 0, 0, 0);
  settimer(0);
  return(r);
}

int get_line(int s, char *buf, size_t len) {
  int     r = 0;
  char   *p = buf;
  int     ret;

  while ( len > 1 ) { /* guard the buf */
    ret = recvfrom(s, p, 1, 0, 0, 0);
    if (ret < 0) {
      if (errno == EINTR) {  /* not thread safe ?? */
	continue;
      }
      r = -1;
      break;
    } else if ( ret == 0 ) { /* EOF */
      len = 0; /* to exit loop */
    } else {
      len--;
      if (*p == 012) { /* New Line */
	len = 0; /* to exit loop */
      }
      r++; p++;
    }
  }
  *p = '\0';
  return(r);

}

int lookup_tbl(struct socks_req *req)
{
  int    i, match, error;
  struct addrinfo hints, *res, *res0;
  char   name[NI_MAXHOST];
  struct bin_addr addr;
  struct sockaddr_in  *sa;
  struct sockaddr_in6 *sa6;

  match = 0;
  for (i=0; i < proxy_tbl_ind; i++) {
    /* check atype */
    if ( req->dest.atype != proxy_tbl[i].dest.atype )
      continue;
    /* check destination port */
    if ( req->port < proxy_tbl[i].port_l
	 || req->port > proxy_tbl[i].port_h)
      continue;

    if (addr_comp(&(req->dest), &(proxy_tbl[i].dest),
		  proxy_tbl[i].mask) == 0) {
      match++;
      break;
    }
  }

  if ( !match && req->dest.atype == S5ATFQDN ) {
    /* fqdn 2nd stage: try resolve and lookup */

    strncpy(name, (char *)req->dest.fqdn, req->dest.len_fqdn);
    name[req->dest.len_fqdn] = '\0';
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    error = getaddrinfo(name, NULL, &hints, &res0);

    if ( !error ) {
      for (res = res0; res; res = res->ai_next) {
	for (i = 0; i < proxy_tbl_ind; i++) {
	  /* check destination port */
	  if ( req->port < proxy_tbl[i].port_l
	       || req->port > proxy_tbl[i].port_h)
	    continue;

	  memset(&addr, 0, sizeof(addr));
	  switch (res->ai_family) {
	  case AF_INET:
	    addr.atype = S5ATIPV4;
	    sa = (struct sockaddr_in *)res->ai_addr;
	    memcpy(addr.v4_addr,
		   &sa->sin_addr, sizeof(struct in_addr));
	    break;
	  case AF_INET6:
	    addr.atype = S5ATIPV6;
	    sa6 = (struct sockaddr_in6 *)res->ai_addr;
	    memcpy(addr.v6_addr,
		   &sa6->sin6_addr, sizeof(struct in6_addr));
	    addr.v6_scope = sa6->sin6_scope_id;
	    break;
	  default:
	    addr.atype = -1;
	    break;
	  }
	  if ( addr.atype != proxy_tbl[i].dest.atype )
	    continue;
	  if (addr_comp(&addr, &(proxy_tbl[i].dest),
			proxy_tbl[i].mask) == 0)
	    match++;
	  break;
	}
	if ( match )
	  break;
      }
      freeaddrinfo(res0);
    }
  }

  memset(&(req->rtbl), 0, sizeof(struct rtbl));

  if (match) {
    memcpy(&(req->rtbl), &(proxy_tbl[i]), sizeof(struct rtbl));
    return(i);
  } else
    return(proxy_tbl_ind);
}

/*
  resolv_host:
       convert binary addr into string replesentation host_info
 */
int resolv_host(struct bin_addr *addr, u_int16_t port, struct host_info *info)
{
  struct  sockaddr_storage ss;
  struct  sockaddr_in  *sa;
  struct  sockaddr_in6 *sa6;
  int     error = 0;
  int     len;

  len = sizeof(ss);
  memset(&ss, 0, len);
  switch (addr->atype) {
  case S5ATIPV4:
    len = sizeof(struct sockaddr_in);
    sa = (struct sockaddr_in *)&ss;
#ifdef HAVE_SOCKADDR_SA_LEN
    sa->sin_len = len;
#endif
    sa->sin_family = AF_INET;
    memcpy(&(sa->sin_addr), addr->v4_addr, sizeof(struct in_addr));
    sa->sin_port = htons(port);
    break;
  case S5ATIPV6:
    len = sizeof(struct sockaddr_in6);
    sa6 = (struct sockaddr_in6 *)&ss;
#ifdef HAVE_SOCKADDR_SA_LEN
    sa6->sin6_len = len;
#endif
    sa6->sin6_family = AF_INET6;
    memcpy(&(sa6->sin6_addr), addr->v6_addr, sizeof(struct in6_addr));
    sa6->sin6_scope_id = addr->v6_scope;
    sa6->sin6_port = htons(port);
    break;
  case S5ATFQDN:
    len = sizeof(struct sockaddr_in);
    sa = (struct sockaddr_in *)&ss;
#ifdef HAVE_SOCKADDR_SA_LEN
    sa->sin_len = len;
#endif
    sa->sin_family = AF_INET;
    sa->sin_port = htons(port);
    break;
  default:
    break;
  }
  if (addr->atype == S5ATIPV4 || addr->atype == S5ATIPV6) {
    error = getnameinfo((struct sockaddr *)&ss, len,
			info->host, sizeof(info->host),
			info->port, sizeof(info->port),
			NI_NUMERICHOST | NI_NUMERICSERV);
  } else if (addr->atype == S5ATFQDN) {
    error = getnameinfo((struct sockaddr *)&ss, len,
			NULL, 0,
			info->port, sizeof(info->port),
			NI_NUMERICSERV);
    strncpy(info->host, (char *)addr->fqdn, addr->len_fqdn);
    info->host[addr->len_fqdn] = '\0';
  } else {
    strcpy(info->host, "?");
    strcpy(info->port, "?");
    error++;
  }
  return(error);
}

/*
  log_request:
*/
int log_request(struct socks_req *req)
{
  struct  sockaddr_storage ss;
  struct  host_info client, dest, proxy;
  int     error = 0;
  char    user[256];
  char    *ats[] =  {"ipv4", "fqdn", "ipv6", "?"};
  char    *reqs[] = {"CON", "BND", "UDP", "?"};
  int     atmap[] = {3, 0, 3, 1, 2};
  int     reqmap[] = {3, 0, 1, 2};
  int     len;
  int     direct = 0;

  if (req->rtbl.rl_meth == DIRECT) {
    /* proxy_XX is N/A */
    strcpy(proxy.host, "-");
    strcpy(proxy.port, "-");
    direct = 1;
  } else {
    error += resolv_host(&req->rtbl.prx[0].proxy,
			 req->rtbl.prx[0].pport,
			 &proxy);
  }
  error += resolv_host(&req->dest, req->port, &dest);

  len = sizeof(ss);
  if (getpeername(req->s, (struct sockaddr *)&ss, (socklen_t *)&len) != 0) {
    strcpy(client.host, "?");
    strcpy(client.port, "?");
    error++;
  } else {
    error += getnameinfo((struct sockaddr *)&ss, len,
			client.host, sizeof(client.host),
			client.port, sizeof(client.port),
			NI_NUMERICHOST | NI_NUMERICSERV);
  }

  strncpy(user, req->user, req->u_len);
  user[req->u_len] = '\0';

  msg_out(norm, "%s:%s %d-%s %s:%s(%s) %s %s%s:%s.",
		client.host, client.port,
		req->ver, reqs[reqmap[req->req]],
	        dest.host, dest.port,
		ats[atmap[req->dest.atype]],
	        user,
		direct ? "direct" : "relay=",
	        proxy.host, proxy.port );
  return(error);
}
