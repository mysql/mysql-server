/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.
    Use is subject to license terms.

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

#include <signaldata/UtilExecute.hpp>

bool printUTIL_EXECUTE_REQ(FILE* out,
                           const Uint32* data,
                           Uint32 len,
                           Uint16 /*rec*/)
{
  if (len < UtilExecuteRef::SignalLength)
  {
    assert(false);
    return false;
  }

  const UtilExecuteReq* const sig = (const UtilExecuteReq*)data;
  fprintf(out, " senderRef: H'%.8x, senderData: H'%.8x prepareId: %d "
          " releaseFlag: %d\n",
	  sig->senderRef,
	  sig->senderData,
	  sig->getPrepareId(),
          sig->getReleaseFlag());
  return true;
}

bool printUTIL_EXECUTE_CONF(FILE* out,
                            const Uint32* data,
                            Uint32 len,
                            Uint16 /*rec*/)
{
  if (len < UtilExecuteConf::SignalLength)
  {
    assert(false);
    return false;
  }

  const UtilExecuteConf* sig = (const UtilExecuteConf*)data;
  fprintf(out, " senderData: H'%.8x gci: %u/%u\n",
	  sig->senderData, sig->gci_hi, sig->gci_lo);
  return true;
}

bool printUTIL_EXECUTE_REF(FILE* out,
                           const Uint32* data,
                           Uint32 len,
                           Uint16 /*rec*/)
{
  if (len < UtilExecuteRef::SignalLength)
  {
    assert(false);
    return false;
  }

  const UtilExecuteRef* sig = (const UtilExecuteRef*)data;
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
