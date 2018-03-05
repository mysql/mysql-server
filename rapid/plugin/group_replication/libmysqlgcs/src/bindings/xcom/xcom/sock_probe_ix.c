/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <sys/ioctl.h>
#include <net/if.h>
#ifndef __linux__
#include <sys/sockio.h>
#endif
#include <netdb.h>

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#define BSD_COMP

#include "simset.h"
#include "xcom_common.h"
#include "xcom_vp.h"
#include "sock_probe.h"
#include "task_os.h"
#include "task_net.h"
#include "task.h"
#include "task_debug.h"
#include "xcom_transport.h"
#include "xcom_base.h"
#include "xcom_memory.h"
#include "node_no.h"
#include "x_platform.h"

/* Buffer size should be always a multiple of 'struct ifreq' size */
#define IF_INIT_BUF_SIZE ((int)sizeof(struct ifreq) * 10)
#define IFRP_INIT_ARR_SIZE 64

struct sock_probe
{
  int tmp_socket;
  struct ifconf ifc;     /* interfaces configuration */
  struct ifreq **ifrp;   /* index over ifc's buffer */
  char *ifbuf;           /* buffer for ifc */
  int nbr_ifs;           /* number of valid pointers in ifrp */

};

static int number_of_interfaces(sock_probe *s);
static sockaddr get_sockaddr(sock_probe *s, int count);
static bool_t is_if_running(sock_probe *s, int count);

static void reset_sock_probe(struct sock_probe *s)
{
  s->tmp_socket = INVALID_SOCKET;
  memset(&s->ifc,  0, sizeof(s->ifc));
  s->ifc.ifc_len=0;
  s->ifrp= NULL;
  s->ifbuf = NULL;
  s->nbr_ifs= 0;
}

/* Initialize socket probe */
static int init_sock_probe(sock_probe *s)
{
  int i= 0, ifrpsize= 0, bufsize= 0;
  bool_t abrt= FALSE;
  char* ptr= NULL, *end= NULL;
  struct ifreq* ifrecc= NULL;
#if TASK_DBUG_ON
  char* if_name  MY_ATTRIBUTE((unused))= NULL;
#endif
#if defined(SA_LEN) || defined(HAVE_STRUCT_SOCKADDR_SA_LEN)
    struct sockaddr *sa MY_ATTRIBUTE((unused))= NULL;
#endif

  /* reset the fields of the structure */
  reset_sock_probe(s);

  /*
   ioctl may overflow without returning an error. Thence we iterate to
   make sure that we don't fill up the buffer. Then, when finally ifc_len
   is smaller than the buffer size, we break the loop.
  */
  do
  {
    bufsize+= IF_INIT_BUF_SIZE;
    if (!(s->ifbuf= (char*)realloc(s->ifbuf, (size_t)bufsize)))
    {
      abrt= TRUE;
      /* Out of memory. */
      goto err;
    }
    memset(&s->ifc,  0, sizeof(s->ifc));
    memset(s->ifbuf, 0, (size_t)bufsize);

    if ((s->tmp_socket = xcom_checked_socket(AF_INET, SOCK_DGRAM, 0).val) == INVALID_SOCKET)
      goto err;

    s->ifc.ifc_len= bufsize;
    s->ifc.ifc_buf= s->ifbuf;
    /* Get information about IP interfaces on this machine.*/
    if (ioctl(s->tmp_socket, SIOCGIFCONF, (char *)&s->ifc)< 0)
    {
      DBGOUT(NUMEXP(errno); STREXP(strerror(errno)););
      abrt= TRUE;
      goto err;
    }
  } while (s->ifc.ifc_len >= bufsize);

  DBGOUT(STRLIT("Registering interfaces:"));

  /*
   Now, lets build an index over the buffer. And calculate the number of
   interfaces. We are doing this, since the size of sockaddr differs on
   some platforms.
   */
  for (i= 0, ptr= s->ifc.ifc_buf, end= s->ifc.ifc_buf + s->ifc.ifc_len;
       ptr<end;
       i++)
  {
    /*
     We are just starting or have filled up all pre-allocated entries.
     Need to allocate some more.
     */
    if (i==ifrpsize || i==0)
    {
		ifrpsize+= IFRP_INIT_ARR_SIZE * (int)sizeof(struct ifreq *);
      /* allocate one more block */
		if (!(s->ifrp= (struct ifreq **) realloc(s->ifrp, (size_t)ifrpsize)))
      {
        abrt= TRUE;
        /* Out of memory. */
        goto err;
      }
    }

    ifrecc= (struct ifreq*) ptr;
    s->ifrp[i]= ifrecc;
#if defined(SA_LEN) || defined(HAVE_STRUCT_SOCKADDR_SA_LEN)
    sa= &ifrecc->ifr_addr;
#endif

#if defined(SA_LEN)
    ptr+= IFNAMSIZ + SA_LEN(sa);
#elif defined(HAVE_STRUCT_SOCKADDR_SA_LEN)
    ptr+= IFNAMSIZ + sa->sa_len;
#else
    ptr+= sizeof(struct ifreq);
#endif

#if defined(TASK_DBUG_ON) && TASK_DBUG_ON
#ifdef HAVE_STRUCT_IFREQ_IFR_NAME
    if_name= ifrecc->ifr_name;
#else
    if_name= ifrecc->ifr_ifrn.ifrn_name;
#endif /* HAVE_STRUCT_IFREQ_IFR_NAME */
#if defined(SA_LEN) || defined(HAVE_STRUCT_SOCKADDR_SA_LEN)
    DBGOUT(NPUT(if_name, s);
           STRLIT("(sa_family="); NPUT(sa->sa_family, d); STRLIT(")"));
#endif
#endif

  }

  s->nbr_ifs= i;
  return 0;
err:
  free(s->ifbuf);
  free(s->ifrp);
  /* reset the values of struct sock_probe */
  reset_sock_probe(s);
  if (abrt)
    abort();
  return -1;
}

/* Close socket of sock_probe */
static void close_sock_probe(sock_probe *s)
{
  if(s->tmp_socket != INVALID_SOCKET){
    CLOSESOCKET(s->tmp_socket);
    s->tmp_socket = INVALID_SOCKET;
  }
}

/* Close any open socket and free sock_probe */
static void delete_sock_probe(sock_probe *s)
{
  close_sock_probe(s);
  X_FREE(s->ifbuf);
  X_FREE(s->ifrp);
  X_FREE(s);
}

/* Return the number of IP interfaces on this machine.*/
static int number_of_interfaces(sock_probe *s)
{
  return s->nbr_ifs; /* Number of interfaces */
}

static bool_t is_if_running(sock_probe *s, int count)
{
  struct ifreq *ifrecc;
  idx_check_ret(count, number_of_interfaces(s), 0) ifrecc = s->ifrp[count];
  assert(s->tmp_socket!=INVALID_SOCKET);
  return (ioctl(s->tmp_socket, SIOCGIFFLAGS, (char *)ifrecc) >= 0) &&
    (ifrecc->ifr_flags & IFF_UP) &&
    (ifrecc->ifr_flags & IFF_RUNNING);
}

/* Return the sockaddr of interface #count. */
static sockaddr get_sockaddr(sock_probe *s, int count)
{
  /* s->ifrp[count].ifr_addr if of type sockaddr */
  idx_check_fail(count, number_of_interfaces(s)) return s->ifrp[count]->ifr_addr;
}

