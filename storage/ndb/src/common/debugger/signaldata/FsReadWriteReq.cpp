/*
  Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

bool printFSREADWRITEREQ(FILE *output,
                         const Uint32 *theData,
                         Uint32 /*len*/,
                         Uint16 /*receiverBlockNo*/)
{
  bool ret = true;

  const FsReadWriteReq *const sig = (const FsReadWriteReq *)theData;

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
    break;
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

  switch(sig->getFormatFlag(sig->operationFlag))
  {
  case FsReadWriteReq::fsFormatListOfPairs:
    for (unsigned i = 0; i < sig->numberOfPages; i ++)
    {
      fprintf(output, " H\'%.8x, H\'%.8x\n",
              sig->data.listOfPair[i].varIndex,
              sig->data.listOfPair[i].fileOffset);
    }
    break;
  case FsReadWriteReq::fsFormatArrayOfPages:
    fprintf(output, " H\'%.8x, H\'%.8x\n", sig->data.arrayOfPages.varIndex,
                                           sig->data.arrayOfPages.fileOffset);
    break;
  case FsReadWriteReq::fsFormatListOfMemPages:
    // Format changed in v8.0.25
    fprintf(output, " H\'%.8x, ", sig->data.listOfMemPages.fileOffset);
    for (unsigned i = 0; i < sig->numberOfPages; i++)
    {
      fprintf(output, " H\'%.8x, ", sig->data.listOfMemPages.varIndex[i]);
    }
    break;
  case FsReadWriteReq::fsFormatGlobalPage:
    fprintf(output, " H\'%.8x, ", sig->data.globalPage.pageNumber);
    break;
  case FsReadWriteReq::fsFormatSharedPage:
    fprintf(output, " H\'%.8x, ", sig->data.sharedPage.pageNumber);
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
