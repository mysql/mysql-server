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

//****************************************************************************
//
//  NAME
//      SendBuffer
//
//  DESCRIPTION
//      The SendBuffer is a circular buffer storing signals waiting to be sent.
//      The signals can be of variable size and are copied into the buffer 
//      in Protocol 6 format. There will be two SendBuffer instances 
//      (priority level A and B) for each transporter using a buffer for 
//      sending. The buffering will in most cases be done to send as big 
//      packages as possible over TCP/IP.
//
//***************************************************************************/
#ifndef SendBuffer_H
#define SendBuffer_H

#include "TransporterDefinitions.hpp"
#include <TransporterCallback.hpp>

#ifdef DEBUG_TRANSPORTER
#include <ndb_global.h>
#endif

class SendBuffer {
  friend class TCP_Transporter;
public:
  // Set member variables
  SendBuffer(Uint32 bufSize);

  // Deallocate the buffer memory
  ~SendBuffer();
  
  // Allocate memory for the buffer and initialize the buffer pointers
  bool initBuffer(Uint32 aRemoteNodeId);  

  // Number of bytes remaining in the buffer
  Uint32 bufferSizeRemaining() const;

  // Number of bytes of data in the buffer 
  int bufferSize(); 

  // Empty the buffer
  void emptyBuffer();
  
  /**
   * The transporter calls updateBuffer after a retrieve followed by 
   * a successful send, to update the cirkular buffer pointers.
   * updateBuffer is called with the number of bytes really sent,
   * it may be that it is less than what was retrived from the buffer.
   * If that is the case there will be an incomplete message (slack)
   * in the SendBuffer. 
   *
   * Returns  0 if buffer empty
   *    else ~0
   */
  Uint32 bytesSent(Uint32 len);
  
#ifdef DEBUG_TRANSPORTER
  // Prints the buffer status on the screen. Can be used for testing purposes.
  void print();
#endif

  Uint32* getInsertPtr(Uint32 bytes);
  void updateInsertPtr(Uint32 bytes);

private:
  
  Uint32   sizeOfBuffer;  // Length, in number of bytes, of the buffer memory
  Uint32   dataSize;      // Number of bytes in buffer
  
  Uint32 * startOfBuffer; // Pointer to the start of the buffer memory
  Uint32 * endOfBuffer;   // Pointer to end of buffer
  
  Uint32 * insertPtr;     // Where to insert next
  
  char *   sendPtr;           // Where data to send starts
  Uint32   sendDataSize;      // Num bytes to send
  
  Uint32   theRemoteNodeId;
};

inline
Uint32
SendBuffer::bytesSent(Uint32 bytes) {

  if(bytes > dataSize){
#ifdef DEBUG_TRANSPORTER
    printf("bytes(%d) > dataSize(%d)\n", bytes, dataSize);
#endif
    abort();
    // reportError(0 ,theRemoteNodeId, TE_INVALID_MESSAGE_LENGTH);
    return 0;
  }//if

  if(bytes > sendDataSize){
#ifdef DEBUG_TRANSPORTER
    printf("bytes(%d) > sendDataSize(%d)\n", bytes, sendDataSize);
#endif
    abort();
    //reportError(0,theRemoteNodeId, TE_INVALID_MESSAGE_LENGTH);
    return 0;
  }//if

  dataSize     -= bytes;
  sendPtr      += bytes;
  sendDataSize -= bytes;
  
  if(sendDataSize == 0){
    if(sendPtr > (char*)insertPtr){
      sendPtr = (char *)startOfBuffer;
      sendDataSize = dataSize;
    } else {
      sendPtr = ((char*)insertPtr) - dataSize;
      sendDataSize = dataSize;
    }
  }
  
  if(dataSize == 0)
    return 0;
  return ~0;
}

inline
Uint32*
SendBuffer::getInsertPtr(Uint32 len){
  if (bufferSizeRemaining() < len){
    return 0;
  }

  const char * const tmpInsertPtr = (char *) insertPtr;

  if(tmpInsertPtr >= sendPtr){
    // Is there enough space at the end of the buffer? 
    if ((tmpInsertPtr + len) < (char*)endOfBuffer){
      sendDataSize += len;
      return insertPtr;
    } else {
      // We have passed the end of the cirkular buffer, 
      // must start from the beginning
      // Is there enough space in the beginning of the buffer?
      if ((Uint32)(sendPtr - (char *)startOfBuffer) <= len){
	// Not enough space available, insert failed
	return 0;
      } else {
	// There is space available at the beginning of the buffer
	// We start from the beginning, set endOfData and insertPtr
	insertPtr = startOfBuffer; 
	if(sendDataSize != 0){
	  return insertPtr;
	}	
	sendPtr      = (char *)startOfBuffer;
	sendDataSize = len;
	return insertPtr;
      }
    }
  } else {
    // sendPtr > insertPtr
    // Is there enought room
    if((tmpInsertPtr + len) < sendPtr){
      return insertPtr;
    }
    return 0;
  }
}

inline
void
SendBuffer::updateInsertPtr(Uint32 lenBytes){
  dataSize  += lenBytes;
  insertPtr += (lenBytes / 4);
}

#endif // Define of SendBuffer_H
