/* Copyright (C) 2000 MySQL AB

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

void my_inet_ntoa(struct in_addr in, char *buf)
{
  char *ptr;
  pthread_mutex_lock(&THR_LOCK_net);
  ptr=inet_ntoa(in);
  strmov(buf,ptr);
  pthread_mutex_unlock(&THR_LOCK_net);
}
