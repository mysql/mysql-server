/*
  Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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



#include <signaldata/FsReadWriteReq.hpp>

bool
printFSREADWRITEREQ(FILE * output, const Uint32 * theData, 
		    Uint32 len, Uint16 receiverBlockNo){

  bool ret = true;
  
  const FsReadWriteReq * const sig = (FsReadWriteReq *) theData;
  
  fprintf(output, " UserPointer: %d\n", sig->userPointer);
  fprintf(output, " FilePointer: %d\n", sig->filePointer);
  fprintf(output, " UserReference: H\'%.8x", sig->userReference);

  fprintf(output, " Operation flag: H\'%.8x (", sig->operationFlag);
  if (sig->getSyncFlag(sig->operationFlag))
    fprintf(output, "Sync,");
  else
    fprintf(output, "No sync,");

  fprintf(output, " Format=");
  switch(sig->getFormatFlag(sig->operationFlag)){
  case FsReadWriteReq::fsFormatListOfPairs:
    fprintf(output, "List of pairs)\n");
    break;
  case FsReadWriteReq::fsFormatArrayOfPages:
    fprintf(output, "Array of pages)\n");
    break;
  case FsReadWriteReq::fsFormatListOfMemPages:
    fprintf(output, "List of mem pages)\n");
    break;
  case FsReadWriteReq::fsFormatGlobalPage:
    fprintf(output, "List of global pages)\n");
  case FsReadWriteReq::fsFormatSharedPage:
    fprintf(output, "List of shared pages)\n");
    break;
  case FsReadWriteReq::fsFormatMemAddress:
    fprintf(output, "Memory offset and file offset)\n");
    break;
  default:
    fprintf(output, "fsFormatMax not handled\n");
    ret = false;
    break;
  }
    
  fprintf(output, " varIndex: %d\n", 
	  sig->varIndex);    
  fprintf(output, " numberOfPages: %d\n", 
	  sig->numberOfPages);
  fprintf(output, " PartialFlag: %d\n",
          sig->getPartialReadFlag(sig->operationFlag));
  if (sig->getFormatFlag(sig->operationFlag) !=
      FsReadWriteReq::fsFormatMemAddress)
    fprintf(output, " pageData: ");

  unsigned int i;
  switch(sig->getFormatFlag(sig->operationFlag)){
  case FsReadWriteReq::fsFormatListOfPairs:
    for (i= 0; i < sig->numberOfPages*2; i += 2){
      fprintf(output, " H\'%.8x, H\'%.8x\n", sig->data.pageData[i], 
                                             sig->data.pageData[i + 1]);
    }
    break;
  case FsReadWriteReq::fsFormatArrayOfPages:
    fprintf(output, " H\'%.8x, H\'%.8x\n", sig->data.pageData[0], 
                                           sig->data.pageData[1]);
    break;
  case FsReadWriteReq::fsFormatListOfMemPages:
    for (i= 0; i < (sig->numberOfPages + 1); i++){
      fprintf(output, " H\'%.8x, ", sig->data.pageData[i]);
    }
    break;
  case FsReadWriteReq::fsFormatGlobalPage:
    for (i= 0; i < sig->numberOfPages; i++){
      fprintf(output, " H\'%.8x, ", sig->data.pageData[i]);
    }
    break;
  case FsReadWriteReq::fsFormatMemAddress:
    fprintf(output, "memoryOffset: H\'%.8x, ",
            sig->data.memoryAddress.memoryOffset);
    fprintf(output, "fileOffset: H\'%.8x, ",
            sig->data.memoryAddress.fileOffset);
    fprintf(output, "size: H\'%.8x",
            sig->data.memoryAddress.size);
    break;
  default:
    fprintf(output, "Impossible event\n");
  }

  fprintf(output, "\n");
  return ret;
}
