/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

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

bool printDEFINE_BACKUP_REQ(FILE *out, const Uint32 *data, Uint32 len,
                            Uint16 /*bno*/) {
  if (len < DefineBackupReq::SignalLength_v1) {
    assert(false);
    return false;
  }

  const DefineBackupReq *sig = (const DefineBackupReq *)data;
  fprintf(out, " backupPtr: %d backupId: %d clientRef: %d clientData: %d\n",
          sig->backupPtr, sig->backupId, sig->clientRef, sig->clientData);
  fprintf(out, " backupKey: [ %08x%08x ] DataLength: %d\n", sig->backupKey[0],
          sig->backupKey[1], sig->backupDataLen);
  return true;
}

bool printDEFINE_BACKUP_REF(FILE *out, const Uint32 *data, Uint32 len,
                            Uint16 /*bno*/) {
  if (len < DefineBackupRef::SignalLength) {
    assert(false);
    return false;
  }

  const DefineBackupRef *sig = (const DefineBackupRef *)data;
  fprintf(out, " backupPtr: %d backupId: %d errorCode: %d\n", sig->backupPtr,
          sig->backupId, sig->errorCode);
  return true;
}

bool printDEFINE_BACKUP_CONF(FILE *out, const Uint32 *data, Uint32 l,
                             Uint16 /*bno*/) {
  if (l < DefineBackupConf::SignalLength) {
    assert(false);
    return false;
  }
  const DefineBackupConf *sig = (const DefineBackupConf *)data;
  fprintf(out, " backupPtr: %d backupId: %d\n", sig->backupPtr, sig->backupId);
  return true;
}

bool printSTART_BACKUP_REQ(FILE *out, const Uint32 *data, Uint32 l,
                           Uint16 /*bno*/) {
  if (l < StartBackupReq::SignalLength) {
    assert(false);
    return false;
  }
  const StartBackupReq *sig = (const StartBackupReq *)data;
  fprintf(out, " backupPtr: %d backupId: %d\n", sig->backupPtr, sig->backupId);
  return true;
}

bool printSTART_BACKUP_REF(FILE *out, const Uint32 *data, Uint32 len,
                           Uint16 /*bno*/) {
  if (len < StartBackupRef::SignalLength) {
    assert(false);
    return false;
  }

  const StartBackupRef *sig = (const StartBackupRef *)data;
  fprintf(out, " backupPtr: %d backupId: %d errorCode: %d\n", sig->backupPtr,
          sig->backupId, sig->errorCode);
  return true;
}

bool printSTART_BACKUP_CONF(FILE *out, const Uint32 *data, Uint32 l,
                            Uint16 /*bno*/) {
  if (l < StartBackupConf::SignalLength) {
    assert(false);
    return false;
  }

  const StartBackupConf *sig = (const StartBackupConf *)data;
  fprintf(out, " backupPtr: %d backupId: %d\n", sig->backupPtr, sig->backupId);
  return true;
}

bool printBACKUP_FRAGMENT_REQ(FILE *out, const Uint32 *data, Uint32 l,
                              Uint16 /*bno*/) {
  if (l < BackupFragmentReq::SignalLength) {
    assert(false);
    return false;
  }

  const BackupFragmentReq *sig = (const BackupFragmentReq *)data;
  fprintf(out, " backupPtr: %d backupId: %d\n", sig->backupPtr, sig->backupId);
  fprintf(out, " tableId: %d fragmentNo: %d (count = %d)\n", sig->tableId,
          sig->fragmentNo, sig->count);
  return true;
}

bool printBACKUP_FRAGMENT_REF(FILE *out, const Uint32 *data, Uint32 l,
                              Uint16 /*bno*/) {
  if (l < BackupFragmentRef::SignalLength) {
    assert(false);
    return false;
  }

  const BackupFragmentRef *sig = (const BackupFragmentRef *)data;
  fprintf(out, " backupPtr: %d backupId: %d nodeId: %d errorCode: %d\n",
          sig->backupPtr, sig->backupId, sig->nodeId, sig->errorCode);
  return true;
}

bool printBACKUP_FRAGMENT_CONF(FILE *out, const Uint32 *data, Uint32 l,
                               Uint16 /*b*/) {
  if (l < BackupFragmentConf::SignalLength) {
    assert(false);
    return false;
  }

  const BackupFragmentConf *sig = (const BackupFragmentConf *)data;
  fprintf(out, " backupPtr: %d backupId: %d\n", sig->backupPtr, sig->backupId);
  fprintf(out, " tableId: %d fragmentNo: %d records: %llu bytes: %llu\n",
          sig->tableId, sig->fragmentNo,
          sig->noOfRecordsLow + (((Uint64)sig->noOfRecordsHigh) << 32),
          sig->noOfBytesLow + (((Uint64)sig->noOfBytesHigh) << 32));
  return true;
}

bool printSTOP_BACKUP_REQ(FILE *out, const Uint32 *data, Uint32 l,
                          Uint16 /*bno*/) {
  if (l < StopBackupReq::SignalLength) {
    assert(false);
    return false;
  }

  const StopBackupReq *sig = (const StopBackupReq *)data;
  fprintf(out, " backupPtr: %d backupId: %d\n", sig->backupPtr, sig->backupId);
  return true;
}

bool printSTOP_BACKUP_REF(FILE *out, const Uint32 *data, Uint32 len,
                          Uint16 /*bno*/) {
  if (len < StopBackupRef::SignalLength) {
    assert(false);
    return false;
  }

  const StopBackupRef *sig = (const StopBackupRef *)data;
  fprintf(out, " backupPtr: %d backupId: %d errorCode: %d\n", sig->backupPtr,
          sig->backupId, sig->errorCode);
  return true;
}

bool printSTOP_BACKUP_CONF(FILE *out, const Uint32 *data, Uint32 l,
                           Uint16 /*bno*/) {
  if (l < StopBackupConf::SignalLength) {
    assert(false);
    return false;
  }

  const StopBackupConf *sig = (const StopBackupConf *)data;
  fprintf(out, " backupPtr: %d backupId: %d\n", sig->backupPtr, sig->backupId);
  return true;
}

bool printBACKUP_STATUS_REQ(FILE *, const Uint32 *, Uint32, Uint16) {
  return false;
}

bool printBACKUP_STATUS_CONF(FILE *, const Uint32 *, Uint32, Uint16) {
  return false;
}
