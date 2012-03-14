/*
   Copyright (C) 2003, 2005, 2006, 2008 MySQL AB
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

#include <signaldata/UtilSequence.hpp>

inline
const char *
type2string(UtilSequenceReq::RequestType type){
  switch(type){
  case UtilSequenceReq::NextVal:
    return "NextVal";
  case UtilSequenceReq::CurrVal:
    return "CurrVal";
  case UtilSequenceReq::Create:
    return "Create";
  case UtilSequenceReq::SetVal:
    return "SetVal";
  default:
    return "Unknown";
  }
}

bool 
printUTIL_SEQUENCE_REQ(FILE * out, const Uint32 * data, Uint32 l, Uint16 b){
  UtilSequenceReq* sig = (UtilSequenceReq*)data;
  fprintf(out, " senderData: %d sequenceId: %d RequestType: %s\n",
	  sig->senderData,
	  sig->sequenceId,
	  type2string((UtilSequenceReq::RequestType)sig->requestType));
  return true;
}

bool 
printUTIL_SEQUENCE_CONF(FILE * out, const Uint32 * data, Uint32 l, Uint16 b){
  UtilSequenceConf* sig = (UtilSequenceConf*)data;
  fprintf(out, " senderData: %d sequenceId: %d RequestType: %s\n",
	  sig->senderData,
	  sig->sequenceId,
	  type2string((UtilSequenceReq::RequestType)sig->requestType));
  fprintf(out, " val: [ %d %d ]\n", 
	  sig->sequenceValue[0],
	  sig->sequenceValue[1]);
  return true;
}

bool 
printUTIL_SEQUENCE_REF(FILE * out, const Uint32 * data, Uint32 l, Uint16 b){
  UtilSequenceRef* sig = (UtilSequenceRef*)data;
  fprintf(out, " senderData: %d sequenceId: %d RequestType: %s\n",
	  sig->senderData,
	  sig->sequenceId,
	  type2string((UtilSequenceReq::RequestType)sig->requestType));
  fprintf(out, " errorCode: %d, TCErrorCode: %d\n",
	  sig->errorCode, sig->TCErrorCode);
  return true;
}
