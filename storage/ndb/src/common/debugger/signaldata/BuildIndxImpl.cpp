/* Copyright (C) 2007 MySQL AB

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

#include <signaldata/BuildIndxImpl.hpp>

bool
printBUILD_INDX_IMPL_REQ(FILE* output, const Uint32* theData, Uint32 len, Uint16 rbn)
{
  const BuildIndxImplReq* sig = (const BuildIndxImplReq*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, " requestType: %u", sig->requestType);
  fprintf(output, " transId: 0x%x", sig->transId);
  fprintf(output, "\n");
  fprintf(output, " buildId: %u", sig->buildId);
  fprintf(output, " buildKey: %u", sig->buildKey);
  fprintf(output, "\n");
  fprintf(output, " tableId: %u", sig->tableId);
  fprintf(output, " indexId: %u", sig->indexId);
  fprintf(output, " indexType: %u", sig->indexType);
  fprintf(output, " parallelism: %u", sig->parallelism);
  fprintf(output, "\n");
  return true;
}

bool
printBUILD_INDX_IMPL_CONF(FILE* output, const Uint32* theData, Uint32 len, Uint16 rbn)
{
  const BuildIndxImplConf* sig = (const BuildIndxImplConf*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, "\n");
  return true;
}

bool printBUILD_INDX_IMPL_REF(FILE* output, const Uint32* theData, Uint32 len, Uint16 rbn)
{
  const BuildIndxImplRef* sig = (const BuildIndxImplRef*)theData;
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
