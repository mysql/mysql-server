/*
   Copyright (C) 2003-2006 MySQL AB
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



#include <signaldata/FsCloseReq.hpp>

bool
printFSCLOSEREQ(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo){
  
  const FsCloseReq * const sig = (FsCloseReq *) theData;
  
  fprintf(output, " UserPointer: %d\n", 
	  sig->userPointer);
  fprintf(output, " FilePointer: %d\n", 
	  sig->filePointer);
  fprintf(output, " UserReference: H\'%.8x\n", 
	  sig->userReference);

  fprintf(output, " Flags: H\'%.8x, ", sig->fileFlag);
  if (sig->getRemoveFileFlag(sig->fileFlag))
    fprintf(output, "Remove file");
  else
    fprintf(output, "Don't remove file");
  fprintf(output, "\n");

  return len == 4;
}
