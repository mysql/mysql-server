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

#include <signaldata/TcIndx.hpp>
#include <signaldata/TcKeyReq.hpp>
#include <BlockNumbers.h>


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

