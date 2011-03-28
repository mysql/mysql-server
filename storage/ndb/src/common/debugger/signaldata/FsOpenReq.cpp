/*
   Copyright (C) 2003, 2005, 2006 MySQL AB, 2008 Sun Microsystems, Inc.
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

  if (flags & FsOpenReq::OM_APPEND)
    fprintf(output, ", Append");
  if (flags & FsOpenReq::OM_SYNC)
    fprintf(output, ", Sync");
  if (flags & FsOpenReq::OM_CREATE)
    fprintf(output, ", Create new file");
  if (flags & FsOpenReq::OM_TRUNCATE)
    fprintf(output, ", Truncate existing file");
  if (flags & FsOpenReq::OM_AUTOSYNC)
    fprintf(output, ", Auto Sync");

  if (flags & FsOpenReq::OM_CREATE_IF_NONE)
    fprintf(output, ", Create if None");
  if (flags & FsOpenReq::OM_INIT)
    fprintf(output, ", Initialise");
  if (flags & FsOpenReq::OM_CHECK_SIZE)
    fprintf(output, ", Check Size");
  if (flags & FsOpenReq::OM_DIRECT)
    fprintf(output, ", O_DIRECT");
  if (flags & FsOpenReq::OM_GZ)
    fprintf(output, ", gz compressed");

  fprintf(output, "\n");
  return true;
}
