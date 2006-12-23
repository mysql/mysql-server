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

#include "SendBuffer.hpp"
#include "TransporterInternalDefinitions.hpp"

SendBuffer::SendBuffer(Uint32 bufSize) {

  sizeOfBuffer = bufSize;
  if(sizeOfBuffer < MAX_MESSAGE_SIZE)
    sizeOfBuffer = 2 * MAX_MESSAGE_SIZE; 
  startOfBuffer = NULL;

  // Initalise pointers 
  endOfBuffer = NULL;
  insertPtr = NULL;
  sendPtr = NULL;
  sendDataSize = 0;
  dataSize = 0;
}

bool
SendBuffer::initBuffer(Uint32 aRemoteNodeId) {

  // Allocate memory for the buffer
#ifdef DEBUG_TRANSPORTER
  ndbout << "Allocating " << sizeOfBuffer << " bytes for send buffer" << endl;
#endif

  startOfBuffer = new Uint32[(sizeOfBuffer >> 2) + 1];
  endOfBuffer   = startOfBuffer + (sizeOfBuffer >> 2);
  
  emptyBuffer();
  theRemoteNodeId = aRemoteNodeId;
  return true;
}

SendBuffer::~SendBuffer() {
  // Deallocate the buffer memory
  if(startOfBuffer != NULL)
    delete[] startOfBuffer;
}

int
SendBuffer::bufferSize() {
  return dataSize;
}

Uint32
SendBuffer::bufferSizeRemaining() const {
  return (sizeOfBuffer - dataSize);
}

void
SendBuffer::emptyBuffer() {
  insertPtr    = startOfBuffer;
  sendPtr      = (char*)startOfBuffer;
  dataSize     = 0;
  sendDataSize = 0;
}

#ifdef DEBUG_TRANSPORTER
void
SendBuffer::print() {
  
  printf("SendBuffer status printouts\n");
  
  printf( "sizeOfBuffer:  %d\n", sizeOfBuffer);
  printf( "startOfBuffer: %.8x\n", startOfBuffer);
  printf( "endOfBuffer:   %.8x\n", endOfBuffer);
  printf( "insertPtr:     %.8x\n", insertPtr);
  printf( "sendPtr:       %.8x\n", sendPtr);
  printf( "sendDataSize:  %d\n", sendDataSize);
  printf( "dataSize:      %d\n", dataSize);
}
#endif
