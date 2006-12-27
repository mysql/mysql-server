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


#include <signaldata/LqhFrag.hpp>

bool
printLQH_FRAG_REQ(FILE * output, const Uint32 * theData, Uint32 len, Uint16 recB){
  LqhFragReq* sig = (LqhFragReq*)theData;
  
  fprintf(output, " senderData: %d senderRef: %x",
	  sig->senderData, sig->senderRef);
  fprintf(output, " tableId: %d fragmentId: %d tableType: %d",
	  sig->tableId, sig->fragmentId, sig->tableType);
  if (sig->primaryTableId == RNIL)
    fprintf(output, " primaryTableId: RNIL\n");
  else
    fprintf(output, " primaryTableId: %d\n", sig->primaryTableId);
  fprintf(output, " localKeyLength: %d maxLoadFactor: %d minLoadFactor: %d\n",
	  sig->localKeyLength, sig->maxLoadFactor, sig->minLoadFactor);
  fprintf(output, " kValue: %d lh3DistrBits: %d lh3PageBits: %d\n",
	  sig->kValue, sig->lh3DistrBits, sig->lh3PageBits);
  
  fprintf(output, " noOfAttributes: %d noOfNullAttributes: %d keyLength: %d\n",
	  sig->noOfAttributes, sig->noOfNullAttributes, sig->keyLength);

  fprintf(output, " maxRowsLow/High: %u/%u  minRowsLow/High: %u/%u\n",
	  sig->maxRowsLow, sig->maxRowsHigh, sig->minRowsLow, sig->minRowsHigh);
  fprintf(output, " schemaVersion: %d nextLCP: %d\n",
	  sig->schemaVersion, sig->nextLCP);
  
  return true;
}
bool
printLQH_FRAG_CONF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 rec){
  LqhFragConf* sig = (LqhFragConf*)theData;
  
  fprintf(output, " senderData: %d lqhFragPtr: %d\n",
	  sig->senderData, sig->lqhFragPtr);
  return true;
}

bool
printLQH_FRAG_REF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 rec){
  LqhFragRef* sig = (LqhFragRef*)theData;
  
  fprintf(output, " senderData: %d errorCode: %d\n",
	  sig->senderData, sig->errorCode);
  return true;
}
