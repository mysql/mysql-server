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

#ifndef AsyncFile_H
#define AsyncFile_H

//===========================================================================
//
// .DESCRIPTION
//    Asynchronous file, All actions are executed concurrently with other 
//    activity of the process. 
//    Because all action are performed in a seperated thread the result of 
//    of a action is send back tru a memory channel. 
//    For the asyncronise notivication of a finished request all the calls
//    have a request as paramater, the user can use the userData pointer
//    to add information it needs when the request is send back.
//      
//
// .TYPICAL USE:
//      Writing or reading data to/from disk concurrently to other activities.
//
//===========================================================================
//=============================================================================
//
// .PUBLIC
//
//=============================================================================
///////////////////////////////////////////////////////////////////////////////
//
// AsyncFile( );
// Description:
//   Initialisation of the class. 
// Parameters:
//      -
///////////////////////////////////////////////////////////////////////////////
//
// ~AsyncFile( );
// Description:
//   Tell the thread to stop and wait for it to return
// Parameters:
//      -
///////////////////////////////////////////////////////////////////////////////
// 
// doStart( );
// Description: 
//   Spawns the new  thread.
// Parameters:
//  Base path of filesystem
//
///////////////////////////////////////////////////////////////////////////////
//
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
//      
///////////////////////////////////////////////////////////////////////////////
//
// void reportTo( MemoryChannel<Request> *reportTo );
// Description:
//   set the channel where the file must report the result of the 
//    actions back to.
// Parameters:
//    reportTo: the memory channel to use use MemoryChannelMultipleWriter 
//              if more 
//              than one file uses this channel to report back.
//      
///////////////////////////////////////////////////////////////////////////////

#include <kernel_types.h>
#include "MemoryChannel.hpp"
#include "Filename.hpp"

const int ERR_ReadUnderflow = 1000;

const int WRITECHUNK = 262144;

class AsyncFile;

class Request
{
public:
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
    rmrf
  };
  Action action;
  union {
    struct {
      Uint32 flags;
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
};


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
public:
  AsyncFile();
  ~AsyncFile();
  
  void reportTo( MemoryChannel<Request> *reportTo );
  
  void execute( Request* request );
  
  void doStart(Uint32 nodeId, const char * fspath);
  // its a thread so its always running
  void run();   

  bool isOpen();

  Filename theFileName;
private:
  
  void openReq(Request *request);
  void readReq(Request *request);
  void readvReq(Request *request);
  void writeReq(Request *request);
  void writevReq(Request *request);
  
  void closeReq(Request *request);
  void syncReq(Request *request);
  void removeReq(Request *request);
  void appendReq(Request *request);
  void rmrfReq(Request *request, char * path, bool removePath);
  void endReq();
  
  int readBuffer(char * buf, size_t size, off_t offset);
  int writeBuffer(const char * buf, size_t size, off_t offset,
		  size_t chunk_size = WRITECHUNK);
  
  int extendfile(Request* request);
  void createDirectories();
    
#ifdef NDB_WIN32
  HANDLE hFile;
#else
  int theFd;
#endif
  
  MemoryChannel<Request> *theReportTo;
  MemoryChannel<Request>* theMemoryChannelPtr;
  
  struct NdbThread* theThreadPtr;
  NdbMutex* theStartMutexPtr;
  NdbCondition* theStartConditionPtr;
  bool   theStartFlag;
  int theWriteBufferSize;
  char* theWriteBuffer;
  
  bool m_openedWithSync;
  Uint32 m_syncCount;
  Uint32 m_syncFrequency;
};

#endif
