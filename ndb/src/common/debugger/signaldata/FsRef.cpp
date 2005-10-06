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
    fprintf(output, ndbd_exit_message(sig->getErrorCode(sig->errorCode), &cl));
    break;
  }
  fprintf(output, "\n");
  fprintf(output, " OS ErrorCode: %d \n", sig->osErrorCode);

  return ret;
}
