/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */



#include <signaldata/TcKeyReq.hpp>

bool
printTCKEYREQ(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo){
  
  const TcKeyReq * const sig = (TcKeyReq *) theData;
  
  UintR requestInfo = sig->requestInfo;

  fprintf(output, " apiConnectPtr: H\'%.8x, apiOperationPtr: H\'%.8x\n", 
	  sig->apiConnectPtr, sig->apiOperationPtr);
  fprintf(output, " Operation: %s, Flags: ", 
	  sig->getOperationType(requestInfo) == ZREAD    ? "Read" :
	  sig->getOperationType(requestInfo) == ZREAD_EX ? "Read-Ex" :
	  sig->getOperationType(requestInfo) == ZUPDATE  ? "Update" :
	  sig->getOperationType(requestInfo) == ZINSERT  ? "Insert" :
	  sig->getOperationType(requestInfo) == ZDELETE  ? "Delete" :
	  sig->getOperationType(requestInfo) == ZWRITE   ? "Write" :
	  "Unknown");
  {
    if(sig->getDirtyFlag(requestInfo)){
      fprintf(output, "Dirty ");
    }    
    if(sig->getStartFlag(requestInfo)){
      fprintf(output, "Start ");
    }    
    if(sig->getExecuteFlag(requestInfo)){
      fprintf(output, "Execute ");
    }
    if(sig->getCommitFlag(requestInfo)){
      fprintf(output, "Commit ");
    }
    if (sig->getExecutingTrigger(requestInfo)) {
      fprintf(output, "Trigger ");
    }

    if (sig->getNoDiskFlag(requestInfo)) {
      fprintf(output, "NoDisk ");
    }
    
    UintR TcommitType = sig->getAbortOption(requestInfo);
    if (TcommitType == TcKeyReq::AbortOnError) {
      fprintf(output, "AbortOnError ");
    } else if (TcommitType == TcKeyReq::IgnoreError) {
      fprintf(output, "IgnoreError ");
    }//if

    if(sig->getSimpleFlag(requestInfo)){
      fprintf(output, "Simple ");
    }   
    if(sig->getScanIndFlag(requestInfo)){
      fprintf(output, "ScanInd ");
    }   
    if(sig->getInterpretedFlag(requestInfo)){
      fprintf(output, "Interpreted ");
    }
    if(sig->getDistributionKeyFlag(sig->requestInfo)){
      fprintf(output, " d-key");
    }
    fprintf(output, "\n");
  }
  
  const int keyLen     = sig->getKeyLength(requestInfo);
  const int attrInThis = sig->getAIInTcKeyReq(requestInfo);
  const int attrLen = sig->getAttrinfoLen(sig->attrLen);
  const int apiVer = sig->getAPIVersion(sig->attrLen);
  fprintf(output, 
	  " keyLen: %d, attrLen: %d, AI in this: %d, tableId: %d, "
	  "tableSchemaVer: %d, API Ver: %d\n",
	  keyLen, attrLen, attrInThis, 
	  sig->tableId, sig->tableSchemaVersion, apiVer);
    
  fprintf(output, " transId(1, 2): (H\'%.8x, H\'%.8x)\n -- Variable Data --\n", 
	  sig->transId1, sig->transId2);
  
  if (len >= TcKeyReq::StaticLength) {
    Uint32 restLen = (len - TcKeyReq::StaticLength);
    const Uint32 * rest = &sig->scanInfo;
    while(restLen >= 7){
      fprintf(output, 
              " H\'%.8x H\'%.8x H\'%.8x H\'%.8x H\'%.8x H\'%.8x H\'%.8x\n",
              rest[0], rest[1], rest[2], rest[3], 
              rest[4], rest[5], rest[6]);
      restLen -= 7;
      rest += 7;
    }
    if(restLen > 0){
      for(Uint32 i = 0; i<restLen; i++)
        fprintf(output, " H\'%.8x", rest[i]);
      fprintf(output, "\n");
    }
  } else {
    fprintf(output, "*** invalid len %u ***\n", len);
  }
  return true;
}

