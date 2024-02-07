/*
   Copyright (c) 2004, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

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

#define NDB_TYPE_UNDEFINED 0

#define NDB_TYPE_TINYINT 1
#define NDB_TYPE_TINYUNSIGNED 2
#define NDB_TYPE_SMALLINT 3
#define NDB_TYPE_SMALLUNSIGNED 4
#define NDB_TYPE_MEDIUMINT 5
#define NDB_TYPE_MEDIUMUNSIGNED 6
#define NDB_TYPE_INT 7
#define NDB_TYPE_UNSIGNED 8
#define NDB_TYPE_BIGINT 9
#define NDB_TYPE_BIGUNSIGNED 10
#define NDB_TYPE_FLOAT 11
#define NDB_TYPE_DOUBLE 12
#define NDB_TYPE_OLDDECIMAL 13
#define NDB_TYPE_CHAR 14
#define NDB_TYPE_VARCHAR 15
#define NDB_TYPE_BINARY 16
#define NDB_TYPE_VARBINARY 17
#define NDB_TYPE_DATETIME 18
#define NDB_TYPE_DATE 19
#define NDB_TYPE_BLOB 20
#define NDB_TYPE_TEXT 21
#define NDB_TYPE_BIT 22
#define NDB_TYPE_LONGVARCHAR 23
#define NDB_TYPE_LONGVARBINARY 24
#define NDB_TYPE_TIME 25
#define NDB_TYPE_YEAR 26
#define NDB_TYPE_TIMESTAMP 27
#define NDB_TYPE_OLDDECIMALUNSIGNED 28
#define NDB_TYPE_DECIMAL 29
#define NDB_TYPE_DECIMALUNSIGNED 30
#define NDB_TYPE_TIME2 31
#define NDB_TYPE_DATETIME2 32
#define NDB_TYPE_TIMESTAMP2 33

#define NDB_TYPE_MAX 34

/*
 * Attribute array type.
 */

#define NDB_ARRAYTYPE_FIXED 0      /* 0 length bytes */
#define NDB_ARRAYTYPE_SHORT_VAR 1  /* 1 length bytes */
#define NDB_ARRAYTYPE_MEDIUM_VAR 2 /* 2 length bytes */
#define NDB_ARRAYTYPE_NONE_VAR 3   /* 0 length bytes */

/*
 * Attribute storage type.
 */

#define NDB_STORAGETYPE_MEMORY 0
#define NDB_STORAGETYPE_DISK 1
#define NDB_STORAGETYPE_DEFAULT 2 /* not set */

/*
 * Table temporary status.
 */
#define NDB_TEMP_TAB_PERMANENT 0
#define NDB_TEMP_TAB_TEMPORARY 1

/*
 * Table single user mode
 */
#define NDB_SUM_LOCKED 0
#define NDB_SUM_READONLY 1
#define NDB_SUM_READ_WRITE 2

/**
 * *No* nodegroup
 */
#define NDB_NO_NODEGROUP 65536

/*
 * SYSTAB_0 reserved keys
 */
#define NDB_BACKUP_SEQUENCE 0x1F000000

/**
 * Defines for index statistics
 */
#define NDB_INDEX_STAT_DB "mysql"
#define NDB_INDEX_STAT_SCHEMA "def"

#define NDB_INDEX_STAT_HEAD_TABLE "ndb_index_stat_head"
#define NDB_INDEX_STAT_SAMPLE_TABLE "ndb_index_stat_sample"
#define NDB_INDEX_STAT_SAMPLE_INDEX1 "ndb_index_stat_sample_x1"
#define NDB_INDEX_STAT_HEAD_EVENT "ndb_index_stat_head_event"

#define NDB_INDEX_STAT_PREFIX "ndb_index_stat"

/**
 * Defines for NDB$INFO.OPERATIONS
 */
#define NDB_INFO_OP_UNKNOWN 0
#define NDB_INFO_OP_READ 1
#define NDB_INFO_OP_READ_SH 2
#define NDB_INFO_OP_READ_EX 3
#define NDB_INFO_OP_INSERT 4
#define NDB_INFO_OP_UPDATE 5
#define NDB_INFO_OP_DELETE 6
#define NDB_INFO_OP_WRITE 7
#define NDB_INFO_OP_UNLOCK 8
#define NDB_INFO_OP_REFRESH 9
#define NDB_INFO_OP_SCAN_UNKNOWN (256 + 0)
#define NDB_INFO_OP_SCAN (256 + 1)
#define NDB_INFO_OP_SCAN_SH (256 + 2)
#define NDB_INFO_OP_SCAN_EX (256 + 3)

/**
 * FK actions
 */
#define NDB_FK_NO_ACTION 0
#define NDB_FK_RESTRICT 1
#define NDB_FK_CASCADE 2
#define NDB_FK_SET_NULL 3
#define NDB_FK_SET_DEFAULT 4

/**
 * Defines for FragmentCount specifier
 */
#define NDB_PARTITION_BALANCE_SPECIFIC ~Uint32(0)
#define NDB_PARTITION_BALANCE_FOR_RP_BY_LDM ~Uint32(1)
#define NDB_PARTITION_BALANCE_FOR_RA_BY_LDM ~Uint32(2)
#define NDB_PARTITION_BALANCE_FOR_RP_BY_NODE ~Uint32(3)
#define NDB_PARTITION_BALANCE_FOR_RA_BY_NODE ~Uint32(4)
#define NDB_PARTITION_BALANCE_FOR_RA_BY_LDM_X_2 ~Uint32(5)
#define NDB_PARTITION_BALANCE_FOR_RA_BY_LDM_X_3 ~Uint32(6)
#define NDB_PARTITION_BALANCE_FOR_RA_BY_LDM_X_4 ~Uint32(7)

#define NDB_DEFAULT_PARTITION_BALANCE NDB_PARTITION_BALANCE_FOR_RP_BY_LDM

#endif
