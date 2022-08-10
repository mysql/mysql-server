/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <net/if.h>
#include <sys/ioctl.h>
#ifndef __linux__
#include <sys/sockio.h>
#endif
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>

#define BSD_COMP

#include "xcom/simset.h"
#include "xcom/sock_probe.h"
#include "xcom/task.h"
#include "xcom/task_debug.h"
#include "xcom/task_net.h"
#include "xcom/task_os.h"
#include "xcom/x_platform.h"
#include "xcom/xcom_base.h"
#include "xcom/xcom_common.h"
#include "xcom/xcom_memory.h"
#include "xcom/xcom_profile.h"
#include "xcom/xcom_transport.h"
#include "xdr_gen/xcom_vp.h"

struct sock_probe {
  int nbr_ifs; /* number of valid pointers in ifrp */
  struct ifaddrs *interfaces;
};

static int number_of_interfaces(sock_probe *s);

static void get_sockaddr_address(sock_probe *s, int count,
                                 struct sockaddr **out);

static void get_sockaddr_netmask(sock_probe *s, int count,
                                 struct sockaddr **out);

typedef enum SockaddrOp {
  kSockaddrOpAddress = 0,
  kSockaddrOpNetmask
} SockaddrOp;

static void get_sockaddr(sock_probe *s, int count, struct sockaddr **out,
                         SockaddrOp addr_operation);
static bool_t is_if_running(sock_probe *s, int count);
static char *get_if_name(sock_probe *s, int count);

/* Initialize socket probe */
static int init_sock_probe(sock_probe *s) {
  struct ifaddrs *ifa_tmp;

  if (s == nullptr) {
    goto err;
  }

  s->interfaces = nullptr;
  if (getifaddrs(&s->interfaces) == -1) {
    goto err;
  }

  ifa_tmp = s->interfaces;

  while (ifa_tmp) {
    if ((ifa_tmp->ifa_addr) && ((ifa_tmp->ifa_addr->sa_family == AF_INET) ||
                                (ifa_tmp->ifa_addr->sa_family == AF_INET6))) {
      s->nbr_ifs++;
    }
    ifa_tmp = ifa_tmp->ifa_next;
  }

  return 0;
err:
  return -1;
}

/* Close socket of sock_probe */
static void close_sock_probe(sock_probe *s) {
  if (s->interfaces) freeifaddrs(s->interfaces);
  free(s);
}

/* Return the number of IP interfaces on this machine.*/
static int number_of_interfaces(sock_probe *s) {
  if (s == nullptr) {
    return 0;
  }

  return s->nbr_ifs; /* Number of interfaces */
}

static struct ifaddrs *get_interface(sock_probe *s, int count) {
  struct ifaddrs *net_if = nullptr;
  int i = 0;

  if (s == nullptr) {
    return nullptr;
  }

  net_if = s->interfaces;
  idx_check_ret(count, number_of_interfaces(s), nullptr) {
    while (net_if != nullptr) {
      if ((net_if->ifa_addr) && ((net_if->ifa_addr->sa_family == AF_INET) ||
                                 (net_if->ifa_addr->sa_family == AF_INET6))) {
        if (i >= count)
          return net_if;
        else
          i++;
      }

      net_if = net_if->ifa_next;
    }
  }

  return nullptr;
}

static bool_t is_if_running(sock_probe *s, int count) {
  struct ifaddrs *net_if = nullptr;

  if (s == nullptr) {
    return 0;
  }

  net_if = get_interface(s, count);
  return net_if != nullptr && (net_if->ifa_flags & IFF_UP) &&
         (net_if->ifa_flags & IFF_RUNNING);
}

static void get_sockaddr_address(sock_probe *s, int count,
                                 struct sockaddr **out) {
  get_sockaddr(s, count, out, kSockaddrOpAddress);
}

static void get_sockaddr_netmask(sock_probe *s, int count,
                                 struct sockaddr **out) {
  get_sockaddr(s, count, out, kSockaddrOpNetmask);
}

/* Return the sockaddr of interface #count. */
static void get_sockaddr(sock_probe *s, int count, struct sockaddr **out,
                         SockaddrOp addr_operation) {
  struct ifaddrs *net_if = get_interface(s, count);

  if (net_if == nullptr) {
    *out = nullptr;
    return;
  }

  switch (addr_operation) {
    case kSockaddrOpAddress:
      *out = (struct sockaddr *)net_if->ifa_addr;
      break;
    case kSockaddrOpNetmask:
      *out = (struct sockaddr *)net_if->ifa_netmask;
      break;
    default:
      break;
  }
}

static char *get_if_name(sock_probe *s, int count) {
  struct ifaddrs *net_if = get_interface(s, count);

  return net_if != nullptr ? net_if->ifa_name : nullptr;
}
