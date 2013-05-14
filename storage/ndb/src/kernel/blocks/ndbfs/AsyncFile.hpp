/* Copyright (c) 2003-2007 MySQL AB


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */


#ifndef AsyncFile_H
#define AsyncFile_H

/**
   AsyncFile

   All file operations executed in thread-per-file, away from the DB threads.
 */


// void execute(Request *request);
// Description:
//   performens the requered action.
// Parameters:
//    request: request to be called when open is finished.
//       action= open|close|read|write|sync
//          if action is open then:
//             par.open.flags= UNIX open flags, see man open
//             par.open.name= name of the file to open
//          if action is read or write then:
//             par.readWrite.buf= user provided buffer to read/write 
//             the data from/to
//             par.readWrite.size= how many bytes must be read/written
//             par.readWrite.offset= absolute offset in file in bytes
// return:
//    return values are stored in the request error field:
//       error= return state of the action, UNIX error see man open/errno
//       userData= is untouched can be used be user.



// void reportTo( MemoryChannel<Request> *reportTo );
// Description:
//   set the channel where the file must report the result of the 
//    actions back to.
// Parameters:
//    reportTo: the memory channel to use use MemoryChannelMultipleWriter 
//              if more 
//              than one file uses this channel to report back.

#include <kernel_types.h>
#include "MemoryChannel.hpp"
#include "Filename.hpp"
#include <signaldata/BuildIndx.hpp>

// Use this define if you want printouts from AsyncFile class
//#define DEBUG_ASYNCFILE

#ifdef DEBUG_ASYNCFILE
#include <NdbOut.hpp>
#define DEBUG(x) x
#define PRINT_ERRORANDFLAGS(f) printErrorAndFlags(f)
void printErrorAndFlags(Uint32 used_flags);
#else
#define DEBUG(x)
#define PRINT_ERRORANDFLAGS(f)
#endif

// Define the size of the write buffer (for each thread)
#define WRITEBUFFERSIZE 262144

const int ERR_ReadUnderflow = 1000;

const int WRITECHUNK = 262144;

class AsyncFile;

class Request
{
public:
  Request() {}

  enum Action {
    open,
    close,
    closeRemove,
    read,   // Allways leave readv directly after 
            // read because SimblockAsyncFileSystem depends on it
    readv,
    write,// Allways leave writev directly after 
	        // write because SimblockAsyncFileSystem depends on it
    writev,
    writeSync,// Allways leave writevSync directly after 
    // writeSync because SimblockAsyncFileSystem depends on it
    writevSync,
    sync,
    end,
    append,
    append_synch,
    rmrf,
    readPartial,
    allocmem,
    buildindx,
    suspend
  };
  Action action;
  union {
    struct {
      Uint32 flags;
      Uint32 page_size;
      Uint64 file_size;
      Uint32 auto_sync_size;
    } open;
    struct {
      int numberOfPages;
      struct{
	char *buf;
	size_t size;
	off_t offset;
      } pages[16];
    } readWrite;
    struct {
      const char * buf;
      size_t size;
    } append;
    struct {
      bool directory;
      bool own_directory;
    } rmrf;
    struct {
      Block_context* ctx;
      Uint32 requestInfo;
    } alloc;
    struct {
      struct mt_BuildIndxReq m_req;
    } build;
    struct {
      Uint32 milliseconds;
    } suspend;
  } par;
  int error;
  
  void set(BlockReference userReference, 
	   Uint32 userPointer,
	   Uint16 filePointer);
  BlockReference theUserReference;
  Uint32 theUserPointer;
  Uint16 theFilePointer;
   // Information for open, needed if the first open action fails.
  AsyncFile* file;
  Uint32 theTrace;

  MemoryChannel<Request>::ListMember m_mem_channel;
};

NdbOut& operator <<(NdbOut&, const Request&);

inline
void 
Request::set(BlockReference userReference, 
	     Uint32 userPointer, Uint16 filePointer) 
{
  theUserReference= userReference;
  theUserPointer= userPointer;
  theFilePointer= filePointer;
}

class AsyncFile
{
  friend class Ndbfs;
public:
  AsyncFile(SimulatedBlock& fs);
  virtual ~AsyncFile() {};

  void reportTo( MemoryChannel<Request> *reportTo );

  void execute( Request* request );

  virtual void doStart();

  virtual void shutdown();

  // its a thread so its always running
  virtual void run();

  virtual bool isOpen() = 0;

  Filename theFileName;
  Request *m_current_request, *m_last_request;
private:

  /**
   * Implementers of AsyncFile interface
   * should implement the following
   */

  /**
   * init()
   *
   * Initialise buffers etc. After init(), ready to execute()
   * Called with theStartMutexPtr held.
   */
  virtual int init();

  /**
   * openReq() - open a file.
   */
  virtual void openReq(Request *request) = 0;

  /**
   * readBuffer - read into buffer
   */
  virtual int readBuffer(Request*, char * buf, size_t size, off_t offset)=0;

  /**
   * writeBuffer() - write into file
   */
  virtual int writeBuffer(const char * buf, size_t size, off_t offset,
		  size_t chunk_size = WRITECHUNK)=0;


  virtual void closeReq(Request *request)=0;
  virtual void syncReq(Request *request)=0;
  virtual void removeReq(Request *request)=0;
  virtual void appendReq(Request *request)=0;
  virtual void rmrfReq(Request *request, char * path, bool removePath)=0;
  virtual void createDirectories()=0;

  /**
   * Unlikely to need to implement these. readvReq for iovec
   */
protected:
  virtual void readReq(Request *request);
  virtual void readvReq(Request *request);

  /**
   * Unlikely to need to implement these, writeBuffer likely sufficient.
   * writevReq for iovec (not yet used)
   */
  virtual void writeReq(Request *request);
  virtual void writevReq(Request *request);

  /**
   * Allocate memory (in separate thread)
   */
  virtual void allocMemReq(Request*);

  /**
   * Build ordered index in multi-threaded fashion
   */
  void buildIndxReq(Request*);

  /**
   * endReq()
   *
   * Inverse to ::init(). Cleans up thread before it exits.
   */
  virtual void endReq();

private:
  /**
   * (end of what implementors need)
   */

  MemoryChannel<Request> *theReportTo;
  MemoryChannel<Request>* theMemoryChannelPtr;

  struct NdbThread* theThreadPtr;
  NdbMutex* theStartMutexPtr;
  NdbCondition* theStartConditionPtr;
  bool   theStartFlag;

protected:
  int theWriteBufferSize;
  char* theWriteBuffer;

  size_t m_write_wo_sync;  // Writes wo/ sync
  size_t m_auto_sync_freq; // Auto sync freq in bytes

public:
  SimulatedBlock& m_fs;

  Uint32 m_page_cnt;
  Ptr<GlobalPage> m_page_ptr;
};

#endif
