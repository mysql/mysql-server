/* Copyright (C) 2007 MySQL AB, 2008 Sun Microsystems, Inc.
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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <signaldata/SchemaTrans.hpp>
#include <signaldata/DictSignal.hpp>

bool
printSCHEMA_TRANS_BEGIN_REQ(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  const SchemaTransBeginReq* sig = (const SchemaTransBeginReq*)theData;
  fprintf(output, " clientRef: 0x%x", sig->clientRef);
  fprintf(output, " transId: 0x%x", sig->transId);
  fprintf(output, " requestInfo: 0x%x", sig->requestInfo);
  fprintf(output, "\n");
  return true;
}

bool
printSCHEMA_TRANS_BEGIN_CONF(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  const SchemaTransBeginConf* sig = (const SchemaTransBeginConf*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " transId: 0x%x", sig->transId);
  fprintf(output, " transKey: %u", sig->transKey);
  fprintf(output, "\n");
  return true;
}

bool
printSCHEMA_TRANS_BEGIN_REF(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  const SchemaTransBeginRef* sig = (const SchemaTransBeginRef*)theData;
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
printSCHEMA_TRANS_END_REQ(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  const SchemaTransEndReq* sig = (const SchemaTransEndReq*)theData;
  fprintf(output, " clientRef: 0x%x", sig->clientRef);
  fprintf(output, " transId: 0x%x", sig->transId);
  fprintf(output, " requestInfo: 0x%x", sig->requestInfo);
  fprintf(output, "\n");
  fprintf(output, " transKey: %u", sig->transKey);
  fprintf(output, " flags: 0x%x", sig->flags);
  fprintf(output, "\n");
  return true;
}

bool
printSCHEMA_TRANS_END_CONF(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  const SchemaTransEndConf* sig = (const SchemaTransEndConf*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " transId: 0x%x", sig->transId);
  fprintf(output, "\n");
  return true;
}

bool
printSCHEMA_TRANS_END_REF(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  const SchemaTransEndRef* sig = (const SchemaTransEndRef*)theData;
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
printSCHEMA_TRANS_END_REP(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  const SchemaTransEndRep* sig = (const SchemaTransEndRep*)theData;
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


