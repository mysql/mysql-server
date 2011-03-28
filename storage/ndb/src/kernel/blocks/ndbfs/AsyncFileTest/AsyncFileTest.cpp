/*
   Copyright (C) 2003-2006 MySQL AB
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

//#define TESTDEBUG 1

#include <ndb_global.h>

#include <kernel_types.h>
#include <Pool.hpp>
#include "AsyncFile.hpp"
#include "NdbOut.hpp"
#include "NdbTick.h"
#include "NdbThread.h"
#include "NdbMain.h"

// Test and benchmark functionality of AsyncFile
// -n Number of files
// -r Number of simultaneous requests
// -s Filesize, number of pages
// -l Number of iterations
// -remove, remove files after close
// -reverse, write files in reverse order, start with the last page

#define MAXFILES 255
#define DEFAULT_NUM_FILES 1
#define MAXREQUESTS 256
#define DEFAULT_NUM_REQUESTS 1
#define MAXFILESIZE 4096
#define DEFAULT_FILESIZE 2048
#define FVERSION 0x01000000
#define PAGESIZE 8192

#define TIMER_START { Uint64 starttick = NdbTick_CurrentMillisecond()
#define TIMER_PRINT(str, ops) Uint64 stoptick = NdbTick_CurrentMillisecond();\
            Uint64 totaltime = (stoptick-starttick);  \
            ndbout << ops << " " << str << \
	    " total time " << (int)totaltime << "ms" << endl;\
            char buf[255];\
            sprintf(buf,  "%d %s/sec\n",(int)((ops*1000)/totaltime), str);\
            ndbout <<buf << endl;}

static int numberOfFiles = DEFAULT_NUM_FILES;
static int numberOfRequests = DEFAULT_NUM_REQUESTS;
static int fileSize = DEFAULT_FILESIZE;
static int removeFiles = 0;
static int writeFilesReverse = 0;
static int numberOfIterations = 1;
Uint32 FileNameArray[4];

Pool<AsyncFile>* files;
AsyncFile* openFiles[MAXFILES];
Pool<Request>* theRequestPool;
MemoryChannelMultipleWriter<Request>* theReportChannel;

char WritePages[MAXFILES][PAGESIZE];
char ReadPages[MAXFILES][PAGESIZE];

int readArguments(int argc, const char** argv);
int openFile(int fileNum);
int openFileWait();
int closeFile(int fileNum);
int closeFileWait();
int writeFile( int fileNum, int pagenum);
int writeFileWait();
int writeSyncFile( int fileNum, int pagenum);
int writeSyncFileWait();
int readFile( int fileNum, int pagenum);
int readFileWait();


NDB_COMMAND(aftest, "aftest", "aftest [-n <Number of files>] [-r <Number of simultaneous requests>] [-s <Filesize, number of pages>] [-l <Number of iterations>] [-remove, remove files after close] [-reverse, write files in reverse order, start with the last page]", "Test the AsyncFile class of Ndb", 8192)
{
  int s, numReq, numOps;

  readArguments(argc, argv);
   
  files = new Pool<AsyncFile>(numberOfFiles, 2);
  theRequestPool = new Pool<Request>;
  theReportChannel = new MemoryChannelMultipleWriter<Request>;

  ndbout << "AsyncFileTest starting" << endl;
  ndbout << "  " << numberOfFiles << " files" << endl;
  ndbout << "  " << numberOfRequests << " requests" << endl;
  ndbout << "  " << fileSize << " * 8k files" << endl << endl;
  ndbout << "  " << numberOfIterations << " iterations" << endl << endl;

  NdbThread_SetConcurrencyLevel(numberOfFiles+2);

  // initialize data to write to files
  for (int i = 0; i < MAXFILES; i++) {
    for (int j = 0; j < PAGESIZE; j++){
      WritePages[i][j] = (64+i+j)%256;
    }
      //      memset(&WritePages[i][0], i+64, PAGESIZE);
  }

  // Set file directory and name
  // /T27/F27/NDBFS/S27Pnn.data
  FileNameArray[0] = 27; // T27
  FileNameArray[1] = 27; // F27
  FileNameArray[2] = 27; // S27 
  FileNameArray[3] = FVERSION; // Version
  
  for (int l = 0; l < numberOfIterations; l++)
    {

  ndbout << "Opening files" << endl;
  // Open files
  for (int f = 0; f < numberOfFiles; f++)
  {
      openFile(f);
 
  }

  // Wait for answer
  openFileWait();

  ndbout << "Files opened!" << endl<< endl;

  // Write to files
  ndbout << "Started writing" << endl;
  TIMER_START;
  s = 0;
  numReq = 0;
  numOps = 0;
  while ( s < fileSize)
  {
    for (int r = 0; r < numberOfRequests; r++)
    {
      for (int f = 0; f < numberOfFiles; f++)
      {
	writeFile(f, s);
	numReq++;
	numOps++;
      }
      
      s++;
    }
    
    while (numReq > 0)
      {	
	writeFileWait();
	numReq--;
      }

  }

  TIMER_PRINT("writes", numOps);


  ndbout << "Started reading" << endl;
  TIMER_START;

  // Read from files
  s = 0;
  numReq = 0;
  numOps = 0;
  while ( s < fileSize)
  {
    for (int r = 0; r < numberOfRequests; r++)
    {
      for (int f = 0; f < numberOfFiles; f++)
      {
	readFile(f, s);
	numReq++;
	numOps++;
      }
      
      s++;
      
    }
    
    while (numReq > 0)
      {	
	readFileWait();
	numReq--;
      }

  }
  TIMER_PRINT("reads", numOps);

  ndbout << "Started writing with sync" << endl;
  TIMER_START;

  // Write to files
  s = 0;
  numReq = 0;
  numOps = 0;
  while ( s < fileSize)
  {
    for (int r = 0; r < numberOfRequests; r++)
    {
      for (int f = 0; f < numberOfFiles; f++)
      {
	writeSyncFile(f, s);
	numReq++;
	numOps++;
      }
      
      s++;
    }
    
    while (numReq > 0)
      {	
	writeSyncFileWait();
	numReq--;
      }

  }

  TIMER_PRINT("writeSync", numOps);
  
  // Close files
  ndbout << "Closing files" << endl;
  for (int f = 0; f < numberOfFiles; f++)
  {
      closeFile(f);
 
  }

  // Wait for answer
  closeFileWait();

  ndbout << "Files closed!" << endl<< endl;
    }

  // Deallocate memory
  delete files;
  delete theReportChannel;
  delete theRequestPool;

  return 0;

}



int forward( AsyncFile * file, Request* request )
{
   file->execute(request);
   ERROR_CHECK 0;
   return 1;
}

int openFile( int fileNum)
{
  AsyncFile* file = (AsyncFile *)files->get();

  FileNameArray[3] = fileNum | FVERSION;
  file->fileName().set( NDBFS_REF, &FileNameArray[0] );
  ndbout << "openFile: " << file->fileName().c_str() << endl;
  
  if( ERROR_STATE ) {
     ERROR_RESET;
     files->put( file );
     ndbout <<  "Failed to set filename" << endl;
     return 1;
  }
  file->reportTo(theReportChannel);
          
  Request* request = theRequestPool->get();
  request->action= Request::open;
  request->error= 0;
  request->par.open.flags = 0x302; //O_RDWR | O_CREAT | O_TRUNC ; // 770
  request->set(NDBFS_REF, 0x23456789, fileNum );
  request->file = file;
  
  if (!forward(file,request)) {
     // Something went wrong
     ndbout << "Could not forward open request" << endl;
     theRequestPool->put(request);
     return 1;
  }
  return 0;
}

int closeFile( int fileNum)
{

  AsyncFile* file = openFiles[fileNum];
          
  Request* request = theRequestPool->get();
  if (removeFiles == 1)
    request->action = Request::closeRemove;
  else
    request->action= Request::close;

  request->error= 0;
  request->set(NDBFS_REF, 0x23456789, fileNum );
  request->file = file;
  
  if (!forward(file,request)) {
     // Something went wrong
     ndbout << "Could not forward close request" << endl;
     theRequestPool->put(request);
     return 1;
  }
  return 0;
}

int writeFile( int fileNum, int pagenum)
{
  AsyncFile* file = openFiles[fileNum];
#ifdef TESTDEBUG
  ndbout << "writeFile" << fileNum <<": "<<pagenum<<", " << file->fileName().c_str()<< endl;
#endif
  Request *request = theRequestPool->get();
  request->action = Request::write;
  request->error = 0;
  request->set(NDBFS_REF, pagenum, fileNum);
  request->file = openFiles[fileNum];

  // Write only one page, choose the correct page for each file using fileNum
  request->par.readWrite.pages[0].buf = &WritePages[fileNum][0];
  request->par.readWrite.pages[0].size = PAGESIZE;
  if (writeFilesReverse == 1)
  {
    // write the last page in the files first
    // This is a normal way for the Blocks in Ndb to write to a file
     request->par.readWrite.pages[0].offset = (fileSize - pagenum - 1) * PAGESIZE;
  }
  else
  {
     request->par.readWrite.pages[0].offset = pagenum * PAGESIZE;
  }
  request->par.readWrite.numberOfPages = 1;
  
  if (!forward(file,request)) {
     // Something went wrong
     ndbout << "Could not forward write request" << endl;
     theRequestPool->put(request);
     return 1;
  }
  return 0;

}

int writeSyncFile( int fileNum, int pagenum)
{
  AsyncFile* file = openFiles[fileNum];
#ifdef TESTDEBUG
  ndbout << "writeFile" << fileNum <<": "<<pagenum<<", " << file->fileName().c_str() << endl;
#endif
  Request *request = theRequestPool->get();
  request->action = Request::writeSync;
  request->error = 0;
  request->set(NDBFS_REF, pagenum, fileNum);
  request->file = openFiles[fileNum];

  // Write only one page, choose the correct page for each file using fileNum
  request->par.readWrite.pages[0].buf = &WritePages[fileNum][0];
  request->par.readWrite.pages[0].size = PAGESIZE;
  request->par.readWrite.pages[0].offset = pagenum * PAGESIZE;
  request->par.readWrite.numberOfPages = 1;
  
  if (!forward(file,request)) {
     // Something went wrong
     ndbout << "Could not forward write request" << endl;
     theRequestPool->put(request);
     return 1;
  }
  return 0;

}

int readFile( int fileNum, int pagenum)
{
  AsyncFile* file = openFiles[fileNum];
#ifdef TESTDEBUG
  ndbout << "readFile" << fileNum <<": "<<pagenum<<", " << file->fileName().c_str() << endl;
#endif
  Request *request = theRequestPool->get();
  request->action = Request::read;
  request->error = 0;
  request->set(NDBFS_REF, pagenum, fileNum);
  request->file = openFiles[fileNum];

  // Read only one page, choose the correct page for each file using fileNum
  request->par.readWrite.pages[0].buf = &ReadPages[fileNum][0];
  request->par.readWrite.pages[0].size = PAGESIZE;
  request->par.readWrite.pages[0].offset = pagenum * PAGESIZE;
  request->par.readWrite.numberOfPages = 1;
  
  if (!forward(file,request)) {
     // Something went wrong
     ndbout << "Could not forward read request" << endl;
     theRequestPool->put(request);
     return 1;
  }
  return 0;

}

int openFileWait()
{
  int openedFiles = 0;
  while (openedFiles < numberOfFiles)
    {
      Request* request = theReportChannel->readChannel();
      if (request) 
	{
	  if (request->action == Request::open)
	    {	      
	      if (request->error ==0)
		{
#ifdef TESTDEBUG
  	          ndbout << "Opened file " << request->file->fileName().c_str() << endl;
#endif
		  openFiles[request->theFilePointer] = request->file;
		}
	      else
		{
		  ndbout << "error while opening file" << endl;
		  exit(1);
		}
	      theRequestPool->put(request);
	      openedFiles++;
	    }
	  else
	    {
	      ndbout << "Unexpected request received" << endl;
	    }
	}
      else
	{
	  ndbout << "Nothing read from theReportChannel" << endl;
	}
    }
  return 0;
}

int closeFileWait()
{
  int closedFiles = 0;
  while (closedFiles < numberOfFiles)
    {
      Request* request = theReportChannel->readChannel();
      if (request) 
	{
	  if (request->action == Request::close || request->action == Request::closeRemove)
	    {	      
	      if (request->error ==0)
		{
#ifdef TESTDEBUG
  	          ndbout << "Closed file " << request->file->fileName().c_str() << endl;
#endif
		  openFiles[request->theFilePointer] = NULL;
		  files->put(request->file);
		}
	      else
		{
		  ndbout << "error while closing file" << endl;
		  exit(1);
		}
	      theRequestPool->put(request);
	      closedFiles++;
	    }
	  else
	    {
	      ndbout << "Unexpected request received" << endl;
	    }
	}
      else
	{
	  ndbout << "Nothing read from theReportChannel" << endl;
	}
    }
  return 0;
}

int writeFileWait()
{
  Request* request = theReportChannel->readChannel();
  if (request) 
    {
      if (request->action == Request::write)
	{	      
	  if (request->error == 0)
	    {
#ifdef TESTDEBUG
	      ndbout << "writeFileWait"<<request->theFilePointer<<", " << request->theUserPointer<<" "<< request->file->fileName().c_str() << endl;
#endif

	    }
	  else
	    {
	      ndbout << "error while writing file, error=" << request->error << endl;
	      exit(1);
	    }
	  theRequestPool->put(request);
	}
      else
	{
	  ndbout << "Unexpected request received" << endl;
	}
    }
  else
    {
      ndbout << "Nothing read from theReportChannel" << endl;
    }
  return 0;
}

int writeSyncFileWait()
{
  Request* request = theReportChannel->readChannel();
  if (request) 
    {
      if (request->action == Request::writeSync)
	{	      
	  if (request->error == 0)
	    {
#ifdef TESTDEBUG
	      ndbout << "writeFileWait"<<request->theFilePointer<<", " << request->theUserPointer<<" "<< request->file->fileName().c_str() << endl;
#endif

	    }
	  else
	    {
	      ndbout << "error while writing file" << endl;
	      exit(1);
	    }
	  theRequestPool->put(request);
	}
      else
	{
	  ndbout << "Unexpected request received" << endl;
	}
    }
  else
    {
      ndbout << "Nothing read from theReportChannel" << endl;
    }
  return 0;
}

int readFileWait()
{
  Request* request = theReportChannel->readChannel();
  if (request) 
    {
      if (request->action == Request::read)
	{	      
	  if (request->error == 0)
	    {
#ifdef TESTDEBUG
	      ndbout << "readFileWait"<<request->theFilePointer<<", " << request->theUserPointer<<" "<< request->file->fileName().c_str() << endl;
#endif
	      if (memcmp(&(ReadPages[request->theFilePointer][0]), &(WritePages[request->theFilePointer][0]), PAGESIZE)!=0) 
		{
		  ndbout <<"Verification error!" << endl;
		  for (int i = 0; i < PAGESIZE; i++ ){
		    ndbout <<" Compare Page " <<  i << " : " << ReadPages[request->theFilePointer][i] <<", " <<WritePages[request->theFilePointer][i] << endl;;
		    if( ReadPages[request->theFilePointer][i] !=WritePages[request->theFilePointer][i])
		
		      exit(1);
		       }
		}
		
	    }
	  else
	    {
	      ndbout << "error while reading file" << endl;
	      exit(1);
	    }
	  theRequestPool->put(request);
	}
      else
	{
	  ndbout << "Unexpected request received" << endl;
	}
    }
  else
    {
      ndbout << "Nothing read from theReportChannel" << endl;
    }
  return 0;
}

int readArguments(int argc, const char** argv)
{

  int i = 1;
  while (argc > 1)
  {
    if (strcmp(argv[i], "-n") == 0)
    {
      numberOfFiles = atoi(argv[i+1]);
      if ((numberOfFiles < 1) || (numberOfFiles > MAXFILES))
      {
	ndbout << "Wrong number of files, default = "<<DEFAULT_NUM_FILES << endl;
	  numberOfFiles = DEFAULT_NUM_FILES;
      }
    }
    else if (strcmp(argv[i], "-r") == 0)
    {
      numberOfRequests = atoi(argv[i+1]);
      if ((numberOfRequests < 1) || (numberOfRequests > MAXREQUESTS))
      {
	ndbout << "Wrong number of requests, default = "<<DEFAULT_NUM_REQUESTS << endl;
	  numberOfRequests = DEFAULT_NUM_REQUESTS;
      }
    }
    else if (strcmp(argv[i], "-s") == 0)
    {
      fileSize = atoi(argv[i+1]);
      if ((fileSize < 1) || (fileSize > MAXFILESIZE))
      {
	ndbout << "Wrong number of 8k pages, default = "<<DEFAULT_FILESIZE << endl;
	  fileSize = DEFAULT_FILESIZE;
      }
    }
    else if (strcmp(argv[i], "-l") == 0)
    {
      numberOfIterations = atoi(argv[i+1]);
      if ((numberOfIterations < 1))
      {
	ndbout << "Wrong number of iterations, default = 1" << endl;
	numberOfIterations = 1;
      }
    }
    else if (strcmp(argv[i], "-remove") == 0)
    {
      removeFiles = 1;
      argc++;
      i--;
    }
    else if (strcmp(argv[i], "-reverse") == 0)
    {
      ndbout << "Writing files reversed" << endl;
      writeFilesReverse = 1;
      argc++;
      i--;
    }

    argc -= 2;
    i = i + 2;
  }
  
  if ((fileSize % numberOfRequests)!= 0)
    {
    numberOfRequests = numberOfRequests - (fileSize % numberOfRequests);
    ndbout <<"numberOfRequest must be modulo of filesize" << endl;
    ndbout << "New numberOfRequest="<<numberOfRequests<<endl;
    }
  return 0;
}


// Needed for linking...

void ErrorReporter::handleError(ErrorCategory type, int messageID,
                                const char* problemData, const char* objRef, NdbShutdownType stype)
{

  ndbout << "ErrorReporter::handleError activated" << endl;
  ndbout << "type= " << type << endl; 
  ndbout << "messageID= " << messageID << endl;
  ndbout << "problemData= " << problemData << endl;
  ndbout << "objRef= " << objRef << endl;

  exit(1);
}

void ErrorReporter::handleAssert(const char* message, const char* file, int line)
{
  ndbout << "ErrorReporter::handleAssert activated" << endl;
  ndbout << "message= " << message << endl;
  ndbout << "file= " << file << endl;
  ndbout << "line= " << line << endl;
  exit(1);
}


GlobalData globalData;


Signal::Signal()
{

}

