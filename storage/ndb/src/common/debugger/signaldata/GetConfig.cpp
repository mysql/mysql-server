/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

#include <signaldata/GetConfig.hpp>

bool
printGET_CONFIG_REQ(FILE * output, const Uint32 * theData,
                   Uint32 len, Uint16 receiverBlockNo)
{
  const GetConfigReq* sig = (const GetConfigReq*)theData;
  fprintf(output, " nodeId : %u senderRef : %x\n",
          sig->nodeId,
          sig->senderRef);
  return true;
}

bool
printGET_CONFIG_REF(FILE * output, const Uint32 * theData,
                   Uint32 len, Uint16 receiverBlockNo) {
  const GetConfigRef* sig = (const GetConfigRef*)theData;
  fprintf(output, " error : %u\n",
          sig->error);
  return true;
}


bool
printGET_CONFIG_CONF(FILE * output, const Uint32 * theData,
                   Uint32 len, Uint16 receiverBlockNo) {
  const GetConfigConf* sig = (const GetConfigConf*)theData;
  fprintf(output, " Config size : %u\n",
          sig->configLength);
  return true;
}
