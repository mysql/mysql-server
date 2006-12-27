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

/*******************************************************************************
 *  NDB Cluster NDB SQL -- A simple SQL Command-line Interface
 *
 ******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef NDB_MACOSX
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#include <errno.h>
#include <editline/editline.h>
#include <NdbOut.hpp>
#include <ctype.h>
#include <wctype.h>

#ifndef SQL_BLOB
#define SQL_BLOB                30
#endif
#ifndef SQL_CLOB
#define SQL_CLOB                40
#endif

/**************************************************************************
 * ------------------------------------------------------------------------
 *  MODULE:       Readline and string handling
 * ------------------------------------------------------------------------
 **************************************************************************/
#define MAXBUF 2048
static char* s_readBuf;
static int s_bufSize = MAXBUF;

static char*
readSQL_File(FILE* inputFile)
{
  int c;
  int i = 0;
  if (feof(inputFile))
    return 0;
  while ((c = getc(inputFile)) != EOF) {
    if (i == s_bufSize-1) {
      s_bufSize *= 2;
      s_readBuf = (char*)realloc(s_readBuf, s_bufSize);
    }
    s_readBuf[i] = c;
    if (c == '\n')
      break;
    i++;
  }
  s_readBuf[i] = 0;
  return s_readBuf;
}
  
static char*
readline_gets(const char* prompt, bool batchMode, FILE* inputFile)
{
  static char *line_read = (char *)NULL;
  
  // Disable the default file-name completion action of TAB
  // rl_bind_key ('\t', rl_insert);
  
  if (batchMode)
    /* Read one line from a file. */
    line_read = readSQL_File(inputFile);
  else
    /* Get a line from the user. */
    line_read = readline(prompt);
  
  /* If the line has any text in it, save it in the history. */
  if (!batchMode)
    if (line_read && *line_read) add_history(line_read);
    
  return (line_read);
}

#ifdef NDB_WIN32
extern "C" 
{
  char* readline(const char* prompt)
  {
    fputs(prompt, stdout);
    return fgets(s_readBuf, MAXBUF, stdin);
  }
  void add_history(char*)
  {
  }
}
#endif

bool emptyString(const char* s) {
  if (s == NULL) {
    return true;
  }

  for (unsigned int i = 0; i < strlen(s); ++i) {
    if (! isspace(s[i])) {
      return false;
    }
  }

  return true;
}

/**************************************************************************
 * ------------------------------------------------------------------------
 *  MODULE:       ODBC Handling
 * ------------------------------------------------------------------------
 **************************************************************************/

#include <sqlext.h>
#include <stdio.h>
#include <string.h>
#ifdef NDB_MACOSX
#include <stdlib.h>
#else
#include <malloc.h>
#endif
/**
 * In the case where the user types a SELECT statement, 
 * the function fetches and displays all rows of the result set.
 *
 * This example illustrates the use of GetDiagField to identify the
 * type of SQL statement executed and, for SQL statements where the
 * row count is defined on all implementations, the use of GetDiagField
 * to obtain the row count.
 */
#define MAXCOLS 100
#undef max
#define max(a,b) ((a)>(b)?(a):(b))

#define MAX_MESSAGE	500

void getDiag(SQLSMALLINT type, SQLHANDLE handle, unsigned k, unsigned count) 
{
  char message[MAX_MESSAGE];
  char state[6];
  SQLINTEGER native;

  SQLSMALLINT length = -1;
  memset(message, 0, MAX_MESSAGE);
  int ret = SQLGetDiagRec(type, handle, k, (SQLCHAR*)state, 
			  &native, (SQLCHAR*)message, MAX_MESSAGE, &length);
  if (ret == SQL_NO_DATA) {
    ndbout << "No error diagnostics available" << endl;
    return;
  }
  ndbout << message << endl;

  if (k <= count && ret != SQL_SUCCESS)
    ndbout_c("SQLGetDiagRec %d of %d: return %d != SQL_SUCCESS", 
	     k, count, (int)ret);
  if (k <= count && (SQLSMALLINT) strlen(message) != length)
    ndbout_c("SQLGetDiagRec %d of %d: message length %d != %d", 
	     k, count, strlen(message), length);
  if (k > count && ret != SQL_NO_DATA)
    ndbout_c("SQLGetDiagRec %d of %d: return %d != SQL_NO_DATA", 
	     k, count, (int)ret);
}

int print_err(SQLSMALLINT handletype, SQLHDBC hdbc) {
  getDiag(handletype, hdbc, 1, 1);

  return -1;
}


/***************************************************************
 * The following functions are given for completeness, but are
 * not relevant for understanding the database processing
 * nature of CLI
 ***************************************************************/
#define MAX_NUM_PRECISION 15
/*#define max length of char string representation of no. as:
= max(precision) + leading sign +E +expsign + max exp length
= 15 +1 +1 +1 +2
= 15 +5
*/
#define MAX_NUM_STRING_SIZE (MAX_NUM_PRECISION + 5)

int build_indicator_message(SQLCHAR *errmsg, SQLPOINTER *data,
			    SQLINTEGER collen, SQLINTEGER *outlen, 
			    SQLSMALLINT colnum) {
  if (*outlen == SQL_NULL_DATA) {
    (void)strcpy((char *)data, "NULL");
    *outlen=4;
  } else {
    sprintf((char *)errmsg+strlen((char *)errmsg),
	    "%ld chars truncated, col %d\n", *outlen-collen+1,
	    colnum);
    *outlen=255;
  }
  return 0;
}


SQLINTEGER display_length(SQLSMALLINT coltype, SQLINTEGER collen, 
			  SQLCHAR *colname) {
  switch (coltype) {
  case SQL_VARCHAR:
  case SQL_CHAR:
  case SQL_VARBINARY:
  case SQL_BINARY:
  case SQL_BLOB:
  case SQL_CLOB:
  case SQL_BIT:
    //case SQL_REF:
    //case SQL_BIT_VARYING:
    return(max(collen,(SQLINTEGER) strlen((char *)colname))+1);
  case SQL_FLOAT:
  case SQL_DOUBLE:
  case SQL_NUMERIC:
  case SQL_REAL:
  case SQL_DECIMAL:
    return(max(MAX_NUM_STRING_SIZE,strlen((char *)colname))+1);
  case SQL_TYPE_DATE:
  case SQL_TYPE_TIME:
    //case SQL_TYPE_TIME_WITH_TIMEZONE:
  case SQL_TYPE_TIMESTAMP:
    //case SQL_TYPE_TIMESTAMP_WITH_TIMEZONE:
  case SQL_INTERVAL_YEAR:
  case SQL_INTERVAL_MONTH:
  case SQL_INTERVAL_DAY:
  case SQL_INTERVAL_HOUR:
  case SQL_INTERVAL_MINUTE:
  case SQL_INTERVAL_SECOND:
  case SQL_INTERVAL_YEAR_TO_MONTH:
  case SQL_INTERVAL_DAY_TO_HOUR:
  case SQL_INTERVAL_DAY_TO_MINUTE:
  case SQL_INTERVAL_DAY_TO_SECOND:
  case SQL_INTERVAL_HOUR_TO_MINUTE:
  case SQL_INTERVAL_HOUR_TO_SECOND:
  case SQL_INTERVAL_MINUTE_TO_SECOND:
    return(max(collen,(SQLINTEGER) strlen((char *)colname))+1);
  case SQL_INTEGER:
    //case SQL_BLOB_LOCATOR:
    //case SQL_CLOB_LOCATOR:
    //case SQL_UDT_LOCATOR:
    //case SQL_ARRAY_LOCATOR:
    return(max(11,strlen((char *)colname))+1);
  case SQL_BIGINT:
    return(max(21,strlen((char *)colname))+1);
  case SQL_SMALLINT:
    return(max(5,strlen((char *)colname))+1);
  default:
    (void)printf("Unknown datatype, %d\n", coltype);
    return(0);
  }
}

struct Con {
  const char* dsn;
  SQLHENV henv;
  SQLHDBC hdbc;
  Con(const char* _dsn) :
    dsn(_dsn), henv(SQL_NULL_HANDLE), hdbc(SQL_NULL_HANDLE) {}
};

static int
do_connect(Con& con)
{
  int ret;

  // allocate an environment handle
  ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &con.henv);
  if (ret != SQL_SUCCESS)
    return -1;
  
  // set odbc version (required)
  ret = SQLSetEnvAttr(con.henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0); 
  if (ret != SQL_SUCCESS)
    return -1;
  
  // allocate a connection handle
  ret = SQLAllocHandle(SQL_HANDLE_DBC, con.henv, &con.hdbc);
  if (ret != SQL_SUCCESS)
    return -1;
  
  // connect to database
  SQLCHAR szConnStrOut[256];
  SQLSMALLINT cbConnStrOut;
  ret = SQLDriverConnect(con.hdbc, 0, (SQLCHAR*)con.dsn, SQL_NTS,
      szConnStrOut, sizeof(szConnStrOut), &cbConnStrOut, SQL_DRIVER_COMPLETE);
  if (ret != SQL_SUCCESS) {
    ndbout << "Connection failure: Could not connect to database" << endl;
    print_err(SQL_HANDLE_DBC, con.hdbc);
    return -1;
  }  

  return 0;
}

static int
do_disconnect(Con& con)
{
  // disconnect from database
  SQLDisconnect(con.hdbc);

  // free connection handle
  SQLFreeHandle(SQL_HANDLE_DBC, con.hdbc);
  con.hdbc = SQL_NULL_HANDLE;

  // free environment handle
  SQLFreeHandle(SQL_HANDLE_ENV, con.henv);
  con.henv = SQL_NULL_HANDLE;

  return 0;
}

static int
get_autocommit(Con& con)
{
  int ret;
  SQLUINTEGER v;
  ret = SQLGetConnectAttr(con.hdbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)&v, SQL_IS_UINTEGER, 0);
  if (ret != SQL_SUCCESS) {
    ndbout << "Get autocommit failed" << endl;
    print_err(SQL_HANDLE_DBC, con.hdbc);
    return -1;
  }
  return v;
}

static int
set_autocommit(Con& con, SQLUINTEGER v)
{
  int ret;
  ret = SQLSetConnectAttr(con.hdbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)v, SQL_IS_UINTEGER);
  if (ret != SQL_SUCCESS) {
    ndbout << "Set autocommit failed" << endl;
    print_err(SQL_HANDLE_DBC, con.hdbc);
    return -1;
  }
  return 0;
}

static int
do_commit(Con& con)
{
  int ret = SQLEndTran(SQL_HANDLE_DBC, con.hdbc, SQL_COMMIT);
  if (ret != SQL_SUCCESS) {
    ndbout << "Commit failed" << endl;
    print_err(SQL_HANDLE_DBC, con.hdbc);
    return -1;
  }
  return 0;
}

static int
do_rollback(Con& con)
{
  int ret = SQLEndTran(SQL_HANDLE_DBC, con.hdbc, SQL_ROLLBACK);
  if (ret != SQL_SUCCESS) {
    ndbout << "Rollback failed" << endl;
    print_err(SQL_HANDLE_DBC, con.hdbc);
    return -1;
  }
  return 0;
}

static int
do_stmt(Con& con, const char *sqlstr)
{
  SQLHSTMT hstmt;
  SQLCHAR errmsg[256];
  SQLCHAR colname[32];
  SQLSMALLINT coltype;
  SQLSMALLINT colnamelen;
  SQLSMALLINT nullable;
  SQLUINTEGER collen[MAXCOLS];
  SQLSMALLINT scale;
  SQLINTEGER outlen[MAXCOLS];
  SQLCHAR *data[MAXCOLS];
  SQLSMALLINT nresultcols = 0;
  SQLINTEGER rowcount;
  SQLINTEGER stmttype;
  SQLRETURN rc;

  /* allocate a statement handle */
  SQLAllocHandle(SQL_HANDLE_STMT, con.hdbc, &hstmt);

  /* execute the SQL statement */
  rc = SQLExecDirect(hstmt, (SQLCHAR*)sqlstr, SQL_NTS);
  if (rc == SQL_ERROR) {
    ndbout << "Operation failed" << endl;
    print_err(SQL_HANDLE_STMT, hstmt);
    return -1;
  }
  if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO && rc != SQL_NO_DATA_FOUND) {
    ndbout << "Operation returned unknown code " << rc << endl;
    return -1;
  }

  /* see what kind of statement it was */
  SQLGetDiagField(SQL_HANDLE_STMT, hstmt, 0,
		  SQL_DIAG_DYNAMIC_FUNCTION_CODE,
		  (SQLPOINTER)&stmttype, SQL_IS_INTEGER, (SQLSMALLINT *)NULL);

  switch (stmttype) {
    /* SELECT statement */
  case SQL_DIAG_SELECT_CURSOR:
    /* determine number of result columns */
    SQLNumResultCols(hstmt, &nresultcols);

    /***********************
     * Display column names 
     ***********************/
    /* Print vertical divider */
    printf("|");
    for (int i=0; i<nresultcols; i++) {
      SQLDescribeCol(hstmt, i+1, colname, sizeof(colname),
		     &colnamelen, &coltype, &collen[i], &scale, &nullable);
      collen[i] = display_length(coltype, collen[i], colname);
      for (SQLUINTEGER j=0; j<collen[i]; j++) printf("-");
      printf("--+");
    }
    printf("\n");

    printf("|");
    for (int i=0; i<nresultcols; i++) {
      SQLDescribeCol(hstmt, i+1, colname, sizeof(colname),
		     &colnamelen, &coltype, &collen[i], &scale, &nullable);
      
      /* assume there is a display_length function which
	 computes correct length given the data type */
      collen[i] = display_length(coltype, collen[i], colname);
      (void)printf(" %*.*s |", (int)collen[i], (int)collen[i], (char *)colname);
      
      /* allocate memory to bind column */
      data[i] = (SQLCHAR *) malloc(collen[i]);
      if (data[i] == NULL) {
	ndbout << "Failed to allocate malloc memory in NDB SQL program" 
	       << endl;
	exit(-1);
      }
      
      /* bind columns to program vars, converting all types to CHAR */
      SQLBindCol(hstmt, i+1, SQL_C_CHAR, data[i], collen[i], &outlen[i]);
    }
    printf("\n");

    /* Print vertical divider */
    printf("|");
    for (int i=0; i<nresultcols; i++) {
      SQLDescribeCol(hstmt, i+1, colname, sizeof(colname),
		     &colnamelen, &coltype, &collen[i], &scale, &nullable);
      collen[i] = display_length(coltype, collen[i], colname);
      for (SQLUINTEGER j=0; j<collen[i]; j++) printf("-");
      printf("--+");
    }
    printf("\n");

    /**********************
     * Display result rows 
     **********************/
    {
      int no_of_rows_fetched=0;
      while (1) {
	rc=SQLFetch(hstmt);
	errmsg[0] = '\0';
	if (rc == SQL_ERROR) {
	  print_err(SQL_HANDLE_STMT, hstmt);
	  break;
	}
	if (rc == SQL_NO_DATA) break;
	if (rc == SQL_SUCCESS) {
	  printf("|");
	  for (int i=0; i<nresultcols; i++) {
	    if (outlen[i] == SQL_NULL_DATA
		|| outlen[i] >= (SQLINTEGER) collen[i])
	      build_indicator_message(errmsg,
				      (SQLPOINTER *)data[i], collen[i],
				      &outlen[i], i);
	    (void)printf(" %*.*s |", (int)collen[i], (int)collen[i],
			 (char *)data[i]);
	  } 
	  /* print any truncation messages */
	  (void)printf("\n%s", (char *)errmsg);
	} else if (rc == SQL_SUCCESS_WITH_INFO) {
	  printf("|");
	  for (int i=0; i<nresultcols; i++) {
	    if (outlen[i] == SQL_NULL_DATA
		|| outlen[i] >= (SQLINTEGER) collen[i])
	      build_indicator_message(errmsg,
				      (SQLPOINTER *)data[i], collen[i],
				      &outlen[i], i);
	    (void)printf(" %*.*s |", (int)collen[i], (int)collen[i],
			 (char *)data[i]);
	  } /* for all columns in this row */
	  /* print any truncation messages */
	  (void)printf("\n%s", (char *)errmsg);
	}
	no_of_rows_fetched++;
      } /* while rows to fetch */
      /* Print vertical divider */
      printf("|");
      for (int i=0; i<nresultcols; i++) {
	SQLDescribeCol(hstmt, i+1, colname, sizeof(colname),
		       &colnamelen, &coltype, &collen[i], &scale, &nullable);
	collen[i] = display_length(coltype, collen[i], colname);
	for (SQLUINTEGER j=0; j<collen[i]; j++) printf("-");
	printf("--+");
      }
      printf("\n");
      ndbout << no_of_rows_fetched << " rows fetched" << endl;
    }
    SQLCloseCursor(hstmt);
    break;
    /* searched DELETE, INSERT or searched UPDATE statement */
  case SQL_DIAG_DELETE_WHERE:
  case SQL_DIAG_INSERT:
  case SQL_DIAG_UPDATE_WHERE:
    /* check rowcount */
    SQLRowCount(hstmt, (SQLINTEGER*)&rowcount);
    ndbout << (int)rowcount << " rows affected" << endl;
    break;
    /* other statements */
  case SQL_DIAG_ALTER_TABLE:
  case SQL_DIAG_CREATE_TABLE:
  case SQL_DIAG_CREATE_VIEW:
  case SQL_DIAG_DROP_TABLE:
  case SQL_DIAG_DROP_VIEW:
  case SQL_DIAG_CREATE_INDEX:
  case SQL_DIAG_DROP_INDEX:
  case SQL_DIAG_DYNAMIC_DELETE_CURSOR:
  case SQL_DIAG_DYNAMIC_UPDATE_CURSOR:
  case SQL_DIAG_GRANT:
  case SQL_DIAG_REVOKE:
    ndbout << "Operation successful" << endl;
    break;
    /* implementation-defined statement */
  default:
    (void)printf("Unknown Statement type=%ld\n", stmttype);
    break;
  }

  /* free data buffers */
  for (int i=0; i<nresultcols; i++) {
    (void)free(data[i]);
  }
  
  SQLFreeHandle(SQL_HANDLE_STMT, hstmt);  // free statement handle 
  return(0);
}

/**************************************************************************
 * ------------------------------------------------------------------------
 *  MODULE:       Help
 * ------------------------------------------------------------------------
 **************************************************************************/

void print_help() {
  ndbout << "Commands:" << endl
	 << "set                Print currect settings" << endl
	 << "set trace N        Set NDB ODBC trace level to N (0-5)" << endl
	 << "set autocommit on  Commit each statement (default)" << endl
	 << "set autocommit off Use explicit commit/rollback - may time out!" << endl
	 << "commit             Commit changes to database" << endl
	 << "rollback           Rollback (undo) any changes" << endl
         << "whenever sqlerror  Define action: exit or continue (default)" << endl
	 << endl
	 << "help               Print this help" << endl
	 << "help create        Print create table examples" << endl
	 << "help insert        Print insert examples" << endl
	 << "help select        Print select examples" << endl
	 << "help delete        Print delete examples" << endl
	 << "help update        Print update examples" << endl
	 << "help virtual       Print help on NDB ODBC virtual tables" << endl
	 << "list tables        Lists all table names" << endl
	 << endl
	 << "All other commands are sent to the NDB ODBC SQL executor" 
	 << endl << endl;
}

void print_help_create() {  
  ndbout << "Create Table Examples" << endl << endl 
	 << "create table t ( a integer not null, b char(20) not null," << endl
	 << "                 c float, primary key(a, b) )" << endl
	 << "create table t ( ndb$tid bigint unsigned primary key," << endl
	 << "                 b char(20) not null, c float )" << endl
	 << "create table t ( a int auto_increment primary key," << endl
	 << "                 b char(20) not null, c float )" << endl
	 << "create table t ( a int primary key," << endl
	 << "                 b int default 100 )" << endl
	 << endl 
	 << "For more information read NDB Cluster ODBC Manual."
	 << endl;
}

void print_help_insert() {  
  ndbout << "Insert Examples" << endl << endl 
	 << "insert into t(a, c) values (123, 'abc')" << endl
	 << "insert into t1(a, c) select a + 10 * b, c from t2" << endl
	 << "insert into t values(null, 'abc', 1.23)" << endl
	 << "insert into t(b, c) values('abc', 1.23)" << endl
	 << endl 
	 << "For more information read NDB Cluster ODBC Manual."
	 << endl;
}

void print_help_select() {  
  ndbout << "Select Examples" << endl << endl 
	 << "select a + b * c from t where a <= b + c and (b > c or c > 10)"
	 << endl
	 << "select a.x, b.y, c.z from t1 a, t2 b, t2 c where a.x + b.y < c.z" 
	 << endl
	 << "select * from t1, t2 where a1 > 5 order by b1 + b2, c1 desc" 
	 << endl
	 << "select count(*), max(a), 1 + sum(b) + avg(c * d) from t" << endl
	 << "select * from t where a < 10 or b > 10" << endl
	 << "select * from t where pk = 5 and b > 10" << endl
	 << "select * from t1, t2, t3 where t1.pk = t2.x and t2.pk = t3.y" 
	 << endl  << endl 
	 << "For more information read NDB Cluster ODBC Manual."
	 << endl;
}

void print_help_update() {  
  ndbout << "Update and Delete Examples" << endl << endl 
	 << "update t set a = b + 5, c = d where c > 10" << endl
	 << "update t set a = b + 5, c = d where pk = 5 and c > 10" << endl
	 << "update t set a = 5, c = 7 where pk = 5" << endl
	 << "delete from t where c > 10" << endl
	 << "delete from t where pk = 5 and c > 10" << endl
	 << "delete from t where pk = 5" << endl 
	 << endl
	 << "For more information read NDB Cluster ODBC Manual."
	 << endl;
}

void print_help_virtual() {  
  ndbout << "Virtual tables" << endl << endl 
	 << "* DUAL" 
	 << "  a 1-row table - example: select SYSDATE from DUAL" << endl
	 << "* ODBC$TYPEINFO" << endl
	 << "  corresponds to SQLGetTypeInfo" << endl
	 << "* ODBC$TABLES" << endl
	 << "  corresponds to SQLTables (ordered by NDB table id)" << endl
	 << "* ODBC$COLUMNS" << endl
	 << "  corresponds to SQLColumns (ordered by NDB table id)" << endl
	 << "* ODBC$PRIMARYKEYS" << endl
	 << "  corresponds to SQLPrimaryKeys (ordered by NDB table id)" << endl
	 << endl
	 << "For more information read NDB Cluster ODBC Manual."
	 << endl;
}

/**************************************************************************
 * ------------------------------------------------------------------------
 *  MODULE:       Main
 * ------------------------------------------------------------------------
 **************************************************************************/

int main(int argc, const char** argv)
{
  ndb_init();
  const char* usage = "Usage: ndbsql [-h] [-d dsn] [-f file] [stmt]\n-h help\n-d <database name or connect string>\n-f <file name> batch mode\nstmt single SQL statement\n";
  const char* dsn = "TEST_DB";
  bool helpFlg = false, batchMode = false;
  const char* fileName = 0;
  FILE* inputFile = stdin;
  const char* singleStmt = 0;

  s_readBuf = (char*)malloc(s_bufSize);
  while (++argv, --argc > 0) {
    const char* arg = argv[0];
    if (arg[0] != '-')
      break;
    if (strcmp(arg, "-d") == 0) {
      if (++argv, --argc > 0) {
        dsn = argv[0];
        continue;
      }
    }
    if (strcmp(arg, "-h") == 0) {
      helpFlg = true;
      continue;
    }
    if (strcmp(arg, "-f") == 0) {
      if (++argv, --argc > 0) {
	fileName = argv[0];
	continue;
      }
    }
    ndbout << usage;
    return 1;
  }
  if (helpFlg) {
    ndbout << usage << "\n";
    print_help();
    return 0;
  }
  if (fileName != 0) {
    if (argc > 0) {
      ndbout << usage;
      return 1;
    }
    if ((inputFile = fopen(fileName, "r")) == 0) {
      ndbout << "Could not read file " << fileName << ": " << strerror(errno) << endl;
      return 1;
    }
    batchMode = true;
  }
  if (argc > 0) {
    singleStmt = argv[0];
    batchMode = true;
  }
  if (! batchMode)
    ndbout << "NDB Cluster NDB SQL -- A simple SQL Command-line Interface\n\n";

  Con con(dsn);
  if (do_connect(con) < 0)
    return 1;
  if (! batchMode)
    ndbout << "Terminate SQL statements with a semi-colon ';'\n";

  char* line = 0;
  char* line2 = 0;
  char* line3 = 0;
  unsigned lineno = 0;
  bool has_semi;
  bool exit_on_error = false;
  int exit_code = 0;
  while (1) {
    free(line);
    line = 0;
    lineno = 0;

more_lines:
    free(line2);
    free(line3);
    line2 = line3 = 0;
    lineno++;
    has_semi = false;
    char prompt[20];
    if (lineno == 1)
      strcpy(prompt, "SQL> ");
    else
      sprintf(prompt, "%4d ", lineno);
    if (singleStmt != 0) {
      line = strdup(singleStmt);
      int n = strlen(line);
      while (n > 0 && isspace(line[n - 1])) {
        line[--n] = 0;
      }
      if (n > 0 && line[n - 1] == ';')
        line[n - 1] = 0;
      has_semi = true;  // regardless
    } else {
      const char *line1 = readline_gets(prompt, batchMode, inputFile); 
      if (line1 != 0) {
        if (line == 0)
          line = strdup(line1);
        else {
          line = (char*)realloc(line, strlen(line) + 1 + strlen(line1) + 1);
          strcat(line, "\n");
          strcat(line, line1);
        }
        if (batchMode)
          ndbout << prompt << line1 << endl;
      } else {
        if (! batchMode)
          ndbout << endl;
        if (line != 0)
          ndbout << "Ignored unterminated SQL statement" << endl;
        break;
      }
    }

    line2 = (char*)malloc(strlen(line) + 1);
    {
      char* p = line2;
      char* q = line;
      bool str = false;
      while (*q != 0) {
        if (*q == '\'') {
          str = !str;
          *p++ = *q++;
        } else if (!str && *q == '-' && *(q + 1) == '-') {
          while (*q != 0 && *q != '\n')
            q++;
        } else
          *p++ = *q++;
      }
      *p = 0;
      int n = strlen(line2);
      while (n > 0 && isspace(line2[n - 1]))
        line2[--n] = 0;
      if (n > 0 && line2[n - 1] == ';') {
        line2[--n] = 0;
        has_semi = true;
      }
    }
    line3 = strdup(line2);
    char* tok[10];
    int ntok = 0;
    tok[ntok] = strtok(line3, " ");
    while (tok[ntok] != 0) {
      ntok++;
      if (ntok == 10)
        break;
      tok[ntok] = strtok(0, " ");
    }
    if (ntok == 0)
      continue;

    if (!strcasecmp(tok[0], "help") || !strcmp(tok[0], "?")) {
      if (ntok != 2)
	print_help();
      else if (!strcasecmp(tok[1], "create"))
	print_help_create();
      else if (!strcasecmp(tok[1], "insert"))
	print_help_insert();
      else if (strcasecmp(tok[1], "select"))
	print_help_select();
      else if (!strcasecmp(tok[1], "delete"))
	print_help_update();
      else if (!strcasecmp(tok[1], "update"))
	print_help_update();
      else if (!strcasecmp(tok[1], "virtual"))
	print_help_virtual();
      else
	print_help();
      continue;
    }

    if (!strcasecmp(tok[0], "list")) {
      if (ntok == 2 && !strcasecmp(tok[1], "tables")) {
	free(line2);
	line2 = strdup("SELECT TABLE_NAME FROM ODBC$TABLES");
        has_semi = true;
      } else {
        ndbout << "Invalid list option - try help" << endl;
        continue;
      }
    }

    if (ntok == 1 && !strcasecmp(tok[0], "quit"))
      break;
    if (ntok == 1 && !strcasecmp(tok[0], "exit"))
      break;
    if (ntok == 1 && !strcasecmp(tok[0], "bye"))
      break;

    if (!strcasecmp(tok[0], "set")) {
      if (ntok == 1) {
	char* p;
	p = getenv("NDB_ODBC_TRACE");
	ndbout << "Trace level is " << (p ? atoi(p) : 0) << endl;
	int ret = get_autocommit(con);
	if (ret != -1)
	  ndbout << "Autocommit is " << (ret == SQL_AUTOCOMMIT_ON ? "on" : "off") << endl;
      } else if (ntok == 3 && !strcasecmp(tok[1], "trace")) {
	static char env[40];
	int n = tok[2] ? atoi(tok[2]) : 0;
	sprintf(env, "NDB_ODBC_TRACE=%d", n);
	putenv(env);
	ndbout << "Trace level set to " << n << endl;
      } else if (ntok == 3 && !strcasecmp(tok[1], "autocommit")) {
	if (tok[2] && !strcasecmp(tok[2], "on")) {
	  int ret = set_autocommit(con, SQL_AUTOCOMMIT_ON);
	  if (ret != -1)
	    ndbout << "Autocommit set to ON" << endl;
	} else if (tok[2] && !strcasecmp(tok[2], "off")) {
	  int ret = set_autocommit(con, SQL_AUTOCOMMIT_OFF);
	  if (ret != -1)
	    ndbout << "Autocommit set to OFF - transaction may time out" << endl;
	} else {
	  ndbout << "Invalid autocommit option - try help" << endl;
	}
      } else {
	ndbout << "Invalid set command - try help" << endl;
      }
      continue;
    }

    if (ntok >= 2 &&
        !strcasecmp(tok[0], "whenever") && !strcasecmp(tok[1], "sqlerror")) {
      if (ntok == 3 && !strcasecmp(tok[2], "exit"))
        exit_on_error = true;
      else if (ntok == 3 && !strcasecmp(tok[2], "continue"))
        exit_on_error = false;
      else {
        ndbout << "Invalid whenever clause - try help" << endl;
      }
      continue;
    }

    if (!strcasecmp(tok[0], "commit")) {
      if (ntok == 1) {
        if (do_commit(con) != -1)
          ndbout << "Commit done" << endl;
        else {
          exit_code = 1;
          if (exit_on_error) {
            ndbout << "Exit on error" << endl;
            break;
          }
        }
      } else {
        ndbout << "Invalid commit command - try help" << endl;
      }
      continue;
    }

    if (!strcasecmp(tok[0], "rollback")) {
      if (ntok == 1) {
        if (do_rollback(con) != -1)
          ndbout << "Rollback done" << endl;
        else {
          exit_code = 1;
          if (exit_on_error) {
            ndbout << "Exit on error" << endl;
            break;
          }
        }
      } else {
        ndbout << "Invalid commit command - try help" << endl;
      }
      continue;
    }

    if (! has_semi)
      goto more_lines;
    if (do_stmt(con, line2) != 0) {
      exit_code = 1;
      if (exit_on_error) {
        ndbout << "Exit on error" << endl;
        break;
      }
    }
    if (singleStmt)
      break;
  }
  do_disconnect(con);
  return exit_code;
}

// vim: set sw=2 et:
