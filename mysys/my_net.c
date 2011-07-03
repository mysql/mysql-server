/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* thread safe version of some common functions */

#include "mysys_priv.h"
#include <m_string.h>

/* for thread safe my_inet_ntoa */
#if !defined(__WIN__)
#include <netdb.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#endif /* !defined(__WIN__) */
#include "my_net.h"


void my_inet_ntoa(struct in_addr in, char *buf)
{
  char *ptr;
  pthread_mutex_lock(&THR_LOCK_net);
  ptr=inet_ntoa(in);
  strmov(buf,ptr);
  pthread_mutex_unlock(&THR_LOCK_net);
}

/* This code is not needed if my_gethostbyname_r is a macro */
#if !defined(my_gethostbyname_r)

/*
  Emulate SOLARIS style calls, not because it's better, but just to make the
  usage of getbostbyname_r simpler.
*/

#if defined(HAVE_GETHOSTBYNAME_R)

#if defined(HAVE_GETHOSTBYNAME_R_GLIBC2_STYLE)

struct hostent *my_gethostbyname_r(const char *name,
				   struct hostent *result, char *buffer,
				   int buflen, int *h_errnop)
{
  struct hostent *hp;
  DBUG_ASSERT((size_t) buflen >= sizeof(*result));
  if (gethostbyname_r(name,result, buffer, (size_t) buflen, &hp, h_errnop))
    return 0;
  return hp;
}

#elif defined(HAVE_GETHOSTBYNAME_R_RETURN_INT)

struct hostent *my_gethostbyname_r(const char *name,
				   struct hostent *result, char *buffer,
				   int buflen, int *h_errnop)
{
  if (gethostbyname_r(name,result,(struct hostent_data *) buffer) == -1)
  {
    *h_errnop= errno;
    return 0;
  }
  return result;
}

#else

/* gethostbyname_r with similar interface as gethostbyname() */

struct hostent *my_gethostbyname_r(const char *name,
				   struct hostent *result, char *buffer,
				   int buflen, int *h_errnop)
{
  struct hostent *hp;
  DBUG_ASSERT(buflen >= sizeof(struct hostent_data));
  hp= gethostbyname_r(name,result,(struct hostent_data *) buffer);
  *h_errnop= errno;
  return hp;
}
#endif /* GLIBC2_STYLE_GETHOSTBYNAME_R */

#else /* !HAVE_GETHOSTBYNAME_R */

#ifdef THREAD
extern pthread_mutex_t LOCK_gethostbyname_r;
#endif

/*
  No gethostbyname_r() function exists.
  In this case we have to keep a mutex over the call to ensure that no
  other thread is going to reuse the internal memory.

  The user is responsible to call my_gethostbyname_r_free() when he
  is finished with the structure.
*/

struct hostent *
my_gethostbyname_r(const char *name,
                   struct hostent *result __attribute__((unused)), 
                   char *buffer __attribute__((unused)),
                   int buflen __attribute__((unused)), 
                   int *h_errnop)
{
  struct hostent *hp;
  pthread_mutex_lock(&LOCK_gethostbyname_r);
  hp= gethostbyname(name);
  *h_errnop= h_errno;
  return hp;
}

void my_gethostbyname_r_free()
{
  pthread_mutex_unlock(&LOCK_gethostbyname_r);  
}

#endif /* !HAVE_GETHOSTBYNAME_R */
#endif /* !my_gethostbyname_r */
