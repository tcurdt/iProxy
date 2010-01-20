/*
  get-bind.c:
  $Id: get-bind.c,v 1.8 2009/12/09 04:07:53 bulkstream Exp $

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

#if defined(FREEBSD) || defined(SOLARIS) || defined(MACOSX) 
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <net/if.h>
#include "route.h"
#ifdef	HAVE_SOCKADDR_DL_STRUCT
# include <net/if_dl.h>
#endif

#endif /* defined(FREEBSD) || defined(SOLARIS) || defined(MACOSX) */

#if defined(LINUX)
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

static int get_ifconf(int, struct addrinfo *);
#endif /* defined(LINUX) */

#if defined(FREEBSD) || defined(SOLARIS) || defined(MACOSX) 

#ifndef RTAX_DST
#define RTAX_DST         0
#define RTAX_GATEWAY     1
#define RTAX_NETMASK     2
#define RTAX_GENMASK     3
#define RTAX_IFP         4
#define RTAX_IFA         5
#define RTAX_AUTHOR      6
#define RTAX_BRD         7
#define RTA_NUMBITS 8
#define RTAX_MAX        RTA_NUMBITS /* Number of bits used in RTA_* */
#endif

struct if_list {
  char if_name[IFNAMSIZ];
  struct in_addr if_addr;
};

/*
 * Round up 'a' to next multiple of 'size'
 */
#define ROUNDUP(a, size) (((a) & ((size)-1)) ? (1 + ((a) | ((size)-1))) : (a))

/*
 * Step to next socket address structure;
 * if sa_len is 0, assume it is sizeof(u_long).
 */
#ifdef HAVE_SOCKADDR_SA_LEN
#define NEXT_SA(ap)	ap = (struct sockaddr *) \
	((caddr_t) ap + (ap->sa_len ? ROUNDUP(ap->sa_len, sizeof (u_long)) : \
				sizeof(u_long)))
#endif

void
get_rtaddrs(int addrs, struct sockaddr *sa, struct sockaddr **rti_info)
{
  int	i;
#ifndef HAVE_SOCKADDR_SA_LEN
  char  *p = (char *)sa;
  size_t len;
#endif

  for (i = 0; i < RTAX_MAX; i++) {
    if (addrs & (1 << i)) {
      rti_info[i] = sa;
#ifdef HAVE_SOCKADDR_SA_LEN
      NEXT_SA(sa);
#else
      switch(sa->sa_family) {
      case AF_INET:
	len = sizeof(struct sockaddr_in);
	break;
      case AF_LINK:
	len = sizeof(struct sockaddr_dl);
	break;
      default:
	len = 0;
      }
      sa = (struct sockaddr *) (p + len);
      p = (char *)sa;
#endif
    } else
      rti_info[i] = NULL;
  }
}

#define	SEQ		9999	/* packet sequence dummy */
#define MAXNUM_IF	256	/* max number of interfaces */

int get_bind_addr(struct socks_req *req, struct addrinfo *ba)
{

  /* list interface name/address
   *   fixed size buffer limits number of recognizable interfaces.
   *   buf size = sizeof(struct ifreq) * 256 interface = 8192
   */
  int			i, ent, sockfd, len;
  char			*ptr, buf[sizeof(struct ifreq) * MAXNUM_IF];
  struct ifconf		ifc;
  struct ifreq		*ifr;
  struct sockaddr_in	*sinptr;
  struct if_list        ifl[MAXNUM_IF];

  pid_t			pid;
  ssize_t		n;
  struct rt_msghdr	*rtm;
  struct sockaddr	*sa, *rti_info[RTAX_MAX];
  struct sockaddr_in	*sin;
  struct sockaddr_dl    *sdl;
  struct in_addr        ia;

  struct addrinfo hints, *res, *res0;
  int    error;
  char   host[256];
  int found = 0;


  /* IPv6 routing is not implemented yet */
  switch (req->dest.atype) {
  case S5ATIPV4:
    memcpy(&ia, &(req->dest.v4_addr), 4);
    break;
  case S5ATIPV6:
    return -1;
  case S5ATFQDN:
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_INET;
    memcpy(host, &req->dest.fqdn, req->dest.len_fqdn);
    host[req->dest.len_fqdn] = '\0';
    error = getaddrinfo(host, NULL, &hints, &res0);
    if (error) {
      return -1;
    }
    for (res = res0; res; res = res->ai_next) {
      if (res->ai_family != AF_INET)
	continue;
      sin = (struct sockaddr_in *)res->ai_addr;
      memcpy(&ia, &(sin->sin_addr), sizeof(ia));
      found++; break;
    }
    freeaddrinfo(res0);
    if (!found)
      return -1;
    break;
  default:
    return -1;
  }

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  ifc.ifc_len = sizeof(buf);
  ifc.ifc_req = (struct ifreq *) buf;
  ioctl(sockfd, SIOCGIFCONF, &ifc);

  close(sockfd);

  i = ent = 0;
  for (ptr = buf; ptr < buf + ifc.ifc_len; ) {
    ifr = (struct ifreq *) ptr;
    len = sizeof(struct sockaddr);
#ifdef	HAVE_SOCKADDR_SA_LEN
    if (ifr->ifr_addr.sa_len > len)
      len = ifr->ifr_addr.sa_len;		/* length > 16 */
#endif
    ptr += sizeof(ifr->ifr_name) + len;	/* for next one in buffer */

    switch (ifr->ifr_addr.sa_family) {
    case AF_INET:
      strncpy(ifl[i].if_name, ifr->ifr_name, IFNAMSIZ);
      sinptr = (struct sockaddr_in *) &ifr->ifr_addr;
      memcpy(&ifl[i].if_addr, &sinptr->sin_addr, sizeof(struct in_addr));
      i++;
      break;

    default:
      break;
    }
  }
  ent = i; /* number of interfaces */

  /* get routing */
  setreuid(PROCUID, 0);
  sockfd = socket(AF_ROUTE, SOCK_RAW, 0);	/* need superuser privileges */
  setreuid(0, PROCUID);
  if (sockfd < 0) {
    /* socket error */
    return(-1);
  }

  memset(buf, 0, sizeof buf);

  rtm = (struct rt_msghdr *) buf;
  rtm->rtm_msglen = sizeof(struct rt_msghdr)
                  + sizeof(struct sockaddr_in)
                  + sizeof(struct sockaddr_dl);
  rtm->rtm_version = RTM_VERSION;
  rtm->rtm_type = RTM_GET;
  rtm->rtm_addrs = RTA_DST|RTA_IFP;
  rtm->rtm_pid = pid = getpid();
  rtm->rtm_seq = SEQ;

  sin = (struct sockaddr_in *) (buf + sizeof(struct rt_msghdr));
  sin->sin_family = AF_INET;
#ifdef HAVE_SOCKADDR_SA_LEN
  sin->sin_len = sizeof(struct sockaddr_in);
#endif

  memcpy(&(sin->sin_addr), &ia, sizeof(struct in_addr));

#ifdef HAVE_SOCKADDR_SA_LEN
  sa = (struct sockaddr *)sin;
  NEXT_SA(sa);
  sdl = (struct sockaddr_dl *)sa;
  sdl->sdl_len = sizeof(struct sockaddr_dl);
#else
  sdl = (struct sockaddr_dl *) (buf
		+ ROUNDUP(sizeof(struct rt_msghdr), sizeof(u_long))
		+ ROUNDUP(sizeof(struct sockaddr_in), sizeof(u_long)));
#endif
  sdl->sdl_family = AF_LINK;

  write(sockfd, rtm, rtm->rtm_msglen);

  do {
    n = read(sockfd, rtm, sizeof buf);
  } while (rtm->rtm_type != RTM_GET || rtm->rtm_seq != SEQ ||
	   rtm->rtm_pid != pid);

  close(sockfd);

  rtm = (struct rt_msghdr *) buf;
  sa = (struct sockaddr *) (rtm + 1);
  get_rtaddrs(rtm->rtm_addrs, sa, rti_info);

  ba->ai_family = AF_INET;          /* IPv4 */
  ba->ai_socktype = SOCK_STREAM;
  ba->ai_protocol = IPPROTO_TCP;
  sin = (struct sockaddr_in *)ba->ai_addr;
  sin->sin_family = AF_INET;
#ifdef HAVE_SOCKADDR_SA_LEN
  sin->sin_len = sizeof(struct sockaddr_in);
#endif
  ba->ai_addrlen = sizeof(struct sockaddr_in);

  if ( (sa = rti_info[RTAX_IFP]) != NULL) {
    sdl = (struct sockaddr_dl *)sa;
    if (sdl->sdl_nlen > 0) {
      for (i=0; i<ent; i++) {
	if (memcmp(ifl[i].if_name, sdl->sdl_data, sdl->sdl_nlen) == 0) {
	  memcpy(&sin->sin_addr, &ifl[i].if_addr, sizeof(struct in_addr));
	  return(0);
	}
      }
    }
  }
  return(-1);
}

#endif /* defined(FREEBSD) || defined(SOLARIS) || defined(MACOSX) */

#if defined(LINUX)

int get_bind_addr(struct socks_req *req, struct addrinfo *ba)
{
  int s;
  int len;
  struct rtattr *rta;
  struct {
    struct nlmsghdr         n;
    struct rtmsg            r;
    char                    buf[1024];
  } rreq;

  int status;
  unsigned seq;
  struct nlmsghdr *h;
  struct sockaddr_nl nladdr;
  struct iovec iov;
  char   buf[8192];
  struct msghdr msg = {
    (void*)&nladdr, sizeof(nladdr),
    &iov,   1,
    NULL,   0,
    0
  };
  pid_t pid;
  struct rtattr * tb[RTA_MAX+1];
  struct rtmsg *r;
  struct sockaddr_in *sin;
  struct in_addr      ia;

  struct addrinfo hints, *res, *res0;
  int    error;
  char   host[256];
  int found = 0;
  unsigned *d;

  /* IPv6 routing is not implemented yet */
  switch (req->dest.atype) {
  case S5ATIPV4:
    memcpy(&ia, &(req->dest.v4_addr), 4);
    break;
  case S5ATIPV6:
    return -1;
  case S5ATFQDN:
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_INET;
    memcpy(host, &req->dest.fqdn, req->dest.len_fqdn);
    host[req->dest.len_fqdn] = '\0';
    error = getaddrinfo(host, NULL, &hints, &res0);
    if (error) {
      return -1;
    }
    for (res = res0; res; res = res->ai_next) {
      if (res->ai_family != AF_INET)
	continue;
      sin = (struct sockaddr_in *)res->ai_addr;
      memcpy(&ia, &(sin->sin_addr), sizeof(ia));
      found++; break;
    }
    freeaddrinfo(res0);
    if (!found)
      return -1;
    break;
  default:
    return -1;
  }

  memset(&rreq, 0, sizeof(rreq));

  rreq.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
  rreq.n.nlmsg_flags = NLM_F_REQUEST;
  rreq.n.nlmsg_type = RTM_GETROUTE;
  rreq.r.rtm_family = AF_INET;

  len = RTA_LENGTH(4);
  if (NLMSG_ALIGN(rreq.n.nlmsg_len) + len > sizeof(rreq))
    return(-1);
  rta = (struct rtattr*)((char *)&rreq.n + NLMSG_ALIGN(rreq.n.nlmsg_len));
  rta->rta_type = RTA_DST;
  rta->rta_len = len;
  memcpy(RTA_DATA(rta), &ia, 4);
  rreq.n.nlmsg_len = NLMSG_ALIGN(rreq.n.nlmsg_len) + len;
  rreq.r.rtm_dst_len = 32;  /* 32 bit */

  s = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);

  memset(&nladdr, 0, sizeof(nladdr));
  nladdr.nl_family = AF_NETLINK;
  nladdr.nl_pid = 0;
  nladdr.nl_groups = 0;

  rreq.n.nlmsg_seq = seq = 9999;

  iov.iov_base = (void*)&rreq.n;
  iov.iov_len  = rreq.n.nlmsg_len;

  status = sendmsg(s, &msg, 0);

  if (status < 0) {
    /* perror("Cannot talk to rtnetlink"); */
    return -1;
  }

  pid = getpid();
  iov.iov_base = buf;
  do {
    iov.iov_len = sizeof(buf);
    status = recvmsg(s, &msg, 0);
    h = (struct nlmsghdr*)buf;
  } while (h->nlmsg_pid != pid || h->nlmsg_seq != seq);

  close(s);
  /*
  msg_out(norm,"nlmsg_pid: %d, nlmsg_seq: %d\n",
	  h->nlmsg_pid, h->nlmsg_seq);
  */
  len = h->nlmsg_len;
  r = NLMSG_DATA(buf);
  rta = RTM_RTA(r);
  while (RTA_OK(rta, len)) {
    if (rta->rta_type <= RTA_MAX)
      tb[rta->rta_type] = rta;
    rta = RTA_NEXT(rta,len);
  }
  /*
  if (tb[RTA_DST]) {
    inet_ntop(AF_INET, RTA_DATA(tb[RTA_DST]), str, sizeof(str));
    msg_out(norm, "DST %s\n", str);
  }
  if (tb[RTA_GATEWAY]) {
    inet_ntop(AF_INET, RTA_DATA(tb[RTA_GATEWAY]), str, sizeof(str));
    msg_out(norm, "GW %s\n", str);
  }
  */
  if (tb[RTA_OIF]) {
    d = RTA_DATA(tb[RTA_OIF]);
    return(get_ifconf(*d, ba));
  }
  return(-1);
}

int get_ifconf(int index, struct addrinfo *ba)
{
  int s;
  struct {
    struct nlmsghdr n;
    struct rtgenmsg g;
  } rreq;
  struct sockaddr_nl nladdr;
  char   buf[8192];
  struct iovec iov;
  struct msghdr msg = {
    (void*)&nladdr, sizeof(nladdr),
    &iov,   1,
    NULL,   0,
    0
  };
  pid_t pid;
  unsigned seq;
  int len;
  struct rtattr * rta;
  struct rtattr * tb[IFA_MAX+1];
  struct nlmsghdr *h;
  struct ifaddrmsg *ifa;
  int status;
  struct sockaddr_in *sin;

  memset(&nladdr, 0, sizeof(nladdr));
  nladdr.nl_family = AF_NETLINK;

  rreq.n.nlmsg_len = sizeof(rreq);
  rreq.n.nlmsg_type = RTM_GETADDR;
  rreq.n.nlmsg_flags = NLM_F_ROOT|NLM_F_MATCH|NLM_F_REQUEST;
  rreq.n.nlmsg_pid = 0;
  rreq.n.nlmsg_seq = seq = 9998;
  rreq.g.rtgen_family = AF_INET;

  s = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
  sendto(s, (void*)&rreq, sizeof(rreq), 0,
	 (struct sockaddr*)&nladdr, sizeof(nladdr));

  pid = getpid();
  iov.iov_base = buf;
  do {
    iov.iov_len = sizeof(buf);
    status = recvmsg(s, &msg, 0);
    h = (struct nlmsghdr*)buf;
  } while (h->nlmsg_pid != pid || h->nlmsg_seq != seq);

  close(s);
  /*
  msg_out(norm,"nlmsg_pid: %d, nlmsg_seq: %d\n",
	  h->nlmsg_pid, h->nlmsg_seq);
  */
  while (NLMSG_OK(h, status)) {
    memset(&tb, 0, sizeof(tb));
    len = h->nlmsg_len;
    ifa = NLMSG_DATA(h);
    rta = IFA_RTA(ifa);
    if (ifa->ifa_index == index) {
      while (RTA_OK(rta, len)) {
	if (rta->rta_type <= IFA_MAX)
	  tb[rta->rta_type] = rta;
	rta = RTA_NEXT(rta,len);
      }
      if (tb[IFA_ADDRESS]) {
	/*
	  char str[128];
	  inet_ntop(AF_INET, RTA_DATA(tb[IFA_ADDRESS]), str, sizeof(str));
	  msg_out(norm, "ADDRESS %s\n", str);
	*/
	ba->ai_family = AF_INET;         /* IPv4 */
	ba->ai_socktype = SOCK_STREAM;
	ba->ai_protocol = IPPROTO_IP;
	ba->ai_addrlen = sizeof(struct sockaddr_in);
	sin = (struct sockaddr_in *)ba->ai_addr;
	sin->sin_family = AF_INET;
	memcpy(&(sin->sin_addr), RTA_DATA(tb[IFA_ADDRESS]), 4);
	return(0);
      }
      /*
	if (tb[IFA_LOCAL]) {
	unsigned *d = RTA_DATA(tb[IFA_LOCAL]);
	msg_out(norm, "LOCAL %08x\n", *d);
	}
      */
    }
    h = NLMSG_NEXT(h, status);
  }
  return(-1);
}

#endif /* defined(LINUX) */
