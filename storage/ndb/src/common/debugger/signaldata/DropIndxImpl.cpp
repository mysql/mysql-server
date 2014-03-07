/* Copyright (C) 2007 MySQL AB
   Use is subject to license terms

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

#include <signaldata/DropIndxImpl.hpp>

bool
printDROP_INDX_IMPL_REQ(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  const DropIndxImplReq* sig = (const DropIndxImplReq*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, " requestType: %u", sig->requestType);
  fprintf(output, "\n");
  fprintf(output, " tableId: %u", sig->tableId);
  fprintf(output, " tableVersion: 0x%x", sig->tableVersion);
  fprintf(output, "\n");
  fprintf(output, " indexId: %u", sig->indexId);
  fprintf(output, " indexVersion: 0x%x", sig->indexVersion);
  fprintf(output, "\n");
  return true;
}

bool
printDROP_INDX_IMPL_CONF(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  const DropIndxImplConf* sig = (const DropIndxImplConf*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, "\n");
  return true;
}

bool
printDROP_INDX_IMPL_REF(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  const DropIndxImplRef* sig = (const DropIndxImplRef*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, "\n");
  fprintf(output, " errorCode: %u", sig->errorCode);
  fprintf(output, " errorLine: %u", sig->errorLine);
  fprintf(output, " errorNodeId: %u", sig->errorNodeId);
  fprintf(output, " masterNodeId: %u", sig->masterNodeId);
  fprintf(output, "\n");
  return true;
}
