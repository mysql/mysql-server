/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/* thread safe version of some common functions */

/* for thread safe my_inet_ntoa */
#ifdef	__cplusplus
extern "C" {
#endif /* __cplusplus */

#if !defined(MSDOS) && !defined(__WIN__) && !defined(__BEOS__)
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#endif /* !defined(MSDOS) && !defined(__WIN__) */

void my_inet_ntoa(struct in_addr in, char *buf);

/*
  Handling of gethostbyname_r()
*/

#if !defined(HPUX)
struct hostent;
#endif /* HPUX */
#if !defined(HAVE_GETHOSTBYNAME_R)
struct hostent *my_gethostbyname_r(const char *name,
				   struct hostent *result, char *buffer,
				   int buflen, int *h_errnop);
void my_gethostbyname_r_free();
#elif defined(HAVE_PTHREAD_ATTR_CREATE) || defined(_AIX) || defined(HAVE_GETHOSTBYNAME_R_GLIBC2_STYLE)
struct hostent *my_gethostbyname_r(const char *name,
				   struct hostent *result, char *buffer,
				   int buflen, int *h_errnop);
#define my_gethostbyname_r_free()
#if !defined(HAVE_GETHOSTBYNAME_R_GLIBC2_STYLE) && !defined(HPUX)
#define GETHOSTBYNAME_BUFF_SIZE sizeof(struct hostent_data)
#endif /* !defined(HAVE_GETHOSTBYNAME_R_GLIBC2_STYLE) */

#elif defined(HAVE_GETHOSTBYNAME_R_RETURN_INT)
#define GETHOSTBYNAME_BUFF_SIZE sizeof(struct hostent_data)
struct hostent *my_gethostbyname_r(const char *name,
				   struct hostent *result, char *buffer,
				   int buflen, int *h_errnop);
#define my_gethostbyname_r_free()
#else
#define my_gethostbyname_r(A,B,C,D,E) gethostbyname_r((A),(B),(C),(D),(E))
#define my_gethostbyname_r_free()
#endif /* !defined(HAVE_GETHOSTBYNAME_R) */

#ifndef GETHOSTBYNAME_BUFF_SIZE
#define GETHOSTBYNAME_BUFF_SIZE 2048
#endif

/* On SCO you get a link error when refering to h_errno */
#ifdef SCO
#undef h_errno
#define h_errno errno
#endif

#ifdef	__cplusplus
}
#endif
