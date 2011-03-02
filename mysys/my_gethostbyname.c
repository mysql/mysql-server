/* Copyright (C) 2002, 2004 MySQL AB, 2008-2009 Sun Microsystems, Inc
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; version 2
   of the License.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/* Thread safe version of gethostbyname_r() */

#include "mysys_priv.h"
#if !defined(__WIN__)
#include <netdb.h>
#endif
#include <my_net.h>

/* This file is not needed if my_gethostbyname_r is a macro */
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

extern mysql_mutex_t LOCK_gethostbyname_r;

/*
  No gethostbyname_r() function exists.
  In this case we have to keep a mutex over the call to ensure that no
  other thread is going to reuse the internal memory.

  The user is responsible to call my_gethostbyname_r_free() when he
  is finished with the structure.
*/

struct hostent *my_gethostbyname_r(const char *name,
                                   struct hostent *res __attribute__((unused)),
                                   char *buffer __attribute__((unused)),
                                   int buflen __attribute__((unused)),
                                   int *h_errnop)
{
  struct hostent *hp;
  mysql_mutex_lock(&LOCK_gethostbyname_r);
  hp= gethostbyname(name);
  *h_errnop= h_errno;
  return hp;
}

void my_gethostbyname_r_free()
{
  mysql_mutex_unlock(&LOCK_gethostbyname_r);
}

#endif /* !HAVE_GETHOSTBYNAME_R */
#endif /* !my_gethostbyname_r */
