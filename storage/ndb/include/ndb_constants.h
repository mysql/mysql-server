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

/**
 * @file ndb_constants.h
 *
 * Constants common to NDB API and NDB kernel.
 * Changing the values makes database upgrade impossible.
 *
 * New or removed definitions must be replicated to
 * NdbDictionary.hpp and NdbSqlUtil.hpp.
 *
 * Not for use by application programs.
 * Use the enums provided by NdbDictionary instead.
 */

#ifndef NDB_CONSTANTS_H
#define NDB_CONSTANTS_H

/*
 * Data type constants.
 */

#define NDB_TYPE_UNDEFINED              0

#define NDB_TYPE_TINYINT                1
#define NDB_TYPE_TINYUNSIGNED           2
#define NDB_TYPE_SMALLINT               3
#define NDB_TYPE_SMALLUNSIGNED          4
#define NDB_TYPE_MEDIUMINT              5
#define NDB_TYPE_MEDIUMUNSIGNED         6
#define NDB_TYPE_INT                    7
#define NDB_TYPE_UNSIGNED               8
#define NDB_TYPE_BIGINT                 9
#define NDB_TYPE_BIGUNSIGNED            10
#define NDB_TYPE_FLOAT                  11
#define NDB_TYPE_DOUBLE                 12
#define NDB_TYPE_OLDDECIMAL             13
#define NDB_TYPE_CHAR                   14
#define NDB_TYPE_VARCHAR                15
#define NDB_TYPE_BINARY                 16
#define NDB_TYPE_VARBINARY              17
#define NDB_TYPE_DATETIME               18
#define NDB_TYPE_DATE                   19
#define NDB_TYPE_BLOB                   20
#define NDB_TYPE_TEXT                   21
#define NDB_TYPE_BIT                    22
#define NDB_TYPE_LONGVARCHAR            23
#define NDB_TYPE_LONGVARBINARY          24
#define NDB_TYPE_TIME                   25
#define NDB_TYPE_YEAR                   26
#define NDB_TYPE_TIMESTAMP              27
#define NDB_TYPE_OLDDECIMALUNSIGNED     28
#define NDB_TYPE_DECIMAL                29
#define NDB_TYPE_DECIMALUNSIGNED        30

#define NDB_TYPE_MAX                    31


/*
 * Attribute array type.
 */

#define NDB_ARRAYTYPE_FIXED             0 /* 0 length bytes */
#define NDB_ARRAYTYPE_SHORT_VAR         1 /* 1 length bytes */
#define NDB_ARRAYTYPE_MEDIUM_VAR        2 /* 2 length bytes */

/*
 * Attribute storage type.
 */

#define NDB_STORAGETYPE_MEMORY          0
#define NDB_STORAGETYPE_DISK            1

/*
 * Table temporary status.
 */
#define NDB_TEMP_TAB_PERMANENT          0
#define NDB_TEMP_TAB_TEMPORARY          1

#endif
