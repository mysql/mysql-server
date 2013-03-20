/* Copyright 2008 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */


#include <signaldata/ApiVersion.hpp>
#include <RefConvert.hpp>

bool
printAPI_VERSION_REQ(FILE * output,
                     const Uint32 * theData,
                     Uint32 len,
                     Uint16 recBlockNo){

  ApiVersionReq * sig = (ApiVersionReq *)&theData[0];

  fprintf(output,
          " senderRef: (node: %d, block: %d), nodeId: %d\n" \
          " version: %d, mysql_version: %d\n",
	  refToNode(sig->senderRef), refToBlock(sig->senderRef),
	  sig->nodeId, sig->version, sig->mysql_version);
  return true;
}

bool
printAPI_VERSION_CONF(FILE * output,
                      const Uint32 * theData,
                      Uint32 len,
                      Uint16 recBlockNo){

  ApiVersionConf * sig = (ApiVersionConf *)&theData[0];

  fprintf(output,
          " senderRef: (node: %d, block: %d), nodeId: %d\n" \
          " version: %d, mysql_version: %d, inet_addr: %d\n",
	  refToNode(sig->senderRef), refToBlock(sig->senderRef),
	  sig->nodeId, sig->version, sig->mysql_version, sig->inet_addr);
  return true;
}
