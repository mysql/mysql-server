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


/**
  @file db2i_ioBuffers.h
  
  @brief Buffer classes used for interacting with QMYSE read/write buffers.
  
*/


#include "db2i_validatedPointer.h"
#include "mysql_priv.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <as400_types.h>

// Needed for compilers which do not include fstatx in standard headers.
extern "C" int fstatx(int, struct stat *, int, int);

/**
  Basic row buffer
  
  Provides the basic structure and methods needed for communicating
  with QMYSE I/O APIs.
  
  @details All QMYSE I/O apis use a buffer that is structured as two integer
  row counts (max and used) and storage for some number of rows. The row counts
  are both input and output for the API, and their usage depends on the 
  particular API invoked. This class encapsulates that buffer definition.
*/
class IORowBuffer
{
  public:
    IORowBuffer() : allocSize(0), rowLength(0) {;} 
    ~IORowBuffer() { freeBuf(); }
    ValidatedPointer<char>& ptr() { return data; }
    
    /**
      Sets up the buffer to hold the size indicated.
      
      @param rowLen  length of the rows that will be stored in this buffer
      @param nullMapOffset  position of null map within each row
      @param size    buffer size requested
    */
    void allocBuf(uint32 rowLen, uint16 nullMapOffset, uint32 size)
    {
      nullOffset = nullMapOffset;
      uint32 newSize = size + sizeof(BufferHdr_t);
        // If the internal structure of the row is changing, we need to
        // remember this and notify the subclasses via initAfterAllocate();
      bool formatChanged = ((size/rowLen) != rowCapacity);
      
      if (newSize > allocSize)
      {
        this->freeBuf();
        data.alloc(newSize);
        if (likely((void*)data))
          allocSize = newSize;        
      }
      
      if (likely((void*)data))
      {
        DBUG_ASSERT((uint64)(void*)data % 16 == 0);
        rowLength = rowLen;
        rowCapacity = size / rowLength;
        initAfterAllocate(formatChanged);
      }
      else
      {
        allocSize = 0;
        rowCapacity = 0;
      }
      
      DBUG_PRINT("db2i_ioBuffers::allocBuf",("rowCapacity = %d", rowCapacity));
    }
   
    void zeroBuf()
    {
      memset(data, 0, allocSize);
    }

    void freeBuf()
    {
      if (likely(allocSize))
      {
        prepForFree();
        DBUG_PRINT("IORowBuffer::freeBuf",("Freeing 0x%p", (char*)data));
        data.dealloc();
      }
    }

    char* getRowN(uint32 n)
    {
      if (unlikely(n >= getRowCapacity()))
        return NULL;
      return (char*)data + sizeof(BufferHdr_t) + (rowLength * n);
    };

    uint32 getRowCapacity() const {return rowCapacity;}
    uint32 getRowNullOffset() const {return nullOffset;}
    uint32 getRowLength() const {return rowLength;}
        
  protected: 
    /**
      Called prior to freeing buffer storage so that subclasses can do
      any required cleanup      
    */
    virtual void prepForFree()
    { 
      allocSize = 0;
      rowCapacity = 0;
    }
    
    /**
      Called after buffer storage so that subclasses can do any required setup.
    */
    virtual void initAfterAllocate(bool sizeChanged) { return;}

    ValidatedPointer<char> data;
    uint32 allocSize;
    uint32 rowCapacity;
    uint32 rowLength;
    uint16 nullOffset;
    uint32& usedRows() const { return ((BufferHdr_t*)(char*)data)->UsedRowCnt; }
    uint32& maxRows() const {return ((BufferHdr_t*)(char*)data)->MaxRowCnt; }
};


/**
  Write buffer
  
  Implements methods for inserting data into a row buffer for use with the
  QMY_WRITE and QMY_UPDATE APIs.
  
  @details The max row count defines how many rows are in the buffer. The used 
  row count is updated by QMYSE to indicate how many rows have been 
  successfully written.
*/
class IOWriteBuffer : public IORowBuffer
{
  public: 
    bool endOfBuffer() const {return (maxRows() == getRowCapacity());}
  
    char* addRow()
    {
      return getRowN(maxRows()++);
    }
    
    void resetAfterWrite()
    {
      maxRows() = 0;
    }
    
    void deleteRow()
    {
      --maxRows();
    }
    
    uint32 rowCount() const {return maxRows();}
    
    uint32 rowsWritten() const {return usedRows()-1;}
    
  private: 
    void initAfterAllocate(bool sizeChanged) {maxRows() = 0; usedRows() = 0;}
};


/**
  Read buffer
  
  Implements methods for reading data from and managing a row buffer for use 
  with the QMY_READ APIs. This is primarily for use with metainformation queries.
*/
class IOReadBuffer : public IORowBuffer
{
  public: 
        
    IOReadBuffer() {;}
    IOReadBuffer(uint32 rows, uint32 rowLength)
    {
      allocBuf(rows, 0, rows * rowLength);
      maxRows() = rows;
    }
    
    uint32 rowCount() {return usedRows();}
    void setRowsToProcess(uint32 rows) { maxRows() = rows; }
};


/**
  Read buffer
  
  Implements methods for reading data from and managing a row buffer for use 
  with the QMY_READ APIs.
  
  @details This class supports both sync and async read modes.  The max row
  count defines the number of rows that are requested to be read. The used row
  count defines how many rows have been read. Sync mode is  reasonably
  straightforward, but async mode has a complex system of communicating with
  QMYSE that is optimized for low latency. In async mode, the used row count is
  updated continuously by QMYSE as rows are read. At the same time, messages are
  sent to the associated pipe indicating that a row has been read. As long as
  the internal read cursor lags behind the used row count,  the pipe is never
  consulted. But if the internal read cursor "catches up to" the used row count,
  then we block on the pipe until we find a message indicating that  a new row
  has been read or that an error has occurred.
*/
class IOAsyncReadBuffer : public IOReadBuffer
{
  public: 
    IOAsyncReadBuffer() : 
      file(0), readIsAsync(false), msgPipe(QMY_REUSE), bridge(NULL)
    {
    }
      
    ~IOAsyncReadBuffer() 
    {
      interruptRead();
      rrnList.dealloc();
    }

    
    /**
      Signal read operation complete
    
      Indicates that the storage engine requires no more data from the table.
      Must be called between calls to newReadRequest().      
    */
    void endRead()
    {
#ifndef DBUG_OFF
      if (readCursor < rowCount())
        DBUG_PRINT("PERF:",("Wasting %d buffered rows!\n", rowCount() - readCursor));
#endif
      interruptRead();
      
      file = 0;
      bridge = NULL;
    }
    
    /**
      Update data that may change on each read operation
    */
    void update(char newAccessIntent, 
              bool* newReleaseRowNeeded,
              char commitLvl)
    {
      accessIntent = newAccessIntent;
      releaseRowNeeded = newReleaseRowNeeded;
      commitLevel = commitLvl;
    }
    
    /**
      Read the next row in the table.
      
      Return a pointer to the next row in the table, where "next" is defined
      by the orientation.
      
      @param orientaton
      @param[out] rrn The relative record number of the row returned. Not reliable
                      if NULL is returned by this function.
      
      @return Pointer to the row. Null if no more rows are available or an error
              occurred.
    */
    char* readNextRow(char orientation, uint32& rrn)
    {
      DBUG_PRINT("db2i_ioBuffers::readNextRow", ("readCursor: %d, filledRows: %d, rc: %d", readCursor, rowCount(), rc));
      
      while (readCursor >= rowCount() && !rc)
      {
        if (!readIsAsync)
          loadNewRows(orientation);
        else
          pollNextRow(orientation);
      }
      
      if (readCursor >= rowCount())
        return NULL;
      
      rrn = rrnList[readCursor];      
      return getRowN(readCursor++);
    }
    
    /**
      Retrieve the return code generated by the last operation.
                  
      @return The return code, translated to the appropriate HA_ERR_*
              value if possible.
    */
    int32 lastrc()
    {
      return db2i_ileBridge::translateErrorCode(rc);
    }
        
    void rewind()
    {
      readCursor = 0;
      rc = 0;
      usedRows() = 0;
    }
    
    bool reachedEOD() { return EOD; }
    
    void newReadRequest(FILE_HANDLE infile,
                         char orientation,
                         uint32 rowsToBuffer,
                         bool useAsync,
                         ILEMemHandle key,
                         int keyLength,
                         int keyParts);
  
  private:       
    
    /**
      End any running async read operation.
    */
    void interruptRead()
    {
      closePipe();
      if (file && readIsAsync && (rc == 0) && (rowCount() < getRowCapacity()))
      {
        DBUG_PRINT("IOReadBuffer::interruptRead", ("PERF: Interrupting %d", (uint32)file));
        getBridge()->readInterrupt(file);
      }
    }
    
    void closePipe()
    {
      if (msgPipe != QMY_REUSE)
      {
        DBUG_PRINT("db2i_ioBuffers::closePipe", ("Closing pipe %d", msgPipe));
        close(msgPipe);
        msgPipe = QMY_REUSE;
      }
    }
    
    /**
      Get a pointer to the active ILE bridge.
      
      Getting the bridge pointer is (relatively) expensive, so we cache
      it off for each operation.
    */
    db2i_ileBridge* getBridge()
    {
      if (unlikely(bridge == NULL))
      {
        bridge = db2i_ileBridge::getBridgeForThread();
      }
      return bridge;
    }
    
    void drainPipe();
    void pollNextRow(char orientation);
    void prepForFree();
    void initAfterAllocate(bool sizeChanged);
    void loadNewRows(char orientation);

    
    uint32 readCursor;                          // Read position within buffer
    int32 rc;                                   // Last return code received
    ValidatedPointer<uint32> rrnList;           // Receiver for list of rrns
    char accessIntent;                          // The access intent for this read
    char commitLevel;                           // What isolation level should be used
    char EOD;                                   // Whether end-of-data was hit
    char readIsAsync;                           // Are reads to be done asynchronously?
    bool* releaseRowNeeded;                     
        /* Does the caller need to release the current row when finished reading */
    FILE_HANDLE file;                           // The file to be read
    int msgPipe;                                
        /* The read descriptor of the pipe used to pass messages during async reads */
    db2i_ileBridge* bridge;                     // Cached pointer to bridge
    uint32 rowsToBlock;                         // Number of rows to request
    enum
    {
      ConsumedFullBufferMsg,
      PendingFullBufferMsg,
      Untouched
    } pipeState;
        /* The state of the async read message pipe */
};

