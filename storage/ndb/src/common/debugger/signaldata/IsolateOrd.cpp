/*
   Copyright (c) 2014  Oracle and/or its affiliates. All rights reserved.

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


#include <signaldata/IsolateOrd.hpp>

#define JAM_FILE_ID 495

bool
printISOLATE_ORD(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo){
  
  const IsolateOrd * const sig = (IsolateOrd *) theData;
  
  fprintf(output, " senderRef : %x step : %s delayMillis : %u, nodesToIsolate :",
          sig->senderRef,
          (sig->isolateStep == IsolateOrd::IS_REQ?"Request" :
           sig->isolateStep == IsolateOrd::IS_BROADCAST?"Broadcast" :
           sig->isolateStep == IsolateOrd::IS_DELAY?"Delay":
           "??"),
          sig->delayMillis);
  
  for (Uint32 i=0; i < NdbNodeBitmask::Size; i++)
  {
    fprintf(output, " %x", sig->nodesToIsolate[i]);
  }
  fprintf(output, "\n");

  return true;
}

#undef JAM_FILE_ID
