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


#include <RefConvert.hpp>
#include <signaldata/StartRec.hpp>
#include <signaldata/StartFragReq.hpp>

bool
printSTART_REC_REQ(FILE * output, 
		  const Uint32 * theData, 
		  Uint32 len, 
		  Uint16 recBlockNo){
  StartRecReq * sig = (StartRecReq *) theData;

  fprintf(output, " receivingNodeId: %d senderRef: (%d, %d)\n",
	  sig->receivingNodeId, 
	  refToNode(sig->senderRef),
	  refToBlock(sig->senderRef));
  
  fprintf(output, " keepGci: %d lastCompletedGci: %d newestGci: %d\n",
	  sig->keepGci, 
	  sig->lastCompletedGci,
	  sig->newestGci);

  return true;
}

bool
printSTART_REC_CONF(FILE * output, 
		    const Uint32 * theData, 
		    Uint32 len, 
		    Uint16 recBlockNo){
  StartRecConf * sig = (StartRecConf *) theData;

  fprintf(output, " startingNodeId: %d\n",
	  sig->startingNodeId);

  return true;
}

bool 
printSTART_FRAG_REQ(FILE * output, 
		    const Uint32 * theData, 
		    Uint32 len, 
		    Uint16 recBlockNo)
{
  StartFragReq* sig = (StartFragReq*)theData;

  fprintf(output, " table: %d frag: %d lcpId: %d lcpNo: %d #nodes: %d \n",
	  sig->tableId, sig->fragId, sig->lcpId, sig->lcpNo, 
	  sig->noOfLogNodes);

  for(Uint32 i = 0; i<sig->noOfLogNodes; i++)
  {
    fprintf(output, " (node: %d startGci: %d lastGci: %d)",
	    sig->lqhLogNode[i],
	    sig->startGci[i],
	    sig->lastGci[i]);
  }
    
  fprintf(output, "\n");
  return true; 
}
