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

/*
 * $Revision: 1.1 $
 * (c) Copyright 1997-2003, TimesTen, Inc.
 * All rights reserved. 
 */

#ifndef TIMESTEN_H_INCLUDED
#define TIMESTEN_H_INCLUDED

#ifdef _WIN32
#include <windows.h>
#endif

#include <sql.h>
#include <sqltypes.h>
#include <sqlext.h>
/*
 * TimesTen extension to application data types; only usable
 * when application directly linked to the TimesTen driver.
 */
#define SQL_C_ADDR      100

#ifndef SQL_C_SBIGINT
#if (ODBCVER < 0x0300)
#define SQL_C_SBIGINT   (SQL_BIGINT+SQL_SIGNED_OFFSET)
#define SQL_C_UBIGINT   (SQL_BIGINT+SQL_UNSIGNED_OFFSET)
#endif
#endif

#define SQL_C_BIGINT    SQL_C_SBIGINT

#if (ODBCVER < 0x0300)
#ifdef _WIN32
typedef __int64 SQLBIGINT;
/* On Unix platforms SQLBIGINT is defined in odbcinclude directory*/
#endif
#endif

#define BIGINT    SQLBIGINT

#ifdef _WIN32
#define UBIGINT   unsigned __int64
#else
#define UBIGINT   unsigned long long
#endif


#define SQL_WCHAR         (-8)
#define SQL_WVARCHAR      (-9)
#define SQL_WLONGVARCHAR  (-10)
#define SQL_C_WCHAR       SQL_WCHAR

/* SQLGetInfo() InfoTypes */
#define SQL_CONVERT_WCHAR         122
#define SQL_CONVERT_WLONGVARCHAR  125
#define SQL_CONVERT_WVARCHAR      126

/* TimesTen specific SQLGetInfo types */
#define TT_REPLICATION_INVALID   (SQL_INFO_DRIVER_START + 2000)

/* SQLGetInfo() return value bitmasks */
#ifndef SQL_CVT_WCHAR
/*
** These definitions differ from Microsoft in that they are not
** specified as long (e.g. 0x00200000L), hence they are protected
** by the ifndef above.
*/
#define SQL_CVT_WCHAR           0x00200000
#define SQL_CVT_WLONGVARCHAR    0x00400000
#define SQL_CVT_WVARCHAR        0x00800000
#endif

/*
** The Microsoft Driver Manager SQLBindParameter() will not pass SQL_WCHAR
** through.  Use this hack to get around it.
*/
#define SQL_WCHAR_DM_SQLBINDPARAMETER_BYPASS    -888

/* This is an extension to ODBC's isolation levels. It reflects an
 * earlier implementation of read-committed that released locks on
 * next fetch, rather than releasing locks before returning value to
 * application.  */
#define SQL_TXN_CURSOR_STABILITY            0x00001000
#define SQL_TXN_NOBLOCK_DELETE              0x00002000

/* TimesTen-specific connection option */
#define TT_PREFETCH_CLOSE                   10001
#define TT_PREFETCH_CLOSE_OFF               0
#define TT_PREFETCH_CLOSE_ON                1

/* Adding a new sql connection option */
#define TT_PREFETCH_COUNT                   10003
#define TT_PREFETCH_COUNT_MAX               128

/*
 * Platform specific data types for integers that scale
 * with pointer size
 */

#ifdef _IA64_
typedef signed   __int64 tt_ptrint;
typedef unsigned __int64 tt_uptrint;
#else
#ifdef _WIN32
typedef signed   long    tt_ptrint;
typedef unsigned long    tt_uptrint;
#else
typedef signed   long    tt_ptrint;
typedef unsigned long    tt_uptrint;
#endif
#endif

#ifdef _WIN32
typedef signed   __int64    tt_int8;
typedef unsigned __int64    tt_uint8;
#else
typedef signed   long long  tt_int8;
typedef unsigned long long  tt_uint8;
#endif

/* printf formats for pointer-sized integers */
#ifdef _IA64_                   /* 64-bit NT */
#define PTRINT_FMT    "I64d"
#define UPTRINT_FMT   "I64u"
#define xPTRINT_FMT   "I64x"
#define XPTRINT_FMT   "I64X"
#else
#ifdef _WIN32                   /* 32-bit NT */
#define PTRINT_FMT    "ld"
#define UPTRINT_FMT   "lu"
#define xPTRINT_FMT   "lx"
#define XPTRINT_FMT   "lX"
#else                           /* 32 and 64-bit UNIX */
#define PTRINT_FMT    "ld"
#define UPTRINT_FMT   "lu"
#define xPTRINT_FMT   "lx"
#define XPTRINT_FMT   "lX"
#endif
#endif

/* printf formats for 8-byte integers */
#ifndef INT8_FMT_DEFINED
#ifdef _WIN32                   /* 32 and 64-bit NT */
#define INT8_FMT      "I64d"
#define UINT8_FMT     "I64u"
#define xINT8_FMT     "I64x"
#define XINT8_FMT     "I64X"
#else                           /* 32 and 64-bit UNIX */
#define INT8_FMT      "lld"
#define UINT8_FMT     "llu"
#define xINT8_FMT     "llx"
#define XINT8_FMT     "llX"
#endif
#define INT8_FMT_DEFINED 1
#endif

/* The following types are defined in the newer odbc include files
   from Microsoft
*/
#if defined (_WIN32) && !defined (_IA64_)
#ifndef SQLROWSETSIZE
#define SQLROWSETSIZE   SQLUINTEGER
#define SQLLEN          SQLINTEGER
#define SQLROWOFFSET    SQLINTEGER
#define SQLROWCOUNT     SQLUINTEGER
#define SQLULEN         SQLUINTEGER
#define SQLSETPOSIROW   SQLUSMALLINT
#endif
#endif


#endif 
