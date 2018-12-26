/*
   Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

#include <signaldata/CreateFK.hpp>

bool
printCREATE_FK_REQ(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  const CreateFKReq* sig = (const CreateFKReq*)theData;
  fprintf(output, " clientData: %u", sig->clientData);
  fprintf(output, " clientRef: 0x%x", sig->clientRef);
  fprintf(output, " requestInfo: %x", sig->requestInfo);
  fprintf(output, "\n");
  fprintf(output, " transId: 0x%x", sig->transId);
  fprintf(output, " transKey: %u", sig->transKey);
  fprintf(output, "\n");
  return true;
}

bool
printCREATE_FK_REF(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  const CreateFKRef* sig = (const CreateFKRef*)theData;
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " transId: 0x%x", sig->transId);
  fprintf(output, "\n");
  fprintf(output, " errorCode: %u", sig->errorCode);
  fprintf(output, " errorLine: %u", sig->errorLine);
  fprintf(output, " errorNodeId: %u", sig->errorNodeId);
  fprintf(output, " masterNodeId: %u", sig->masterNodeId);
  fprintf(output, "\n");
  return true;
}

bool
printCREATE_FK_CONF(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  const CreateFKConf* sig = (const CreateFKConf*)theData;
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " transId: 0x%x", sig->transId);
  fprintf(output, "\n");
  fprintf(output, " fkId: %u", sig->fkId);
  fprintf(output, " fkVersion: %u", sig->fkVersion);
  fprintf(output, "\n");
  return true;
}
