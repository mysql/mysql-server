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


#include <signaldata/GCPSave.hpp>
#include <RefConvert.hpp>

bool
printGCPSaveReq(FILE * output, 
		const Uint32 * theData, 
		Uint32 len, 
		Uint16 receiverBlockNo){
  
  GCPSaveReq * sr = (GCPSaveReq*)theData;
  
  fprintf(output, " dihBlockRef = (%d, %d) dihPtr = %d gci = %d\n",
	  refToBlock(sr->dihBlockRef), refToNode(sr->dihBlockRef),
	  sr->dihPtr, sr->gci);
  
  return true;
}

bool
printGCPSaveRef(FILE * output, 
		const Uint32 * theData, 
		Uint32 len, 
		Uint16 receiverBlockNo){
  
  GCPSaveRef * sr = (GCPSaveRef*)theData;
  
  fprintf(output, " nodeId = %d dihPtr = %d gci = %d reason: ",
	  sr->nodeId,
	  sr->dihPtr, sr->gci);
  
  switch(sr->errorCode){
  case GCPSaveRef::NodeShutdownInProgress:
    fprintf(output, "NodeShutdownInProgress\n");
    break;
  case GCPSaveRef::FakedSignalDueToNodeFailure:
    fprintf(output, "FakedSignalDueToNodeFailure\n");
    break;
  default:
    fprintf(output, "Unknown reason: %d\n", sr->errorCode);
    return false;
  }
  
  return true;
}

bool
printGCPSaveConf(FILE * output, 
		 const Uint32 * theData, 
		 Uint32 len, 
		 Uint16 receiverBlockNo){
  
  GCPSaveConf * sr = (GCPSaveConf*)theData;
  
  fprintf(output, " nodeId = %d dihPtr = %d gci = %d\n",
	  sr->nodeId,
	  sr->dihPtr, sr->gci);
  
  return true;
}


