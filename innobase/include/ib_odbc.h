/******************************************************
Innobase ODBC client library header; this is equivalent to
the standard sql.h ODBC header file

(c) 1998 Innobase Oy

Created 2/22/1998 Heikki Tuuri
*******************************************************/

#ifndef ib_odbc_h
#define ib_odbc_h

typedef unsigned char       UCHAR;
typedef signed char         SCHAR;
typedef long int            SDWORD;
typedef short int           SWORD;
typedef unsigned long int   UDWORD;
typedef unsigned short int  UWORD;

typedef void*               PTR;

typedef void*               HENV;
typedef void*               HDBC;
typedef void*               HSTMT;

typedef signed short        RETCODE;

/* RETCODEs */
#define SQL_NO_DATA_FOUND	(-3)
#define SQL_INVALID_HANDLE	(-2)
#define SQL_ERROR		(-1)
#define SQL_SUCCESS 		0

/* Standard SQL datatypes, using ANSI type numbering */
#define SQL_CHAR		1
#define SQL_INTEGER 		4
#define SQL_VARCHAR 		12

/* C datatype to SQL datatype mapping */
#define SQL_C_CHAR	  SQL_CHAR
#define SQL_C_LONG	  SQL_INTEGER

/* Special length value */
#define SQL_NULL_DATA		(-1)

#define SQL_PARAM_INPUT         1
#define SQL_PARAM_OUTPUT	4

/* Null handles */
#define SQL_NULL_HENV		NULL
#define SQL_NULL_HDBC		NULL
#define SQL_NULL_HSTM		NULL


/**************************************************************************
Allocates an SQL environment. */

RETCODE
SQLAllocEnv(
/*========*/
			/* out: SQL_SUCCESS */
	HENV*	phenv);	/* out: pointer to an environment handle */
/**************************************************************************
Allocates an SQL connection. */

RETCODE
SQLAllocConnect(
/*============*/
			/* out: SQL_SUCCESS */
	HENV	henv,	/* in: pointer to an environment handle */
	HDBC*	phdbc);	/* out: pointer to a connection handle */
/**************************************************************************
Allocates an SQL statement. */

RETCODE
SQLAllocStmt(
/*=========*/
	HDBC	hdbc,	/* in: SQL connection */
	HSTMT*	phstmt);	/* out: pointer to a statement handle */
/**************************************************************************
Connects to a database server process (establishes a connection and a
session). */

RETCODE
SQLConnect(
/*=======*/
				/* out: SQL_SUCCESS */
	HDBC	hdbc,		/* in: SQL connection handle */
	UCHAR*	szDSN,		/* in: data source name (server name) */
	SWORD	cbDSN,		/* in: data source name length */
	UCHAR*	szUID,		/* in: user name */
	SWORD	cbUID,		/* in: user name length */
	UCHAR*	szAuthStr,	/* in: password */
	SWORD	cbAuthStr);	/* in: password length */
/**************************************************************************
Makes the server to parse and optimize an SQL string. */

RETCODE
SQLPrepare(
/*=======*/
				/* out: SQL_SUCCESS */
	HSTMT	hstmt,		/* in: statement handle */
	UCHAR*	szSqlStr,	/* in: SQL string */
	SDWORD	cbSqlStr);	/* in: SQL string length */
/**************************************************************************
Binds a parameter in a prepared statement. */

RETCODE
SQLBindParameter(
/*=============*/
				/* out: SQL_SUCCESS */
	HSTMT	hstmt,		/* in: statement handle */
	UWORD	ipar,		/* in: parameter index, starting from 1 */
	SWORD	fParamType,	/* in: SQL_PARAM_INPUT or SQL_PARAM_OUTPUT */
	SWORD	fCType,		/* in: SQL_C_CHAR, ... */
	SWORD	fSqlType,	/* in: SQL_CHAR, ... */
	UDWORD	cbColDef,	/* in: precision: ignored */
	SWORD	ibScale,	/* in: scale: ignored */
	PTR	rgbValue,	/* in: pointer to a buffer for the data */
	SDWORD	cbValueMax,	/* in: buffer size */
	SDWORD*	pcbValue);	/* in: pointer to a buffer for the data
				length or SQL_NULL_DATA */
/**************************************************************************
Executes a prepared statement where all parameters have been bound. */

RETCODE
SQLExecute(
/*=======*/
			/* out: SQL_SUCCESS or SQL_ERROR */
	HSTMT	hstmt);	/* in: statement handle */
/**************************************************************************
Queries an error message. */

RETCODE
SQLError(
/*=====*/
				/* out: SQL_SUCCESS or SQL_NO_DATA_FOUND */
	HENV	henv,		/* in: SQL_NULL_HENV */
	HDBC	hdbc,		/* in: SQL_NULL_HDBC */
	HSTMT	hstmt,		/* in: statement handle */
	UCHAR*	szSqlState,	/* in/out: SQLSTATE as a null-terminated string,
				(currently, always == "S1000") */
	SDWORD*	pfNativeError,	/* out: native error code */
	UCHAR*	szErrorMsg,	/* in/out: buffer for an error message as a
				null-terminated string */
	SWORD	cbErrorMsgMax,	/* in: buffer size for szErrorMsg */
	SWORD*	pcbErrorMsg);	/* out: error message length */

#endif 
