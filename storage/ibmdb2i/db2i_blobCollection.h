/*
Licensed Materials - Property of IBM
DB2 Storage Engine Enablement
Copyright IBM Corporation 2007,2008
All rights reserved

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met: 
 (a) Redistributions of source code must retain this list of conditions, the
     copyright notice in section {d} below, and the disclaimer following this
     list of conditions. 
 (b) Redistributions in binary form must reproduce this list of conditions, the
     copyright notice in section (d) below, and the disclaimer following this
     list of conditions, in the documentation and/or other materials provided
     with the distribution. 
 (c) The name of IBM may not be used to endorse or promote products derived from
     this software without specific prior written permission. 
 (d) The text of the required copyright notice is: 
       Licensed Materials - Property of IBM
       DB2 Storage Engine Enablement 
       Copyright IBM Corporation 2007,2008 
       All rights reserved

THIS SOFTWARE IS PROVIDED BY IBM CORPORATION "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL IBM CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/


#ifndef DB2I_BLOBCOLLECTION_H
#define DB2I_BLOBCOLLECTION_H

#include "db2i_global.h"
#include "db2i_file.h"

/** 
   @class ProtectedBuffer
   @brief Implements memory management for (optionally) protected buffers.

   Buffers created with the protection option will have a guard page set on the
   page following requested allocation size. The side effect is that the actual
   allocation is up to 2*4096-1 bytes larger than the size requested by the 
   using code.
*/

class ProtectedBuffer
{
public:
  ProtectedBuffer() : protectBuf(false)
  {;}
  
  void Malloc(size_t size, bool protect = false)
  {
    protectBuf = protect;
    bufptr.alloc(size + (protectBuf ? 0x1fff : 0x0));
    if ((void*)bufptr != NULL)
    {
      len = size;
      if (protectBuf) 
        mprotect(protectedPage(), 0x1000, PROT_NONE);
#ifndef DBUG_OFF
      // Prevents a problem with DBUG_PRINT over-reading in recent versions of 
      // MySQL
      *((char*)protectedPage()-1) = 0;
#endif
    }
  }
  
  void Free()
  {
    if ((void*)bufptr != NULL)
    {
      if (protectBuf)  
        mprotect(protectedPage(), 0x1000, PROT_READ | PROT_WRITE);
      bufptr.dealloc();
    }
  }
  
  ~ProtectedBuffer()
  {
    Free();
  }
  
  ValidatedPointer<char>& ptr()  {return bufptr;}
  bool isProtected() const {return protectBuf;}
  size_t allocLen() const {return len;}
private:
  void* protectedPage()
  {
    return (void*)(((address64_t)(void*)bufptr + len + 0x1000) & ~0xfff);
  }
    
  ValidatedPointer<char> bufptr;
  size_t len;
  bool protectBuf;
  
};


/**
   @class BlobCollection
   @brief Manages memory allocation for reading blobs associated with a table. 
   
   Allocations are done on-demand and are protected with a guard page if less
   than the max possible size is allocated.
*/
class BlobCollection
{
  public: 
  BlobCollection(db2i_table* db2Table, uint32 defaultAllocSize) : 
      defaultAllocation(defaultAllocSize), table(db2Table)
  {
    buffers = new ProtectedBuffer[table->getBlobCount()];
  }

  ~BlobCollection()
  {
    delete[] buffers;
  }
    
  ValidatedPointer<char>& getBufferPtr(int fieldIndex)
  {
    int blobIndex = table->getBlobIdFromField(fieldIndex);
    if ((char*)buffers[blobIndex].ptr() == NULL)
      generateBuffer(fieldIndex);
    
    return buffers[blobIndex].ptr();
  }

  ValidatedPointer<char>& reallocBuffer(int fieldIndex, size_t size);
    
  
  private: 
      
  uint32 getSizeToAllocate(int fieldIndex, bool& shouldProtect);      
  void generateBuffer(int fieldIndex);
  
  db2i_table* table;                            // The table being read
  ProtectedBuffer* buffers;                     // The buffers
  uint32 defaultAllocation;                     
    /* The default size to use when first allocating a buffer */
};

#endif
