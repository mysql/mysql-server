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

#include <signaldata/TcIndx.hpp>
#include <signaldata/TcKeyReq.hpp>
#include <BlockNumbers.h>

bool
printTCINDXREQ(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo){
  
  const TcIndxReq * const sig = (TcIndxReq *) theData;
  
  UintR requestInfo = sig->requestInfo;
  UintR scanInfo    = sig->scanInfo;

  fprintf(output, " apiConnectPtr: H\'%.8x, senderData: H\'%.8x\n", 
	  sig->apiConnectPtr, sig->senderData);
 
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
    if (TcKeyReq::getExecuteFlag(sig->requestInfo)) {
      fprintf(output, "Execute ");
    }
    if(sig->getCommitFlag(requestInfo)){
      fprintf(output, "Commit, Type = ");
      UintR TcommitType = sig->getCommitType(requestInfo);
      if (TcommitType == TcIndxReq::CommitIfFailFree) {
	fprintf(output, "FailFree ");
      } else if (TcommitType == TcIndxReq::TryCommit) {
        fprintf(output, "TryCommit ");
      } else if (TcommitType == TcIndxReq::CommitAsMuchAsPossible) {
	fprintf(output, "Always ");
      }//if
    }    
    if(sig->getSimpleFlag(requestInfo)){
      fprintf(output, "Simple ");
    }   
    if(sig->getInterpretedFlag(requestInfo)){
      fprintf(output, "Interpreted ");
    }
    if(sig->getDistributionGroupFlag(requestInfo)){
      fprintf(output, "DGroup = %d ", sig->distrGroupHashValue);
    }
    if(sig->getDistributionKeyFlag(sig->requestInfo)){
      fprintf(output, "DKey = %d ", sig->distributionKeySize);
    }
    fprintf(output, "\n");
  }
  
  const int indexLen     = sig->getIndexLength(requestInfo);
  const int attrInThis = sig->getAIInTcIndxReq(requestInfo);
  fprintf(output, 
	  " indexLen: %d, attrLen: %d, AI in this: %d, indexId: %d, "
	  "indexSchemaVer: %d, API Ver: %d\n",
	  indexLen, sig->attrLen, attrInThis, 
	  sig->indexId, sig->indexSchemaVersion, sig->getAPIVersion(scanInfo));
    
  fprintf(output, " transId(1, 2): (H\'%.8x, H\'%.8x)\n -- Variable Data --\n", 
	  sig->transId1, sig->transId2);
  
  Uint32 restLen = (len - 8);
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
  
  return true;
}

bool
printTCINDXCONF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo){
  
  if (receiverBlockNo == API_PACKED) {
    fprintf(output, "Signal data: ");
    Uint32 i = 0;
    while (i < len)
      fprintf(output, "H\'%.8x ", theData[i++]);
    fprintf(output,"\n");
  }
  else {
    const TcIndxConf * const sig = (TcIndxConf *) theData;
    
    fprintf(output, "Signal data: ");
    Uint32 i = 0;
    Uint32 confInfo = sig->confInfo;
    Uint32 noOfOp = TcIndxConf::getNoOfOperations(confInfo);
    while (i < len)
      fprintf(output, "H\'%.8x ", theData[i++]);
    fprintf(output,"\n");
    fprintf(output, "apiConnectPtr: H'%.8x, gci: %u, transId:(H'%.8x, H'%.8x)\n",
	    sig->apiConnectPtr, sig->gci, sig->transId1, sig->transId2);
    
    fprintf(output, "noOfOperations: %u, commitFlag: %s, markerFlag: %s\n", 
	    noOfOp,
	    (TcIndxConf::getCommitFlag(confInfo) == 0)?"false":"true",
	    (TcIndxConf::getMarkerFlag(confInfo) == 0)?"false":"true");
    fprintf(output, "Operations:\n");
    for(i = 0; i < noOfOp; i++) {
      fprintf(output,
	      "apiOperationPtr: H'%.8x, attrInfoLen: %u\n",
	      sig->operations[i].apiOperationPtr,
	      sig->operations[i].attrInfoLen);
    }
  }

  return true;
}

bool
printTCINDXREF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo){
  
//  const TcIndxRef * const sig = (TcIndxRef *) theData;
  
  fprintf(output, "Signal data: ");
  Uint32 i = 0;
  while (i < len)
    fprintf(output, "H\'%.8x ", theData[i++]);
  fprintf(output,"\n");
  
  return true;
}

