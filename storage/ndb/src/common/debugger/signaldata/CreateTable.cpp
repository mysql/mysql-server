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

#include <signaldata/CreateTable.hpp>

bool
printCREATE_TABLE_REQ(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  const CreateTableReq* sig = (const CreateTableReq*)theData;
  fprintf(output, " clientRef: 0x%x", sig->clientRef);
  fprintf(output, " clientData: %u", sig->clientData);
  fprintf(output, " requestInfo: %x", sig->requestInfo);
  fprintf(output, " transId: 0x%x", sig->transId);
  fprintf(output, " transKey: %u", sig->transKey);
  fprintf(output, "\n");
  return true;
}

bool
printCREATE_TABLE_CONF(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  const CreateTableConf* sig = (const CreateTableConf*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " clientData: %u", sig->clientData);
  fprintf(output, " transId: 0x%x", sig->transId);
  fprintf(output, "\n");
  fprintf(output, " tableId: %u", sig->tableId);
  fprintf(output, " tableVersion: %u", sig->tableVersion);
  fprintf(output, "\n");
  return true;
}

bool
printCREATE_TABLE_REF(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  const CreateTableRef* sig = (const CreateTableRef*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " clientData: %u", sig->clientData);
  fprintf(output, " transId: 0x%x", sig->transId);
  fprintf(output, "\n");
  fprintf(output, " errorCode: %u", sig->errorCode);
  fprintf(output, " errorLine: %u", sig->errorLine);
  fprintf(output, " errorNodeId: %u", sig->errorNodeId);
  fprintf(output, " masterNodeId: %u", sig->masterNodeId);
  fprintf(output, "\n");
  fprintf(output, " errorStatus: %u", sig->errorStatus);
  fprintf(output, " errorKey: %u", sig->errorKey);
  fprintf(output, "\n");
  return true;
}
