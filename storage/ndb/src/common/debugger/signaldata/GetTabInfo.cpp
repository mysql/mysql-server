/* Copyright (C) 2007 MySQL AB
   Use is subject to license terms

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

#include <signaldata/GetTabInfo.hpp>
#include <signaldata/DictTabInfo.hpp>

bool
printGET_TABINFO_REQ(FILE* output, const Uint32* theData, Uint32 len, Uint16 rbn)
{
  const GetTabInfoReq* sig = (const GetTabInfoReq*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, "\n");
  fprintf(output, " requestType: 0x%x", sig->requestType);
  bool requestById = !(sig->requestType & GetTabInfoReq::RequestByName);
  bool requestByName = (sig->requestType & GetTabInfoReq::RequestByName);
  bool longSignalConf = (sig->requestType & GetTabInfoReq::LongSignalConf);
  if (requestById)
    fprintf(output, " RequestById");
  if (requestByName)
    fprintf(output, " RequestByName");
  if (longSignalConf)
    fprintf(output, " LongSignalConf");
  fprintf(output, "\n");
  if (requestById)
    fprintf(output, " tableId: %u", sig->tableId);
  if (requestByName)
    fprintf(output, " tableNameLen: %u", sig->tableNameLen);
  fprintf(output, " schemaTransId: 0x%x", sig->schemaTransId);
  fprintf(output, "\n");
  return true;
}

bool
printGET_TABINFO_CONF(FILE* output, const Uint32* theData, Uint32 len, Uint16 rbn)
{
  const GetTabInfoConf* sig = (const GetTabInfoConf*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, "\n");
  fprintf(output, " tableId: %u", sig->tableId);
  fprintf(output, " tableType: %u", sig->tableType);
  fprintf(output, "\n");
  switch (sig->tableType) {
  case DictTabInfo::Tablespace:
    fprintf(output, " freeExtents: %u", sig->freeExtents);
    break;
  case DictTabInfo::LogfileGroup:
    fprintf(output, " freeWordsHi: %u", sig->freeWordsHi);
    fprintf(output, " freeWordsLo: %u", sig->freeWordsLo);
    break;
  case DictTabInfo::Datafile:
  case DictTabInfo::Undofile:
    fprintf(output, " freeExtents: %u", sig->freeExtents);
    break;
  default:
    fprintf(output, " gci: %u", sig->gci);
    fprintf(output, " totalLen: %u", sig->totalLen);
    break;
  }
  fprintf(output, "\n");
  return true;
}

bool
printGET_TABINFO_REF(FILE* output, const Uint32* theData, Uint32 len, Uint16 rbn)
{
  const GetTabInfoRef* sig = (const GetTabInfoRef*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, "\n");
  fprintf(output, " requestType: 0x%x", sig->requestType);
  bool requestById = !(sig->requestType & GetTabInfoReq::RequestByName);
  bool requestByName = (sig->requestType & GetTabInfoReq::RequestByName);
  bool longSignalConf = (sig->requestType & GetTabInfoReq::LongSignalConf);
  if (requestById)
    fprintf(output, " RequestById");
  if (requestByName)
    fprintf(output, " RequestByName");
  if (longSignalConf)
    fprintf(output, " LongSignalConf");
  fprintf(output, "\n");
  if (requestById)
    fprintf(output, " tableId: %u", sig->tableId);
  if (requestByName)
    fprintf(output, " tableNameLen: %u", sig->tableNameLen);
  fprintf(output, " schemaTransId: 0x%x", sig->schemaTransId);
  fprintf(output, " errorCode: %u", sig->errorCode);
  fprintf(output, " errorLine: %u", sig->errorLine);
  fprintf(output, "\n");
  return true;
}
