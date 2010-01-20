/*
  auth-pwd.c
  $Id: auth-pwd.c,v 1.10 2009/12/17 04:12:17 bulkstream Exp $

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
#if defined(FREEBSD) || defined(LINUX) || defined(MACOSX)
#include <pwd.h>
#elif  SOLARIS
#include <shadow.h>
#include <crypt.h>
#endif

#define TIMEOUTSEC    30

/* proto types */
int checkpasswd(char *, char *);

int auth_pwd_server(int s)
{
  u_char buf[512];
  int  r, len;
  char user[256];
  char pass[256];
  struct sockaddr_storage client;
  char client_ip[NI_MAXHOST];
  int  error = 0;
  int  code = 0;

  r = timerd_read(s, buf, sizeof(buf), TIMEOUTSEC, MSG_PEEK);
  if ( r < 2 ) {
    return(-1);
  }
  if (buf[0] != 0x01) { /* current username/password auth version */
    /* error in version */
    return(-1);
  }
  len = buf[1];
  if (len < 1 || len > 255) {
    /* invalid username len */
    return(-1);
  }
  /* read username */
  r = timerd_read(s, buf, 2+len, TIMEOUTSEC, 0);
  if (r < 2+len) {
    /* read error */
    return(-1);
  }
  strncpy(user, (char *)&buf[2], len);
  user[len] = '\0';

  /* get passwd */
  r = timerd_read(s, buf, sizeof(buf), TIMEOUTSEC, MSG_PEEK);
  if ( r < 1 ) {
    return(-1);
  }
  len = buf[0];
  if (len < 1 || len > 255) {
    /* invalid password len */
    return(-1);
  }
  /* read passwd */
  r = timerd_read(s, buf, 1+len, TIMEOUTSEC, 0);
  if (r < 1+len) {
    /* read error */
    return(-1);
  }
  strncpy(pass, (char *)&buf[1], len);
  pass[len] = '\0';

  /* do authentication */
  r = checkpasswd(user, pass);

  /* logging */
  len = sizeof(struct sockaddr_storage);
  if (getpeername(s, (struct sockaddr *)&client, (socklen_t *)&len) != 0) {
    client_ip[0] = '\0';
  } else {
    error = getnameinfo((struct sockaddr *)&client, len,
			client_ip, sizeof(client_ip),
			NULL, 0, NI_NUMERICHOST);
    if (error) {
      client_ip[0] = '\0';
    }
  }
  msg_out(norm, "%s 5-U/P_AUTH %s %s.", client_ip,
	  user, r == 0 ? "accepted" : "denied");

  /* erace uname and passwd storage */
  memset(user, 0, sizeof(user));
  memset(pass, 0, sizeof(pass));

  code = ( r == 0 ? 0 : -1 );

  /* reply to client */
  buf[0] = 0x01;  /* sub negotiation version */
  buf[1] = code & 0xff;  /* grant or not */
  r = timerd_write(s, buf, 2, TIMEOUTSEC);
  if (r < 2) {
    /* write error */
    return(-1);
  }
  return(code);   /* access granted or not */
}

int auth_pwd_client(int s, struct socks_req *req)
{
  u_char buf[640];
  int  r, ret, done;
  FILE *fp = NULL;
  struct user_pass up;

  ret = -1; done = 0;
  /* get username/password */
  if (pwdfile != NULL) {
    setreuid(PROCUID, 0);
    fp = fopen(pwdfile, "r");
    setreuid(0, PROCUID);
  }

  if ( fp != NULL ) {
    r = readpasswd(fp, req, &up);
    fclose(fp);
    if ( r == 0 ) { /* readpasswd gets match */
      if ( up.ulen >= 1 && up.ulen <= 255
	   && up.plen >= 1 && up.plen <= 255 ) {
	/* build auth data */
	buf[0] = 0x01;
	buf[1] = up.ulen & 0xff;
	memcpy(&buf[2], up.user, up.ulen);
	buf[2+up.ulen] = up.plen & 0xff;
	memcpy(&buf[2+up.ulen+1], up.pass, up.plen);
	done++;
      }
    }
  }
  if (! done) {
    /* build fake auth data */
    /* little bit BAD idea */
    buf[0] = 0x01;
    buf[1] = 0x01;
    buf[2] = ' ';
    buf[3] = 0x01;
    buf[4] = ' ';
    up.ulen = up.plen = 1;
  }

  r = timerd_write(s, buf, 3+up.ulen+up.plen, TIMEOUTSEC);
  if (r < 3+up.ulen+up.plen) {
    /* cannot write */
    goto err_ret;
  }

  /* get server reply */
  r = timerd_read(s, buf, 2, TIMEOUTSEC, 0);
  if (r < 2) {
    /* cannot read */
    goto err_ret;
  }
  if (buf[0] == 0x01 && buf[1] == 0) {
    /* username/passwd auth succeded */
    ret = 0;
  }
 err_ret:

  /* erace uname and passwd storage */
  memset(&up, 0, sizeof(struct user_pass));
  return(ret);
}

int checkpasswd(char *user, char *pass)
{
#if defined(FREEBSD) || defined(LINUX) || defined(MACOSX)
  struct passwd *pwd;
#elif SOLARIS
  struct spwd *spwd, sp;
  char   buf[512];
#endif
  int matched = 0;

  if (user == NULL) {
    /* user must be specified */
    return(-1);
  }

#if defined(FREEBSD) || defined(LINUX) || defined(MACOSX)
  setreuid(PROCUID, 0);
  pwd = getpwnam(user);
  setreuid(0, PROCUID);
  if (pwd == NULL) {
    /* error in getpwnam */
    return(-1);
  }
  if (pwd->pw_passwd == NULL && pass == NULL) {
    /* null password matched */
    return(0);
  }
  if (*pwd->pw_passwd) {
    if (strcmp(pwd->pw_passwd, crypt(pass, pwd->pw_passwd)) == 0) {
      matched = 1;
    }
  }
  memset(pwd->pw_passwd, 0, strlen(pwd->pw_passwd));

#elif SOLARIS
  setreuid(PROCUID, 0);
  spwd = getspnam_r(user, &sp, buf, sizeof buf);
  setreuid(0, PROCUID);
  if (spwd == NULL) {
    /* error in getspnam */
    return(-1);
  }
  if (spwd->sp_pwdp == NULL && pass == NULL) {
    /* null password matched */
    return(0);
  }
  if (*spwd->sp_pwdp) {
    if (strcmp(spwd->sp_pwdp, crypt(pass, spwd->sp_pwdp)) == 0) {
      matched = 1;
    }
  }
  memset(spwd->sp_pwdp, 0, strlen(spwd->sp_pwdp));
#endif

#if defined(FREEBSD) || defined(SOLARIS)
  if (matched) {
    return(0);
  } else {
    return(-1);
  }
#endif
  return(0);
}
