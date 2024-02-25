/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.
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

#include <signaldata/BackupSignalData.hpp>

bool printBACKUP_REQ(FILE* output,
                     const Uint32* theData,
                     Uint32 len,
                     Uint16 /*bno*/)
{
  //If no inputBackupId is provided, len will be SignalLength-1.
  if (len < BackupReq::SignalLength-1)
  {
    assert(false);
    return false;
  }

  const BackupReq* sig = (const BackupReq*)theData;
  fprintf(output, " senderData: %d DataLength: %d flags: %d\n", 
	  sig->senderData,
	  sig->backupDataLen,
	  sig->flags);
  return true;
}

bool printBACKUP_DATA(FILE* output,
                      const Uint32* theData,
                      Uint32 len,
                      Uint16 /*bno*/)
{
  if (len < BackupData::SignalLength)
  {
    assert(false);
    return false;
  }

  const BackupData* sig = (const BackupData*)theData;
  if(sig->requestType == BackupData::ClientToMaster){
    fprintf(output, " ClientToMaster: senderData: %d backupId: %d\n",
	    sig->senderData, sig->backupId);
  } else if(sig->requestType == BackupData::MasterToSlave){
    fprintf(output, " MasterToSlave: backupPtr: %d backupId: %d\n",
	    sig->backupPtr, sig->backupId);
  }
  return false;
}

bool printBACKUP_REF(FILE* output,
                     const Uint32* theData,
                     Uint32 len,
                     Uint16 /*bno*/)
{
  if (len < BackupRef::SignalLength)
  {
    assert(false);
    return false;
  }

  const BackupRef* sig = (const BackupRef*)theData;
  fprintf(output, " senderData: %d errorCode: %d masterRef: %d\n",
	  sig->senderData,
	  sig->errorCode,
	  sig->masterRef);
  return true;
}

bool printBACKUP_CONF(FILE* output,
                      const Uint32* theData,
                      Uint32 len,
                      Uint16 /*bno*/)
{
  if (len < BackupConf::SignalLength)
  {
    assert(false);
    return false;
  }

  const BackupConf* sig = (const BackupConf*)theData;
  fprintf(output, " senderData: %d backupId: %d\n",
	  sig->senderData,
	  sig->backupId);
  return true;
}

bool printBACKUP_ABORT_REP(FILE* out,
                           const Uint32* data,
                           Uint32 len,
                           Uint16 /*bno*/)
{
  if (len < BackupAbortRep::SignalLength)
  {
    assert(false);
    return false;
  }

  const BackupAbortRep* sig = (const BackupAbortRep*)data;
  fprintf(out, " senderData: %d backupId: %d reason: %d\n",
	  sig->senderData,
	  sig->backupId,
	  sig->reason);
  return true;
}

bool printBACKUP_COMPLETE_REP(FILE* out,
                              const Uint32* data,
                              Uint32 len,
                              Uint16 /*b*/)
{
  if (len < BackupCompleteRep::SignalLength)
  {
    assert(false);
    return false;
  }

  const BackupCompleteRep* sig = (const BackupCompleteRep*)data;
  fprintf(out, " senderData: %d backupId: %d records: %llu bytes: %llu\n",
	  sig->senderData,
	  sig->backupId,
	  sig->noOfRecordsLow + (((Uint64)sig->noOfRecordsHigh) << 32),
          sig->noOfBytesLow + (((Uint64)sig->noOfBytesHigh) << 32));
  return true;
}

bool 
printBACKUP_NF_COMPLETE_REP(FILE*, const Uint32*, Uint32, Uint16){
  return false;
}

bool printABORT_BACKUP_ORD(FILE* out,
                           const Uint32* data,
                           Uint32 len,
                           Uint16 /*b*/)
{
  if (len < AbortBackupOrd::SignalLength)
  {
    assert(false);
    return false;
  }

  const AbortBackupOrd* sig = (const AbortBackupOrd*)data;

  AbortBackupOrd::RequestType rt =(AbortBackupOrd::RequestType)sig->requestType;
  switch(rt){
  case AbortBackupOrd::ClientAbort:
    fprintf(out, " ClientAbort: senderData: %d backupId: %d\n",
	    sig->senderData, sig->backupId);
    return true;
    break;
  case AbortBackupOrd::BackupComplete:
    fprintf(out, " BackupComplete: backupPtr: %d backupId: %d\n",
	    sig->backupPtr, sig->backupId);
    return true;
  case AbortBackupOrd::BackupFailure:
    fprintf(out, " BackupFailure: backupPtr: %d backupId: %d\n",
	    sig->backupPtr, sig->backupId);
    return true;
  case AbortBackupOrd::LogBufferFull:
    fprintf(out, " LogBufferFull: backupPtr: %d backupId: %d\n",
	    sig->backupPtr, sig->backupId);
    return true;
    break;
  case AbortBackupOrd::FileOrScanError:
    fprintf(out, " FileOrScanError: backupPtr: %d backupId: %d\n",
	    sig->backupPtr, sig->backupId);
    return true;
    break;
  case AbortBackupOrd::BackupFailureDueToNodeFail:
    fprintf(out, " BackupFailureDueToNodeFail: backupPtr: %d backupId: %d\n",
	    sig->backupPtr, sig->backupId);
    return true;
    break;
  case AbortBackupOrd::OkToClean:
    fprintf(out, " OkToClean: backupPtr: %d backupId: %d\n",
	    sig->backupPtr, sig->backupId);
    return true;
    break;
  case AbortBackupOrd::AbortScan:
  case AbortBackupOrd::IncompatibleVersions:
    return false;
  }
  return false;
}
