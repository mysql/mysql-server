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

static const volatile char cvsid[] = "$Id: tpcb.cpp,v 1.4 2003/09/26 09:04:34 johan Exp $";
/*
 * $Revision: 1.4 $
 * (c) Copyright 1996-2003, TimesTen, Inc.
 * All rights reserved.
 */

/* This source is best displayed with a tabstop of 4 */

#define NDB

//#define MYSQL

#ifdef WIN32
#include <windows.h>
#include "ttRand.h"
#else 
#if !defined NDB && !defined MYSQL
#include <sqlunix.h>
#endif
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef SB_P_OS_CHORUS
#include "ttRand.h"
#endif
#endif

#include <math.h>
#include <time.h>
#include <sql.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#if !defined NDB && !defined MYSQL
#include "ttTime.h"
#include "utils.h"
#include "tt_version.h"
#include "timesten.h"
#endif

#if defined NDB || defined MYSQL
#include <NdbOut.hpp>
#include <string.h>

#include <sqlext.h>
#include <sql.h>
extern "C" {
#include "ttTime.h"
#include "timesten.h"
  void ttGetWallClockTime(ttWallClockTime* timeP);
  void ttCalcElapsedWallClockTime(ttWallClockTime* beforeP,
                                  ttWallClockTime* afterP,
                                  double* nmillisecondsP);
  void ttGetThreadTimes(ttThreadTimes * endRes);
  void ttCalcElapsedThreadTimes(ttThreadTimes*  startRes, 
                                ttThreadTimes * endRes, 
                                double * kernel, 
                                double * user);
}

#define app_exit exit
#define status_msg0 ndbout_c
#define status_msg1 ndbout_c
#define status_msg2 ndbout_c
#define err_msg0 ndbout_c
#define err_msg1 ndbout_c
#define err_msg3 ndbout_c
#define out_msg0 ndbout_c
#define out_msg1 ndbout_c
#define out_msg3 ndbout_c
#define CONN_STR_LEN 255
#define DBMS_TIMESTEN 1
#define DBMS_MSSQL 2
#define DBMS_UNKNOWN 3
#define ABORT_DISCONNECT_EXIT 1
#define NO_EXIT 0
#define ERROR_EXIT  1
#define DISCONNECT_EXIT 2
#endif

#define VERBOSE_NOMSGS 0
/* this value is used for results (and err msgs) only */
#define VERBOSE_RESULTS    1
/* this value is the default for the cmdline demo */
#define VERBOSE_DFLT   2
#define VERBOSE_ALL    3

#ifdef MYSQL
#define DSNNAME "DSN=myodbc3"
#elif defined NDB
#define DSNNAME "DSN=ndb"
#else
#define DSNNAME "DSN="
#endif

/* number of branches, tellers, and accounts */

#define NumBranches             1
#define TellersPerBranch        10
#define AccountsPerBranch       10000

/* number of transactions to execute */

#define NumXacts                25000

/* starting seed value for the random number generator */

#define SeedVal                 84773

/* for MS SQL, the drop, create and use database statements */

#define DatabaseDropStmt        "drop database tpcbDB;"
#ifdef MYSQL
#define DatabaseCreateStmt      "create database tpcbDB;"
#else
#define DatabaseCreateStmt      "create database tpcbDB ON DEFAULT = %d;"
#endif
#define DatabaseUseStmt         "use tpcbDB;"

/*
 * Specifications of table columns.
 * Fillers of 80, 80, 84, and 24 bytes, respectively, are used
 * to ensure that rows are the width required by the benchmark.
 *
 * Note: The TimesTen and MS SQL CREATE TABLE statements for the
 *       accounts, tellers and branches tables are different.
 *
 */

#define TuplesPerPage 256


#ifdef MYSQL

#define AccountCrTblStmt "create table accounts \
(number integer not null primary key, \
branchnum integer not null, \
balance float not null, \
filler char(80));"

#define TellerCrTblStmt "create table tellers \
(number integer not null primary key, \
branchnum integer not null, \
balance float not null, \ 
filler char(80));"

#define BranchCrTblStmt "create table branches \
(number integer not null primary key, \
balance float not null, \
filler char(84));"

#endif


#ifdef NDB
#define AccountCrTblStmt "create table accounts \
(number integer not null primary key, \
branchnum integer not null, \
balance float not null, \
filler char(80)) nologging"

#define TellerCrTblStmt "create table tellers \
(number integer not null primary key, \
branchnum integer not null, \
balance float not null, \ 
filler char(80)) nologging"

#define BranchCrTblStmt "create table branches \
(number integer not null primary key, \
balance float not null, \
filler char(84)) nologging"
#endif

#ifdef NDB

#define HistoryCrTblStmt "create table History \
(tellernum integer not null, \
branchnum integer not null, \
accountnum integer not null, \
delta float not null, \
createtime integer not null, \
filler char(24), \
primary key (tellernum, branchnum, accountnum, delta, createtime)) nologging"

#else

#ifdef MYSQL

#define HistoryCrTblStmt "create table History \
(tellernum integer not null, \
branchnum integer not null, \
accountnum integer not null, \
delta float(53) not null, \
createtime integer not null, \
filler char(24))"
#endif

#define HistoryCrTblStmt "create table History \
(tellernum integer not null, \
branchnum integer not null, \
accountnum integer not null, \
delta float(53) not null, \
createtime integer not null, \
filler char(24));"
#endif

#define TTAccountCrTblStmt "create table accounts \
(number integer not null primary key, \
branchnum integer not null, \
balance float(53) not null, \
filler char(80)) unique hash on (number) pages = %" PTRINT_FMT ";"

#define TTTellerCrTblStmt "create table tellers \
(number integer not null primary key, \
branchnum integer not null, \
balance float(53) not null, \
filler char(80)) unique hash on (number) pages = %" PTRINT_FMT ";"

#define TTBranchCrTblStmt "create table branches \
(number integer not null primary key, \
balance float(53) not null, \
filler char(84)) unique hash on (number) pages = %" PTRINT_FMT ";"


/* Insertion statements used to populate the tables */

#define NumInsStmts 3
char* insStmt[NumInsStmts] = {
  "insert into branches values (?, 0.0, NULL)",
  "insert into tellers  values (?, ?, 0.0, NULL)",
  "insert into accounts values (?, ?, 0.0, NULL)"
};

/* Transaction statements used to update the tables */

#define NumXactStmts 5

#ifdef NDB
char* tpcbXactStmt[NumXactStmts] = {
  "update accounts \
set    balance = balance + ? \
where  number = ?",

  "select balance \
from   accounts \
where  number = ?",

  "update tellers \
set    balance = balance + ? \
where  number = ?",

  "update branches \
set    balance = balance + ? \
where  number = ?",

  "insert into History(tellernum, branchnum, \
accountnum, delta, createtime, filler) \
values (?, ?, ?, ?, ?, NULL)"
};

#else
char* tpcbXactStmt[NumXactStmts] = {
  "update accounts \
set    balance = balance + ? \
where  number = ?;",

  "select balance \
from   accounts \
where  number = ?;",

  "update tellers \
set    balance = balance + ? \
where  number = ?;",

  "update branches \
set    balance = balance + ? \
where  number = ?;",

  "insert into History \
values (?, ?, ?, ?, ?, NULL);"
};


#endif

/* Global parameters and flags (typically set by parse_args()) */

int     tabFlag = 0;                 /* Default is NOT tab output mode */
char    szConnStrIn[CONN_STR_LEN];   /* ODBC Connection String */
int     printXactTimes = 0;          /* Transaction statistics
                                      * gathering flag */
char    statFile[FILENAME_MAX];      /* Transaction statistics filename */
int     scaleFactor = 2;             /* Default TPS scale factor */
int numBranchTups;             /* Number of branches */
int numTellerTups;             /* Number of tellers */
int numAccountTups;            /* Number of accounts */
int numNonLocalAccountTups;    /* Number of local accounts */
int numXacts = NumXacts;       /* Default number of transactions */
int     verbose = VERBOSE_DFLT;      /* Verbose level */
FILE   *statusfp;                    /* File for status messages */



int DBMSType;                 /* DBMS type (DBMS_TIMESTEN, DBMS_MSSQL...) */


SQLHENV henv;                        /* Environment handle */







void handle_errors( SQLHDBC hdbc, SQLHSTMT hstmt, int errcode, int action,                   char * msg,
		  char * file, int line) {

  if (errcode == SQL_SUCCESS)
    return;

  if(errcode == SQL_ERROR) {
    int ret;
    long diagCount=0;
    short length=0;
    SQLCHAR state[10] = "";
    SQLCHAR message[200] = "";
    long native = 0;
    if(hstmt != 0) {
      ret = SQLGetDiagField(SQL_HANDLE_STMT, hstmt, 0, SQL_DIAG_NUMBER, &diagCount, SQL_IS_INTEGER, 0);
      
      for(long i = 0; i < diagCount; i++) {
        ret = SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, i, (SQLCHAR*)state, &native, (SQLCHAR*)message, 200, &length);
        ndbout_c("GetDiagRec: Message : %s  ", message);
      }
    }
  }
    
  if(errcode != SQL_SUCCESS) {
    ndbout_c("Message: %s", msg);
    switch(errcode) {
    case SQL_SUCCESS_WITH_INFO:
      ndbout_c("SQL_SUCCESS_WITH_INFO");
      break;
    case SQL_STILL_EXECUTING:
      ndbout_c("SQL_STILL_EXECUTING");
      break;
    case SQL_ERROR:
      ndbout_c("SQL_ERROR");
      break;
    case SQL_INVALID_HANDLE:
      ndbout_c("SQL_INVALID_HANDLE");
      break;
    default:
      ndbout_c("Some other error");
    }
    exit(1);
  }


}





/*********************************************************************
 *  FUNCTION:       usage
 *
 *  DESCRIPTION:    This function prints a usage message describing
 *                  the command line options of the program.
 *
 *  PARAMETERS:     char* prog      full program path name
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

static void usage(char *prog)
{
  char  *progname;

  /* Get the name of the program (sans path). */

#ifdef WIN32
  progname = strrchr(prog, '\\');
#else
  progname = strrchr(prog, '/');
#endif
  if (progname == 0)
    progname = prog;
  else
    ++progname;

  /* Print the usage message */

  fprintf(stderr,
          "Usage:\t%s [-h] [-help] [-V] [-connStr <string>] [-v <level>]\n"
          "\t\t[-xact <xacts>] [-scale <scale>] [-tabs] [-s <statfile>]\n\n"
          "  -h                  Prints this message and exits.\n"
          "  -help               Same as -h.\n"
          "  -V                  Prints version number and exits.\n"
          "  -connStr <string>   Specifies an ODBC connection string to replace the\n"
          "                      default DSN for the program. The default is\n"
          "                      \"DSN=TpcbData<version>;OverWrite=1\".\n"
          "  -v <level>          Verbose level\n"
          "                         0 = errors only\n"
          "                         1 = results only\n"
          "                         2 = results and some status messages (default)\n"
          "                         3 = all messages\n"
          "  -xact <xacts>       Specifies the number of transactions to be run\n"
          "                      The default is 25000 transactions.\n"
          "  -scale <scale>      Specifies a scale factor which determines the\n"
          "                      number of branches (scale), tellers (scale x 10),\n"
          "                      accounts (scale x 10000) and non-local accounts\n"
          "                      ((scale-1) x 10000. The default scale factor is 2.\n"
          "  -tabs               Specifies that the output be a tab-separated\n"
          "                      format suitable for import into a spreadsheet.\n"
          "                      Results only go to stdout; status and other\n"
          "                      messages go to stderr.\n"
          "  -s <statfile>       Prints individual transaction times to <statfile>.\n",
          progname);
}

/*********************************************************************
 *
 *  FUNCTION:       parse_args
 *
 *  DESCRIPTION:    This function parses the command line arguments
 *                  passed to main(), setting the appropriate global
 *                  variables and issuing a usage message for
 *                  invalid arguments.
 *
 *  PARAMETERS:     int argc         # of arguments from main()
 *                  char *argv[]     arguments  from main()
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

void
parse_args(int argc, char *argv[])
{
  int           i = 1;

  *szConnStrIn = 0;

  while (i < argc) {

    if ( !strcmp(argv[i], "-h") || !strcmp(argv[i], "-help") ) {
      usage(argv[0]);
      app_exit(0);
    }
    /*
    if (!strcmp(argv[i], "-V")) {
      printf("%s\n", TTVERSION_STRING);
      app_exit(0);
    }
    */
    if (strcmp(argv[i], "-s") == 0) {
      if (argc < i+2 ) {
        usage(argv[0]);
        app_exit(1);
      }
      if (sscanf(argv[i+1], "%s", statFile) == 0) {
        usage(argv[0]);
        app_exit(1);
      }
      printXactTimes = 1;
      i += 2;
    }
    else if (!strcmp(argv[i], "-connStr")) {
      if (argc < i+2 ) {
        usage(argv[0]);
        app_exit(1);
      }
      strcpy(szConnStrIn, argv[i+1]);
      i += 2;
      continue;
    }
    else if (strcmp("-v", argv[i]) == 0) {
      if (argc < i+2 ) {
        usage(argv[0]);
        app_exit(1);
      }
      if (sscanf(argv[i+1], "%d", &verbose) == -1 ||
          verbose < 0 || verbose > 3) {
        fprintf(stderr, "-v flag requires an integer parameter (0-3)\n");
        usage(argv[0]);
        app_exit(1);
      }
      i += 2;
    }
    else if (strcmp("-xact",argv[i]) == 0) {
      if (argc < i+2 ) {
        usage(argv[0]);
        app_exit(1);
      }
      
      if (sscanf(argv[i+1], "%" PTRINT_FMT, &numXacts) == -1 || numXacts < 0) {
        fprintf(stderr, "-xact flag requires a non-negative integer argument\n");
        usage(argv[0]);
        app_exit(1);
      }
      
      i += 2;
    }
    else if (strcmp("-scale",argv[i]) == 0) {
      if (argc < i+2 ) {
        usage(argv[0]);
        app_exit(1);
      }
      if (sscanf(argv[i+1], "%d", &scaleFactor) == -1 || scaleFactor < 1) {
        fprintf(stderr, "-scale flag requires an integer argument >= 1\n");
        usage(argv[0]);
        app_exit(1);
      }
      /* Calculate tuple sizes */
      numBranchTups = NumBranches * scaleFactor;
      numTellerTups = TellersPerBranch * scaleFactor;
      numAccountTups = AccountsPerBranch * scaleFactor;
      numNonLocalAccountTups = AccountsPerBranch * (scaleFactor-1);
      i += 2;
    }
    else if (strcmp("-tabs",argv[i]) == 0) {
      tabFlag = 1;
      statusfp = stderr;
      i += 1;
    }
    else {
      usage(argv[0]);
      app_exit(1);
    }
  }
}

/*********************************************************************
 *
 *  FUNCTION:       doImmed
 *
 *  DESCRIPTION:    This function executes and frees the specified
 *                  statement. It is used as a direct means to
 *                  create the tables used by this benchmark,
 *
 *  PARAMETERS:     SQLHDBC hdbc    SQL Connection handle
 *                  SQLHSTMT hs     SQL Statement handle
 *                  char* cmd       SQL Statement text
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

void
doImmed(SQLHDBC hdbc, SQLHSTMT hs, char* cmd)
{
  SQLRETURN rc;

  /* Execute the command */

  rc = SQLExecDirect(hs, (SQLCHAR *) cmd, SQL_NTS);
  handle_errors(hdbc, hs, rc, ABORT_DISCONNECT_EXIT,
                "Error executing statement", __FILE__, __LINE__);

  /* Close associated cursor and drop pending results */

  rc = SQLFreeStmt(hs, SQL_CLOSE);
  handle_errors(hdbc, hs, rc, ABORT_DISCONNECT_EXIT,
                "closing statement handle",
                __FILE__, __LINE__);

}


/*********************************************************************
 *
 *  FUNCTION:       main
 *
 *  DESCRIPTION:    This is the main function of the tpcb benchmark.
 *                  It connects to an ODBC data source, creates and
 *                  populates tables, updates the tables in a user-
 *                  specified number of transactions and reports on
 *                  on the transaction times.
 *
 *  PARAMETERS:     int argc        # of command line arguments
 *                  char *argv[]    command line arguments
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

int
main(int argc, char *argv[])
{

  /* variables used for setting up the tables */

  char               cmdStr[1024];
  char               errstr[4096];

  /* variables used during transactions */

  int                accountNum;
  int                tellerNum;
  int                branchNum;
  int                timeStamp;
  double             delta;
  unsigned int               lrand;
  unsigned short     *srands, localLimit;
  int                lp64;

  /* variables used for timing and statistics */

  int                warmup;
  double             kernel, user, real;
  ttThreadTimes      startRes, endRes;
  ttWallClockTime    startT, endT;
  ttWallClockTime**  rtStart;
  ttWallClockTime**  rtEnd;
  double**           resTime;
  double             maxTime, totTime;
  int                i;
  int                j;
  int          numLocalXacts=0, numRemoteXacts=0;

  /* variables for ODBC */

  SQLHDBC            hdbc;
  SQLHSTMT           hstmt;
  SQLHSTMT           txstmt[NumXactStmts];
  SQLRETURN          rc;
  char               DBMSName[32];
  char               DBMSVersion[32];
  int                databaseSize;

  int                fThreadTime = 1;

#ifdef WIN32
  OSVERSIONINFO      sysInfo;

  sysInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  GetVersionEx (&sysInfo);

    /* GetThreadTimes is not supported on 95/98. Hence,
     we do not support Resource/User/System times */
  if (sysInfo.dwPlatformId != VER_PLATFORM_WIN32_NT)
        fThreadTime = 0;
#endif
#if defined(TTCLIENTSERVER) && defined(__hpux) && !defined(__LP64__)
  /* HP requires this for C main programs that call aC++ shared libs */
  _main();
#endif /* hpux32 */

  /* Set up default signal handlers */

#ifndef NDB
  /*  StopRequestClear();
  if (HandleSignals() != 0) {
    err_msg0("Unable to set signal handlers\n");
    return 1;
  }
  */
#endif
  /* set IO mode for demo */
  /* set_io_mode(); */

  /* initialize the file for status messages */
  statusfp = stdout;

  /* set variable for 8-byte longs */
  lp64 = (sizeof(lrand) == 8);

  /* set the default tuple sizes */

  numBranchTups = NumBranches * scaleFactor;
  numTellerTups = TellersPerBranch * scaleFactor;
  numAccountTups = AccountsPerBranch * scaleFactor;
  numNonLocalAccountTups = AccountsPerBranch * (scaleFactor-1);

  /* parse the command arguments */
  parse_args(argc, argv);

  /* allocate the transaction-based variables */

  rtStart = (ttWallClockTime**) malloc(numXacts * sizeof(ttWallClockTime*));
  if (!rtStart) {
    err_msg0("Cannot allocate the transaction timing structures");
    app_exit(1);
  }
  for (i = 0; i < numXacts; i++) {
    rtStart[i] = (ttWallClockTime*) malloc(sizeof(ttWallClockTime));
    if (!rtStart[i]) {
      err_msg0("Cannot allocate the transaction timing structures");
      app_exit(1);
    }
  }

  rtEnd = (ttWallClockTime**) malloc(numXacts * sizeof(ttWallClockTime*));
  if (!rtEnd) {
    err_msg0("Cannot allocate the transaction timing structures");
    app_exit(1);
  }
  for (i = 0; i < numXacts; i++) {
    rtEnd[i] = (ttWallClockTime*) malloc(sizeof(ttWallClockTime));
    if (!rtEnd[i]) {
      err_msg0("Cannot allocate the transaction timing structures");
      app_exit(1);
    }
  }

  resTime = (double**) malloc(numXacts * sizeof(double*));
  if (!resTime) {
    err_msg0("Cannot allocate the transaction timing structures");
    app_exit(1);
  }
  for (i = 0; i < numXacts; i++) {
    resTime[i] = (double*) malloc(sizeof(double));
    if (!resTime[i]) {
      err_msg0("Cannot allocate the transaction timing structures");
      app_exit(1);
    }
  }

  /* ODBC initialization */

  rc = SQLAllocEnv(&henv);
  if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
    /* error occurred -- don't bother calling handle_errors, since handle
     * is not valid so SQLError won't work */
    err_msg3("ERROR in %s, line %d: %s\n",
             __FILE__, __LINE__, "allocating an environment handle");
    app_exit(1);
  }
  SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, SQL_IS_INTEGER);

  /* call this in case of warning */
  handle_errors(NULL, NULL, rc, NO_EXIT,
                "allocating execution environment",
                __FILE__, __LINE__);

  rc = SQLAllocConnect(henv, &hdbc);
  handle_errors(NULL, NULL, rc, ERROR_EXIT,
                "allocating connection handle",
                __FILE__, __LINE__);

  /* Connect to data store */

  status_msg0("Connecting to the data source...\n");

  /* Set up the connection options if not specified on the command line
   *    (default to TimesTen settings).
   */

  if ( !*szConnStrIn ) {
    /* Running the benchmark with a scale factor creates (scale) branches,
     * (scale x 10) tellers, (scale x 10000) accounts and ((scale-1) x 10000)
     * non-local accounts. The size of the table rows are branches (141)
     * tellers (141) and accounts (141). Therefore the data size requirements
     * of this benchmark is:
     *   size ~= 141 * ((scale * 20011) - 10000) (bytes)
     *
     * Multiply data size by 20% to account for additional DB overhead (e.g.
     * indexes), and round up the nearest 10Mb for safety.
     */

    int est_size = (int) (3.6 * scaleFactor + 10.0);
    est_size = est_size - (est_size % 10);

    sprintf(szConnStrIn,"OverWrite=1;PermSize=%d;%s",
            est_size, DSNNAME);
    status_msg0("Connecting to the data source... %s \n", szConnStrIn);
  }

  rc = SQLDriverConnect(hdbc, NULL, (SQLCHAR *) szConnStrIn, SQL_NTS,
                        NULL, 0, NULL,
                        SQL_DRIVER_NOPROMPT);

  status_msg0("Connected to the data source...\n");
  sprintf(errstr, "connecting to driver (connect string %s)\n",
          szConnStrIn);
  handle_errors(hdbc, NULL, rc, ERROR_EXIT,
                errstr, __FILE__, __LINE__);

  /* Turn auto-commit off */

  rc = SQLSetConnectOption(hdbc, SQL_AUTOCOMMIT, SQL_AUTOCOMMIT_OFF);
  handle_errors(hdbc, NULL, rc, DISCONNECT_EXIT,
                "switching off the AUTO_COMMIT option",
                __FILE__, __LINE__);

  /* Allocate a statement handle */

  rc = SQLAllocStmt(hdbc, &hstmt);
  handle_errors(hdbc, NULL, rc, DISCONNECT_EXIT,
                "allocating a statement handle",
                __FILE__, __LINE__);

  /* (Implicit) Transaction begin */

  /* Determine the DBMS Type*/

  DBMSName[0] = '\0';
  rc = SQLGetInfo(hdbc, SQL_DBMS_NAME, (PTR) &DBMSName,
                  sizeof(DBMSName), NULL);
  rc = SQLGetInfo(hdbc, SQL_DRIVER_VER, (PTR) &DBMSVersion,
                  sizeof(DBMSVersion), NULL);

  if (strcmp(DBMSName, "TimesTen") == 0)
    DBMSType = DBMS_TIMESTEN;
  else if (strcmp(DBMSName, "Microsoft SQL Server") == 0)
    DBMSType = DBMS_MSSQL;
  else DBMSType = DBMS_UNKNOWN;

  /* if not TimesTen: delete (if it exists), create & use the new database */

  if (DBMSType != DBMS_TIMESTEN) {
    status_msg0("Deleting the database...\n");
    rc = SQLExecDirect(hstmt, (SQLCHAR *) DatabaseDropStmt, SQL_NTS);

    /* estimate database size, size = data space + log space
     * data space = (#tuples)/(tuples per page) * 2K bytes/page
     * tuples per page = useable page size / row size (no index) = 2016/(96+2)
     * log space = #transactions * average log size for the program transaction mix
     * database size is in MB
     */

    databaseSize = (int) ceil((((numBranchTups + numTellerTups + numAccountTups)/
                                (2016/98)) * 2048 + (numXacts * 600)) / 1000000.0);

    status_msg1("Creating the database (%dMB)...\n", databaseSize);
#ifndef NDB
    sprintf(cmdStr, DatabaseCreateStmt, databaseSize);
    doImmed(hdbc, hstmt, cmdStr);
    strcpy(cmdStr, DatabaseUseStmt);
    doImmed(hdbc, hstmt, cmdStr);
#endif
  }

  status_msg2("Connected to '%s' version '%s'...\n", DBMSName, DBMSVersion);

  /* create branches table */
  status_msg0("Creating tasddbles...\n");
#ifndef NDB
  if (DBMSType == DBMS_TIMESTEN)
    sprintf(cmdStr, TTBranchCrTblStmt, numBranchTups/TuplesPerPage + 1);
  else
#endif
    sprintf(cmdStr, BranchCrTblStmt);
  doImmed(hdbc, hstmt, cmdStr);

  /* create tellers table */
#ifndef NDB
  if (DBMSType == DBMS_TIMESTEN)
    sprintf(cmdStr, TTTellerCrTblStmt, numTellerTups/TuplesPerPage + 1);

  else
#endif
    sprintf(cmdStr, TellerCrTblStmt);
  doImmed(hdbc, hstmt, cmdStr);

  /* create accounts table */
#ifndef NDB
  if (DBMSType == DBMS_TIMESTEN)
    sprintf(cmdStr, TTAccountCrTblStmt, numAccountTups/TuplesPerPage + 1);
  else
#endif
    sprintf(cmdStr, AccountCrTblStmt);
  doImmed(hdbc, hstmt, cmdStr);
  
  /* create History table */
  
  doImmed(hdbc, hstmt, HistoryCrTblStmt);

  /* lock the database during population */
#ifndef NDB
  if ( DBMSType == DBMS_TIMESTEN ) {
    rc = SQLExecDirect(hstmt, (SQLCHAR *)"call ttlocklevel('DS')", SQL_NTS);
    handle_errors(hdbc, hstmt, rc,  ABORT_DISCONNECT_EXIT,
                  "specifying dbs lock usage",
                  __FILE__, __LINE__);
    /* make sure dbs lock take effect in next transaction */
    rc = SQLTransact(henv,hdbc,SQL_COMMIT);
    if ( rc != SQL_SUCCESS) {
      handle_errors(hdbc, SQL_NULL_HSTMT, rc, ERROR_EXIT,
                    "committing transaction",
                    __FILE__, __LINE__);
    }
  }
#endif
  /* populate branches table */
  

  rc = SQLPrepare(hstmt, (SQLCHAR *) insStmt[0], SQL_NTS);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "preparing statement",
                __FILE__, __LINE__);
  
  rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 10, 0, &branchNum, sizeof branchNum, NULL);
  
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);


  status_msg1("Populating branches table (%" PTRINT_FMT " rows)...\n",
              numBranchTups);


  for (i=0; i<numBranchTups; i++) {
    branchNum = i;
    rc = SQLExecute(hstmt);
    handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                  "Error executing statement",
                  __FILE__, __LINE__);
  }

  /* Reset all bind-parameters for the statement handle. */
  rc = SQLFreeStmt(hstmt, SQL_RESET_PARAMS);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "resetting parms on statement handle",
                __FILE__, __LINE__);

  /* populate tellers table */

  status_msg1("Populating tellers table (%" PTRINT_FMT " rows)...\n",
              numTellerTups);


  rc = SQLPrepare(hstmt, (SQLCHAR *) insStmt[1], SQL_NTS);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "preparing statement",
                __FILE__, __LINE__);

  rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                        10, 0, &tellerNum, sizeof tellerNum, NULL);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                        10, 0, &branchNum, sizeof branchNum, NULL);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  for (i=0; i<numTellerTups; i++) {
    tellerNum = i;
    branchNum = i/TellersPerBranch;
    rc = SQLExecute(hstmt);
    handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                  "Error executing statement",
                  __FILE__, __LINE__);
  }

  /* Reset all bind-parameters for the statement handle. */

  rc = SQLFreeStmt(hstmt, SQL_RESET_PARAMS);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "resetting parms on statement handle",
                __FILE__, __LINE__);

  /* populate accounts table */

  status_msg1("Populating accounts table (%" PTRINT_FMT " rows)...\n",
              numAccountTups);

  rc = SQLPrepare(hstmt, (SQLCHAR *) insStmt[2], SQL_NTS);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "preparing statement",
                __FILE__, __LINE__);

  rc = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                        10, 0, &accountNum, sizeof accountNum, NULL);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                        10, 0, &branchNum, sizeof branchNum, NULL);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  for (i=0; i<numAccountTups; i++) {
    accountNum = i;
    branchNum = i/AccountsPerBranch;
    rc = SQLExecute(hstmt);
    handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                  "Error executing statement",
                  __FILE__, __LINE__);
  }
  status_msg0("Commit...\n");
  rc = SQLTransact(henv, hdbc, SQL_COMMIT);
  status_msg0("Commit done...\n");
  handle_errors(hdbc, NULL, rc, ERROR_EXIT,
                "committing transaction",
                __FILE__, __LINE__);

  /* compile SQL statements of transaction */

  status_msg0("Compiling statements of transaction...\n");
  for (i=0; i<NumXactStmts; i++) {
#ifndef NDB
    rc = SQLAllocStmt(hdbc, &txstmt[i]);
    handle_errors(hdbc, NULL, rc, ABORT_DISCONNECT_EXIT,
                  "allocating a statement handle",
                  __FILE__, __LINE__);
#else
    rc = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &txstmt[i]);
    handle_errors(hdbc, NULL, rc, ABORT_DISCONNECT_EXIT,
                  "allocating a statement handle",
                  __FILE__, __LINE__);

#endif
   
    rc = SQLPrepare(txstmt[i], (SQLCHAR *) tpcbXactStmt[i], SQL_NTS);
    handle_errors(hdbc, txstmt[i], rc, ABORT_DISCONNECT_EXIT,
                  "preparing statement",
                  __FILE__, __LINE__);
  }

  /*  unuse dbs lock */
#ifndef NDB
  if ( DBMSType == DBMS_TIMESTEN ) {
    rc = SQLExecDirect(hstmt, (SQLCHAR *)"call ttlocklevel('Row')", SQL_NTS);
    handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                  "specifying row lock usage",
                  __FILE__, __LINE__);
  }
#endif

  /* commit transaction */

  rc = SQLTransact(henv, hdbc, SQL_COMMIT);
  handle_errors(hdbc, NULL, rc, ERROR_EXIT,
                "committing transaction",
                __FILE__, __LINE__);


  /* Initialize random seed and timers */

  srand48(SeedVal);
  localLimit = (unsigned short)((1<<16) * 0.85);

  /* Initialize parameter lists for each of the transactions */

  rc = SQLBindParameter(txstmt[0], 1, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                        SQL_DOUBLE, 15, 0, &delta, sizeof delta, NULL);
  handle_errors(hdbc, txstmt[0], rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(txstmt[0], 2, SQL_PARAM_INPUT, SQL_C_SLONG,
                        SQL_INTEGER, 10, 0, &accountNum, sizeof accountNum,
                        NULL);
  handle_errors(hdbc, txstmt[0], rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(txstmt[1], 1, SQL_PARAM_INPUT, SQL_C_SLONG,
                        SQL_INTEGER, 10, 0, &accountNum, sizeof accountNum,
                        NULL);
  handle_errors(hdbc, txstmt[1], rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(txstmt[2], 1, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                        SQL_DOUBLE, 15, 0, &delta, sizeof delta, NULL);
  handle_errors(hdbc, txstmt[2], rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(txstmt[2], 2, SQL_PARAM_INPUT, SQL_C_SLONG,
                        SQL_INTEGER, 10, 0, &tellerNum, sizeof tellerNum,
                        NULL);
  handle_errors(hdbc, txstmt[2], rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(txstmt[3], 1, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                        SQL_DOUBLE, 15, 0, &delta, sizeof delta, NULL);
  handle_errors(hdbc, txstmt[3], rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(txstmt[3], 2, SQL_PARAM_INPUT, SQL_C_SLONG,
                        SQL_INTEGER, 10, 0, &branchNum, sizeof branchNum,
                        NULL);
  handle_errors(hdbc, txstmt[3], rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(txstmt[4], 1, SQL_PARAM_INPUT, SQL_C_SLONG,
                        SQL_INTEGER, 10, 0, &tellerNum, sizeof tellerNum,
                        NULL);
  handle_errors(hdbc, txstmt[4], rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(txstmt[4], 2, SQL_PARAM_INPUT, SQL_C_SLONG,
                        SQL_INTEGER, 10, 0, &branchNum, sizeof branchNum,
                        NULL);
  handle_errors(hdbc, txstmt[4], rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(txstmt[4], 3, SQL_PARAM_INPUT, SQL_C_SLONG,
                        SQL_INTEGER, 10, 0, &accountNum, sizeof accountNum,
                        NULL);
  handle_errors(hdbc, txstmt[4], rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(txstmt[4], 4, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                        SQL_DOUBLE, 15, 0, &delta, sizeof delta, NULL);
  handle_errors(hdbc, txstmt[4], rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  rc = SQLBindParameter(txstmt[4], 5, SQL_PARAM_INPUT, SQL_C_SLONG,
                        SQL_INTEGER, 10, 0, &timeStamp, sizeof timeStamp,
                        NULL);
  handle_errors(hdbc, txstmt[4], rc, ABORT_DISCONNECT_EXIT,
                "binding parameter",
                __FILE__, __LINE__);

  /* Execute transaction loop.
   * Do it twice, once briefly as a warm-up. */



  for (warmup = 1; warmup >= 0; warmup--) {

    int max_i = (warmup ? numXacts/10 : numXacts);

    /* Execute tpcb transaction max_i times.*/

    if (warmup) {
      status_msg1("\nWarming up with %d tpcb transactions...\n", max_i);
    }
    else {
      status_msg1("Executing and timing %d tpcb transactions...\n", max_i);
    }

    ttGetWallClockTime(&startT);
    ttGetThreadTimes(&startRes);

    for (i = 0; i < max_i; i++) {

      lrand = lrand48();
      srands = (unsigned short *)(&lrand);
      if (lp64) srands += 2; /* skip high half -- all zero */

      /* randomly choose a teller */

      tellerNum = srands[0] % numTellerTups;

      /* compute branch */

      branchNum = (tellerNum / TellersPerBranch);

      /* randomly choose an account */

      if (srands[1] < localLimit || numBranchTups == 1) {

        /* choose account local to selected branch */

        accountNum = branchNum * AccountsPerBranch +
          (lrand48() % AccountsPerBranch);

        ++numLocalXacts;

      }
      else {
        /* choose account not local to selected branch */

        /* first select account in range [0,numNonLocalAccountTups) */

        accountNum = lrand48() % numNonLocalAccountTups;

        /* if branch number of selected account is at least as big
         * as local branch number, then increment account number
         * by AccountsPerBranch to skip over local accounts
         */

        if ((accountNum/AccountsPerBranch) >= branchNum)
          accountNum += AccountsPerBranch;

        ++numRemoteXacts;
      }

      /* select delta amount, -999,999 to +999,999 */

      delta = ((lrand48() % 1999999) - 999999);


      /* begin timing the "residence time" */

      ttGetWallClockTime(rtStart[i]);

      for ( j = 0; j < NumXactStmts - 2; j++) {
        rc = SQLExecute(txstmt[j]);
        handle_errors(hdbc, txstmt[j], rc, ABORT_DISCONNECT_EXIT,
                      "Error executing statement1",
                      __FILE__, __LINE__);

        /* Close the handle after the SELECT statement
         * (txstmt[1]) for non TimesTen DBMS' */

        if ((DBMSType != DBMS_TIMESTEN) && (j == 1)) {
          SQLFreeStmt(txstmt[1], SQL_CLOSE);
        }
        
        
      }

      /* note that time must be taken within the */
      timeStamp = time(NULL);
       
      rc = SQLExecute(txstmt[NumXactStmts - 1]);
            handle_errors(hdbc, txstmt[NumXactStmts - 1], rc,
                    ABORT_DISCONNECT_EXIT, "Error executing statement2",
                    __FILE__, __LINE__);
      
      rc = SQLTransact(henv, hdbc, SQL_COMMIT);
      handle_errors(hdbc, NULL, rc, ERROR_EXIT,
                    "Error committing transaction",
                    __FILE__, __LINE__);

      ttGetWallClockTime(rtEnd[i]);

    } /* end fortransaction loop */




    ttGetThreadTimes(&endRes);
    ttGetWallClockTime(&endT);
    ttCalcElapsedThreadTimes(&startRes, &endRes, &kernel, &user);
    ttCalcElapsedWallClockTime(&startT, &endT, &real);

    if (warmup) {
      if (!tabFlag) {
        if (verbose) {
          if (fThreadTime) {
            out_msg0("                           time              user            system\n");

            out_msg3("Warmup time (sec): %12.3f      %12.3f      %12.3f\n\n",
                     real/1000.0, user, kernel);
          } else {
            out_msg1("Warmup time (sec): %12.3f\n\n", real/1000.0);
          }
        }
      } else {
        if (verbose) {
          if (fThreadTime) {
            out_msg0("\ttime\tuser\tsystem\n");

            out_msg3("Warmup time (sec):\t%12.3f\t%12.3f\t%12.3f\n",
                     real/1000.0, user, kernel);
          } else {
            out_msg1("Warmup time (sec):\t%12.3f\n", real/1000.0);
          }
        }
      }
    }
  }

  status_msg0("\nExecution completed...\n");

  /* Compute and report timing statistics */

  maxTime = 0.0;
  totTime = 0.0;

  for (i = 0; i < numXacts; i++) {
    ttCalcElapsedWallClockTime(rtStart[i], rtEnd[i], resTime[i]);
    totTime += *(resTime[i]);

    if (*(resTime[i]) > maxTime) maxTime = *(resTime[i]);
  }

  if (!tabFlag) {
    if (verbose) {
      if (fThreadTime) {
        out_msg0("                            time              user            system\n");
        out_msg3("Total time (sec):   %12.3f      %12.3f      %12.3f\n",
                 real/1000.0, user, kernel);
      } else {
        out_msg1("Total time (sec):   %12.3f\n", real/1000.0);
      }
    }

    if (verbose)
      out_msg1("\nAverage transaction time (msec):%12.3f\n",
               totTime/numXacts);
    if (verbose)
      out_msg1("Maximum transaction time (msec):%12.3f\n", maxTime);
    if (verbose)
      out_msg1("\nLocal transactions:  %7" PTRINT_FMT "\n", numLocalXacts);
    if (verbose)
      out_msg1("Remote transactions: %7" PTRINT_FMT "\n", numRemoteXacts);

  } else {
    if (verbose) {
      if (fThreadTime) {
        out_msg0("\ttime\tuser\tsystem\n");
        out_msg3("Total time (sec):\t%12.3f\t%12.3f\t%12.3f\n",
                 real/1000.0, user, kernel);
      } else {
        out_msg1("Total time (sec):\t%12.3f\n", real/1000.0);
      }
    }

    if (verbose)
      out_msg1("\nAverage transaction time (msec):\t%12.3f\n",
               totTime/numXacts);
    if (verbose)
      out_msg1("Maximum transaction time (msec):\t%12.3f\n", maxTime);
    if (verbose)
      out_msg1("Local transactions:\t%7" PTRINT_FMT "\n", numLocalXacts);

    if (verbose)
      out_msg1("Remote transactions:\t%7" PTRINT_FMT "\n", numRemoteXacts);


  
  }

  /* If the statfile option is selected, print each transaction's time */

  if (printXactTimes) {
    FILE * fp;
    if ( (fp = fopen (statFile, "w")) == NULL ) {
      err_msg1("Unable to open stat file %s for writing\n\n", statFile);
    } else {
      for (int i = 0; i < numXacts; i++)
        fprintf(fp,"%6d: %12.3f\n", i, *(resTime[i]));
      fclose(fp);
    }
  }

  /* Disconnect and return */

  rc = SQLFreeStmt(hstmt, SQL_DROP);
  handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                "dropping the statement handle",
                __FILE__, __LINE__);

  for (int i=0; i<NumXactStmts; i++) {
    rc = SQLFreeStmt(txstmt[i], SQL_DROP);
    handle_errors(hdbc, hstmt, rc, ABORT_DISCONNECT_EXIT,
                  "dropping the statement handle",
                  __FILE__, __LINE__);
  }

  if (verbose >= VERBOSE_DFLT)
    status_msg0("Disconnecting from the data source...\n");

  rc = SQLDisconnect(hdbc);
  handle_errors(hdbc, NULL, rc, ERROR_EXIT,
                "disconnecting",
                __FILE__, __LINE__);

  rc = SQLFreeConnect(hdbc);
  handle_errors(hdbc, NULL, rc, ERROR_EXIT,
                "freeing connection handle",
                __FILE__, __LINE__);

  rc = SQLFreeEnv(henv);
  handle_errors(NULL, NULL, rc, ERROR_EXIT,
                "freeing environment handle",
                __FILE__, __LINE__);

  app_exit(0);
  return 0;
}





/* Emacs variable settings */
/* Local Variables: */
/* tab-width:8 */
/* indent-tabs-mode:nil */
/* c-basic-offset:2 */
/* End: */



