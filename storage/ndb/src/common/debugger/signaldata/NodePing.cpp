/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <signaldata/NodePing.hpp>

bool
printNODE_PING_REQ(FILE * output, const Uint32 * theData,
                   Uint32 len, Uint16 receiverBlockNo) {
  const NodePingReq * const sig = CAST_CONSTPTR(NodePingReq, theData);
  fprintf(output, " senderRef : %x round : %u\n",
          sig->senderRef,
          sig->senderData);
  return true;
}

bool
printNODE_PING_CONF(FILE * output, const Uint32 * theData,
                    Uint32 len, Uint16 receiverBlockNo) {
  const NodePingConf * const sig = CAST_CONSTPTR(NodePingConf, theData);
  fprintf(output, " senderRef : %x round : %u\n",
          sig->senderRef,
          sig->senderData);
  return true;
}
