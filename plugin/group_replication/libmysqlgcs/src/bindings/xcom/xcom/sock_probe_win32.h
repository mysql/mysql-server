/* Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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

/**
  Utility functions to check if a network address or name matches an
  interface on the machine we are running on. This is useful to deduce
  the node number from a list of network addresses or names. The node
  number is the index of the list element which matches.
*/

#include <windows.h>

#undef FD_SETSIZE
#define FD_SETSIZE 256
#include <assert.h>
#include <errno.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define BSD_COMP

#include "xcom/node_no.h"
#include "xcom/simset.h"
#include "xcom/sock_probe.h"
#include "xcom/task.h"
#include "xcom/task_debug.h"
#include "xcom/task_net.h"
#include "xcom/task_os.h"
#include "xcom/xcom_base.h"
#include "xcom/xcom_common.h"
#include "xcom/xcom_memory.h"
#include "xcom/xcom_profile.h"
#include "xcom/xcom_transport.h"
#include "xdr_gen/xcom_vp.h"

#define WORKING_BUFFER_SIZE 1024 * 1024

typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr sockaddr;

typedef struct in_addr in_addr;

/*
  The sock_probe class provides utillity functions for
  accessing the (set of) IP addresses on the current machine and
  checking if one of these addresses matches a host.
*/
struct sock_probe {
  PIP_ADAPTER_ADDRESSES addresses;
  int number_of_interfaces;
};

typedef struct sock_probe sock_probe;

typedef enum SockaddrOp {
  kSockaddrOpAddress = 0,
  kSockaddrOpNetmask
} SockaddrOp;

/* Initialize socket probe */
static int init_sock_probe(sock_probe *s) {
  ULONG flags = GAA_FLAG_INCLUDE_PREFIX, family = AF_UNSPEC, out_buflen = 0;
  DWORD retval = 0;
  PIP_ADAPTER_ADDRESSES curr_addresses;

  if (s == NULL) {
    return 1;
  }

  out_buflen = WORKING_BUFFER_SIZE;

  s->addresses = (IP_ADAPTER_ADDRESSES *)xcom_malloc(out_buflen);
  if (s->addresses == NULL) return 1;

  retval = GetAdaptersAddresses(family, flags, NULL, s->addresses, &out_buflen);

  curr_addresses = s->addresses;
  while (curr_addresses) {
    PIP_ADAPTER_UNICAST_ADDRESS_LH curr_unicast_address;

    IFDBG(D_TRANSPORT, STRLIT("Adapter status: ");
          if (curr_addresses->OperStatus == IfOperStatusUp) STRLIT("UP");
          else STRLIT("DOWN"););

    curr_unicast_address = curr_addresses->FirstUnicastAddress;
    while (curr_unicast_address) {
      if (curr_unicast_address->Address.lpSockaddr->sa_family == AF_INET ||
          curr_unicast_address->Address.lpSockaddr->sa_family == AF_INET6) {
        s->number_of_interfaces++;
      }
      curr_unicast_address = curr_unicast_address->Next;
    }

    curr_addresses = curr_addresses->Next;
  }

  return retval != NO_ERROR;
}

/* Close socket of sock_probe */
static void close_sock_probe(sock_probe *s) {
  if (s && s->addresses) free(s->addresses);
  free(s);
}

/* Return the number of IP interfaces on this machine.*/
static int number_of_interfaces(sock_probe *s) {
  if (s == NULL) {
    return 0;
  }
  return s->number_of_interfaces;
}

/* Return TRUE if interface #count is running. */
static bool_t is_if_running(sock_probe *s, int /* count */) {
  if (s == NULL) {
    return 0;
  }
  return 1; /* We will always report active because GetAdaptersAddresses */
            /* always returns active interfaces. */
}

struct interface_info {
  PIP_ADAPTER_ADDRESSES network_interface;
  PIP_ADAPTER_UNICAST_ADDRESS_LH network_address;
};
typedef struct interface_info interface_info;

static interface_info get_interface(sock_probe *s, int count) {
  int i = 0;
  interface_info retval;
  retval.network_address = NULL;
  retval.network_interface = NULL;

  idx_check_fail(count, number_of_interfaces(s)) {
    PIP_ADAPTER_ADDRESSES curr_addresses = s->addresses;
    while (curr_addresses && (i <= count)) {
      PIP_ADAPTER_UNICAST_ADDRESS_LH curr_unicast_address =
          curr_addresses->FirstUnicastAddress;
      while (curr_unicast_address && (i <= count)) {
        if (curr_unicast_address->Address.lpSockaddr->sa_family == AF_INET ||
            curr_unicast_address->Address.lpSockaddr->sa_family == AF_INET6) {
          if (i == count) {
            retval.network_address = curr_unicast_address;
            retval.network_interface = curr_addresses;
          }
          i++;
        }
        curr_unicast_address = curr_unicast_address->Next;
      }

      curr_addresses = curr_addresses->Next;
    }
  }

  return retval;
}

/* Return the sockaddr of interface #count. */
/**
 * @brief Get the sockaddr object that pertains to a certain interface index
 * Depending of the addr_operation parameter value, it can be from:
 * - Netmask
 * - Physical Address
 *
 * @param s an initialized sock_probe structure
 * @param count the interface index to return
 * @param out the return value sockaddr. NULL in case of error.
 * @param addr_operation either request the sockaddr for the physical address or
 * for a netmask
 */
static void get_sockaddr(sock_probe *s, int count, struct sockaddr **out,
                         SockaddrOp addr_operation) {
  interface_info interface_info;

  if (s == NULL) {
    *out = NULL;
  }

  interface_info = get_interface(s, count);
  if (interface_info.network_interface == NULL) {
    *out = NULL;
    return;
  }

  /* Let see what the function caller wants... */
  switch (addr_operation) {
    /* Return the interface address sockaddr */
    case kSockaddrOpAddress:
      *out = interface_info.network_address->Address.lpSockaddr;
      break;
    /* Return the interface address netmask */
    case kSockaddrOpNetmask:
      /* Windows is the opposite of Nix.
         While Nix has a sockaddr that contains the netmask,
         and then you need to count the bits to see how many are
         set, in case of Windows, you already have the number of bits that
         are set in OnLinkPrefixLength field.
         The issue with that is that then you need to convert them to a network
         format. In case of IPv4, you have a method called
         ConvertLengthToIpv4Mask. In case of V6 you need to do it by hand
         setting the bits in the correct place of sin6_addr.s6_addr. */
      if (interface_info.network_address->Address.lpSockaddr->sa_family ==
          AF_INET) {
        struct sockaddr_in *out_value =
            (struct sockaddr_in *)xcom_malloc(sizeof(struct sockaddr_in));
        ConvertLengthToIpv4Mask(
            interface_info.network_address->OnLinkPrefixLength,
            &out_value->sin_addr.s_addr);
        *out = (struct sockaddr *)out_value;
      } else {
        long i, j;
        struct sockaddr_in6 *out_value =
            (struct sockaddr_in6 *)xcom_calloc(1, sizeof(struct sockaddr_in6));
        for (i = interface_info.network_address->OnLinkPrefixLength, j = 0;
             i > 0; i -= 8, ++j) {
          out_value->sin6_addr.s6_addr[j] =
              i >= 8 ? 0xff : (ULONG)((0xffU << (8 - i)));
        }
        *out = (struct sockaddr *)out_value;
      }
      break;
    default:
      break;
  }
}

static void get_sockaddr_address(sock_probe *s, int count,
                                 struct sockaddr **out) {
  get_sockaddr(s, count, out, kSockaddrOpAddress);
}

static void get_sockaddr_netmask(sock_probe *s, int count,
                                 struct sockaddr **out) {
  get_sockaddr(s, count, out, kSockaddrOpNetmask);
}

static char *get_if_name(sock_probe *s, int count) {
  interface_info interface_info = get_interface(s, count);

  if (interface_info.network_address == NULL) {
    return NULL;
  }

  return interface_info.network_interface->AdapterName;
}
