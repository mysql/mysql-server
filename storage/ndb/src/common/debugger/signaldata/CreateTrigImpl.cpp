/* Copyright (c) 2007, 2023, Oracle and/or its affiliates.

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

#include <signaldata/CreateTrigImpl.hpp>
#include <trigger_definitions.h>

bool
printCREATE_TRIG_IMPL_REQ(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  if (len < CreateTrigImplReq::SignalLength)
  {
    assert(false);
    return false;
  }

  const CreateTrigImplReq* sig = (const CreateTrigImplReq*)theData;
  const Uint32 triggerType =
    TriggerInfo::getTriggerType(sig->triggerInfo);
  const Uint32 triggerActionTime =
    TriggerInfo::getTriggerActionTime(sig->triggerInfo);
  const Uint32 triggerEvent =
    TriggerInfo::getTriggerEvent(sig->triggerInfo);
  const Uint32 monitorReplicas =
    TriggerInfo::getMonitorReplicas(sig->triggerInfo);
  const Uint32 monitorAllAttributes =
    TriggerInfo::getMonitorAllAttributes(sig->triggerInfo);
  const Uint32 reportAllMonitoredAttributes =
    TriggerInfo::getReportAllMonitoredAttributes(sig->triggerInfo);
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
  fprintf(output, "   triggerType: %u [%s]",
                  triggerType,
                  TriggerInfo::triggerTypeName(triggerType));
  fprintf(output, "\n");
  fprintf(output, "   triggerActionTime: %u [%s]",
                  triggerActionTime,
                  TriggerInfo::triggerActionTimeName(triggerActionTime));
  fprintf(output, "\n");
  fprintf(output, "   triggerEvent: %u [%s]",
                  triggerEvent,
                  TriggerInfo::triggerEventName(triggerEvent));
  fprintf(output, "\n");
  fprintf(output, "   monitorReplicas: %u",
                  monitorReplicas);
  fprintf(output, "\n");
  fprintf(output, "   monitorAllAttributes: %u",
                  monitorAllAttributes);
  fprintf(output, "\n");
  fprintf(output, "   reportAllMonitoredAttributes: %u",
                  reportAllMonitoredAttributes);
  fprintf(output, " receiverRef: 0x%x", sig->receiverRef);
  fprintf(output, "\n");  
  return true;
}

bool
printCREATE_TRIG_IMPL_CONF(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  if (len < CreateTrigImplConf::SignalLength)
  {
    assert(false);
    return false;
  }

  const CreateTrigImplConf* sig = (const CreateTrigImplConf*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, "\n");  
  fprintf(output, " tableId: %u", sig->tableId);
  fprintf(output, " triggerId: %u", sig->triggerId);
  fprintf(output, " triggerInfo: 0x%x", sig->triggerInfo);
  fprintf(output, "\n");  
  return true;
}

bool
printCREATE_TRIG_IMPL_REF(FILE* output, const Uint32* theData, Uint32 len, Uint16)
{
  if (len < CreateTrigImplRef::SignalLength)
  {
    assert(false);
    return false;
  }

  const CreateTrigImplRef* sig = (const CreateTrigImplRef*)theData;
  fprintf(output, " senderRef: 0x%x", sig->senderRef);
  fprintf(output, " senderData: %u", sig->senderData);
  fprintf(output, "\n");  
  fprintf(output, " tableId: %u", sig->tableId);
  fprintf(output, " triggerId: %u", sig->triggerId);
  fprintf(output, " triggerInfo: 0x%x", sig->triggerInfo);
  fprintf(output, "\n");  
  fprintf(output, " errorCode: %u", sig->errorCode);
  fprintf(output, " errorLine: %u", sig->errorLine);
  fprintf(output, " errorNodeId: %u", sig->errorNodeId);
  fprintf(output, " masterNodeId: %u", sig->masterNodeId);
  fprintf(output, "\n");  
  return true;
}
