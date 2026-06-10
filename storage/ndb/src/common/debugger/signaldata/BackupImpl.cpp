/*
   Copyright (c) 2003, 2026, Oracle and/or its affiliates.

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

  const auto *sig = (const DefineBackupReq *)data;
  fprintf(out,
          " backupPtr: %u backupId: %u clientRef: %u clientData: %u "
          "senderRef: %u senderData: %u\n",
          sig->backupPtr, sig->backupId, sig->clientRef, sig->clientData,
          sig->senderRef, sig->senderData);
  fprintf(out,
          " backupKey: [ %08x%08x ] DataLength: %u flags: %u masterRef: %u\n",
          sig->backupKey[0], sig->backupKey[1], sig->backupDataLen, sig->flags,
          sig->masterRef);
  if (len == sig->SignalLength_v1) {
    char buf[NdbNodeBitmask48::TextLength + 1];
    fprintf(out, " nodes: %s\n", sig->nodes.getText(buf));
  }
  return true;
}

bool printDEFINE_BACKUP_REF(FILE *out, const Uint32 *data, Uint32 len,
                            Uint16 /*bno*/) {
  if (len < DefineBackupRef::SignalLength) {
    assert(false);
    return false;
  }

  const auto *sig = (const DefineBackupRef *)data;
  fprintf(out, " backupPtr: %u backupId: %u errorCode: %u nodeId: %u\n",
          sig->backupPtr, sig->backupId, sig->errorCode, sig->nodeId);
  return true;
}

bool printDEFINE_BACKUP_CONF(FILE *out, const Uint32 *data, Uint32 l,
                             Uint16 /*bno*/) {
  if (l < DefineBackupConf::SignalLength) {
    assert(false);
    return false;
  }
  const auto *sig = (const DefineBackupConf *)data;
  fprintf(out, " backupPtr: %u backupId: %u\n", sig->backupPtr, sig->backupId);
  return true;
}

bool printSTART_BACKUP_REQ(FILE *out, const Uint32 *data, Uint32 l,
                           Uint16 /*bno*/) {
  if (l < StartBackupReq::SignalLength) {
    assert(false);
    return false;
  }
  const auto *sig = (const StartBackupReq *)data;
  fprintf(out, " backupPtr: %u backupId: %u senderRef: %u senderData: %u\n",
          sig->backupPtr, sig->backupId, sig->senderRef, sig->senderData);
  return true;
}

bool printSTART_BACKUP_REF(FILE *out, const Uint32 *data, Uint32 len,
                           Uint16 /*bno*/) {
  if (len < StartBackupRef::SignalLength) {
    assert(false);
    return false;
  }

  const auto *sig = (const StartBackupRef *)data;
  fprintf(out, " backupPtr: %u backupId: %u errorCode: %u nodeId: %u\n",
          sig->backupPtr, sig->backupId, sig->errorCode, sig->nodeId);
  return true;
}

bool printSTART_BACKUP_CONF(FILE *out, const Uint32 *data, Uint32 l,
                            Uint16 /*bno*/) {
  if (l < StartBackupConf::SignalLength) {
    assert(false);
    return false;
  }

  const auto *sig = (const StartBackupConf *)data;
  fprintf(out, " backupPtr: %u backupId: %u\n", sig->backupPtr, sig->backupId);
  return true;
}

bool printBACKUP_FRAGMENT_REQ(FILE *out, const Uint32 *data, Uint32 l,
                              Uint16 /*bno*/) {
  if (l < BackupFragmentReq::SignalLength) {
    assert(false);
    return false;
  }

  const auto *sig = (const BackupFragmentReq *)data;
  fprintf(out, " backupPtr: %u backupId: %u\n", sig->backupPtr, sig->backupId);
  fprintf(out, " tableId: %u fragmentNo: %u (count = %u)\n", sig->tableId,
          sig->fragmentNo, sig->count);
  fprintf(out, " senderRef: %u senderData: %u\n", sig->senderRef,
          sig->senderData);
  return true;
}

bool printBACKUP_FRAGMENT_REF(FILE *out, const Uint32 *data, Uint32 l,
                              Uint16 /*bno*/) {
  if (l < BackupFragmentRef::SignalLength) {
    assert(false);
    return false;
  }

  const auto *sig = (const BackupFragmentRef *)data;
  fprintf(out, " backupPtr: %u backupId: %u nodeId: %u errorCode: %u\n",
          sig->backupPtr, sig->backupId, sig->nodeId, sig->errorCode);
  fprintf(out, " tableId: %u fragmentNo: %u\n", sig->tableId, sig->fragmentNo);
  return true;
}

bool printBACKUP_FRAGMENT_CONF(FILE *out, const Uint32 *data, Uint32 l,
                               Uint16 /*b*/) {
  if (l < BackupFragmentConf::SignalLength) {
    assert(false);
    return false;
  }

  const auto *sig = (const BackupFragmentConf *)data;
  fprintf(out, " backupPtr: %u backupId: %u\n", sig->backupPtr, sig->backupId);
  fprintf(out, " tableId: %u fragmentNo: %u records: %llu bytes: %llu\n",
          sig->tableId, sig->fragmentNo,
          (Uint64(sig->noOfRecordsHigh) << 32) + sig->noOfRecordsLow,
          (Uint64(sig->noOfBytesHigh) << 32) + sig->noOfBytesLow);
  return true;
}

bool printSTOP_BACKUP_REQ(FILE *out, const Uint32 *data, Uint32 l,
                          Uint16 /*bno*/) {
  if (l < StopBackupReq::SignalLength) {
    assert(false);
    return false;
  }

  const auto *sig = (const StopBackupReq *)data;
  fprintf(out, " backupPtr: %u backupId: %u\n", sig->backupPtr, sig->backupId);
  fprintf(out, " startGCP: %u stopGCP: %u senderRef: %u senderData: %u\n",
          sig->startGCP, sig->stopGCP, sig->senderRef, sig->senderData);
  return true;
}

bool printSTOP_BACKUP_REF(FILE *out, const Uint32 *data, Uint32 len,
                          Uint16 /*bno*/) {
  if (len < StopBackupRef::SignalLength) {
    assert(false);
    return false;
  }

  const auto *sig = (const StopBackupRef *)data;
  fprintf(out, " backupPtr: %u backupId: %u errorCode: %u nodeId: %u\n",
          sig->backupPtr, sig->backupId, sig->errorCode, sig->nodeId);
  return true;
}

bool printSTOP_BACKUP_CONF(FILE *out, const Uint32 *data, Uint32 l,
                           Uint16 /*bno*/) {
  if (l < StopBackupConf::SignalLength) {
    assert(false);
    return false;
  }

  const auto *sig = (const StopBackupConf *)data;
  fprintf(out, " backupPtr: %u backupId: %u\n", sig->backupPtr, sig->backupId);
  fprintf(out, " noOfLogBytes: %llu noOfLogRecords: %llu\n",
          (Uint64(sig->noOfLogBytesHigh) << 32) + sig->noOfLogBytesLow,
          (Uint64(sig->noOfLogRecordsHigh) << 32) + sig->noOfLogRecordsLow);
  return true;
}

bool printBACKUP_STATUS_REQ(FILE *, const Uint32 *, Uint32, Uint16) {
  return false;
}

bool printBACKUP_STATUS_CONF(FILE *, const Uint32 *, Uint32, Uint16) {
  return false;
}
