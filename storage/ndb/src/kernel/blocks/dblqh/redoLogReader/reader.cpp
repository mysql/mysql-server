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

#include "records.hpp"

#define RETURN_ERROR 1
#define RETURN_OK 0

#define FROM_BEGINNING 0

void usage(const char * prg);
Uint32 readFromFile(FILE * f, Uint32 *toPtr, Uint32 sizeInWords);
void readArguments(int argc, const char** argv);
void doExit();

FILE * f= 0;
char fileName[256];
bool theDumpFlag = false;
bool thePrintFlag = true;
bool theCheckFlag = true;
bool onlyPageHeaders = false;
bool onlyMbyteHeaders = false;
bool onlyFileDesc = false;
bool firstLap = true;
Uint32 startAtMbyte = 0;
Uint32 startAtPage = 0;
Uint32 startAtPageIndex = 0;
Uint32 *redoLogPage;

NDB_COMMAND(redoLogFileReader,  "redoLogFileReader", "redoLogFileReader", "Read a redo log file", 16384) { 
  int wordIndex = 0;
  int oldWordIndex = 0;
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
    exit(RETURN_ERROR);
  }
  
  Uint32 tmpFileOffset = startAtMbyte * PAGESIZE * NO_PAGES_IN_MBYTE * sizeof(Uint32);
  if (fseek(f, tmpFileOffset, FROM_BEGINNING)) {
    perror("Error: Move in file");
    exit(RETURN_ERROR);
  }

  redoLogPage = new Uint32[PAGESIZE*NO_PAGES_IN_MBYTE];
  Uint32 words_from_previous_page = 0;

  // Loop for every mbyte.
  bool lastPage = false;
  for (Uint32 j = startAtMbyte; j < NO_MBYTE_IN_FILE && !lastPage; j++) {

    readFromFile(f, redoLogPage, PAGESIZE*NO_PAGES_IN_MBYTE);

    words_from_previous_page = 0;

    // Loop for every page.
    for (int i = 0; i < NO_PAGES_IN_MBYTE; i++) {
      wordIndex = 0;
      thePageHeader = (PageHeader *) &redoLogPage[i*PAGESIZE];
      // Print out mbyte number, page number and page index.
      ndbout << j << ":" << i << ":" << wordIndex << endl 
	     << " " << j*32 + i << ":" << wordIndex << " ";
      if (thePrintFlag) ndbout << (*thePageHeader);
      if (theCheckFlag) {
	if(!thePageHeader->check()) {
	  ndbout << "Error in thePageHeader->check()" << endl;
	  doExit();
	}

	Uint32 checkSum = 37;
	for (int ps = 1; ps < PAGESIZE; ps++)
	  checkSum = redoLogPage[i*PAGESIZE+ps] ^ checkSum;

	if (checkSum != redoLogPage[i*PAGESIZE]){
	  ndbout << "WRONG CHECKSUM: checksum = " << redoLogPage[i*PAGESIZE]
		 << " expected = " << checkSum << endl;
	  doExit();
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
      Uint32 *redoLogPagePos = redoLogPage + i*PAGESIZE;
      if (words_from_previous_page)
      {
	memmove(redoLogPagePos + wordIndex ,
		redoLogPagePos - words_from_previous_page,
		words_from_previous_page*4);
      }

      do {
	if (words_from_previous_page)
	{
	  // Print out mbyte number, page number and word index.
	  ndbout << j << ":" << i-1 << ":" << PAGESIZE-words_from_previous_page << endl 
		 << j << ":" << i   << ":" << wordIndex+words_from_previous_page << endl 
		 << " " << j*32 + i-1 << ":" << PAGESIZE-words_from_previous_page << " ";
	  words_from_previous_page = 0;
	}
	else
	{
	  // Print out mbyte number, page number and word index.
	  ndbout << j << ":" << i << ":" << wordIndex << endl 
		 << " " << j*32 + i << ":" << wordIndex << " ";
	}
	redoLogPagePos = redoLogPage + i*PAGESIZE + wordIndex;
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
	  if (wordIndex <= PAGESIZE) {
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
	  if (wordIndex <= PAGESIZE) {
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
	  wordIndex += poRecord->getLogRecordSize(PAGESIZE-wordIndex);
	  if (wordIndex <= PAGESIZE) {
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
	  if (wordIndex <= PAGESIZE) {
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
	  if (wordIndex <= PAGESIZE) {
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
	  i = NO_PAGES_IN_MBYTE;
	  break;
	
	case ZABORT_TYPE:
	  atRecord = (AbortTransactionRecord *) redoLogPagePos;
	  wordIndex += atRecord->getLogRecordSize();
	  if (wordIndex <= PAGESIZE) {
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
	  for (int k = wordIndex; k < PAGESIZE; k++){
	    Uint32 unknown = redoLogPage[i*PAGESIZE + k];
	    ndbout_c("%-30d%-12u%-12x", k, unknown, unknown);
	  }
	  
	  doExit();
	}
      } while(wordIndex < lastWord && i < NO_PAGES_IN_MBYTE);


      if (lastPage)
      {
	if (theDumpFlag)
	{
	  ndbout << " ------PAGE END: DUMPING REST OF PAGE------" << endl;
	  for (int k = wordIndex > PAGESIZE ? oldWordIndex : wordIndex;
	       k < PAGESIZE; k++)
	  {
	    Uint32 word = redoLogPage[i*PAGESIZE + k];
	    ndbout_c("%-30d%-12u%-12x", k, word, word);
	  }
	}
	break;
      }
      if (wordIndex > PAGESIZE) {
	words_from_previous_page = PAGESIZE - oldWordIndex;
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
  exit(RETURN_OK);
}

//----------------------------------------------------------------
// 
//----------------------------------------------------------------

Uint32 readFromFile(FILE * f, Uint32 *toPtr, Uint32 sizeInWords) {
  Uint32 noOfReadWords;
  if ( !(noOfReadWords = fread(toPtr, sizeof(Uint32), sizeInWords, f)) ) {
    ndbout << "Error reading file" << endl;
    doExit();
  } 

  return noOfReadWords;
}


//----------------------------------------------------------------
// 
//----------------------------------------------------------------


void usage(const char * prg){
  ndbout << endl << "Usage: " << endl << prg 
	 << " <Binary log file> [-noprint] [-nocheck] [-mbyte <0-15>] "
	 << "[-mbyteheaders] [-pageheaders] [-filedescriptors] [-page <0-31>] "
	 << "[-pageindex <12-8191>]" 
	 << endl << endl;
  
}
void readArguments(int argc, const char** argv)
{
  if(argc < 2 || argc > 9){
    usage(argv[0]);
    doExit();
  }

  strcpy(fileName, argv[1]);
  argc--;

  int i = 2;
  while (argc > 1)
    {
      if (strcmp(argv[i], "-noprint") == 0) {
	thePrintFlag = false;
      } else if (strcmp(argv[i], "-dump") == 0) {
	theDumpFlag = true;
      } else if (strcmp(argv[i], "-nocheck") == 0) {
	theCheckFlag = false;
      } else if (strcmp(argv[i], "-mbyteheaders") == 0) {
	onlyMbyteHeaders = true;
      } else if (strcmp(argv[i], "-pageheaders") == 0) {
	onlyPageHeaders = true;
      } else if (strcmp(argv[i], "-filedescriptors") == 0) {
	onlyFileDesc = true;
      } else if (strcmp(argv[i], "-mbyte") == 0) {
	startAtMbyte = atoi(argv[i+1]);
	if (startAtMbyte > 15) {
	  usage(argv[0]);
	  doExit();
	}
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
  exit(RETURN_ERROR);
}
