/*
   Copyright (C) 2003, 2005-2008 MySQL AB
    All rights reserved. Use is subject to license terms.

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

#include <signaldata/AlterTab.hpp>

bool
printALTER_TAB_REQ(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  const AlterTabReq* sig = (const AlterTabReq*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, " requestType: %u", sig->requestType);
  fprintf(output, "\n");
  fprintf(output, " tableId: %u", sig->tableId);
  fprintf(output, " tableVersion: 0x%x", sig->tableVersion);
  fprintf(output, " newTableVersion: 0x%x", sig->newTableVersion);
  fprintf(output, " gci: %u", sig->gci);
  fprintf(output, " changeMask: 0x%x", sig->changeMask);
  fprintf(output, "\n");
  fprintf(output, " connectPtr: %u", sig->connectPtr);
  fprintf(output, " noOfNewAttr: %u", sig->noOfNewAttr);
  fprintf(output, " newNoOfCharsets: %u", sig->newNoOfCharsets);
  fprintf(output, " newNoOfKeyAttrs: %u", sig->newNoOfKeyAttrs);
  fprintf(output, "\n");
  return true;
}

bool
printALTER_TAB_CONF(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  const AlterTabConf* sig = (const AlterTabConf*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, " connectPtr: %u", sig->connectPtr);
  fprintf(output, "\n");
  return true;
}

bool
printALTER_TAB_REF(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  const AlterTabRef* sig = (const AlterTabRef*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, "\n");
  fprintf(output, " errorCode: %u", sig->errorCode);
  fprintf(output, " errorLine: %u", sig-> errorLine);
  fprintf(output, " errorKey: %u", sig->errorKey);
  fprintf(output, " errorStatus: %u", sig->errorStatus);
  fprintf(output, "\n");
  return true;
}
