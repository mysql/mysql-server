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


#include <DebuggerNames.hpp>
#include <signaldata/NFCompleteRep.hpp>

bool 
printNF_COMPLETE_REP(FILE * output, 
		     const Uint32 * theData, 
		     Uint32 len, 
		     Uint16 recBlockNo){

  NFCompleteRep * sig = (NFCompleteRep*)theData;
  const char * who = getBlockName(sig->blockNo, 0);
  
  if(who == 0){
    fprintf(output, 
	    " Node: %d has completed failure of node %d\n",
	    sig->nodeId, sig->failedNodeId);
  } else {
    fprintf(output, 
	    " Node: %d block: %s has completed failure of node %d\n",
	    sig->nodeId, who, sig->failedNodeId);
  }

  fprintf(output, "Sent from line: %d\n",
	  sig->from);
  
  return true;
}
