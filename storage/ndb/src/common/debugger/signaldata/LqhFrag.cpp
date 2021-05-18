/*
   Copyright (C) 2003, 2005, 2006, 2008 MySQL AB
    Use is subject to license terms.

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


#include <signaldata/LqhFrag.hpp>

bool
printLQH_FRAG_REQ(FILE * output, const Uint32 * theData, Uint32 len, Uint16 recB){
  LqhFragReq* sig = (LqhFragReq*)theData;
  
  fprintf(output, " senderData: %d senderRef: %x",
	  sig->senderData, sig->senderRef);
  fprintf(output, " tableId: %d fragmentId: %d", sig->tableId, sig->fragmentId);
  fprintf(output, " localKeyLength: %d maxLoadFactor: %d minLoadFactor: %d\n",
	  sig->localKeyLength, sig->maxLoadFactor, sig->minLoadFactor);
  fprintf(output, " kValue: %d lh3DistrBits: %d lh3PageBits: %d\n",
	  sig->kValue, sig->lh3DistrBits, sig->lh3PageBits);
  
  fprintf(output, " keyLength: %d\n",
	  sig->keyLength);

  fprintf(output, " maxRowsLow/High: %u/%u  minRowsLow/High: %u/%u\n",
	  sig->maxRowsLow, sig->maxRowsHigh, sig->minRowsLow, sig->minRowsHigh);
  fprintf(output, " nextLCP: %d\n",
	  sig->nextLCP);
  
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
