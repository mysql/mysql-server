/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <signaldata/CreateTrig.hpp>

bool printCREATE_TRIG_REQ(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo)
{
  const CreateTrigReq * const sig = (CreateTrigReq *) theData;

  //char triggerName[MAX_TAB_NAME_SIZE];
  char triggerType[32];
  char triggerActionTime[32];
  char triggerEvent[32];

  //sig->getTriggerName((char *) &triggerName);
  switch (sig->getTriggerType()) {
  case(TriggerType::SECONDARY_INDEX): 
    BaseString::snprintf(triggerType, sizeof(triggerType), "SECONDARY_INDEX");
    break;
  case(TriggerType::SUBSCRIPTION):
    BaseString::snprintf(triggerType, sizeof(triggerType), "SUBSCRIPTION");
    break;
  case(TriggerType::ORDERED_INDEX): 
    BaseString::snprintf(triggerType, sizeof(triggerType), "ORDERED_INDEX");
    break;
  default:
    BaseString::snprintf(triggerType, sizeof(triggerType), "UNKNOWN [%d]", (int)sig->getTriggerType());
    break;
  }
  switch (sig->getTriggerActionTime()) {
  case (TriggerActionTime::TA_BEFORE):
    BaseString::snprintf(triggerActionTime, sizeof(triggerActionTime), "BEFORE");
    break;
  case(TriggerActionTime::TA_AFTER):
    BaseString::snprintf(triggerActionTime, sizeof(triggerActionTime), "AFTER");
    break;
  case (TriggerActionTime::TA_DEFERRED):
    BaseString::snprintf(triggerActionTime, sizeof(triggerActionTime), "DEFERRED");
    break;
  case (TriggerActionTime::TA_DETACHED):
    BaseString::snprintf(triggerActionTime, sizeof(triggerActionTime), "DETACHED");
    break;
  default:
    BaseString::snprintf(triggerActionTime, sizeof(triggerActionTime),
	     "UNKNOWN [%d]", (int)sig->getTriggerActionTime());
    break;
  }
  switch (sig->getTriggerEvent()) {
  case (TriggerEvent::TE_INSERT):
    BaseString::snprintf(triggerEvent, sizeof(triggerEvent), "INSERT");
    break;
  case(TriggerEvent::TE_DELETE):
    BaseString::snprintf(triggerEvent, sizeof(triggerEvent), "DELETE");
    break;
  case(TriggerEvent::TE_UPDATE):
    BaseString::snprintf(triggerEvent, sizeof(triggerEvent), "UPDATE");
    break;
  case(TriggerEvent::TE_CUSTOM):
    BaseString::snprintf(triggerEvent, sizeof(triggerEvent), "CUSTOM");
    break;
  default:
    BaseString::snprintf(triggerEvent, sizeof(triggerEvent), "UNKNOWN [%d]", (int)sig->getTriggerEvent());
    break;
  }
  
  fprintf(output, "User: %u, ", sig->getUserRef());
  //fprintf(output, "Trigger name: \"%s\"\n", triggerName);
  fprintf(output, "Type: %s, ", triggerType);
  fprintf(output, "Action: %s, ", triggerActionTime);
  fprintf(output, "Event: %s, ", triggerEvent);
  fprintf(output, "Trigger id: %u, ", sig->getTriggerId());
  fprintf(output, "Table id: %u, ", sig->getTableId());
  fprintf(output, "Monitor replicas: %s ", (sig->getMonitorReplicas())?"true":"false");
  fprintf(output, "Monitor all attributes: %s ", (sig->getMonitorAllAttributes())?"true":"false");
  const AttributeMask& attributeMask = sig->getAttributeMask();

  char buf[MAXNROFATTRIBUTESINWORDS * 8 + 1];
  fprintf(output, "Attribute mask: %s", attributeMask.getText(buf));
  fprintf(output, "\n");  

  return false;
}

bool printCREATE_TRIG_CONF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo)
{
  const CreateTrigConf * const sig = (CreateTrigConf *) theData;
  
  fprintf(output, "User: %u, ", sig->getUserRef());
  fprintf(output, "Trigger id: %u, ", sig->getTriggerId());
  fprintf(output, "Table id: %u, ", sig->getTableId());
  fprintf(output, "\n");  

  return false;
}

bool printCREATE_TRIG_REF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo)
{
  const CreateTrigRef * const sig = (CreateTrigRef *) theData;

  fprintf(output, "User: %u, ", sig->getUserRef());
  fprintf(output, "Trigger id: %u, ", sig->getTriggerId());
  fprintf(output, "Table id: %u, ", sig->getTableId());
  fprintf(output, "Error code: %u, ", sig->getErrorCode());
  fprintf(output, "\n");  
  
  return false;
}
