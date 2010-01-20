/*
  v6defs.h:
  $Id: v6defs.h,v 1.3 2009/12/09 04:07:53 bulkstream Exp $
         IPv6 related definisions mainly for old Solaris.

Copyright (C) 2003-2009 Tomo.M (author).
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

#ifndef AF_INET6
#define AF_INET6        24
#endif

#ifndef PF_INET6
#define PF_INET6        AF_INET6
#endif
struct in6_addr {
  u_int8_t        s6_addr[16];
};
struct sockaddr_in6 {
#ifdef  HAVE_SOCKADDR_SA_LEN
  u_int8_t        sin6_len;       /* length of this struct */
  u_int8_t        sin6_family;    /* AF_INET6 */
#else
  u_int16_t       sin6_family;    /* AF_INET6 */
#endif
  u_int16_t       sin6_port;      /* transport layer port # */
  u_int32_t       sin6_flowinfo;  /* IPv6 flow information */
  struct in6_addr sin6_addr;      /* IPv6 address */
  u_int32_t       sin6_scope_id;  /* set of interfaces for a scope */
};
#ifndef IN6ADDR_ANY_INIT
#define IN6ADDR_ANY_INIT        {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}}
#endif
#if defined(NEED_SOCKADDR_STORAGE) || !defined(HAS_INET6_STRUCTS)
#define __SS_MAXSIZE 128
#define __SS_ALLIGSIZE (sizeof (long))

struct sockaddr_storage {
#ifdef  HAVE_SOCKADDR_SA_LEN
  u_int8_t        ss_len;       /* address length */
  u_int8_t        ss_family;    /* address family */
  char            __ss_pad1[__SS_ALLIGSIZE - 2 * sizeof(u_int8_t)];
  long            __ss_align;
  char            __ss_pad2[__SS_MAXSIZE - 2 * __SS_ALLIGSIZE];
#else
  u_int16_t       ss_family;    /* address family */
  char            __ss_pad1[__SS_ALLIGSIZE - sizeof(u_int16_t)];
  long            __ss_align;
  char            __ss_pad2[__SS_MAXSIZE - 2 * __SS_ALLIGSIZE];
#endif
};
#endif
