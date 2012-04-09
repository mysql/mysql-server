/*
   Copyright (C) 2003, 2005, 2006 MySQL AB
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



#include <signaldata/FsRef.hpp>

bool 
printFSREF(FILE * output, const Uint32 * theData, 
	   Uint32 len, Uint16 receiverBlockNo){
  
  bool ret = true;

  const FsRef * const sig = (FsRef *) theData;
  
  fprintf(output, " UserPointer: %d\n", 
	  sig->userPointer);

  fprintf(output, " ErrorCode: %d, ", sig->errorCode);
  ndbd_exit_classification cl;
  switch (sig->getErrorCode(sig->errorCode)){
  case FsRef::fsErrNone:
    fprintf(output, "No error");
    break;
  default:
    fprintf(output, "%s", ndbd_exit_message(sig->getErrorCode(sig->errorCode), &cl));
    break;
  }
  fprintf(output, "\n");
  fprintf(output, " OS ErrorCode: %d \n", sig->osErrorCode);

  return ret;
}
