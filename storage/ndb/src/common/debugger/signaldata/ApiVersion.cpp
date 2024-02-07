/* Copyright (c) 2008, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "signaldata/ApiVersion.hpp"
#include "RefConvert.hpp"
#include "portlib/NdbTCP.h"

bool printAPI_VERSION_REQ(FILE *output, const Uint32 *theData, Uint32 len,
                          Uint16 /*recBlockNo*/) {
  if (len < ApiVersionReq::SignalLength) {
    assert(false);
    return false;
  }

  const ApiVersionReq *sig = (const ApiVersionReq *)&theData[0];

  fprintf(output,
          " senderRef: (node: %d, block: %d), nodeId: %d\n"
          " version: %d, mysql_version: %d\n",
          refToNode(sig->senderRef), refToBlock(sig->senderRef), sig->nodeId,
          sig->version, sig->mysql_version);
  return true;
}

bool printAPI_VERSION_CONF(FILE *output, const Uint32 *theData, Uint32 len,
                           Uint16 /*recBlockNo*/) {
  const ApiVersionConf *sig = (const ApiVersionConf *)&theData[0];

  if (len <= ApiVersionConf::SignalLengthIPv4) {
    fprintf(output,
            " senderRef: (node: %d, block: %d), nodeId: %d\n"
            " version: %d, mysql_version: %d, inet_addr: %d\n"
            " isSingleUser: %d",
            refToNode(sig->senderRef), refToBlock(sig->senderRef), sig->nodeId,
            sig->version, sig->mysql_version, sig->m_inet_addr,
            sig->isSingleUser);
  } else {
    ndb_sockaddr in((const in6_addr *)sig->m_inet6_addr, 0);
    char addr_buf[INET6_ADDRSTRLEN];
    char *address = Ndb_inet_ntop(&in, addr_buf, INET6_ADDRSTRLEN);
    fprintf(output,
            " senderRef: (node: %d, block: %d), nodeId: %d\n"
            " version: %d, mysql_version: %d, inet6_addr: %s\n"
            " isSingleUser: %d",
            refToNode(sig->senderRef), refToBlock(sig->senderRef), sig->nodeId,
            sig->version, sig->mysql_version, address, sig->isSingleUser);
  }
  return true;
}
