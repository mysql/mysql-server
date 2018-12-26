/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

//----------------------------------------------------------------
// REDOLOGFILEREADER
// Reads a redo log file and checks it for errors and/or prints
// the file in a human readable format.
//
// Usage: redoLogFileReader <file> [-noprint] [-nocheck] 
//        [-mbyte <0-15>] [-mbyteHeaders] [-pageHeaders] 
//	
//----------------------------------------------------------------


#include <ndb_global.h>
#include "my_dir.h"

#include "records.hpp"

#define JAM_FILE_ID 449


#define RETURN_ERROR 1
#define RETURN_OK 0

#define FROM_BEGINNING 0

void usage(const char * prg);
Uint32 readFromFile(FILE * f, Uint32 *toPtr, Uint32 sizeInWords);
void readArguments(int argc, char** argv);
[[noreturn]] void doExit();

FILE * f= 0;
char fileName[256];
bool theDumpFlag = false;
bool thePrintFlag = true;
bool theCheckFlag = true;
bool onlyPageHeaders = false;
bool onlyMbyteHeaders = false;
bool onlyFileDesc = false;
bool onlyLap = false;
bool theTwiddle = false;
Uint32 startAtMbyte = 0;
Uint32 startAtPage = 0;
Uint32 startAtPageIndex = 0;
Uint32 *redoLogPage;

unsigned NO_MBYTE_IN_FILE = 16;

[[noreturn]] inline void ndb_end_and_exit(int exitcode)
{
  ndb_end(0);
  exit(exitcode);
}

int main(int argc, char** argv)
{
  ndb_init();
  Int32 wordIndex = 0;
  Uint32 oldWordIndex = 0;
  Uint32 recordType = 1234567890;

  PageHeader *thePageHeader;
  CompletedGCIRecord *cGCIrecord;
  PrepareOperationRecord *poRecord;
  NextLogRecord *nlRecord;
  FileDescriptor *fdRecord;
  CommitTransactionRecord *ctRecord;
  InvalidCommitTransactionRecord *ictRecord;
  NextMbyteRecord *nmRecord;
  AbortTransactionRecord *atRecord;
  
  readArguments(argc, argv);
 
  f = fopen(fileName, "rb");
  if(!f){
    perror("Error: open file");
    ndb_end_and_exit(RETURN_ERROR);
  }

  {
    MY_STAT buf;
    my_stat(fileName, &buf, MYF(0));
    NO_MBYTE_IN_FILE = (unsigned)(buf.st_size / (1024 * 1024));
    if (NO_MBYTE_IN_FILE != 16)
    {
      ndbout_c("Detected %umb files", NO_MBYTE_IN_FILE);
    }
  }
  
  const Uint32 tmpFileOffset =
      startAtMbyte * REDOLOG_PAGESIZE * REDOLOG_PAGES_IN_MBYTE * sizeof(Uint32);
  if (fseek(f, tmpFileOffset, FROM_BEGINNING)) {
    perror("Error: Move in file");
    ndb_end_and_exit(RETURN_ERROR);
  }

  redoLogPage = new Uint32[REDOLOG_PAGESIZE*REDOLOG_PAGES_IN_MBYTE];
  Uint32 words_from_previous_page = 0;

  // Loop for every mbyte.
  bool lastPage = false;
  for (Uint32 j = startAtMbyte; j < NO_MBYTE_IN_FILE && !lastPage; j++) {

    ndbout_c("mb: %d", j);
    readFromFile(f, redoLogPage, REDOLOG_PAGESIZE*REDOLOG_PAGES_IN_MBYTE);

    words_from_previous_page = 0;

    // Loop for every page.
    for (int i = 0; i < REDOLOG_PAGES_IN_MBYTE; i++)
    {
      wordIndex = 0;
      thePageHeader = (PageHeader *) &redoLogPage[i*REDOLOG_PAGESIZE];
      // Print out mbyte number, page number and page index.
      ndbout << j << ":" << i << ":" << wordIndex << endl 
	     << " " << j*32 + i << ":" << wordIndex << " ";
      if (thePrintFlag) ndbout << (*thePageHeader);
      if (onlyLap)
      {
	ndbout_c("lap: %d maxgcicompleted: %d maxgcistarted: %d",
		 thePageHeader->m_lap,
		 thePageHeader->m_max_gci_completed,
		 thePageHeader->m_max_gci_started);
	continue;
      }
      if (theCheckFlag) {
	if(!thePageHeader->check()) {
	  ndbout << "Error in thePageHeader->check()" << endl;
	  doExit();
	}
	
	Uint32 checkSum = 37;
        for (int ps = 1; ps < REDOLOG_PAGESIZE; ps++)
          checkSum = redoLogPage[i*REDOLOG_PAGESIZE+ps] ^ checkSum;

        if (checkSum != redoLogPage[i*REDOLOG_PAGESIZE]){
	  ndbout_c("WRONG CHECKSUM: checksum = 0x%x expected: 0x%x",
                   redoLogPage[i*REDOLOG_PAGESIZE],
                   checkSum);
	  //doExit();
	}
	else
	  ndbout << "expected checksum: " << checkSum << endl;

      }

      lastPage = i != 0 && thePageHeader->lastPage();
      Uint32 lastWord = thePageHeader->lastWord();

      if (onlyMbyteHeaders) {
	// Show only the first page header in every mbyte of the file.
	break;
      }

      if (onlyPageHeaders) {
	// Show only page headers. Continue with the next page in this for loop.
	continue;
      }


      wordIndex = thePageHeader->getLogRecordSize() - words_from_previous_page;
      Uint32 *redoLogPagePos = redoLogPage + i*REDOLOG_PAGESIZE;
      if (words_from_previous_page)
      {
	memmove(redoLogPagePos + wordIndex,
		redoLogPagePos - words_from_previous_page,
		words_from_previous_page*4);
      }

      do {
	if (words_from_previous_page)
	{
	  // Print out mbyte number, page number and word index.
          ndbout << j << ":" << i-1 << ":" << REDOLOG_PAGESIZE-words_from_previous_page << endl
		 << j << ":" << i   << ":" << wordIndex+words_from_previous_page << endl 
                 << " " << j*32 + i-1 << ":" << REDOLOG_PAGESIZE-words_from_previous_page << " ";
	  words_from_previous_page = 0;
	}
	else
	{
	  // Print out mbyte number, page number and word index.
          ndbout_c("mb: %u fp: %u pos: %u",
                   j, (j*32 + i), wordIndex);
	}
        redoLogPagePos = redoLogPage + i*REDOLOG_PAGESIZE + wordIndex;
	oldWordIndex = wordIndex;
	recordType = *redoLogPagePos;
	switch(recordType) {
	case ZFD_TYPE:
	  fdRecord = (FileDescriptor *) redoLogPagePos;
	  if (thePrintFlag) ndbout << (*fdRecord);
	  if (theCheckFlag) {
	    if(!fdRecord->check()) {
	      ndbout << "Error in fdRecord->check()" << endl;
	      doExit();
	    }
	  }
	  if (onlyFileDesc) {
	    delete [] redoLogPage;
	    exit(RETURN_OK);
	  }
	  wordIndex += fdRecord->getLogRecordSize();
	  break;
    
	case ZNEXT_LOG_RECORD_TYPE:
	  nlRecord = (NextLogRecord *) redoLogPagePos;
	  wordIndex += nlRecord->getLogRecordSize(wordIndex);
          if (wordIndex <= REDOLOG_PAGESIZE) {
	    if (thePrintFlag) ndbout << (*nlRecord);
	    if (theCheckFlag) {
	      if(!nlRecord->check()) {
		ndbout << "Error in nlRecord->check()" << endl;
		doExit();
	      }
	    }
	  }
	  break;

	case ZCOMPLETED_GCI_TYPE:
	  cGCIrecord = (CompletedGCIRecord *) redoLogPagePos;
	  wordIndex += cGCIrecord->getLogRecordSize();
          if (wordIndex <= REDOLOG_PAGESIZE) {
	    if (thePrintFlag) ndbout << (*cGCIrecord);
	    if (theCheckFlag) {
	      if(!cGCIrecord->check()) {
		ndbout << "Error in cGCIrecord->check()" << endl;
		doExit();
	      }
	    }
	  }
	  break;

	case ZPREP_OP_TYPE:
	  poRecord = (PrepareOperationRecord *) redoLogPagePos;
          wordIndex += poRecord->getLogRecordSize(REDOLOG_PAGESIZE-wordIndex);
          if (wordIndex <= REDOLOG_PAGESIZE) {
	    if (thePrintFlag) ndbout << (*poRecord);
	    if (theCheckFlag) {
	      if(!poRecord->check()) {
		ndbout << "Error in poRecord->check()" << endl;
		doExit();
	      }
	    }
	  }
	  break;

	case ZCOMMIT_TYPE:
	  ctRecord = (CommitTransactionRecord *) redoLogPagePos;
	  wordIndex += ctRecord->getLogRecordSize();
          if (wordIndex <= REDOLOG_PAGESIZE) {
	    if (thePrintFlag) ndbout << (*ctRecord);
	    if (theCheckFlag) {
	      if(!ctRecord->check()) {
		ndbout << "Error in ctRecord->check()" << endl;
		doExit();
	      }
	    }
	  }
	  break;
      
	case ZINVALID_COMMIT_TYPE:
	  ictRecord = (InvalidCommitTransactionRecord *) redoLogPagePos;
	  wordIndex += ictRecord->getLogRecordSize();
          if (wordIndex <= REDOLOG_PAGESIZE) {
	    if (thePrintFlag) ndbout << (*ictRecord);
	    if (theCheckFlag) {
	      if(!ictRecord->check()) {
		ndbout << "Error in ictRecord->check()" << endl;
		doExit();
	      }
	    }
	  }
	  break;

	case ZNEXT_MBYTE_TYPE:
	  nmRecord = (NextMbyteRecord *) redoLogPagePos;
	  if (thePrintFlag) ndbout << (*nmRecord);
          i = REDOLOG_PAGES_IN_MBYTE;
	  break;
	
	case ZABORT_TYPE:
	  atRecord = (AbortTransactionRecord *) redoLogPagePos;
	  wordIndex += atRecord->getLogRecordSize();
          if (wordIndex <= REDOLOG_PAGESIZE) {
	    if (thePrintFlag) ndbout << (*atRecord);
	    if (theCheckFlag) {
	      if(!atRecord->check()) {
		ndbout << "Error in atRecord->check()" << endl;
		doExit();
	      }
	    }
	  }
	  break;

	case ZNEW_PREP_OP_TYPE: 
	case ZFRAG_SPLIT_TYPE:
	  ndbout << endl << "Record type = " << recordType << " not implemented." << endl;
	  doExit();

	default:
	  ndbout << " ------ERROR: UNKNOWN RECORD TYPE------" << endl;

	  // Print out remaining data in this page
          for (int k = wordIndex; k < REDOLOG_PAGESIZE; k++){
            Uint32 unknown = redoLogPage[i*REDOLOG_PAGESIZE + k];
	    ndbout_c("%-30d%-12u%-12x", k, unknown, unknown);
	  }
	  
          if (theCheckFlag)
          {
            doExit();
          }
          else
          {
            wordIndex = lastWord;
          }
	}
      } while(wordIndex < (Int32)lastWord && i < REDOLOG_PAGES_IN_MBYTE);


      if (false && lastPage)
      {
	if (theDumpFlag)
	{
	  ndbout << " ------PAGE END: DUMPING REST OF PAGE------" << endl;
          for (int k = wordIndex > REDOLOG_PAGESIZE ? oldWordIndex : wordIndex;
               k < REDOLOG_PAGESIZE; k++)
	  {
            Uint32 word = redoLogPage[i*REDOLOG_PAGESIZE + k];
	    ndbout_c("%-30d%-12u%-12x", k, word, word);
	  }
	}
	break;
      }
      if (wordIndex > REDOLOG_PAGESIZE) {
        words_from_previous_page = REDOLOG_PAGESIZE - oldWordIndex;
	ndbout << " ----------- Record continues on next page -----------" << endl;
      } else {
	wordIndex = 0;
	words_from_previous_page = 0;
      }
      ndbout << endl;
    }//for  
    ndbout << endl;
    if (startAtMbyte != 0) {
      break;
    }
  }//for
  fclose(f);
  delete [] redoLogPage;
  ndb_end_and_exit(RETURN_OK);
}

static
Uint32
twiddle_32(Uint32 in)
{
  Uint32 retVal = 0;

  retVal = ((in & 0x000000FF) << 24) |
    ((in & 0x0000FF00) << 8)  |
    ((in & 0x00FF0000) >> 8)  |
    ((in & 0xFF000000) >> 24);

  return(retVal);
}

//----------------------------------------------------------------
// 
//----------------------------------------------------------------

Uint32 readFromFile(FILE * f, Uint32 *toPtr, Uint32 sizeInWords) {
  Uint32 noOfReadWords;
  if ( !(noOfReadWords = (Uint32)fread(toPtr, sizeof(Uint32), sizeInWords, f))){
    ndbout << "Error reading file" << endl;
    doExit();
  } 

  if (theTwiddle)
  {
    for (Uint32 i = 0; i<noOfReadWords; i++)
      toPtr[i] = twiddle_32(toPtr[i]);
  }

  return noOfReadWords;
}


//----------------------------------------------------------------
// 
//----------------------------------------------------------------


void usage(const char * prg){
  ndbout << endl << "Usage: ndbd_read_log_reader [OPTIONS]:" << endl << prg 
	 << " <Binary log file> [-noprint] [-dump] [-twiddle] [-lap] [-nocheck] [--help] " 
	 <<"[-mbyte <0-15>] [-mbyteheaders] [-pageheaders] [-filedescriptors] [-page <0-31>]  [-pageindex <12-8191>]"
	 << endl << endl;
  
}
void readArguments(int argc, char** argv)
{
  if(argc < 2 ){
    usage(argv[0]);
    doExit();
  }

  int i = 1;
  while (argc > 1)
    {
      if (strcmp(argv[i], "-noprint") == 0) {
	thePrintFlag = false;
      } else if (strcmp(argv[i], "-dump") == 0) {
	theDumpFlag = true;
      } else if (strcmp(argv[i], "-twiddle") == 0) {
        theTwiddle = true;
      } else if (strcmp(argv[i], "-nocheck") == 0) {
	theCheckFlag = false;
      } else if (strcmp(argv[i], "-mbyteheaders") == 0) {
	onlyMbyteHeaders = true;
      } else if (strcmp(argv[i], "-pageheaders") == 0) {
	onlyPageHeaders = true;
      } else if (strcmp(argv[i], "-filedescriptors") == 0) {
	onlyFileDesc = true;
      } else if (strcmp(argv[i], "-lap") == 0) {
	thePrintFlag = false;
	onlyLap = true;
      } else if (strcmp(argv[i], "--help") == 0) {
      	ndbout<<"\nThis command reads a redo log file, checking it for errors, printing its contents in a human-readable format, or both.";
      	usage(argv[0]);
      	exit(0);
      } else if (strcmp(argv[i], "-mbyte") == 0) {
	startAtMbyte = atoi(argv[i+1]);
	argc--;
	i++;
      } else if (strcmp(argv[i], "-page") == 0) {
	startAtPage = atoi(argv[i+1]);
	if (startAtPage > 31) {
	  usage(argv[0]);
	  doExit();
	}
	argc--;
	i++;
      } else if (strcmp(argv[i], "-pageindex") == 0) {
	startAtPageIndex = atoi(argv[i+1]);
	if (startAtPageIndex > 8191 || startAtPageIndex < 12) {
	  usage(argv[0]);
	  doExit();
	}
	argc--;
	i++;
      } else if (i==1) {
      	 strcpy(fileName, argv[1]);
      } else {
	usage(argv[0]);
	doExit();
      }
      argc--;
      i++;
    }
  
}

void doExit() {
  ndbout << "Error in redoLogReader(). Exiting!" << endl;
  if (f) fclose(f);
  delete [] redoLogPage;
  ndb_end_and_exit(RETURN_ERROR);
}
