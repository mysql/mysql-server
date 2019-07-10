/*
   Copyright (C) 2003-2006 MySQL AB
    All rights reserved. Use is subject to license terms.

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
#include <signaldata/LCP.hpp>
#include <DebuggerNames.hpp>

bool
printSTART_LCP_REQ(FILE * output, const Uint32 * theData, 
		  Uint32 len, Uint16 receiverBlockNo){
  
  const StartLcpReq * const sig = (StartLcpReq *) theData;
 
  char buf1[8*_NDB_NODE_BITMASK_SIZE+1];
  char buf2[8*_NDB_NODE_BITMASK_SIZE+1];
  fprintf(output, 
	  " Sender: %d LcpId: %d\n"
	  " ParticipatingDIH = %s\n"
	  " ParticipatingLQH = %s\n",
	  refToNode(sig->senderRef), sig->lcpId,
	  sig->participatingDIH.getText(buf1),
	  sig->participatingLQH.getText(buf2));
  
  return true;
}

bool
printSTART_LCP_CONF(FILE * output, const Uint32 * theData, 
		  Uint32 len, Uint16 receiverBlockNo){
  
  const StartLcpConf * const sig = (StartLcpConf *) theData;
  
  fprintf(output, " Sender: %d LcpId: %d\n",
	  refToNode(sig->senderRef), sig->lcpId);
  
  return true;
}

bool
printLCP_FRAG_ORD(FILE * output, const Uint32 * theData, 
		  Uint32 len, Uint16 receiverBlockNo){
  
  const LcpFragOrd * const sig = (LcpFragOrd *) theData;
  
  fprintf(output, " LcpId: %d LcpNo: %d Table: %d Fragment: %d\n",
	  sig->lcpId, sig->lcpNo, sig->tableId, sig->fragmentId);
  
  fprintf(output, " KeepGCI: %d LastFragmentFlag: %d\n",
	  sig->keepGci, sig->lastFragmentFlag);
  return true;
}

bool
printLCP_FRAG_REP(FILE * output, const Uint32 * theData, 
		  Uint32 len, Uint16 receiverBlockNo){
  
  const LcpFragRep * const sig = (LcpFragRep *) theData;
  
  fprintf(output, " LcpId: %d LcpNo: %d NodeId: %d Table: %d Fragment: %d\n",
	  sig->lcpId, sig->lcpNo, sig->nodeId, sig->tableId, sig->fragId);
  fprintf(output, " Max GCI Started: %d Max GCI Completed: %d\n",
	  sig->maxGciStarted, sig->maxGciCompleted);
  return true;
}

bool
printLCP_COMPLETE_REP(FILE * output, const Uint32 * theData, 
		      Uint32 len, Uint16 receiverBlockNo){
  
  const LcpCompleteRep * const sig = (LcpCompleteRep *) theData;
  
  fprintf(output, " LcpId: %d NodeId: %d Block: %s\n",
	  sig->lcpId, sig->nodeId, getBlockName(sig->blockNo));
  return true;
}
