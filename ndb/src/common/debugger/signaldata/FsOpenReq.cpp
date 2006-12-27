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



#include <signaldata/FsOpenReq.hpp>

bool 
printFSOPENREQ(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo){
  
  const FsOpenReq * const sig = (FsOpenReq *) theData;
  

  fprintf(output, " UserReference: H\'%.8x, userPointer: H\'%.8x\n", 
	  sig->userReference, sig->userPointer);
  fprintf(output, " FileNumber[1-4]: H\'%.8x H\'%.8x H\'%.8x H\'%.8x\n", 
	  sig->fileNumber[0], sig->fileNumber[1], sig->fileNumber[2], sig->fileNumber[3]);
  fprintf(output, " FileFlags: H\'%.8x ", 
	  sig->fileFlags);
  
  // File open mode must be one of ReadOnly, WriteOnly or ReadWrite
  const Uint32 flags = sig->fileFlags;
  switch(flags & 3){
  case FsOpenReq::OM_READONLY:
    fprintf(output, "Open read only");
    break;
  case FsOpenReq::OM_WRITEONLY:
    fprintf(output, "Open write only");
    break;
  case FsOpenReq::OM_READWRITE:
    fprintf(output, "Open read and write");
    break;
  default:
    fprintf(output, "Open mode unknown!");
  }

  if (flags & FsOpenReq::OM_CREATE)
    fprintf(output, ", Create new file");
  if (flags & FsOpenReq::OM_TRUNCATE)
    fprintf(output, ", Truncate existing file");
  if (flags & FsOpenReq::OM_APPEND)
    fprintf(output, ", Append");
  
  fprintf(output, "\n");
  return true;
}
