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
Uint32 readRecordOverPageBoundary (Uint32 *, Uint32 , Uint32 , Uint32);
Uint32 readFromFile(FILE * f, Uint32 *toPtr, Uint32 sizeInWords);
void readArguments(int argc, const char** argv);
void doExit();

FILE * f;
char fileName[256];
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
  Uint32 pageIndex = 0;
  Uint32 oldPageIndex = 0;
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

  // Loop for every mbyte.
  for (Uint32 j = startAtMbyte; j < NO_MBYTE_IN_FILE; j++) {
    readFromFile(f, redoLogPage, PAGESIZE*NO_PAGES_IN_MBYTE);
 
    if (firstLap) {
      pageIndex = startAtPageIndex;
      firstLap = false;
    } else
      pageIndex = 0;

    // Loop for every page.
    for (int i = startAtPage; i < NO_PAGES_IN_MBYTE; i++) {

      if (pageIndex == 0) {
	thePageHeader = (PageHeader *) &redoLogPage[i*PAGESIZE];
	// Print out mbyte number, page number and page index.
	ndbout << j << ":" << i << ":" << pageIndex << endl 
	       << " " << j*32 + i << ":" << pageIndex << " ";
	if (thePrintFlag) ndbout << (*thePageHeader);
	if (theCheckFlag) {
	  if(!thePageHeader->check()) {
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
	pageIndex += thePageHeader->getLogRecordSize();
      }

      if (onlyMbyteHeaders) {
	// Show only the first page header in every mbyte of the file.
	break;
      }

      if (onlyPageHeaders) {
	// Show only page headers. Continue with the next page in this for loop.
	pageIndex = 0;
	continue;
      }

      do {
	// Print out mbyte number, page number and page index.
	ndbout << j << ":" << i << ":" << pageIndex << endl 
	       << " " << j*32 + i << ":" << pageIndex << " ";
	recordType = redoLogPage[i*PAGESIZE + pageIndex];
	switch(recordType) {
	case ZFD_TYPE:
	  fdRecord = (FileDescriptor *) &redoLogPage[i*PAGESIZE + pageIndex];
	  if (thePrintFlag) ndbout << (*fdRecord);
	  if (theCheckFlag) {
	    if(!fdRecord->check()) {
	      doExit();
	    }
	  }
	  if (onlyFileDesc) {
	    delete [] redoLogPage;
	    exit(RETURN_OK);
	  }
	  pageIndex += fdRecord->getLogRecordSize();
	  break;
    
	case ZNEXT_LOG_RECORD_TYPE:
	  nlRecord = (NextLogRecord *) (&redoLogPage[i*PAGESIZE] + pageIndex);
	  pageIndex += nlRecord->getLogRecordSize(pageIndex);
	  if (pageIndex <= PAGESIZE) {
	    if (thePrintFlag) ndbout << (*nlRecord);
	    if (theCheckFlag) {
	      if(!nlRecord->check()) {
		doExit();
	      }
	    }
	  }
	  break;

	case ZCOMPLETED_GCI_TYPE:
	  cGCIrecord = (CompletedGCIRecord *) &redoLogPage[i*PAGESIZE + pageIndex];
	  pageIndex += cGCIrecord->getLogRecordSize();
	  if (pageIndex <= PAGESIZE) {
	    if (thePrintFlag) ndbout << (*cGCIrecord);
	    if (theCheckFlag) {
	      if(!cGCIrecord->check()) {
		doExit();
	      }
	    }
	  }
	  break;

	case ZPREP_OP_TYPE:
	  poRecord = (PrepareOperationRecord *) &redoLogPage[i*PAGESIZE + pageIndex];
	  pageIndex += poRecord->getLogRecordSize();
	  if (pageIndex <= PAGESIZE) {
	    if (thePrintFlag) ndbout << (*poRecord);
	    if (theCheckFlag) {
	      if(!poRecord->check()) {
		doExit();
	      }
	    }
	  }
	  else {
	    oldPageIndex = pageIndex - poRecord->getLogRecordSize();
	  }
	  break;

	case ZCOMMIT_TYPE:
	  ctRecord = (CommitTransactionRecord *) &redoLogPage[i*PAGESIZE + pageIndex];
	  pageIndex += ctRecord->getLogRecordSize();
	  if (pageIndex <= PAGESIZE) {
	    if (thePrintFlag) ndbout << (*ctRecord);
	    if (theCheckFlag) {
	      if(!ctRecord->check()) {
		doExit();
	      }
	    }
	  }
	  else {
	    oldPageIndex = pageIndex - ctRecord->getLogRecordSize();
	  }	
	  break;
      
	case ZINVALID_COMMIT_TYPE:
	  ictRecord = (InvalidCommitTransactionRecord *) &redoLogPage[i*PAGESIZE + pageIndex];
	  pageIndex += ictRecord->getLogRecordSize();
	  if (pageIndex <= PAGESIZE) {
	    if (thePrintFlag) ndbout << (*ictRecord);
	    if (theCheckFlag) {
	      if(!ictRecord->check()) {
		doExit();
	      }
	    }
	  }
	  else {
	    oldPageIndex = pageIndex - ictRecord->getLogRecordSize();
	  }	
	  break;

	case ZNEXT_MBYTE_TYPE:
	  nmRecord = (NextMbyteRecord *) &redoLogPage[i*PAGESIZE + pageIndex];
	  if (thePrintFlag) ndbout << (*nmRecord);
	  i = NO_PAGES_IN_MBYTE;
	  break;
	
	case ZABORT_TYPE:
	  atRecord = (AbortTransactionRecord *) &redoLogPage[i*PAGESIZE + pageIndex];
	  pageIndex += atRecord->getLogRecordSize();
	  if (pageIndex <= PAGESIZE) {
	    if (thePrintFlag) ndbout << (*atRecord);
	    if (theCheckFlag) {
	      if(!atRecord->check()) {
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
	  for (int j = pageIndex; j < PAGESIZE; j++){
	    Uint32 unknown = redoLogPage[i*PAGESIZE + j];

	    ndbout_c("%-30d%-12u%-12x", j, unknown, unknown);
	  }
	  
	  doExit();
	}
      } while(pageIndex < PAGESIZE && i < NO_PAGES_IN_MBYTE);

      if (pageIndex > PAGESIZE) {
	// The last record overlapped page boundary. Must redo that record.
	pageIndex = readRecordOverPageBoundary(&redoLogPage[i*PAGESIZE], 
				 pageIndex, oldPageIndex, recordType);
      } else {
	pageIndex = 0;
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

Uint32  readRecordOverPageBoundary(Uint32 *pagePtr, Uint32 pageIndex, Uint32 oldPageIndex, Uint32 recordType) {
  Uint32 pageHeader[PAGEHEADERSIZE];
  Uint32 tmpPages[PAGESIZE*10];
  PageHeader *thePageHeader;
  Uint32 recordSize = 0;

  PrepareOperationRecord *poRecord;
  CommitTransactionRecord *ctRecord;
  InvalidCommitTransactionRecord *ictRecord;

  memcpy(pageHeader, pagePtr + PAGESIZE, PAGEHEADERSIZE*sizeof(Uint32));
  memcpy(tmpPages, pagePtr + oldPageIndex, (PAGESIZE - oldPageIndex)*sizeof(Uint32));
  memcpy(tmpPages + PAGESIZE - oldPageIndex , 
	 (pagePtr + PAGESIZE + PAGEHEADERSIZE), 
	 (PAGESIZE - PAGEHEADERSIZE)*sizeof(Uint32));

  switch(recordType) {
  case ZPREP_OP_TYPE:
    poRecord = (PrepareOperationRecord *) tmpPages;
    recordSize = poRecord->getLogRecordSize();
    if (recordSize < (PAGESIZE - PAGEHEADERSIZE)) {
      if (theCheckFlag) {
	if(!poRecord->check()) {
	  doExit();
	}
      } 
      if (thePrintFlag) ndbout << (*poRecord);
    } else {
      ndbout << "Error: Record greater than a Page" << endl;
    }
    break;

  case ZCOMMIT_TYPE:
    ctRecord = (CommitTransactionRecord *) tmpPages;
    recordSize = ctRecord->getLogRecordSize();
    if (recordSize < (PAGESIZE - PAGEHEADERSIZE)) {
      if (theCheckFlag) {
	if(!ctRecord->check()) {
	  doExit();
	}
      }
      if (thePrintFlag) ndbout << (*ctRecord);
    } else {
      ndbout << endl << "Error: Record greater than a Page" << endl;
    }
    break;

  case ZINVALID_COMMIT_TYPE:
   ictRecord = (InvalidCommitTransactionRecord *) tmpPages;
    recordSize = ictRecord->getLogRecordSize();
    if (recordSize < (PAGESIZE - PAGEHEADERSIZE)) {
      if (theCheckFlag) {
	if(!ictRecord->check()) {
	  doExit();
	}
      }
      if (thePrintFlag) ndbout << (*ictRecord);
    } else {
      ndbout << endl << "Error: Record greater than a Page" << endl;
    }
    break;

  case ZNEW_PREP_OP_TYPE: 
  case ZABORT_TYPE:
  case ZFRAG_SPLIT_TYPE:
  case ZNEXT_MBYTE_TYPE:
    ndbout << endl << "Record type = " << recordType << " not implemented." << endl;
    return 0;

  default:
    ndbout << endl << "Error: Unknown record type. Record type = " << recordType << endl;
    return 0;
  }

  thePageHeader = (PageHeader *) (pagePtr + PAGESIZE);
  if (thePrintFlag) ndbout << (*thePageHeader);

  return PAGEHEADERSIZE - PAGESIZE + oldPageIndex + recordSize;
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
  fclose(f);
  delete [] redoLogPage;
  exit(RETURN_ERROR);
}
