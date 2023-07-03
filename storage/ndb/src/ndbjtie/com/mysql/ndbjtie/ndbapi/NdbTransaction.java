/*
  Copyright (c) 2010, 2023, Oracle and/or its affiliates.

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
/*
 * NdbTransaction.java
 */

package com.mysql.ndbjtie.ndbapi;

import java.nio.ByteBuffer;

import com.mysql.jtie.Wrapper;

public class NdbTransaction extends Wrapper implements NdbTransactionConst
{
    public final native NdbErrorConst/*_const NdbError &_*/ getNdbError() /*_const_*/;
    public final native NdbOperationConst/*_const NdbOperation *_*/ getNdbErrorOperation() /*_const_*/;
    public final native NdbOperationConst/*_const NdbOperation *_*/ getNextCompletedOperation(NdbOperationConst/*_const NdbOperation *_*/ op) /*_const_*/;
    public interface /*_enum_*/ ExecType
    {
        int NoExecTypeDef = -1 /*_::NoExecTypeDef,_*/,
            Prepare = 0 /*_::Prepare,_*/,
            NoCommit = 1 /*_::NoCommit,_*/,
            Commit = 2 /*_::Commit,_*/,
            Rollback = 3 /*_::Rollback_*/;
    }
    public final native NdbOperation/*_NdbOperation *_*/ getNdbOperation(NdbDictionary.TableConst/*_const NdbDictionary.Table *_*/ aTable);
    public final native NdbScanOperation/*_NdbScanOperation *_*/ getNdbScanOperation(NdbDictionary.TableConst/*_const NdbDictionary.Table *_*/ aTable);
    public final native NdbIndexScanOperation/*_NdbIndexScanOperation *_*/ getNdbIndexScanOperation(NdbDictionary.IndexConst/*_const NdbDictionary.Index *_*/ anIndex);
    public final native NdbIndexOperation/*_NdbIndexOperation *_*/ getNdbIndexOperation(NdbDictionary.IndexConst/*_const NdbDictionary.Index *_*/ anIndex);
    public final native int execute(int/*_NdbTransaction.ExecType_*/ execType, int/*_NdbOperation.AbortOption_*/ p0 /*_= NdbOperation.DefaultAbortOption_*/, int force /*_= 0_*/);
    public final native int refresh();
    public final native void close();
    public final native int getGCI(long[]/*_Uint64 *_*/ gciptr);
    public final native long/*_Uint64_*/ getTransactionId();
    public interface /*_enum_*/ CommitStatusType
    {
        int NotStarted = 0 /*__*/,
            Started = 1 /*__*/,
            Committed = 2 /*__*/,
            Aborted = 3 /*__*/,
            NeedAbort = 4 /*__*/;
    }
    public final native int/*_CommitStatusType_*/ commitStatus();
    public final native int getNdbErrorLine();
    public final native NdbOperationConst/*_const NdbOperation *_*/ readTuple(NdbRecordConst/*_const NdbRecord *_*/ key_rec, ByteBuffer/*_const char *_*/ key_row, NdbRecordConst/*_const NdbRecord *_*/ result_rec, ByteBuffer/*_char *_*/ result_row, int/*_NdbOperation.LockMode_*/ lock_mode /*_= NdbOperation.LM_Read_*/, byte[]/*_const unsigned char *_*/ result_mask /*_= 0_*/, NdbOperation.OperationOptionsConst/*_const NdbOperation.OperationOptions *_*/ opts /*_= 0_*/, int/*_Uint32_*/ sizeOfOptions /*_= 0_*/);
    public final native NdbOperationConst/*_const NdbOperation *_*/ insertTuple(NdbRecordConst/*_const NdbRecord *_*/ key_rec, ByteBuffer/*_const char *_*/ key_row, NdbRecordConst/*_const NdbRecord *_*/ attr_rec, ByteBuffer/*_const char *_*/ attr_row, byte[]/*_const unsigned char *_*/ mask /*_= 0_*/, NdbOperation.OperationOptionsConst/*_const NdbOperation.OperationOptions *_*/ opts /*_= 0_*/, int/*_Uint32_*/ sizeOfOptions /*_= 0_*/);
    public final native NdbOperationConst/*_const NdbOperation *_*/ insertTuple(NdbRecordConst/*_const NdbRecord *_*/ combined_rec, ByteBuffer/*_const char *_*/ combined_row, byte[]/*_const unsigned char *_*/ mask /*_= 0_*/, NdbOperation.OperationOptionsConst/*_const NdbOperation.OperationOptions *_*/ opts /*_= 0_*/, int/*_Uint32_*/ sizeOfOptions /*_= 0_*/);
    public final native NdbOperationConst/*_const NdbOperation *_*/ updateTuple(NdbRecordConst/*_const NdbRecord *_*/ key_rec, ByteBuffer/*_const char *_*/ key_row, NdbRecordConst/*_const NdbRecord *_*/ attr_rec, ByteBuffer/*_const char *_*/ attr_row, byte[]/*_const unsigned char *_*/ mask /*_= 0_*/, NdbOperation.OperationOptionsConst/*_const NdbOperation.OperationOptions *_*/ opts /*_= 0_*/, int/*_Uint32_*/ sizeOfOptions /*_= 0_*/);
    public final native NdbOperationConst/*_const NdbOperation *_*/ writeTuple(NdbRecordConst/*_const NdbRecord *_*/ key_rec, ByteBuffer/*_const char *_*/ key_row, NdbRecordConst/*_const NdbRecord *_*/ attr_rec, ByteBuffer/*_const char *_*/ attr_row, byte[]/*_const unsigned char *_*/ mask /*_= 0_*/, NdbOperation.OperationOptionsConst/*_const NdbOperation.OperationOptions *_*/ opts /*_= 0_*/, int/*_Uint32_*/ sizeOfOptions /*_= 0_*/);
    public final native NdbOperationConst/*_const NdbOperation *_*/ deleteTuple(NdbRecordConst/*_const NdbRecord *_*/ key_rec, ByteBuffer/*_const char *_*/ key_row, NdbRecordConst/*_const NdbRecord *_*/ result_rec, ByteBuffer/*_char *_*/ result_row /*_= 0_*/, byte[]/*_const unsigned char *_*/ result_mask /*_= 0_*/, NdbOperation.OperationOptionsConst/*_const NdbOperation.OperationOptions *_*/ opts /*_= 0_*/, int/*_Uint32_*/ sizeOfOptions /*_= 0_*/);
    public final native NdbScanOperation/*_NdbScanOperation *_*/ scanTable(NdbRecordConst/*_const NdbRecord *_*/ result_record, int/*_NdbOperation.LockMode_*/ lock_mode /*_= NdbOperation.LM_Read_*/, byte[]/*_const unsigned char *_*/ result_mask /*_= 0_*/, NdbScanOperation.ScanOptionsConst/*_const NdbScanOperation.ScanOptions *_*/ options /*_= 0_*/, int/*_Uint32_*/ sizeOfOptions /*_= 0_*/);
    public final native NdbIndexScanOperation/*_NdbIndexScanOperation *_*/ scanIndex(NdbRecordConst/*_const NdbRecord *_*/ key_record, NdbRecordConst/*_const NdbRecord *_*/ result_record, int/*_NdbOperation.LockMode_*/ lock_mode /*_= NdbOperation.LM_Read_*/, byte[]/*_const unsigned char *_*/ result_mask /*_= 0_*/, NdbIndexScanOperation.IndexBoundConst/*_const NdbIndexScanOperation.IndexBound *_*/ bound /*_= 0_*/, NdbScanOperation.ScanOptionsConst/*_const NdbScanOperation.ScanOptions *_*/ options /*_= 0_*/, int/*_Uint32_*/ sizeOfOptions /*_= 0_*/);
    public final native NdbOperationConst/*_const NdbOperation *_*/ unlock(NdbLockHandleConst/*_const NdbLockHandle *_*/ lockHandle, int/*_NdbOperation::AbortOption_*/ ao /*_= NdbOperation::DefaultAbortOption_*/);
    public final native int releaseLockHandle(NdbLockHandleConst/*_const NdbLockHandle *_*/ lockHandle);
}
