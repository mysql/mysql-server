/*
   Copyright (C) 2003, 2005-2007 MySQL AB
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <signaldata/PackedSignal.hpp>
#include <signaldata/LqhKey.hpp>
#include <debugger/DebuggerNames.hpp>

bool
printPACKED_SIGNAL(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo){
  fprintf(output, "Signal data: ");
  Uint32 i = 0;
  while (i < len)
    fprintf(output, "H\'%.8x ", theData[i++]);
  fprintf(output,"\n");
  fprintf(output, "--------- Begin Packed Signals --------\n");  
  // Print each signal separately
  for (i = 0; i < len;) {
    switch (PackedSignal::getSignalType(theData[i])) {
    case ZCOMMIT: {
      Uint32 signalLength = 5;
      fprintf(output, "--------------- Signal ----------------\n");
      fprintf(output, "r.bn: %u \"%s\", length: %u \"COMMIT\"\n", 
	      receiverBlockNo, getBlockName(receiverBlockNo,""), signalLength);
      fprintf(output, "Signal data: ");
      for(Uint32 j = 0; j < signalLength; j++)
	fprintf(output, "H\'%.8x ", theData[i++]);
      fprintf(output,"\n");
      break;
    }
    case ZCOMPLETE: {
      Uint32 signalLength = 3;
      fprintf(output, "--------------- Signal ----------------\n");
      fprintf(output, "r.bn: %u \"%s\", length: %u \"COMPLETE\"\n",
	      receiverBlockNo, getBlockName(receiverBlockNo,""), signalLength);
      fprintf(output, "Signal data: ");
      for(Uint32 j = 0; j < signalLength; j++)
	fprintf(output, "H\'%.8x ", theData[i++]);
      fprintf(output,"\n");
      break;
    }    
    case ZCOMMITTED: {
      Uint32 signalLength = 3;
      fprintf(output, "--------------- Signal ----------------\n");
      fprintf(output, "r.bn: %u \"%s\", length: %u \"COMMITTED\"\n",
	      receiverBlockNo, getBlockName(receiverBlockNo,""), signalLength);
      fprintf(output, "Signal data: ");
      for(Uint32 j = 0; j < signalLength; j++)
	fprintf(output, "H\'%.8x ", theData[i++]);
      fprintf(output,"\n");
      break;
    }
    case ZCOMPLETED: {
      Uint32 signalLength = 3;
      fprintf(output, "--------------- Signal ----------------\n");
      fprintf(output, "r.bn: %u \"%s\", length: %u \"COMPLETED\"\n",
	      receiverBlockNo, getBlockName(receiverBlockNo,""), signalLength);
      fprintf(output, "Signal data: ");
      for(Uint32 j = 0; j < signalLength; j++)
	fprintf(output, "H\'%.8x ", theData[i++]);
      fprintf(output,"\n");
      break;
    }
    case  ZLQHKEYCONF: {
      Uint32 signalLength = LqhKeyConf::SignalLength;

      fprintf(output, "--------------- Signal ----------------\n");
      fprintf(output, "r.bn: %u \"%s\", length: %u \"LQHKEYCONF\"\n",
	      receiverBlockNo, getBlockName(receiverBlockNo,""), signalLength);
      printLQHKEYCONF(output, theData + i, signalLength, receiverBlockNo);
      i += signalLength;
      break;
    }
    case ZREMOVE_MARKER: {
      Uint32 signalLength = 2;
      fprintf(output, "--------------- Signal ----------------\n");
      fprintf(output, "r.bn: %u \"%s\", length: %u \"REMOVE_MARKER\"\n",
	      receiverBlockNo, getBlockName(receiverBlockNo,""), signalLength);
      fprintf(output, "Signal data: ");
      i++; // Skip first word!
      for(Uint32 j = 0; j < signalLength; j++)
	fprintf(output, "H\'%.8x ", theData[i++]);
      fprintf(output,"\n");
      break;
    }
    case ZFIRE_TRIG_REQ: {
      Uint32 signalLength = 3;

      fprintf(output, "--------------- Signal ----------------\n");
      fprintf(output, "r.bn: %u \"%s\", length: %u \"FIRE_TRIG_REQ\"\n",
	      receiverBlockNo, getBlockName(receiverBlockNo,""), signalLength);
      i += signalLength;
      break;
    }
    case ZFIRE_TRIG_CONF: {
      Uint32 signalLength = 4;

      fprintf(output, "--------------- Signal ----------------\n");
      fprintf(output, "r.bn: %u \"%s\", length: %u \"FIRE_TRIG_CONF\"\n",
	      receiverBlockNo, getBlockName(receiverBlockNo,""), signalLength);
      i += signalLength;
      break;
    }
    default:
      fprintf(output, "Unknown signal type\n");
      i = len; // terminate printing
      break;
    }
  }//for
  fprintf(output, "--------- End Packed Signals ----------\n");
  return true;
}
