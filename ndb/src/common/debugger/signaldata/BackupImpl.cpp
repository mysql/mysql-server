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

#include <trigger_definitions.h>
#include <signaldata/BackupImpl.hpp>

bool 
printDEFINE_BACKUP_REQ(FILE * out, const Uint32 * data, Uint32 len, Uint16 bno){
  DefineBackupReq* sig = (DefineBackupReq*)data;
  fprintf(out, " backupPtr: %d backupId: %d clientRef: %d clientData: %d\n",
	  sig->backupPtr, sig->backupId, sig->clientRef, sig->clientData);
  fprintf(out, " backupKey: [ %08x%08x ] DataLength: %d\n",
	  sig->backupKey[0], sig->backupKey[1], sig->backupDataLen);
  char buf[_NDB_NODE_BITMASK_SIZE * 8 + 1];
  fprintf(out, " Nodes: %s\n", sig->nodes.getText(buf));
  return true;
}

bool 
printDEFINE_BACKUP_REF(FILE * out, const Uint32 * data, Uint32 len, Uint16 bno){
  DefineBackupRef* sig = (DefineBackupRef*)data;
  fprintf(out, " backupPtr: %d backupId: %d errorCode: %d\n",
	  sig->backupPtr, sig->backupId, sig->errorCode);
  return true;
}

bool 
printDEFINE_BACKUP_CONF(FILE * out, const Uint32 * data, Uint32 l, Uint16 bno){
  DefineBackupConf* sig = (DefineBackupConf*)data;
  fprintf(out, " backupPtr: %d backupId: %d\n",
	  sig->backupPtr, sig->backupId);
  return true;
}

bool 
printSTART_BACKUP_REQ(FILE * out, const Uint32 * data, Uint32 l, Uint16 bno){
  StartBackupReq* sig = (StartBackupReq*)data;
  fprintf(out, " backupPtr: %d backupId: %d signalNo: %d of %d\n",
	  sig->backupPtr, sig->backupId,
	  sig->signalNo + 1, sig->noOfSignals);
  for(Uint32 i = 0; i<sig->noOfTableTriggers; i++)
    fprintf(out, 
	    "   Table: %d Triggers = [ insert: %d update: %d delete: %d ]\n",
	    sig->tableTriggers[i].tableId,
	    sig->tableTriggers[i].triggerIds[TriggerEvent::TE_INSERT],
	    sig->tableTriggers[i].triggerIds[TriggerEvent::TE_UPDATE],
	    sig->tableTriggers[i].triggerIds[TriggerEvent::TE_DELETE]);
  return true;
}

bool 
printSTART_BACKUP_REF(FILE * out, const Uint32 * data, Uint32 len, Uint16 bno){
  StartBackupRef* sig = (StartBackupRef*)data;
  fprintf(out, " backupPtr: %d backupId: %d errorCode: %d\n",
	  sig->backupPtr, sig->backupId, sig->errorCode);
  return true;
}

bool 
printSTART_BACKUP_CONF(FILE * out, const Uint32 * data, Uint32 l, Uint16 bno){
  StartBackupConf* sig = (StartBackupConf*)data;
  fprintf(out, " backupPtr: %d backupId: %d\n",
	  sig->backupPtr, sig->backupId);
  return true;
}

bool 
printBACKUP_FRAGMENT_REQ(FILE * out, const Uint32 * data, Uint32 l, Uint16 bno){
  BackupFragmentReq* sig = (BackupFragmentReq*)data;
  fprintf(out, " backupPtr: %d backupId: %d\n",
	  sig->backupPtr, sig->backupId);
  fprintf(out, " tableId: %d fragmentNo: %d (count = %d)\n",
	  sig->tableId, sig->fragmentNo, sig->count);
  return true;
}

bool 
printBACKUP_FRAGMENT_REF(FILE * out, const Uint32 * data, Uint32 l, Uint16 bno){
  BackupFragmentRef* sig = (BackupFragmentRef*)data;
  fprintf(out, " backupPtr: %d backupId: %d\n",
	  sig->backupPtr, sig->backupId);
  fprintf(out, " tableId: %d fragmentNo: %d errorCode: %d\n",
	  sig->tableId, sig->fragmentNo, sig->errorCode);
  return true;
}

bool 
printBACKUP_FRAGMENT_CONF(FILE * out, const Uint32 * data, Uint32 l, Uint16 b){
  BackupFragmentConf* sig = (BackupFragmentConf*)data;
  fprintf(out, " backupPtr: %d backupId: %d\n",
	  sig->backupPtr, sig->backupId);
  fprintf(out, " tableId: %d fragmentNo: %d records: %d bytes: %d\n",
	  sig->tableId, sig->fragmentNo, sig->noOfRecords, sig->noOfBytes);
  return true;
}

bool 
printSTOP_BACKUP_REQ(FILE * out, const Uint32 * data, Uint32 l, Uint16 bno){
  StopBackupReq* sig = (StopBackupReq*)data;
  fprintf(out, " backupPtr: %d backupId: %d\n",
	  sig->backupPtr, sig->backupId);
  return true;
}

bool 
printSTOP_BACKUP_REF(FILE * out, const Uint32 * data, Uint32 len, Uint16 bno){
  StopBackupRef* sig = (StopBackupRef*)data;
  fprintf(out, " backupPtr: %d backupId: %d errorCode: %d\n",
	  sig->backupPtr, sig->backupId, sig->errorCode);
  return true;
}

bool 
printSTOP_BACKUP_CONF(FILE * out, const Uint32 * data, Uint32 l, Uint16 bno){
  StopBackupConf* sig = (StopBackupConf*)data;
  fprintf(out, " backupPtr: %d backupId: %d\n",
	  sig->backupPtr, sig->backupId);
  return true;
}

bool 
printBACKUP_STATUS_REQ(FILE *, const Uint32 *, Uint32, Uint16){
  return false;
}

bool 
printBACKUP_STATUS_CONF(FILE *, const Uint32 *, Uint32, Uint16){
  return false;
}
