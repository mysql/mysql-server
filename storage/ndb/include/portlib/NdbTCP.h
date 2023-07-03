/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_TCP_H
#define NDB_TCP_H

#include "ndb_global.h"
#include "portlib/ndb_socket.h"

#define NDB_ADDR_STRLEN 512

/*
 * Host name length from 1035 DOMAIN NAMES - IMPLEMENTATION AND SPECIFICATION.
 */
#define NDB_DNS_HOST_NAME_LENGTH 255
/*
 * Service name length from RFC 6335 Internet Assigned Numbers Authority (IANA)
 * Procedures for the Management of the Service Name and Transport Protocol
 * Port Number Registry
 */
#define NDB_IANA_SERVICE_NAME_LENGTH 15

/* Set user's preference for IPv4 or IPv6 when resolving names.
   The preference is stored and used for subsequent lookups.
   The default at startup is to prefer IPv4.
 */
void NdbTCP_set_preferred_IP_version(int version);

/*  Convert host name or ip address to in6_addr
    Returns 0 on success, -1 on failure
 */
int Ndb_getInAddr6(struct in6_addr * dst, const char *address);

/* src is an address of family af, either AF_INET or AF_INET6.
   dst is a character buffer that will hold address in presentation format.
   Returns dst.
 */
char* Ndb_inet_ntop(int af,
                    const void *src,
                    char *dst,
                    size_t dst_size);

int Ndb_split_string_address_port(const char *arg,
                               char *host,
                               size_t hostlen,
                               char *serv,
                               size_t servlen);

char* Ndb_combine_address_port(char *buf,
                               size_t bufsize,
                               const char *host,
                               Uint16 port);

#endif
