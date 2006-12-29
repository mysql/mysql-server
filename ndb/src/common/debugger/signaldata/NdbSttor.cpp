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

#include <signaldata/NdbSttor.hpp>

bool
printNDB_STTOR(FILE * output, const Uint32 * theData, 
	       Uint32 len, Uint16 receiverBlockNo) {
  const NdbSttor * const sig = (NdbSttor *)theData;
  fprintf(output, " senderRef: %x\n", sig->senderRef);
  fprintf(output, " nodeId: %x\n", sig->nodeId);
  fprintf(output, " internalStartPhase: %x\n", sig->internalStartPhase);
  fprintf(output, " typeOfStart: %x\n", sig->typeOfStart);
  fprintf(output, " masterNodeId: %x\n", sig->masterNodeId);

  int left = len - NdbSttor::SignalLength;
  if(left > 0){
    fprintf(output, " config: ");
    for(int i = 0; i<left; i++){
      fprintf(output, "%x ", sig->config[i]);
      if(((i + 1) % 7) == 0 && (i+1) < left){
	fprintf(output, "\n config: ");
      }
    }
    fprintf(output, "\n");
  }
  return true;
}

bool
printNDB_STTORRY(FILE * output, const Uint32 * theData, 
		Uint32 len, Uint16 receiverBlockNo) {
  const NdbSttorry * const sig = (NdbSttorry *)theData;
  fprintf(output, " senderRef: %x\n", sig->senderRef);
  return true;
}

