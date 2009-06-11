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


#include "db2i_sqlStatementStream.h"
#include "as400_types.h"

/**
  Add a statement to the statement stream, allocating additional memory as needed.
  
  @parm stmt  The statement text
  @parm length  The length of the statement text
  @parm fileSortSequence  The DB2 sort sequence identifier, in EBCDIC
  @parm fileSortSequenceLibrary  The DB2 sort sequence library, in EBCDIC
  
  @return Reference to this object
*/
SqlStatementStream& SqlStatementStream::addStatementInternal(const char* stmt, 
                                                             uint32 length, 
                                                             const char* fileSortSequence, 
                                                             const char* fileSortSequenceLibrary)
{
  uint32 storageNeeded = length + sizeof(StmtHdr_t);
  storageNeeded = (storageNeeded + 3) & ~3; // We have to be 4-byte aligned.
  if (storageNeeded > storageRemaining())
  {
    // We overallocate new storage to reduce number of times reallocation is
    // needed.
    int newSize = curSize + 2 * storageNeeded;
    DBUG_PRINT("SqlStatementStream::addStatementInternal", 
               ("PERF: Had to realloc! Old size=%d. New size=%d", curSize, newSize));
    char* old_space = block;
    char* new_space = (char*)getNewSpace(newSize);
    memcpy(new_space, old_space, curSize);
    ptr = new_space + (ptr - old_space);
    curSize = newSize;
  }

  DBUG_ASSERT((address64_t)ptr % 4 == 0);      

  memcpy(((StmtHdr_t*)ptr)->SrtSeqNam, 
         fileSortSequence, 
         sizeof(((StmtHdr_t*)ptr)->SrtSeqNam));
  memcpy(((StmtHdr_t*)ptr)->SrtSeqSch, 
         fileSortSequenceLibrary, 
         sizeof(((StmtHdr_t*)ptr)->SrtSeqSch));
  ((StmtHdr_t*)ptr)->Length = length;
  memcpy(ptr + sizeof(StmtHdr_t), stmt, length);

  ptr += storageNeeded;
  ++statements;

  return *this;
}
