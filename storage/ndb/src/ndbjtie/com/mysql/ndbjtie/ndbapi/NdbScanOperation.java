/*
  Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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
/*
 * NdbScanOperation.java
 */

package com.mysql.ndbjtie.ndbapi;

import java.nio.ByteBuffer;

import com.mysql.jtie.Wrapper;
import com.mysql.jtie.ArrayWrapper;

public class NdbScanOperation extends NdbOperation implements NdbScanOperationConst
{
    public final native NdbTransaction/*_NdbTransaction *_*/ getNdbTransaction() /*_const_*/;
    public interface /*_enum_*/ ScanFlag
    {
        int SF_TupScan = 1<<16,
            SF_DiskScan = 2<<16,
            SF_OrderBy = 1<<24,
            SF_Descending = 2<<24,
            SF_ReadRangeNo = 4<<24,
            SF_MultiRange = 8<<24,
            SF_KeyInfo = 1;
    }
    public interface /*_struct_*/ ScanOptionsConst
    {
        long/*_Uint64_*/ optionsPresent();
        public interface /*_enum_*/ Type
        {
            int SO_SCANFLAGS = 0x01,
                SO_PARALLEL = 0x02,
                SO_BATCH = 0x04,
                SO_GETVALUE = 0x08,
                SO_PARTITION_ID = 0x10,
                SO_INTERPRETED = 0x20,
                SO_CUSTOMDATA = 0x40;
        }
        int/*_Uint32_*/ scan_flags();
        int/*_Uint32_*/ parallel();
        int/*_Uint32_*/ batch();
        NdbOperation.GetValueSpecArray/*_NdbOperation.GetValueSpec *_*/ extraGetValues();
        int/*_Uint32_*/ numExtraGetValues();
        int/*_Uint32_*/ partitionId();
        NdbInterpretedCodeConst/*_const NdbInterpretedCode *_*/ interpretedCode();
        // MMM! support <out:BB> or check if needed: ByteBuffer/*_void *_*/ customData();
    }
    static public class /*_struct_*/ ScanOptions extends Wrapper implements ScanOptionsConst
    {
        static public final native int/*_Uint32_*/ size();
        public final native long/*_Uint64_*/ optionsPresent();
        public final native int/*_Uint32_*/ scan_flags();
        public final native int/*_Uint32_*/ parallel();
        public final native int/*_Uint32_*/ batch();
        public final native NdbOperation.GetValueSpecArray/*_NdbOperation.GetValueSpec *_*/ extraGetValues();
        public final native int/*_Uint32_*/ numExtraGetValues();
        public final native int/*_Uint32_*/ partitionId();
        public final native NdbInterpretedCodeConst/*_const NdbInterpretedCode *_*/ interpretedCode();
        // MMM! support <out:BB> or check if needed: public final native ByteBuffer/*_void *_*/ customData();
        public final native void optionsPresent(long/*_Uint64_*/ p0);
        public final native void scan_flags(int/*_Uint32_*/ p0);
        public final native void parallel(int/*_Uint32_*/ p0);
        public final native void batch(int/*_Uint32_*/ p0);
        public final native void extraGetValues(NdbOperation.GetValueSpecArray/*_NdbOperation.GetValueSpec *_*/ p0);
        public final native void numExtraGetValues(int/*_Uint32_*/ p0);
        public final native void partitionId(int/*_Uint32_*/ p0);
        public final native void interpretedCode(NdbInterpretedCodeConst/*_const NdbInterpretedCode *_*/ p0);
        // MMM! support <out:BB> or check if needed: public final native void customData(ByteBuffer/*_void *_*/ p0);
        static public final native ScanOptions create();
        static public final native void delete(ScanOptions p0);
    }
    public /*_virtual_*/ native int readTuples(int/*_LockMode_*/ lock_mode /*_= LM_Read_*/, int/*_Uint32_*/ scan_flags /*_= 0_*/, int/*_Uint32_*/ parallel /*_= 0_*/, int/*_Uint32_*/ batch /*_= 0_*/);
    public final native int nextResult(boolean fetchAllowed /*_= true_*/, boolean forceSend /*_= false_*/);
    // MMM! support <out:char *> or check if needed: public final native int nextResult(const char * * out_row_ptr, boolean fetchAllowed, boolean forceSend);
    public final native int nextResultCopyOut(ByteBuffer/*_char *_*/ buffer, boolean fetchAllowed, boolean forceSend);
    public final native void close(boolean forceSend /*_= false_*/, boolean releaseOp /*_= false_*/);
    public final native NdbOperation/*_NdbOperation *_*/ lockCurrentTuple();
    public final native NdbOperation/*_NdbOperation *_*/ lockCurrentTuple(NdbTransaction/*_NdbTransaction *_*/ lockTrans);
    public final native NdbOperation/*_NdbOperation *_*/ updateCurrentTuple();
    public final native NdbOperation/*_NdbOperation *_*/ updateCurrentTuple(NdbTransaction/*_NdbTransaction *_*/ updateTrans);
    public final native int deleteCurrentTuple();
    public final native int deleteCurrentTuple(NdbTransaction/*_NdbTransaction *_*/ takeOverTransaction);
    public final native NdbOperationConst/*_const NdbOperation *_*/ lockCurrentTuple(NdbTransaction/*_NdbTransaction *_*/ takeOverTrans, NdbRecordConst/*_const NdbRecord *_*/ result_rec, ByteBuffer/*_char *_*/ result_row /*_= 0_*/, byte[]/*_const unsigned char *_*/ result_mask /*_= 0_*/, NdbOperation.OperationOptionsConst/*_const NdbOperation.OperationOptions *_*/ opts /*_= 0_*/, int/*_Uint32_*/ sizeOfOptions /*_= 0_*/);
    public final native NdbOperationConst/*_const NdbOperation *_*/ updateCurrentTuple(NdbTransaction/*_NdbTransaction *_*/ takeOverTrans, NdbRecordConst/*_const NdbRecord *_*/ attr_rec, String/*_const char *_*/ attr_row, byte[]/*_const unsigned char *_*/ mask /*_= 0_*/, NdbOperation.OperationOptionsConst/*_const NdbOperation.OperationOptions *_*/ opts /*_= 0_*/, int/*_Uint32_*/ sizeOfOptions /*_= 0_*/);
    public final native NdbOperationConst/*_const NdbOperation *_*/ deleteCurrentTuple(NdbTransaction/*_NdbTransaction *_*/ takeOverTrans, NdbRecordConst/*_const NdbRecord *_*/ result_rec, ByteBuffer/*_char *_*/ result_row /*_= 0_*/, byte[]/*_const unsigned char *_*/ result_mask /*_= 0_*/, NdbOperation.OperationOptionsConst/*_const NdbOperation.OperationOptions *_*/ opts /*_= 0_*/, int/*_Uint32_*/ sizeOfOptions /*_= 0_*/);
}
