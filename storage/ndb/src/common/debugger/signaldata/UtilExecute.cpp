/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <signaldata/UtilExecute.hpp>

bool 
printUTIL_EXECUTE_REQ(FILE* out, const Uint32 * data, Uint32 len, Uint16 rec) 
{
  const UtilExecuteReq* const sig = (UtilExecuteReq*)data;
  fprintf(out, " senderRef: H'%.8x, senderData: H'%.8x prepareId: %d "
          " releaseFlag: %d\n",
	  sig->senderRef,
	  sig->senderData,
	  sig->getPrepareId(),
          sig->getReleaseFlag());
  return true;
}

bool 
printUTIL_EXECUTE_CONF(FILE* out, const Uint32 * data, Uint32 len, Uint16 rec)
{
  UtilExecuteConf* sig = (UtilExecuteConf*)data;
  fprintf(out, " senderData: H'%.8x\n",
	  sig->senderData);
  return true;
}

bool 
printUTIL_EXECUTE_REF(FILE* out, const Uint32 * data, Uint32 len, Uint16 rec)
{
  UtilExecuteRef* sig = (UtilExecuteRef*)data;
  fprintf(out, " senderData: H'%.8x, ", sig->senderData);
  fprintf(out, " errorCode: %s, ",
	  sig->errorCode == UtilExecuteRef::IllegalKeyNumber ? 
	  "IllegalKeyNumber" : 
	  sig->errorCode == UtilExecuteRef::IllegalAttrNumber ? 
	  "IllegalAttrNumber" : 
	  sig->errorCode == UtilExecuteRef::TCError ? 
	  "TCError" : 
	  sig->errorCode == UtilExecuteRef::AllocationError ? 
	  "AllocationError" :
	  "Unknown");
  fprintf(out, " TCErrorCode: %d\n",
	  sig->TCErrorCode);
  return true;
}
