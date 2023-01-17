/* Copyright (c) 2007, 2023, Oracle and/or its affiliates.
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

#include <signaldata/DropTrigImpl.hpp>

bool
printDROP_TRIG_IMPL_REQ(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  if (len < DropTrigImplReq::SignalLength)
  {
    assert(false);
    return false;
  }

  const DropTrigImplReq* sig = (const DropTrigImplReq*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, " requestType: %u", sig->requestType);
  fprintf(output, "\n");
  fprintf(output, " tableId: %u", sig->tableId);
  fprintf(output, " tableVersion: 0x%x", sig->tableVersion);
  fprintf(output, " indexId: %u", sig->indexId);
  fprintf(output, " indexVersion: 0x%x", sig->indexVersion);
  fprintf(output, " triggerNo: %u", sig->triggerNo);
  fprintf(output, "\n");
  fprintf(output, " triggerId: %u", sig->triggerId);
  fprintf(output, " triggerInfo: 0x%x", sig->triggerInfo);
  fprintf(output, "\n");
  return true;
}

bool
printDROP_TRIG_IMPL_CONF(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  if (len < DropTrigImplConf::SignalLength)
  {
    assert(false);
    return false;
  }

  const DropTrigImplConf* sig = (const DropTrigImplConf*) theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, "\n");
  fprintf(output, " tableId: %u", sig->tableId);
  fprintf(output, " triggerId: %u", sig->triggerId);
  fprintf(output, "\n");
  return true;
}

bool
printDROP_TRIG_IMPL_REF(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  if (len < DropTrigImplRef::SignalLength)
  {
    assert(false);
    return false;
  }

  const DropTrigImplRef * sig = (const DropTrigImplRef*) theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, "\n");
  fprintf(output, " tableId: %u", sig->tableId);
  fprintf(output, " triggerId: %u", sig->triggerId);
  fprintf(output, "\n");
  fprintf(output, " errorCode: %u", sig->errorCode);
  fprintf(output, " errorLine: %u", sig->errorLine);
  fprintf(output, " errorNodeId: %u", sig->errorNodeId);
  fprintf(output, " masterNodeId: %u", sig->masterNodeId);
  fprintf(output, "\n");
  return true;
}
