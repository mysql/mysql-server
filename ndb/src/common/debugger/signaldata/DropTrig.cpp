/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <signaldata/DropTrig.hpp>

bool printDROP_TRIG_REQ(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo)
{
  const DropTrigReq * const sig = (DropTrigReq *) theData;

  //char triggerName[MAX_TAB_NAME_SIZE];
  //char triggerType[32];
  //char triggerActionTime[32];
  //char triggerEvent[32];

  //sig->getTriggerName((char *) &triggerName);
  //switch(sig->getTriggerType()) {
  //case(TriggerType::SECONDARY_INDEX): 
    //strcpy(triggerType, "SECONDARY_INDEX");
    //break;
  //case(TriggerType::SUBSCRIPTION):
    //strcpy(triggerType, "SUBSCRIPTION");
    //break;
  //default:
    //strcpy(triggerType, "UNSUPPORTED");
  //}
  //strcpy(triggerActionTime, 
         //(sig->getTriggerActionTime() == TriggerActionTime::BEFORE)?
         //"BEFORE":"AFTER");
  //switch(sig->getTriggerEvent()) {
  //case (TriggerEvent::TE_INSERT):
    //strcpy(triggerEvent, "INSERT");
    //break;
  //case(TriggerEvent::TE_DELETE):
    //strcpy(triggerEvent, "DELETE");
    //break;
  //case(TriggerEvent::TE_UPDATE):
    //strcpy(triggerEvent, "UPDATE");
    //break;
  //}

  fprintf(output, "User: %u, ", sig->getUserRef());
  //fprintf(output, "Trigger name: \"%s\"\n", triggerName);
  //fprintf(output, "Type: %s, ", triggerType);
  //fprintf(output, "Action: %s, ", triggerActionTime);
  //fprintf(output, "Event: %s, ", triggerEvent);
  fprintf(output, "Trigger id: %u, ", sig->getTriggerId());
  fprintf(output, "Table id: %u, ", sig->getTableId());
  fprintf(output, "\n");  

  return false;
}

bool printDROP_TRIG_CONF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo)
{
  const DropTrigConf * const sig = (DropTrigConf *) theData;

  fprintf(output, "User: %u, ", sig->getUserRef());
  fprintf(output, "Trigger id: %u, ", sig->getTriggerId());
  fprintf(output, "Table id: %u, ", sig->getTableId());
  fprintf(output, "\n");  

  return false;
}

bool printDROP_TRIG_REF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo)
{
  const DropTrigRef * const sig = (DropTrigRef *) theData;

  fprintf(output, "User: %u, ", sig->getUserRef());
  fprintf(output, "Trigger id: %u, ", sig->getTriggerId());
  fprintf(output, "Table id: %u, ", sig->getTableId());
  fprintf(output, "Error code: %u, ", sig->getErrorCode());
  fprintf(output, "\n");  
  
  return false;
}
