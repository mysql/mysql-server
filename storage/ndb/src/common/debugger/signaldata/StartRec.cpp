/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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


#include <RefConvert.hpp>
#include <signaldata/StartRec.hpp>
#include <signaldata/StartFragReq.hpp>

bool
printSTART_REC_REQ(FILE * output, 
		  const Uint32 * theData, 
		  Uint32 len, 
		  Uint16 recBlockNo){
  StartRecReq * sig = (StartRecReq *) theData;

  if (len != StartRecReq::SignalLength)
    return false;

  fprintf(output, " receivingNodeId: %d senderRef: (%d, %d)\n",
	  sig->receivingNodeId, 
	  refToNode(sig->senderRef),
	  refToBlock(sig->senderRef));
  
  fprintf(output, 
          " keepGci: %d lastCompletedGci: %d newestGci: %d senderData: %x\n",
	  sig->keepGci, 
	  sig->lastCompletedGci,
	  sig->newestGci,
          sig->senderData);

  NdbNodeBitmask mask;
  mask.assign(NdbNodeBitmask::Size, sig->sr_nodes);
  
  char buf[100];
  fprintf(output,
          " sr_nodes: %s\n", mask.getText(buf));

  return true;
}

bool
printSTART_REC_CONF(FILE * output, 
		    const Uint32 * theData, 
		    Uint32 len, 
		    Uint16 recBlockNo){
  StartRecConf * sig = (StartRecConf *) theData;

  if (len != StartRecConf::SignalLength)
    return false;

  fprintf(output, " startingNodeId: %d senderData: %u\n",
	  sig->startingNodeId,
          sig->senderData);
  
  return true;
}

bool 
printSTART_FRAG_REQ(FILE * output, 
		    const Uint32 * theData, 
		    Uint32 len, 
		    Uint16 recBlockNo)
{
  StartFragReq* sig = (StartFragReq*)theData;

  fprintf(output, " table: %d frag: %d lcpId: %d lcpNo: %d #nodes: %d"
                  ", reqinfo: %x \n",
	  sig->tableId, sig->fragId, sig->lcpId, sig->lcpNo, 
	  sig->noOfLogNodes,sig->requestInfo);

  for(Uint32 i = 0; i<sig->noOfLogNodes; i++)
  {
    fprintf(output, " (node: %d startGci: %d lastGci: %d)",
	    sig->lqhLogNode[i],
	    sig->startGci[i],
	    sig->lastGci[i]);
  }
  if (len == StartFragReq::SignalLength)
  {
    fprintf(output, "\nnodeRestorableGci: %u", sig->nodeRestorableGci);
  }
  else
  {
    fprintf(output, "\nnodeRestorableGci: 0 (from older version)");
  }
  fprintf(output, "\n");
  return true; 
}
