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

#ifndef ODBC_COMMON_common_hpp
#define ODBC_COMMON_common_hpp

#define stpcpy stpcpy
#include <ndb_global.h>
#undef swap

// misc defs

#ifdef NDB_GCC // only for odbc
#define PRINTFLIKE(i,j)	__attribute__ ((format (printf, i, j)))
#else
#define PRINTFLIKE(i,j)
#endif

// odbc defs

#define ODBCVER			0x0351

#ifdef NDB_WIN32
#include <windows.h>
#endif

extern "C" {
#include <sqlext.h>
}
// some types which may be missing
#ifndef SQL_BLOB
#define SQL_BLOB		30
#endif
#ifndef SQL_BLOB_LOCATOR
#define SQL_BLOB_LOCATOR	31
#endif
#ifndef SQL_CLOB
#define SQL_CLOB		40
#endif
#ifndef SQL_CLOB_LOCATOR
#define SQL_CLOB_LOCATOR	41
#endif

// until real blobs use Varchar of this size
#define FAKE_BLOB_SIZE		2000

#define SQL_HANDLE_ROOT	0	// assume real handles != 0

enum OdbcHandle {
    Odbc_handle_root = 0,	// not an odbc handle
    Odbc_handle_env = 1,
    Odbc_handle_dbc = 2,
    Odbc_handle_stmt = 4,
    Odbc_handle_desc = 8,
    Odbc_handle_all = (1|2|4|8)
};

// ndb defs

#undef BOOL
#include <ndb_types.h>
// this info not yet on api side
#include <kernel/ndb_limits.h>
#include <ndb_version.h>

#ifndef MAX_TAB_NAME_SIZE
#define MAX_TAB_NAME_SIZE	128
#endif

#ifndef MAX_ATTR_NAME_SIZE
#define MAX_ATTR_NAME_SIZE	32
#endif

#ifndef MAX_ATTR_DEFAULT_VALUE_SIZE
#define MAX_ATTR_DEFAULT_VALUE_SIZE	128
#endif

typedef Uint32 NdbAttrId;
typedef Uint64 CountType;

// ndb odbc defs

#define NDB_ODBC_COMPONENT_VENDOR	"[MySQL]"
#define NDB_ODBC_COMPONENT_DRIVER	"[ODBC driver]"
#define NDB_ODBC_COMPONENT_DATABASE	"[NDB Cluster]"

#define NDB_ODBC_VERSION_MAJOR		0
#define NDB_ODBC_VERSION_MINOR		22
#define NDB_ODBC_VERSION_STRING		"0.22"

// reserved error codes for non-NDB errors
#define NDB_ODBC_ERROR_MIN		5000
#define NDB_ODBC_ERROR_MAX		5100

// maximum log level compiled in
#ifdef VM_TRACE
#define NDB_ODBC_MAX_LOG_LEVEL		5
#else
#define NDB_ODBC_MAX_LOG_LEVEL		3
#endif

// driver specific statement attribute for number of NDB tuples fetched
#define SQL_ATTR_NDB_TUPLES_FETCHED	66601

#include <BaseString.hpp>
#include <common/Sqlstate.hpp>
#include <common/Ctx.hpp>

#undef assert

#endif
