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



#include <BlockNumbers.h>
#include <signaldata/ScanTab.hpp>
#include <signaldata/ScanFrag.hpp>

bool
printSCANTABREQ(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo){
  
  const ScanTabReq * const sig = (ScanTabReq *) theData;
  
  const UintR requestInfo = sig->requestInfo;

  fprintf(output, " apiConnectPtr: H\'%.8x", 
	  sig->apiConnectPtr);
  fprintf(output, " requestInfo: H\'%.8x:\n",  requestInfo);
  fprintf(output, "  Parallellism: %u, Batch: %u LockMode: %u, Keyinfo: %u Holdlock: %u, RangeScan: %u\n",
	  sig->getParallelism(requestInfo), 
	  sig->getScanBatch(requestInfo), 
	  sig->getLockMode(requestInfo), 
	  sig->getHoldLockFlag(requestInfo), 
	  sig->getRangeScanFlag(requestInfo),
	  sig->getKeyinfoFlag(requestInfo));
  
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

bool
printSCANTABCONF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo){
  
  const ScanTabConf * const sig = (ScanTabConf *) theData;
  
  const UintR requestInfo = sig->requestInfo;

  fprintf(output, " apiConnectPtr: H\'%.8x\n", 
	  sig->apiConnectPtr);
  fprintf(output, " transId(1, 2): (H\'%.8x, H\'%.8x)\n",
	  sig->transId1, sig->transId2);

  fprintf(output, " requestInfo: Eod: %d OpCount: %d\n", 
	  (requestInfo & ScanTabConf::EndOfData == ScanTabConf::EndOfData),
	  (requestInfo & (~ScanTabConf::EndOfData)));
  size_t op_count= requestInfo & (~ScanTabConf::EndOfData);
  if(op_count){
    fprintf(output, " Operation(s) [api tc rows len]:\n");
    ScanTabConf::OpData * op = (ScanTabConf::OpData*)
      (theData + ScanTabConf::SignalLength);
    for(size_t i = 0; i<op_count; i++){
      if(op->info != ScanTabConf::EndOfData)
	fprintf(output, " [0x%x 0x%x %d %d]",
		op->apiPtrI, op->tcPtrI,
		ScanTabConf::getRows(op->info),
		ScanTabConf::getLength(op->info));
      else
	fprintf(output, " [0x%x 0x%x eod]",
		op->apiPtrI, op->tcPtrI);
      
      op++;
    }
    fprintf(output, "\n");
  }
  return false;
}

bool
printSCANTABREF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo){
  
  const ScanTabRef * const sig = (ScanTabRef *) theData;
  
  fprintf(output, " apiConnectPtr: H\'%.8x\n", 
	  sig->apiConnectPtr);

  fprintf(output, " transId(1, 2): (H\'%.8x, H\'%.8x)\n",
	  sig->transId1, sig->transId2);
  
  fprintf(output, " Errorcode: %u\n", sig->errorCode);
  
  fprintf(output, " closeNeeded: %u\n", sig->closeNeeded);
  return false;
}


bool
printSCANFRAGNEXTREQ(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo){
  const ScanFragNextReq * const sig = (ScanFragNextReq *) theData;
  
  fprintf(output, " senderData: H\'%.8x\n", 
	  sig->senderData);
  
  fprintf(output, " transId(1, 2): (H\'%.8x, H\'%.8x)\n",
	  sig->transId1, sig->transId2);
  
  fprintf(output, " Close scan: %u\n", sig->closeFlag);

  return false;
}

bool
printSCANNEXTREQ(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo){

  if(receiverBlockNo == DBTC){
    const ScanNextReq * const sig = (ScanNextReq *) theData;
    
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

