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
  default:
    fprintf(output, "fsFormatMax not handled\n");
    ret = false;
    break;
  }
    
  fprintf(output, " varIndex: %d\n", 
	  sig->varIndex);    
  fprintf(output, " numberOfPages: %d\n", 
	  sig->numberOfPages);
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
  default:
    fprintf(output, "Impossible event\n");
  }

  fprintf(output, "\n");
  return ret;
}
