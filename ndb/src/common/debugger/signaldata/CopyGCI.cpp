/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <signaldata/CopyGCIReq.hpp>

static
void
print(char * buf, size_t buf_len, CopyGCIReq::CopyReason r){
  switch(r){
  case CopyGCIReq::IDLE:
    BaseString::snprintf(buf, buf_len, "IDLE");
    break;
  case CopyGCIReq::LOCAL_CHECKPOINT:
    BaseString::snprintf(buf, buf_len, "LOCAL_CHECKPOINT");
    break;
  case CopyGCIReq::RESTART:
    BaseString::snprintf(buf, buf_len, "RESTART");
    break;
  case CopyGCIReq::GLOBAL_CHECKPOINT:
    BaseString::snprintf(buf, buf_len, "GLOBAL_CHECKPOINT");
    break;
  case CopyGCIReq::INITIAL_START_COMPLETED:
    BaseString::snprintf(buf, buf_len, "INITIAL_START_COMPLETED");
    break;
  default:
    BaseString::snprintf(buf, buf_len, "<Unknown>");
  }
}

bool
printCOPY_GCI_REQ(FILE * output, 
		  const Uint32 * theData, 
		  Uint32 len, 
		  Uint16 recBlockNo){
  CopyGCIReq * sig = (CopyGCIReq*)theData;

  static char buf[255];
  print(buf, sizeof(buf), (CopyGCIReq::CopyReason)sig->copyReason);

  fprintf(output, " SenderData: %d CopyReason: %s StartWord: %d\n",
	  sig->anyData,
	  buf,
	  sig->startWord);
  return false;
}
