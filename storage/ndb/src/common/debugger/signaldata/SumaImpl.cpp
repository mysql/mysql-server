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

#include <signaldata/SumaImpl.hpp>

bool
printSUB_CREATE_REQ(FILE * output, const Uint32 * theData, 
		    Uint32 len, Uint16 receiverBlockNo) {
  const SubCreateReq * const sig = (SubCreateReq *)theData;
  fprintf(output, " senderRef: %x\n", sig->senderRef);
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " subscriptionType: %x\n", sig->subscriptionType);
  fprintf(output, " tableId: %x\n", sig->tableId);
  return false;
}

bool
printSUB_CREATE_CONF(FILE * output, const Uint32 * theData, 
		     Uint32 len, Uint16 receiverBlockNo) {
  const SubCreateConf * const sig = (SubCreateConf *)theData;
  fprintf(output, " senderData: %x\n", sig->senderData);
  return false;
}

bool
printSUB_CREATE_REF(FILE * output, const Uint32 * theData, 
		    Uint32 len, Uint16 receiverBlockNo) {
  const SubCreateRef * const sig = (SubCreateRef *)theData;
  fprintf(output, " senderData: %x\n", sig->senderData);
  return false;
}

bool
printSUB_REMOVE_REQ(FILE * output, const Uint32 * theData, 
		    Uint32 len, Uint16 receiverBlockNo)
{
  const SubRemoveReq * const sig = (SubRemoveReq *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  return false;
}

bool
printSUB_REMOVE_CONF(FILE * output, const Uint32 * theData, 
		     Uint32 len, Uint16 receiverBlockNo)
{
  const SubRemoveConf * const sig = (SubRemoveConf *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " senderData: %x\n", sig->senderData);
  return false;
}

bool
printSUB_REMOVE_REF(FILE * output, const Uint32 * theData, 
		    Uint32 len, Uint16 receiverBlockNo)
{
  const SubRemoveRef * const sig = (SubRemoveRef *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " errorCode: %x\n", sig->errorCode);
  return false;
}

bool
printSUB_START_REQ(FILE * output, const Uint32 * theData, 
		   Uint32 len, Uint16 receiverBlockNo) {
  const SubStartReq * const sig = (SubStartReq *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " senderData: %x\n", sig->senderData);
  return false;
}

bool
printSUB_START_REF(FILE * output, const Uint32 * theData, 
		   Uint32 len, Uint16 receiverBlockNo) {
  const SubStartRef * const sig = (SubStartRef *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " startPart: %x\n", sig->part);
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " errorCode: %x\n", sig->errorCode);
  return false;
}

bool
printSUB_START_CONF(FILE * output, const Uint32 * theData, 
		    Uint32 len, Uint16 receiverBlockNo) {
  const SubStartConf * const sig = (SubStartConf *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " startPart: %x\n", sig->part);
  fprintf(output, " senderData: %x\n", sig->senderData);
  return false;
}

bool
printSUB_STOP_REQ(FILE * output, const Uint32 * theData, 
		   Uint32 len, Uint16 receiverBlockNo) {
  const SubStopReq * const sig = (SubStopReq *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " senderData: %x\n", sig->senderData);
  return false;
}

bool
printSUB_STOP_REF(FILE * output, const Uint32 * theData, 
		   Uint32 len, Uint16 receiverBlockNo) {
  const SubStopRef * const sig = (SubStopRef *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " errorCode: %x\n", sig->errorCode);
  return false;
}

bool
printSUB_STOP_CONF(FILE * output, const Uint32 * theData, 
		    Uint32 len, Uint16 receiverBlockNo) {
  const SubStopConf * const sig = (SubStopConf *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " senderData: %x\n", sig->senderData);
  return false;
}

bool
printSUB_SYNC_REQ(FILE * output, const Uint32 * theData, 
		  Uint32 len, Uint16 receiverBlockNo) {
  const SubSyncReq * const sig = (SubSyncReq *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " syncPart: %x\n", sig->part);
  return false;
}

bool
printSUB_SYNC_REF(FILE * output, const Uint32 * theData, 
		  Uint32 len, Uint16 receiverBlockNo) {
  const SubSyncRef * const sig = (SubSyncRef *)theData;
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " errorCode: %x\n", sig->errorCode);
  return false;
}

bool
printSUB_SYNC_CONF(FILE * output, const Uint32 * theData, 
		   Uint32 len, Uint16 receiverBlockNo) {
  const SubSyncConf * const sig = (SubSyncConf *)theData;
  fprintf(output, " senderData: %x\n", sig->senderData);
  return false;
}

bool
printSUB_META_DATA(FILE * output, const Uint32 * theData, 
		   Uint32 len, Uint16 receiverBlockNo) {
  const SubMetaData * const sig = (SubMetaData *)theData;
  fprintf(output, " gci: %x\n", sig->gci);
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " tableId: %x\n", sig->tableId);
  return false;
}

bool
printSUB_TABLE_DATA(FILE * output, const Uint32 * theData, 
		    Uint32 len, Uint16 receiverBlockNo) {
  const SubTableData * const sig = (SubTableData *)theData;
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " gci: %x\n", sig->gci);
  fprintf(output, " tableId: %x\n", sig->tableId);
  fprintf(output, " operation: %x\n", sig->operation);
  return false;
}

bool
printSUB_SYNC_CONTINUE_REQ(FILE * output, const Uint32 * theData, 
			   Uint32 len, Uint16 receiverBlockNo) {
  const SubSyncContinueReq * const sig = (SubSyncContinueReq *)theData;
  fprintf(output, " subscriberData: %x\n", sig->subscriberData);
  fprintf(output, " noOfRowsSent: %x\n", sig->noOfRowsSent);
  return false;
}

bool
printSUB_SYNC_CONTINUE_REF(FILE * output, const Uint32 * theData, 
			   Uint32 len, Uint16 receiverBlockNo) {
  const SubSyncContinueRef * const sig = (SubSyncContinueRef *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  return false;
}

bool
printSUB_SYNC_CONTINUE_CONF(FILE * output, const Uint32 * theData, 
			    Uint32 len, Uint16 receiverBlockNo) {
  const SubSyncContinueConf * const sig = (SubSyncContinueConf *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  return false;
}

bool
printSUB_GCP_COMPLETE_REP(FILE * output, const Uint32 * theData, 
			  Uint32 len, Uint16 receiverBlockNo) {
  const SubGcpCompleteRep * const sig = (SubGcpCompleteRep *)theData;
  fprintf(output, " gci: %x\n", sig->gci);
  return false;
}

