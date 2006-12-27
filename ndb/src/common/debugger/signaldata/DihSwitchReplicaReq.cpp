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


#include <signaldata/DihSwitchReplicaReq.hpp>

bool
printDIH_SWITCH_REPLICA_REQ(FILE * output, 
			    const Uint32 * theData, 
			    Uint32 len, 
			    Uint16 recBlockNo){
  
  DihSwitchReplicaReq * req = (DihSwitchReplicaReq *)&theData[0];
  
  const Uint32 requestInfo = req->requestInfo;

  switch(DihSwitchReplicaReq::getRequestType(requestInfo)){
  case DihSwitchReplicaReq::RemoveNodeAsPrimary:{
    fprintf(output, " RemoveNodeAsPrimary: Node=%d", req->nodeId);
    if(DihSwitchReplicaReq::getAllTables(requestInfo))
      fprintf(output, " All Tables");
    else
      fprintf(output, " TableId=%d", req->tableId);
    
    if(DihSwitchReplicaReq::getDistribute(requestInfo))
      fprintf(output, " Distribute");
    fprintf(output, "\n");
    return true;
  }
  break;
  default:
    fprintf(output, " Unknown request type:\n");
  }
  return false;
}
