/* Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA
*/



#include <signaldata/FsConf.hpp>

bool 
printFSCONF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo){
  
  const FsConf * const sig = (FsConf *) theData;
  
  fprintf(output, " UserPointer: %d\n", sig->userPointer);

  if (len > 1){
    // Only valid if this is a FSOPENCONF
    fprintf(output, " FilePointer: %d\n", sig->filePointer);
  }

  if (len > 2)
  {
    fprintf(output, " FileInfo: %08x\n", sig->fileInfo);
  }

  return true;
}
