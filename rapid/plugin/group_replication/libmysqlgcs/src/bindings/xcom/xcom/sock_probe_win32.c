/* Copyright (c) 2010, 2016, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file sock_probe_win32.c
  Utility functions to check if a network address or name matches an
  interface on the machine we are running on. This is useful to deduce
  the node number from a list of network addresses or names. The node
  number is the index of the list element which matches.
*/

#include <windows.h>
#undef FD_SETSIZE
#define FD_SETSIZE 256
#include <winsock2.h>
#include <ws2tcpip.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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

typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr sockaddr;

/*typedef struct sockaddr_in6 SOCKADDR_IN6; */
typedef struct in_addr in_addr;

/*
  The sock_probe class provides utillity functions for
  accessing the (set of) IP addresses on the current machine and
  checking if one of these addresses matches a host.
*/
struct sock_probe
{
  int tmp_socket;
  struct _INTERFACE_INFO interfaceInfo[64];
  DWORD noBytesReturned;
  /*
  struct ifconf ifc;
  struct ifreq *ifrp;
  struct ifreq conf[64];
  */
};

typedef struct sock_probe sock_probe;

/* Defines for registry traversal */
#define MAX_KEY_LENGTH 255
#define MAX_VALUE_NAME 16383

/*
 * Search the registry on this machine for any network interfaces.
 */
static void reg_search(sock_probe *s, gchar *super_key_handle, gchar *name)
{
   /* This code is windows style to ease referencing the MSDN docs */
   HKEY key_handle;
   if(ERROR_SUCCESS == RegOpenKeyEx((HKEY)super_key_handle,
                                    name,
                                    0,
                                    KEY_READ,
                                    &key_handle)){
      DWORD sub_keys = 0;   /* number of subkeys */
      DWORD num_val;        /* number of values for key */
      DWORD max_subkey;     /* longest subkey size */
      DWORD max_value_data; /* longest value data */
      DWORD name_len;       /* size of name string */
      DWORD security_desc;  /* size of security descriptor */
      DWORD class_name_len = MAX_PATH;  /* size of class string */
      DWORD max_class;      /* longest class string */
      DWORD max_value;      /* longest value name */
      DWORD i = 0;
      DWORD ret_code = ERROR_SUCCESS;
      FILETIME last_write;   /* last write time */
      TCHAR class_name[MAX_PATH] = TEXT("");  /* Empty class name */
      TCHAR sub_key_name[MAX_KEY_LENGTH];   /* buffer for subkey name */
      TCHAR value_name[MAX_VALUE_NAME];
      DWORD value_name_len = MAX_VALUE_NAME;

      /* Get the number of subkeys and number of values */
      ret_code = RegQueryInfoKey(key_handle, class_name, &class_name_len,0,
                                 &sub_keys, &max_subkey, &max_class, &num_val,
                                 &max_value, &max_value_data, &security_desc,
                                 &last_write);
      /* Check all subkeys */
      for(i=0; i<sub_keys; i++){
         name_len = MAX_KEY_LENGTH;
         ret_code = RegEnumKeyEx(key_handle, i, sub_key_name, &name_len,
                                 0, 0, 0, &last_write);
         if(ERROR_SUCCESS == ret_code){
            strcat(sub_key_name,"\\Parameters\\Tcpip");
            /* Search for subkeys matching Parameters\Tcpip */
            reg_search(s, (gchar*)key_handle, (gchar*)sub_key_name);

         }
      }

      /* Process all values, looking for values matching IPAddress */
      for(i=0; i < num_val; i++){
         unsigned char ip_str[256];
         DWORD ip_str_size = (DWORD)sizeof(ip_str);
         DWORD type;

         value_name_len = MAX_VALUE_NAME;
         value_name[0] = '\0';
         ret_code = RegEnumValue(key_handle, i, value_name, &value_name_len,
                                 0, &type, ip_str, &ip_str_size);

      }
      RegCloseKey(key_handle);
   }
}

/* Initialize socket probe */
static int init_sock_probe(sock_probe *s)
{
  s->tmp_socket = INVALID_SOCKET;
  s->noBytesReturned = 0;
  memset(&s->interfaceInfo,  0, sizeof(s->interfaceInfo));

  if ((s->tmp_socket = xcom_checked_socket(AF_INET, SOCK_DGRAM, 0).val) == INVALID_SOCKET) {
    return -1;
  }

  /*
   * Get information about IP interfaces on this machine.
   *
   * FIXME: Should we rewrite to IPHelper API functions as they provide much of this already ?
   */
  if(WSAIoctl(s->tmp_socket, SIO_GET_INTERFACE_LIST,
                  NULL, 0, &(s->interfaceInfo),
                  sizeof(s->interfaceInfo), &(s->noBytesReturned),
                  NULL, NULL) == SOCKET_ERROR) {
     DWORD err = WSAGetLastError();
     DBGOUT(NUMEXP(err); STREXP(gai_strerror(err)););
     abort();
  }
  /* Extract interface list from registry */
  reg_search(s, (gchar*)HKEY_LOCAL_MACHINE,(gchar*)TEXT("System\\CurrentControlSet\\Services"));

  return 0;
}

/* Close socket of sock_probe */
static void close_sock_probe(sock_probe *s)
{
  if(s->tmp_socket != INVALID_SOCKET){
    closesocket(s->tmp_socket);
    s->tmp_socket = INVALID_SOCKET;
  }
}

/* Close any open socket and free sock_probe */
static void delete_sock_probe(sock_probe *s)
{
  close_sock_probe(s);
  X_FREE(s);
}

/* Return the number of IP interfaces on this machine.*/
static int number_of_interfaces(sock_probe *s)
{
    return s->noBytesReturned/sizeof(struct _INTERFACE_INFO);
}

/* Return TRUE if interface #count is running. */
static bool_t is_if_running(sock_probe *s, int count)
{
   return (s->tmp_socket >= 0) && (s->interfaceInfo[count].iiFlags & IFF_UP);
}

/* Return the sockaddr of interface #count. */
static sockaddr get_sockaddr(sock_probe *s, int count)
{
  /* s->ifrp[count].ifr_addr if of type sockaddr */
  idx_check_fail(count, number_of_interfaces(s)) return s->interfaceInfo[count].iiAddress.Address;
}

/* Return the IP address of interface #count. */
static in_addr get_in_addr(sock_probe *s, int count)
{
  idx_check_fail(count, number_of_interfaces(s)) return s->interfaceInfo[count].iiAddress.AddressIn.sin_addr;
}

