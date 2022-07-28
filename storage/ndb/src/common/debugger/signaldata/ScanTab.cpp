/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#include <BlockNumbers.h>
#include <signaldata/ScanTab.hpp>
#include <signaldata/ScanFrag.hpp>

bool printSCANTABREQ(FILE *output,
                     const Uint32 *theData,
                     Uint32 len,
                     Uint16 /*receiverBlockNo*/)
{
  if (len < ScanTabReq::StaticLength)
  {
    assert(false);
    return false;
  }

  const ScanTabReq *const sig = (const ScanTabReq *)theData;

  const UintR requestInfo = sig->requestInfo;

  fprintf(output, " apiConnectPtr: H\'%.8x", 
	  sig->apiConnectPtr);
  fprintf(output, " requestInfo: H\'%.8x:\n",  requestInfo);
  fprintf(output, "  Parallellism: %u Batch: %u LockMode: %u Keyinfo: %u Holdlock: %u RangeScan: %u Descending: %u TupScan: %u\n ReadCommitted: %u DistributionKeyFlag: %u NoDisk: %u Spj: %u MultiFrag: %u",
	  sig->getParallelism(requestInfo), 
	  sig->getScanBatch(requestInfo), 
	  sig->getLockMode(requestInfo), 
	  sig->getKeyinfoFlag(requestInfo),
	  sig->getHoldLockFlag(requestInfo), 
	  sig->getRangeScanFlag(requestInfo),
          sig->getDescendingFlag(requestInfo),
          sig->getTupScanFlag(requestInfo),
	  sig->getReadCommittedFlag(requestInfo),
	  sig->getDistributionKeyFlag(requestInfo),
	  sig->getNoDiskFlag(requestInfo),
          sig->getViaSPJFlag(requestInfo),
          sig->getMultiFragFlag(requestInfo));
  
  if(sig->getDistributionKeyFlag(requestInfo))
    fprintf(output, " DKey: %x", sig->distributionKey);
  
  Uint32 keyLen = (sig->attrLenKeyLen >> 16);
  Uint32 attrLen = (sig->attrLenKeyLen & 0xFFFF);
  fprintf(output, " attrLen: %d, keyLen: %d tableId: %d, tableSchemaVer: %d\n",
	  attrLen, keyLen, sig->tableId, sig->tableSchemaVersion);
  
  fprintf(output, " transId(1, 2): (H\'%.8x, H\'%.8x) storedProcId: H\'%.8x\n",
	  sig->transId1, sig->transId2, sig->storedProcId);
  fprintf(output, " batch_byte_size: %d, first_batch_size: %d\n",
          sig->batch_byte_size, sig->first_batch_size);
  return false;
}

bool printSCANTABCONF(FILE *output,
                      const Uint32 *theData,
                      Uint32 len,
                      Uint16 /*receiverBlockNo*/)
{
  if (len < ScanTabConf::SignalLength)
  {
    assert(false);
    return false;
  }

  const ScanTabConf *const sig = (const ScanTabConf *)theData;

  const UintR requestInfo = sig->requestInfo;

  fprintf(output, " apiConnectPtr: H\'%.8x\n", 
	  sig->apiConnectPtr);
  fprintf(output, " transId(1, 2): (H\'%.8x, H\'%.8x)\n",
	  sig->transId1, sig->transId2);

  fprintf(output, " requestInfo: Eod: %d OpCount: %d\n", 
	  (requestInfo & ScanTabConf::EndOfData) == ScanTabConf::EndOfData,
	  (requestInfo & (~ScanTabConf::EndOfData)));
  size_t op_count= requestInfo & (~ScanTabConf::EndOfData);
  if (op_count)
  {
    if (len == ScanTabConf::SignalLength + 4 * op_count)
    {
      fprintf(output, " Operation(s) [api tc rows len]:\n");
      const ScanTabConf::OpData *op =
          (const ScanTabConf::OpData *)(theData + ScanTabConf::SignalLength);
      for(size_t i = 0; i<op_count; i++)
      {
        fprintf(output, " [0x%x 0x%x %d %d]",
                op->apiPtrI, op->tcPtrI,
                op->rows, op->len);
        op++;
      }
    }
    else if (len == ScanTabConf::SignalLength + 3 * op_count)
    {
      fprintf(output, " Operation(s) [api tc rows len]:\n");      
      for(size_t i = 0; i<op_count; i++)
      {
        const ScanTabConf::OpData *op =
            (const ScanTabConf::OpData *)(theData + ScanTabConf::SignalLength +
                                          3 * i);
        fprintf(output, " [0x%x 0x%x %d %d]",
                op->apiPtrI, op->tcPtrI,
                ScanTabConf::getRows(op->rows),
                ScanTabConf::getLength(op->rows));
      }
    }
    else
    {
      // ScanTabConf::OpData stored in section 0 of signal.
      assert(len == ScanTabConf::SignalLength);
      fprintf(output, " Long signal. Cannot print operations.");
    }
    fprintf(output, "\n");
  }
  return false;
}

bool printSCANTABREF(FILE *output,
                     const Uint32 *theData,
                     Uint32 len,
                     Uint16 /*receiverBlockNo*/)
{
  if (len < ScanTabRef::SignalLength)
  {
    assert(false);
    return false;
  }

  const ScanTabRef *const sig = (const ScanTabRef *)theData;

  fprintf(output, " apiConnectPtr: H\'%.8x\n", 
	  sig->apiConnectPtr);

  fprintf(output, " transId(1, 2): (H\'%.8x, H\'%.8x)\n",
	  sig->transId1, sig->transId2);
  
  fprintf(output, " Errorcode: %u\n", sig->errorCode);
  
  fprintf(output, " closeNeeded: %u\n", sig->closeNeeded);
  return false;
}

bool printSCANFRAGNEXTREQ(FILE *output,
                          const Uint32 *theData,
                          Uint32 len,
                          Uint16 /*receiverBlockNo*/)
{
  if (len < ScanFragNextReq::SignalLength)
  {
    assert(false);
    return false;
  }

  const ScanFragNextReq *const sig = (const ScanFragNextReq *)theData;

  fprintf(output, " senderData: H\'%.8x\n", 
	  sig->senderData);
  
  fprintf(output, " transId(1, 2): (H\'%.8x, H\'%.8x)\n",
	  sig->transId1, sig->transId2);
  
  fprintf(output, " requestInfo: 0x%.8x\n", sig->requestInfo);

  fprintf(output, " batch_size_rows: %u\n", sig->batch_size_rows);
  fprintf(output, " batch_size_bytes: %u\n", sig->batch_size_bytes);

  return false;
}

bool
printSCANNEXTREQ(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo){

  if(receiverBlockNo == DBTC){
    const ScanNextReq *const sig = (const ScanNextReq *)theData;

    fprintf(output, " apiConnectPtr: H\'%.8x\n", 
	    sig->apiConnectPtr);
    
    fprintf(output, " transId(1, 2): (H\'%.8x, H\'%.8x) ",
	    sig->transId1, sig->transId2);
    
    fprintf(output, " Stop this scan: %u\n", sig->stopScan);

    const Uint32 * ops = theData + ScanNextReq::SignalLength;
    if(len > ScanNextReq::SignalLength){
      fprintf(output, " tcFragPtr(s): ");
      for(size_t i = ScanNextReq::SignalLength; i<len; i++)
	fprintf(output, " 0x%x", * ops++);
      fprintf(output, "\n");
    }
  }
  if (receiverBlockNo == DBLQH){
    return printSCANFRAGNEXTREQ(output, theData, len, receiverBlockNo);
  }
  return false;
}

