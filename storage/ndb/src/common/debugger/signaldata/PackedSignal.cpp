/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#include <signaldata/PackedSignal.hpp>
#include <signaldata/LqhKey.hpp>
#include <signaldata/FireTrigOrd.hpp>
#include <debugger/DebuggerNames.hpp>

#include <EventLogger.hpp>

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
    case ZREMOVE_MARKER:
    {
      bool removed_by_api = !(theData[i] & 1);
      Uint32 signalLength = 2;
      fprintf(output, "--------------- Signal ----------------\n");
      if (removed_by_api)
      {
        fprintf(output, "r.bn: %u \"%s\", length: %u \"REMOVE_MARKER\"\n",
	        receiverBlockNo, getBlockName(receiverBlockNo,""), signalLength);
      }
      else
      {
        fprintf(output, "r.bn: %u \"%s\", length: %u \"REMOVE_MARKER_FAIL_API\"\n",
	        receiverBlockNo, getBlockName(receiverBlockNo,""), signalLength);
      }
      fprintf(output, "Signal data: ");
      i++; // Skip first word!
      for(Uint32 j = 0; j < signalLength; j++)
	fprintf(output, "H\'%.8x ", theData[i++]);
      fprintf(output,"\n");
      break;
    }
    case ZFIRE_TRIG_REQ: {
      Uint32 signalLength = FireTrigReq::SignalLength;

      fprintf(output, "--------------- Signal ----------------\n");
      fprintf(output, "r.bn: %u \"%s\", length: %u \"FIRE_TRIG_REQ\"\n",
	      receiverBlockNo, getBlockName(receiverBlockNo,""), signalLength);
      i += signalLength;
      break;
    }
    case ZFIRE_TRIG_CONF: {
      Uint32 signalLength = FireTrigConf::SignalLength;

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

bool
PackedSignal::verify(const Uint32* data, Uint32 len, Uint32 receiverBlockNo, 
                     Uint32 typesExpected, Uint32 commitLen)
{
  Uint32 pos = 0;
  bool bad = false;

  if (unlikely(len > 25))
  {
    g_eventLogger->info("Bad PackedSignal length : %u", len);
    bad = true;
  }
  else
  {
    while ((pos < len) && ! bad)
    {
      Uint32 sigType = data[pos] >> 28;
      if (unlikely(((1 << sigType) & typesExpected) == 0))
      {
        g_eventLogger->info(
            "Unexpected sigtype in packed signal: %u at pos %u. Expected : %u",
            sigType, pos, typesExpected);
        bad = true;
        break;
      }
      switch (sigType)
      {
      case ZCOMMIT:
        assert(commitLen > 0);
        pos += commitLen;
        break;
      case ZCOMPLETE:
        pos+= 3;
        break;
      case ZCOMMITTED:
        pos+= 3;
        break;
      case ZCOMPLETED:
        pos+= 3;
        break;
      case ZLQHKEYCONF:
        pos+= LqhKeyConf::SignalLength;
        break;
      case ZREMOVE_MARKER:
        pos+= 3;
        break;
      case ZFIRE_TRIG_REQ:
        pos+= FireTrigReq::SignalLength;
        break;
      case ZFIRE_TRIG_CONF:
        pos+= FireTrigConf::SignalLength;
        break;
      default :
        g_eventLogger->info("Unrecognised signal type %u at pos %u", sigType,
                            pos);
        bad = true;
        break;
      }
    }
    
    if (likely(pos == len))
    {
      /* Looks ok */
      return true;
    }
    
    if (!bad)
    {
      g_eventLogger->info(
          "Packed signal component length (%u) != total length (%u)",
          pos, len);
    }
  }

  printPACKED_SIGNAL(stderr, data, len, receiverBlockNo);
  
  return false;
}
