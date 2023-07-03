/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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
#include <vector>
#include "my_dir.h"

#include "records.hpp"
#include "kernel/signaldata/FsOpenReq.hpp"
#include "portlib/ndb_file.h"
#include "util/ndb_opts.h"
#include "util/ndb_openssl_evp.h"
#include "util/ndbxfrm_file.h"

#define JAM_FILE_ID 449


#define RETURN_ERROR 1
#define RETURN_OK 0

#define FROM_BEGINNING 0

using byte = unsigned char;

static ndb_off_t readFromFile(ndbxfrm_file * xfrm,
                              ndb_off_t word_pos,
                              Uint32 *toPtr,
                              Uint32 sizeInWords);
[[noreturn]] static void doExit();

static ndb_file file;
static ndbxfrm_file xfrm;

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

static ndb_key_state opt_file_key_state("file", nullptr);
static ndb_key_option opt_file_key(opt_file_key_state);
static ndb_key_from_stdin_option opt_file_key_from_stdin(opt_file_key_state);

static struct my_option my_long_options[] =
{
  NdbStdOpt::usage,
  NdbStdOpt::help,
  NdbStdOpt::version,

  // Specific options
  { "check", NDB_OPT_NOSHORT, "Check records for errors",
    &theCheckFlag, nullptr, nullptr,
    GET_BOOL, NO_ARG, 1, 0, 0, nullptr, 0, nullptr},
  { "dump", NDB_OPT_NOSHORT, "Print dump info",
    &theDumpFlag, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
  { "file-key", 'K', "File encryption key",
    nullptr, nullptr, nullptr,
    GET_PASSWORD, OPT_ARG, 0, 0, 0, nullptr, 0, &opt_file_key},
  { "file-key-from-stdin", NDB_OPT_NOSHORT, "File encryption key from stdin",
    &opt_file_key_from_stdin.opt_value, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, &opt_file_key_from_stdin},
  { "filedescriptors", NDB_OPT_NOSHORT, "Print file descriptors only",
    &onlyFileDesc, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
  { "lap", NDB_OPT_NOSHORT, "Provide lap info, with max GCI started and completed",
    &onlyLap, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
  { "mbyte", NDB_OPT_NOSHORT, "Starting megabyte",
    &startAtMbyte, nullptr, nullptr,
    GET_INT, NO_ARG, 0, 0, 15, nullptr, 0, nullptr},
  { "mbyteheaders", NDB_OPT_NOSHORT, "Show only first page header of each megabyte in file",
    &onlyMbyteHeaders, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
  { "nocheck", 'C', "Do not check records for errors",
    nullptr, nullptr, nullptr,
    GET_NO_ARG, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
  { "noprint", 'P', "Do not print records",
    nullptr, nullptr, nullptr,
    GET_NO_ARG, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
  { "page", NDB_OPT_NOSHORT, "Start with this page",
    &startAtPage, nullptr, nullptr,
    GET_INT, NO_ARG, 0, 0, 31, nullptr, 0, nullptr},
  { "pageheaders", NDB_OPT_NOSHORT, "Show page headers only",
    &onlyPageHeaders, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
  { "pageindex", NDB_OPT_NOSHORT, "Start with this page index",
    &startAtPageIndex, nullptr, nullptr,
    GET_INT, NO_ARG, 12, 12, 8191, nullptr, 0, nullptr},
  { "print", NDB_OPT_NOSHORT, "Print records",
    &thePrintFlag, nullptr, nullptr,
    GET_BOOL, NO_ARG, 1, 0, 0, nullptr, 0, nullptr},
  { "twiddle", NDB_OPT_NOSHORT, "Bit-shifted dump",
    &theTwiddle, nullptr, nullptr,
    GET_BOOL, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
  NdbStdOpt::end_of_options
};

static const char* load_defaults_groups[] = { "ndb_redo_log_reader",
                                              nullptr };

static bool get_one_option(int optid, const struct my_option *opt, char *argument)
{
  switch (optid)
  {
    case 'C': // nocheck
      theCheckFlag = false;
      break;
    case 'P': // noprint
      thePrintFlag = false;
      break;
    default:
      return ndb_std_get_one_option(optid, opt, argument);
  }
  return false;
}

static void print_utility_help()
{
  ndbout<<"\nThis command reads a redo log file, checking it for errors, printing its contents in a human-readable format, or both.";
}

[[noreturn]] inline void ndb_end_and_exit(int exitcode)
{
  ndb_end(0);
  ndb_openssl_evp::library_end();
  exit(exitcode);
}

static std::vector<char*> convert_legacy_options(size_t argc, char** argv);

int main(int argc, char** argv)
{
  std::vector<char*> new_argv = convert_legacy_options(argc, argv);
  argv = new_argv.data();

  NDB_INIT(argv[0]);
  ndb_openssl_evp::library_init();
  Ndb_opts opts(argc, argv, my_long_options, load_defaults_groups);
  if (opts.handle_options(&get_one_option))
  {
    print_utility_help();
    opts.usage();
    ndb_end_and_exit(1);
  }

  if (ndb_option::post_process_options())
  {
    BaseString err_msg = opt_file_key_state.get_error_message();
    if (!err_msg.empty())
    {
      fprintf(stderr, "Error: file key: %s\n", err_msg.c_str());
    }
    print_utility_help();
    opts.usage();
    ndb_end_and_exit(1);
  }

  if (opt_file_key_state.get_key() != nullptr &&
      !ndb_openssl_evp::is_aeskw256_supported())
  {
    fprintf(stderr,
            "Error: file key options requires OpenSSL 1.0.2 or newer.\n");
    return 2;
  }

  if (onlyLap) thePrintFlag = false;

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
  
  if (argc != 1 || strlen(argv[0]) >= sizeof(fileName))
  {
    print_utility_help();
    opts.usage();
    ndb_end_and_exit(1);
  }
  strcpy(fileName, argv[0]);
 
  int r;
  r = file.open(fileName, FsOpenReq::OM_READONLY);
  if (r != 0)
  {
    perror("Error: open file");
    ndb_end_and_exit(RETURN_ERROR);
  }
  const byte* key = opt_file_key_state.get_key();
  const size_t key_len = opt_file_key_state.get_key_length();
  r = xfrm.open(file, key, key_len);
  if (r != 0)
  {
    if (r == -2) xfrm.close(true);
    file.close();
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
  
  ndb_off_t tmpFileOffset =
      startAtMbyte * REDOLOG_PAGESIZE * REDOLOG_PAGES_IN_MBYTE * sizeof(Uint32);

  redoLogPage = new Uint32[REDOLOG_PAGESIZE*REDOLOG_PAGES_IN_MBYTE];
  Uint32 words_from_previous_page = 0;

  // Loop for every mbyte.
  bool lastPage = false;
  for (Uint32 j = startAtMbyte; j < NO_MBYTE_IN_FILE && !lastPage; j++) {

    ndbout_c("mb: %d", j);
    ndb_off_t sz = readFromFile(&xfrm, tmpFileOffset, redoLogPage,
                            REDOLOG_PAGESIZE * REDOLOG_PAGES_IN_MBYTE);
    tmpFileOffset += sz;

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
  xfrm.close(false);
  file.close();
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

ndb_off_t readFromFile(ndbxfrm_file * xfrm,
                   ndb_off_t word_pos,
                   Uint32 *toPtr,
                   Uint32 sizeInWords)
{
  ndbxfrm_output_iterator it = {(byte*)toPtr, (byte*)toPtr + sizeof(Uint32) * sizeInWords, false};
  int r = xfrm->read_transformed_pages(word_pos * sizeof(Uint32), &it);
  if (r == -1)
  {
    ndbout << "Error reading file" << endl;
    doExit();
  }
  ndb_off_t noOfReadWords = (it.begin() - (byte*)toPtr) / sizeof(Uint32);
  if (noOfReadWords == 0)
  {
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


std::vector<char*> convert_legacy_options(size_t argc, char** argv)
{
  static const char * legacy_options[][2] = {
    { "-dump", "--dump" },
    { "-filedescriptors", "--filedescriptors" },
    { "-lap", "--lap" },
    { "-mbyte", "--mbyte" },
    { "-mbyteheaders", "--mbyteheaders" },
    { "-nocheck", "--nocheck" },
    { "-noprint", "--noprint" },
    { "-page", "--page" },
    { "-pageindex", "--pageindex" },
    { "-pageheaders", "--pageheaders" },
    { "-twiddle", "--twiddle" } };
  std::vector<char*> new_argv(argc + 1);
  new_argv[0] = argv[0];
  for (size_t i = 1; i < argc; i++)
  {
    new_argv[i] = argv[i];
    for (size_t j = 0; j < std::size(legacy_options); j++)
      if (strcmp(new_argv[i], legacy_options[j][0]) == 0)
      {
        fprintf(stderr,
                "Warning: Option '%s' is deprecated, use '%s' instead.\n",
                new_argv[i],
                legacy_options[j][1]);
        new_argv[i] = (char*)legacy_options[j][1];
        break;
      }
  }
  new_argv[argc] = nullptr;
  return new_argv;
}

void doExit()
{
  ndbout << "Error in redoLogReader(). Exiting!" << endl;
  xfrm.close(true);
  file.close();
  delete [] redoLogPage;
  ndb_end_and_exit(RETURN_ERROR);
}
