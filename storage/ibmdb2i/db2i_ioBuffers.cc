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


#include "db2i_ioBuffers.h"

/**
  Request another block of rows

  Request the next set of rows from DB2. This must only be called after 
  newReadRequest().
  
  @param orientation The direction to use when reading through the table.
*/
void IOAsyncReadBuffer::loadNewRows(char orientation)
{
  rewind();
  maxRows() = rowsToBlock;

  DBUG_PRINT("db2i_ioBuffers::loadNewRows", ("Requesting %d rows, async = %d", rowsToBlock, readIsAsync));

  rc = getBridge()->expectErrors(QMY_ERR_END_OF_BLOCK, QMY_ERR_LOB_SPACE_TOO_SMALL)
                  ->read(file,
                         ptr(),
                         accessIntent,
                         commitLevel,
                         orientation,
                         readIsAsync,
                         rrnList,
                         0, 
                         0,
                         0);

  DBUG_PRINT("db2i_ioBuffers::loadNewRows", ("recordsRead: %d, rc: %d", (uint32)rowCount(), rc));

  
  *releaseRowNeeded = true;
  
  if (rc == QMY_ERR_END_OF_BLOCK)
  {
    // This is really just an informational error, so we ignore it.
    rc = 0;
    DBUG_PRINT("db2i_ioBuffers::loadNewRows", ("End of block signalled"));
  }
  else if (rc == QMY_ERR_END_OF_FILE)
  {
    // If we reach EOF or end-of-key, DB2 guarantees that no rows will be locked.
    rc = HA_ERR_END_OF_FILE;
    *releaseRowNeeded = false;
  }
  else if (rc == QMY_ERR_KEY_NOT_FOUND)
  {
    rc = HA_ERR_KEY_NOT_FOUND;
    *releaseRowNeeded = false;
  }
  
  if (rc) closePipe();
}


/**
  Empty the message pipe to prepare for another read.
*/
void IOAsyncReadBuffer::drainPipe()
{
  DBUG_ASSERT(pipeState == PendingFullBufferMsg);      
  PipeRpy_t msg[32];
  int bytes;
  PipeRpy_t* lastMsg;
  while ((bytes = read(msgPipe, msg, sizeof(msg))) > 0)
  {
    DBUG_PRINT("db2i_ioBuffers::drainPipe",("Pipe returned %d bytes", bytes));
    lastMsg = &msg[bytes / (sizeof(msg[0]))-1];
    if (lastMsg->CumRowCnt == maxRows() ||
        lastMsg->RtnCod != 0)
    {
      pipeState = ConsumedFullBufferMsg;
      break;
    }

  } 
  DBUG_PRINT("db2i_ioBuffers::drainPipe",("rc = %d, rows = %d, max = %d", lastMsg->RtnCod, lastMsg->CumRowCnt, (uint32)maxRows()));
}


/**
  Poll the message pipe for async read messages

  Only valid in async 
  
  @param orientation The direction to use when reading through the table.
*/
void IOAsyncReadBuffer::pollNextRow(char orientation)
{
  DBUG_ASSERT(readIsAsync);
  
  // Handle the case in which the buffer is full.
  if (rowCount() == maxRows())
  {
    // If we haven't read to the end, exit here.
    if (readCursor < rowCount())
      return;
    
    if (pipeState == PendingFullBufferMsg)
      drainPipe();
    if (pipeState == ConsumedFullBufferMsg)
      loadNewRows(orientation);
  }

  if (!rc)
  {
    PipeRpy_t* lastMsg = NULL;
    while (true)
    {    
      PipeRpy_t msg[32];
      int bytes = read(msgPipe, msg, sizeof(msg));
      DBUG_PRINT("db2i_ioBuffers::pollNextRow",("Pipe returned %d bytes", bytes));

      if (unlikely(bytes < 0))
      {
        DBUG_PRINT("db2i_ioBuffers::pollNextRow", ("Error"));
        rc = errno;
        break;
      }
      else if (bytes == 0)
        break;

      DBUG_ASSERT(bytes % sizeof(msg[0]) == 0);
      lastMsg = &msg[bytes / (sizeof(msg[0]))-1];

      if (lastMsg->RtnCod || (lastMsg->CumRowCnt == usedRows()))
      {
        rc = lastMsg->RtnCod;
        break;
      }
    } 

    *releaseRowNeeded = true;

    if (rc == QMY_ERR_END_OF_BLOCK)
      rc = 0;
    else if (rc == QMY_ERR_END_OF_FILE)
    {
      // If we reach EOF or end-of-key, DB2 guarantees that no rows will be locked.
      rc = HA_ERR_END_OF_FILE;
      *releaseRowNeeded = false;
    }
    else if (rc == QMY_ERR_KEY_NOT_FOUND)
    {
      rc = HA_ERR_KEY_NOT_FOUND;
      *releaseRowNeeded = false;
    }

    if (lastMsg)
      DBUG_PRINT("db2i_ioBuffers::pollNextRow", ("Good data: rc=%d; rows=%d; usedRows=%d", lastMsg->RtnCod, lastMsg->CumRowCnt, (uint32)usedRows()));
    if (lastMsg && likely(!rc))
    {
      if (lastMsg->CumRowCnt < maxRows())
        pipeState = PendingFullBufferMsg;
      else
        pipeState = ConsumedFullBufferMsg;

      DBUG_ASSERT(lastMsg->CumRowCnt <= usedRows());

    }
    DBUG_ASSERT(rowCount() <= getRowCapacity());
  }
  DBUG_PRINT("db2i_ioBuffers::pollNextRow", ("filledRows: %d, rc: %d", rowCount(), rc));
  if (rc) closePipe();
}


/**
  Prepare for the destruction of the row buffer storage.  
*/
void IOAsyncReadBuffer::prepForFree()
{
  interruptRead();
  rewind();
  IORowBuffer::prepForFree();
}


/**
  Initialize the newly allocated storage.

  @param sizeChanged  Indicates whether the storage capacity is being changed.
*/
void IOAsyncReadBuffer::initAfterAllocate(bool sizeChanged)
{
  rewind();

  if (sizeChanged || ((void*)rrnList == NULL))
    rrnList.realloc(getRowCapacity() * sizeof(uint32));
}


/**
  Send an initial read request

  @param infile  The file (table/index) being read from
  @param orientation  The orientation to use for this read request
  @param rowsToBuffer  The number of rows to request each time
  @param useAsync  Whether reads should be performed asynchronously.
  @param key  The key to use (if any)
  @param keyLength  The length of key (if any)
  @param keyParts  The number of columns in the key (if any)

*/
void IOAsyncReadBuffer::newReadRequest(FILE_HANDLE infile,
                                       char orientation,
                                       uint32 rowsToBuffer,
                                       bool useAsync,
                                       ILEMemHandle key,
                                       int keyLength,
                                       int keyParts)
{
  DBUG_ENTER("db2i_ioBuffers::newReadRequest");
  DBUG_ASSERT(rowsToBuffer <= getRowCapacity());
#ifndef DBUG_OFF
  if (readCursor < rowCount())
    DBUG_PRINT("PERF:",("Wasting %d buffered rows!\n", rowCount() - readCursor));
#endif

  int fildes[2];
  int ileDescriptor = QMY_REUSE;

  interruptRead();

  if (likely(useAsync))
  {
    if (rowsToBuffer == 1)
    {
      // Async provides little or no benefit for single row reads, so we turn it off
      DBUG_PRINT("db2i_ioBuffers::newReadRequest", ("Disabling async"));
      useAsync = false;
    }
    else
    {          
      rc = pipe(fildes);
      if (rc) DBUG_VOID_RETURN;

      // Translate the pipe write descriptor into the equivalent ILE descriptor
      rc = fstatx(fildes[1], (struct stat*)&ileDescriptor, sizeof(ileDescriptor), STX_XPFFD_PASE);
      if (rc)
      {
        close(fildes[0]);
        close(fildes[1]);
        DBUG_VOID_RETURN;
      }
      pipeState = Untouched;
      msgPipe = fildes[0];
      
      DBUG_PRINT("db2i_ioBuffers::newReadRequest", ("Opened pipe %d", fildes[0]));
    }
  }

  file = infile;
  readIsAsync = useAsync;
  rowsToBlock = rowsToBuffer;

  rewind();
  maxRows() = 1;
  rc = getBridge()->expectErrors(QMY_ERR_END_OF_BLOCK, QMY_ERR_LOB_SPACE_TOO_SMALL)
                  ->read(file,
                        ptr(),
                        accessIntent,
                        commitLevel,
                        orientation,
                        useAsync,
                        rrnList,
                        key, 
                        keyLength,
                        keyParts,
                        ileDescriptor);

  // Having shared the pipe with ILE, we relinquish our claim on the write end
  // of the pipe.
  if (useAsync)
    close(fildes[1]);

  // If we reach EOF or end-of-key, DB2 guarantees that no rows will be locked.
  if (rc == QMY_ERR_END_OF_FILE)
  {
    rc = HA_ERR_END_OF_FILE;
    *releaseRowNeeded = false;
  }
  else if (rc == QMY_ERR_KEY_NOT_FOUND)
  {
    if (rowCount())
      rc = HA_ERR_END_OF_FILE;
    else
      rc = HA_ERR_KEY_NOT_FOUND;
    *releaseRowNeeded = false;
  }
  else
    *releaseRowNeeded = true;

  DBUG_VOID_RETURN;
}
