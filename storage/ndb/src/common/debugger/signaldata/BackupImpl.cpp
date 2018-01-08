/*
   Copyright (C) 2003-2006 MySQL AB
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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

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
  fprintf(out, " backupPtr: %d backupId: %d\n",
	  sig->backupPtr, sig->backupId);
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
  fprintf(out, " backupPtr: %d backupId: %d nodeId: %d errorCode: %d\n",
	  sig->backupPtr, sig->backupId, sig->nodeId, sig->errorCode);
  return true;
}

bool 
printBACKUP_FRAGMENT_CONF(FILE * out, const Uint32 * data, Uint32 l, Uint16 b){
  BackupFragmentConf* sig = (BackupFragmentConf*)data;
  fprintf(out, " backupPtr: %d backupId: %d\n",
	  sig->backupPtr, sig->backupId);
  fprintf(out, " tableId: %d fragmentNo: %d records: %llu bytes: %llu\n",
	  sig->tableId, sig->fragmentNo,
          sig->noOfRecordsLow + (((Uint64)sig->noOfRecordsHigh) << 32),
          sig->noOfBytesLow + (((Uint64)sig->noOfBytesHigh) << 32));
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
