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

#include <signaldata/CreateTab.hpp>
#include <SignalLoggerManager.hpp>

bool
printCREATE_TAB_REQ(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  const CreateTabReq* sig = (const CreateTabReq*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, " requestType: %u", sig->requestType);
  fprintf(output, "\n");
  fprintf(output, " tableId: %u", sig->tableId);
  fprintf(output, " tableVersion: 0x%x", sig->tableVersion);
  fprintf(output, " gci: %u", sig->gci);
  fprintf(output, "\n");
  return true;
}

bool
printCREATE_TAB_CONF(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  const CreateTabConf* sig = (const CreateTabConf*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, "\n");
  return true;
}

bool
printCREATE_TAB_REF(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  const CreateTabRef* sig = (const CreateTabRef*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, "\n");
  fprintf(output, " errorCode: %u", sig->errorCode);
  fprintf(output, " errorLine: %u", sig->errorLine );
  fprintf(output, " errorKey: %u", sig->errorKey);
  fprintf(output, " errorStatus: %u", sig->errorStatus);
  fprintf(output, "\n");
  return true;
}
