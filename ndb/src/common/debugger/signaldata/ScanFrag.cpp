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



#include <BlockNumbers.h>
#include <signaldata/ScanTab.hpp>
#include <signaldata/ScanFrag.hpp>

bool
printSCAN_FRAGREQ(FILE * output, const Uint32 * theData, 
		  Uint32 len, Uint16 receiverBlockNo) {
  const ScanFragReq * const sig = (ScanFragReq *)theData;
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " resultRef: %x\n", sig->resultRef);
  fprintf(output, " savePointId: %x\n", sig->savePointId);
  fprintf(output, " requestInfo: %x\n", sig->requestInfo);
  fprintf(output, " tableId: %x\n", sig->tableId);
  fprintf(output, " fragmentNo: %x\n", sig->fragmentNoKeyLen & 0xFFFF);
  fprintf(output, " keyLen: %x\n", sig->fragmentNoKeyLen >> 16);
  fprintf(output, " schemaVersion: %x\n", sig->schemaVersion);
  fprintf(output, " transId1: %x\n", sig->transId1);
  fprintf(output, " transId2: %x\n", sig->transId2);
  fprintf(output, " clientOpPtr: %x\n", sig->clientOpPtr);
  fprintf(output, " batch_size_rows: %x\n", sig->batch_size_rows);
  fprintf(output, " batch_size_bytes: %x\n", sig->batch_size_bytes);
  return true;
}

