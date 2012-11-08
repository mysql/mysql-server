/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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


#include <ndb_global.h>

#include <ndbd_exit_codes.h>
#include "ErrorReporter.hpp"

#include <FastScheduler.hpp>
#include <DebuggerNames.hpp>
#include <NdbHost.h>
#include <NdbConfig.h>
#include <Configuration.hpp>
#include "EventLogger.hpp"
extern EventLogger * g_eventLogger;

#include "TimeModule.hpp"

#include <NdbAutoPtr.hpp>

#define MESSAGE_LENGTH 500

static int WriteMessage(int thrdMessageID,
			const char* thrdProblemData, 
			const char* thrdObjRef,
                        NdbShutdownType & nst);

static void dumpJam(FILE* jamStream, 
		    Uint32 thrdTheEmulatedJamIndex, 
		    const Uint32 thrdTheEmulatedJam[],
                    Uint32 aBlockNumber);

static
const char *
ndb_basename(const char * path)
{
  if (path == NULL)
    return NULL;

  const char separator = '/';
  const char * p = path + strlen(path);
  while (p > path && p[0] != separator)
    p--;

  if (p[0] == separator)
    return p + 1;

  return p;
}

const char*
ErrorReporter::formatTimeStampString(){
  TimeModule DateTime;          /* To create "theDateTimeString" */
  
  static char theDateTimeString[39]; 
  /* Used to store the generated timestamp */
  /* ex: "Wednesday 18 September 2000 - 18:54:37" */

  DateTime.setTimeStamp();
  
  BaseString::snprintf(theDateTimeString, 39, "%s %d %s %d - %s:%s:%s", 
	   DateTime.getDayName(), DateTime.getDayOfMonth(),
	   DateTime.getMonthName(), DateTime.getYear(), DateTime.getHour(),
	   DateTime.getMinute(), DateTime.getSecond());
  
  return (const char *)&theDateTimeString;
}

int
ErrorReporter::get_trace_no(){
  
  FILE *stream;
  unsigned int traceFileNo;
  
  char *file_name= NdbConfig_NextTraceFileName(globalData.ownId);
  NdbAutoPtr<char> tmp_aptr(file_name);

  /* 
   * Read last number from tracefile
   */  
  stream = fopen(file_name, "r+");
  if (stream == NULL){
    traceFileNo = 1;
  } else {
    char buf[255];
    fgets(buf, 255, stream);
    const int scan = sscanf(buf, "%u", &traceFileNo);
    if(scan != 1){
      traceFileNo = 1;
    }
    fclose(stream);
    traceFileNo++;
  }

  /**
   * Wrap tracefile no 
   */
  Uint32 tmp = globalEmulatorData.theConfiguration->maxNoOfErrorLogs();
  if (traceFileNo > tmp ) {
    traceFileNo = 1;
  }

  /**
   *  Save new number to the file
   */
  stream = fopen(file_name, "w");
  if(stream != NULL){
    fprintf(stream, "%u", traceFileNo);
    fclose(stream);
  }

  return traceFileNo;
}

// Using my_progname without including all of mysys
extern "C" const char* my_progname;

void
ErrorReporter::formatMessage(int thr_no,
                             Uint32 num_threads, int faultID,
			     const char* problemData, 
			     const char* objRef,
			     const char* theNameOfTheTraceFile,
			     char* messptr){
  int processId;
  ndbd_exit_classification cl;
  ndbd_exit_status st;
  const char *exit_msg = ndbd_exit_message(faultID, &cl);
  const char *exit_cl_msg = ndbd_exit_classification_message(cl, &st);
  const char *exit_st_msg = ndbd_exit_status_message(st);
  int sofar;

  processId = NdbHost_GetProcessId();
  char thrbuf[100] = "";
  if (thr_no >= 0)
  {
    BaseString::snprintf(thrbuf, sizeof(thrbuf), " thr: %u", thr_no);
  }

  BaseString::snprintf(messptr, MESSAGE_LENGTH,
                       "Time: %s\n"
                       "Status: %s\n"
                       "Message: %s (%s)\n"
                       "Error: %d\n"
                       "Error data: %s\n"
                       "Error object: %s\n"
                       "Program: %s\n"
                       "Pid: %d%s\n"
                       "Version: %s\n"
                       "Trace: %s",
                       formatTimeStampString() , 
                       exit_st_msg,
                       exit_msg, exit_cl_msg,
                       faultID, 
                       (problemData == NULL) ? "" : problemData, 
                       objRef, 
                       ndb_basename(my_progname),
                       processId, 
                       thrbuf,
                       NDB_VERSION_STRING,
                       theNameOfTheTraceFile ? 
                       theNameOfTheTraceFile : "<no tracefile>");

  if (theNameOfTheTraceFile)
  {
    sofar = (int)strlen(messptr);
    if(sofar < MESSAGE_LENGTH)
    {
      BaseString::snprintf(messptr + sofar, MESSAGE_LENGTH - sofar,
                           " [t%u..t%u]", 1, num_threads);
    }
  }

  sofar = (int)strlen(messptr);
  if(sofar < MESSAGE_LENGTH)
  {
    BaseString::snprintf(messptr + sofar, MESSAGE_LENGTH - sofar,
                         "\n"
                         "***EOM***\n");
  }

  // Add trailing blanks to get a fixed length of the message
  while (strlen(messptr) <= MESSAGE_LENGTH-3){
    strcat(messptr, " ");
  }
  
  messptr[MESSAGE_LENGTH -2]='\n';
  messptr[MESSAGE_LENGTH -1]=0;
  
  return;
}

NdbShutdownType ErrorReporter::s_errorHandlerShutdownType = NST_ErrorHandler;

void
ErrorReporter::handleAssert(const char* message, const char* file, int line, int ec)
{
  char refMessage[100];
  Uint32 jamBlockNumber;

#ifdef NO_EMULATED_JAM
  BaseString::snprintf(refMessage, 100, "file: %s lineNo: %d",
	   file, line);
#else
  const EmulatedJamBuffer *jamBuffer =
    (EmulatedJamBuffer *)NdbThread_GetTlsKey(NDB_THREAD_TLS_JAM);
  jamBlockNumber = jamBuffer->theEmulatedJamBlockNumber;
  const char *blockName = getBlockName(jamBlockNumber);

  BaseString::snprintf(refMessage, 100, "%s line: %d (block: %s)",
	   file, line, blockName);
#endif
  NdbShutdownType nst = s_errorHandlerShutdownType;
  WriteMessage(ec, message, refMessage, nst);

  NdbShutdown(ec, nst);
  exit(1);                                      // Deadcode
}

void
ErrorReporter::handleError(int messageID,
			   const char* problemData, 
			   const char* objRef,
			   NdbShutdownType nst)
{
  if(messageID == NDBD_EXIT_ERROR_INSERT)
  {
    nst = NST_ErrorInsert;
  } 
  else 
  {
    if (nst == NST_ErrorHandler)
      nst = s_errorHandlerShutdownType;
  }
  
  WriteMessage(messageID, ndb_basename(problemData), objRef, nst);

  g_eventLogger->info("%s", problemData);
  g_eventLogger->info("%s", objRef);

  NdbShutdown(messageID, nst);
  exit(1); // kill warning
}

int 
WriteMessage(int thrdMessageID,
	     const char* thrdProblemData, 
             const char* thrdObjRef,
             NdbShutdownType & nst){
  FILE *stream;
  unsigned offset;
  unsigned long maxOffset;  // Maximum size of file.
  char theMessage[MESSAGE_LENGTH];
  Uint32 thrdTheEmulatedJamIndex;
  const Uint32 *thrdTheEmulatedJam;
  Uint32 jamBlockNumber;

  Uint32 threadCount = globalScheduler.traceDumpGetNumThreads();
  int thr_no = globalScheduler.traceDumpGetCurrentThread();

  /**
   * Format trace file name
   */
  char *theTraceFileName= 0;
  if (globalData.ownId > 0)
    theTraceFileName= NdbConfig_TraceFileName(globalData.ownId,
					      ErrorReporter::get_trace_no());
  NdbAutoPtr<char> tmp_aptr1(theTraceFileName);
  
  // The first 69 bytes is info about the current offset
  Uint32 noMsg = globalEmulatorData.theConfiguration->maxNoOfErrorLogs();

  maxOffset = (69 + (noMsg * MESSAGE_LENGTH));
  
  char *theErrorFileName= (char *)NdbConfig_ErrorFileName(globalData.ownId);
  NdbAutoPtr<char> tmp_aptr2(theErrorFileName);

  stream = fopen(theErrorFileName, "r+");
  if (stream == NULL) { /* If the file could not be opened. */
    
    // Create a new file, and skip the first 69 bytes, 
    // which are info about the current offset
    stream = fopen(theErrorFileName, "w");
    if(stream == NULL)
    {
      fprintf(stderr,"Unable to open error log file: %s\n", theErrorFileName);
      return -1;
    }
    fprintf(stream, "%s%u%s", "Current byte-offset of file-pointer is: ", 69,
	    "                        \n\n\n");   
    
    // ...and write the error-message...
    ErrorReporter::formatMessage(thr_no,
                                 threadCount, thrdMessageID,
				 thrdProblemData, thrdObjRef,
				 theTraceFileName, theMessage);
    fprintf(stream, "%s", theMessage);
    fflush(stream);
    
    /* ...and finally, at the beginning of the file, 
       store the position where to
       start writing the next message. */
    offset = ftell(stream);
    // If we have not reached the maximum number of messages...
    if (offset <= (maxOffset - MESSAGE_LENGTH)){
      fseek(stream, 40, SEEK_SET);
      // ...set the current offset...
      fprintf(stream,"%d", offset);
    } else {
      fseek(stream, 40, SEEK_SET);
      // ...otherwise, start over from the beginning.
      fprintf(stream, "%u%s", 69, "             ");
    }
  } else {
    // Go to the latest position in the file...
    fseek(stream, 40, SEEK_SET);
    fscanf(stream, "%u", &offset);
    fseek(stream, offset, SEEK_SET);
    
    // ...and write the error-message there...
    ErrorReporter::formatMessage(thr_no,
                                 threadCount, thrdMessageID,
				 thrdProblemData, thrdObjRef,
				 theTraceFileName, theMessage);
    fprintf(stream, "%s", theMessage);
    fflush(stream);
    
    /* ...and finally, at the beginning of the file, 
       store the position where to
       start writing the next message. */
    offset = ftell(stream);
    
    // If we have not reached the maximum number of messages...
    if (offset <= (maxOffset - MESSAGE_LENGTH)){
      fseek(stream, 40, SEEK_SET);
      // ...set the current offset...
      fprintf(stream,"%d", offset);
    } else {
      fseek(stream, 40, SEEK_SET);
      // ...otherwise, start over from the beginning.
      fprintf(stream, "%u%s", 69, "             ");
    }
  }
  fflush(stream);
  fclose(stream);
  
  if (theTraceFileName) {
    /* Attempt to stop all processing to be able to dump a consistent state. */
    globalScheduler.traceDumpPrepare(nst);

    char *traceFileEnd = theTraceFileName + strlen(theTraceFileName);
    for (Uint32 i = 0; i < threadCount; i++)
    {
      // Open the tracefile...
      if (i > 0)
        sprintf(traceFileEnd, "_t%u", i);
      FILE *jamStream = fopen(theTraceFileName, "w");

      //  ...and "dump the jam" there.
      bool ok = globalScheduler.traceDumpGetJam(i, jamBlockNumber,
                                                thrdTheEmulatedJam,
                                                thrdTheEmulatedJamIndex);
      if(ok && thrdTheEmulatedJam != 0)
      {
        dumpJam(jamStream, thrdTheEmulatedJamIndex,
                thrdTheEmulatedJam, jamBlockNumber);
      }

      globalScheduler.dumpSignalMemory(i, jamStream);

      fclose(jamStream);
    }
  }

  return 0;
}

void 
dumpJam(FILE *jamStream, 
	Uint32 thrdTheEmulatedJamIndex, 
	const Uint32 thrdTheEmulatedJam[],
        Uint32 aBlockNumber) {
#ifndef NO_EMULATED_JAM   
  // print header
  const int maxaddr = 8;
  fprintf(jamStream, "JAM CONTENTS up->down left->right ?=not block entry\n");
  fprintf(jamStream, "%-7s ", "BLOCK");
  for (int i = 0; i < maxaddr; i++)
    fprintf(jamStream, "%-6s ", "ADDR");
  fprintf(jamStream, "\n");

  const int first = thrdTheEmulatedJamIndex;	// oldest
  int cnt, idx;

  // look for first block entry
  for (cnt = 0, idx = first; cnt < EMULATED_JAM_SIZE; cnt++, idx++) {
    if (idx >= EMULATED_JAM_SIZE)
      idx = 0;
    const Uint32 aJamEntry = thrdTheEmulatedJam[idx];
    if (aJamEntry > (1 << 20))
      break;
  }

  // 1. if first entry is a block entry, it is printed in the main loop
  // 2. else if any block entry exists, the jam starts in an unknown block
  // 3. else if no block entry exists, the block is theEmulatedJamBlockNumber
  // a "?" indicates first addr is not a block entry
  if (cnt == 0)
    ;
  else if (cnt < EMULATED_JAM_SIZE)
    fprintf(jamStream, "%-7s?", "");
  else {
    const char *aBlockName = getBlockName(aBlockNumber);
    if (aBlockName != 0)
      fprintf(jamStream, "%-7s?", aBlockName);
    else
      fprintf(jamStream, "0x%-5X?", aBlockNumber);
  }

  // loop over all entries
  int cntaddr = 0;
  for (cnt = 0, idx = first; cnt < EMULATED_JAM_SIZE; cnt++, idx++) {
    globalData.incrementWatchDogCounter(4);	// watchdog not to kill us ?
    if (idx >= EMULATED_JAM_SIZE)
      idx = 0;
    const Uint32 aJamEntry = thrdTheEmulatedJam[idx];
    if (aJamEntry > (1 << 20)) {
      const Uint32 aBlockNumber = aJamEntry >> 20;
      const char *aBlockName = getBlockName(aBlockNumber);
      if (cnt > 0)
	  fprintf(jamStream, "\n");
      if (aBlockName != 0)
	fprintf(jamStream, "%-7s ", aBlockName);
      else
	fprintf(jamStream, "0x%-5X ", aBlockNumber);
      cntaddr = 0;
    }
    if (cntaddr == maxaddr) {
      fprintf(jamStream, "\n%-7s ", "");
      cntaddr = 0;
    }
    fprintf(jamStream, "%06u ", aJamEntry & 0xFFFFF);
    cntaddr++;
  }
  fprintf(jamStream, "\n");
  fflush(jamStream);
#endif // ifndef NO_EMULATED_JAM
}
