/* Copyright (C) 2000 MySQL AB

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

/* This file includes constants used with all databases */

#ifndef _my_base_h
#define _my_base_h

#ifndef stdin				/* Included first in handler */
#define CHSIZE_USED
#include <my_global.h>
#include <my_dir.h>			/* This includes types */
#include <my_sys.h>
#include <m_string.h>
#include <errno.h>

#ifndef EOVERFLOW
#define EOVERFLOW 84
#endif

#if !defined(USE_MY_FUNC) && !defined(THREAD)
#include <my_nosys.h>			/* For faster code, after test */
#endif	/* USE_MY_FUNC */
#endif	/* stdin */
#include <my_list.h>

/* The following is bits in the flag parameter to ha_open() */

#define HA_OPEN_ABORT_IF_LOCKED		0	/* default */
#define HA_OPEN_WAIT_IF_LOCKED		1
#define HA_OPEN_IGNORE_IF_LOCKED	2
#define HA_OPEN_TMP_TABLE		4	/* Table is a temp table */
#define HA_OPEN_DELAY_KEY_WRITE		8	/* Don't update index  */
#define HA_OPEN_ABORT_IF_CRASHED	16
#define HA_OPEN_FOR_REPAIR		32	/* open even if crashed */
#define HA_OPEN_FROM_SQL_LAYER          64
#define HA_OPEN_MMAP                    128     /* open memory mapped */
#define HA_OPEN_COPY			256     /* Open copy (for repair) */
/* Internal temp table, used for temporary results */
#define HA_OPEN_INTERNAL_TABLE          512

/* The following is parameter to ha_rkey() how to use key */

/*
  We define a complete-field prefix of a key value as a prefix where
  the last included field in the prefix contains the full field, not
  just some bytes from the start of the field. A partial-field prefix
  is allowed to contain only a few first bytes from the last included
  field.

  Below HA_READ_KEY_EXACT, ..., HA_READ_BEFORE_KEY can take a
  complete-field prefix of a key value as the search
  key. HA_READ_PREFIX and HA_READ_PREFIX_LAST could also take a
  partial-field prefix, but currently (4.0.10) they are only used with
  complete-field prefixes. MySQL uses a padding trick to implement
  LIKE 'abc%' queries.

  NOTE that in InnoDB HA_READ_PREFIX_LAST will NOT work with a
  partial-field prefix because InnoDB currently strips spaces from the
  end of varchar fields!
*/

enum ha_rkey_function {
  HA_READ_KEY_EXACT,              /* Find first record else error */
  HA_READ_KEY_OR_NEXT,            /* Record or next record */
  HA_READ_KEY_OR_PREV,            /* Record or previous */
  HA_READ_AFTER_KEY,              /* Find next rec. after key-record */
  HA_READ_BEFORE_KEY,             /* Find next rec. before key-record */
  HA_READ_PREFIX,                 /* Key which as same prefix */
  HA_READ_PREFIX_LAST,            /* Last key with the same prefix */
  HA_READ_PREFIX_LAST_OR_PREV,    /* Last or prev key with the same prefix */
  HA_READ_MBR_CONTAIN,
  HA_READ_MBR_INTERSECT,
  HA_READ_MBR_WITHIN,
  HA_READ_MBR_DISJOINT,
  HA_READ_MBR_EQUAL
};

	/* Key algorithm types */

enum ha_key_alg {
  HA_KEY_ALG_UNDEF=	0,		/* Not specified (old file) */
  HA_KEY_ALG_BTREE=	1,		/* B-tree, default one          */
  HA_KEY_ALG_RTREE=	2,		/* R-tree, for spatial searches */
  HA_KEY_ALG_HASH=	3,		/* HASH keys (HEAP tables) */
  HA_KEY_ALG_FULLTEXT=	4		/* FULLTEXT (MyISAM tables) */
};

        /* Storage media types */ 

enum ha_storage_media {
  HA_SM_DEFAULT=        0,		/* Not specified (engine default) */
  HA_SM_DISK=           1,		/* DISK storage */
  HA_SM_MEMORY=         2		/* MAIN MEMORY storage */
};

	/* The following is parameter to ha_extra() */

enum ha_extra_function {
  HA_EXTRA_NORMAL=0,			/* Optimize for space (def) */
  HA_EXTRA_QUICK=1,			/* Optimize for speed */
  HA_EXTRA_NOT_USED=2,
  HA_EXTRA_CACHE=3,			/* Cache record in HA_rrnd() */
  HA_EXTRA_NO_CACHE=4,			/* End caching of records (def) */
  HA_EXTRA_NO_READCHECK=5,		/* No readcheck on update */
  HA_EXTRA_READCHECK=6,			/* Use readcheck (def) */
  HA_EXTRA_KEYREAD=7,			/* Read only key to database */
  HA_EXTRA_NO_KEYREAD=8,		/* Normal read of records (def) */
  HA_EXTRA_NO_USER_CHANGE=9,		/* No user is allowed to write */
  HA_EXTRA_KEY_CACHE=10,
  HA_EXTRA_NO_KEY_CACHE=11,
  HA_EXTRA_WAIT_LOCK=12,		/* Wait until file is avalably (def) */
  HA_EXTRA_NO_WAIT_LOCK=13,		/* If file is locked, return quickly */
  HA_EXTRA_WRITE_CACHE=14,		/* Use write cache in ha_write() */
  HA_EXTRA_FLUSH_CACHE=15,		/* flush write_record_cache */
  HA_EXTRA_NO_KEYS=16,			/* Remove all update of keys */
  HA_EXTRA_KEYREAD_CHANGE_POS=17,	/* Keyread, but change pos */
					/* xxxxchk -r must be used */
  HA_EXTRA_REMEMBER_POS=18,		/* Remember pos for next/prev */
  HA_EXTRA_RESTORE_POS=19,
  HA_EXTRA_REINIT_CACHE=20,		/* init cache from current record */
  HA_EXTRA_FORCE_REOPEN=21,		/* Datafile have changed on disk */
  HA_EXTRA_FLUSH,			/* Flush tables to disk */
  HA_EXTRA_NO_ROWS,			/* Don't write rows */
  HA_EXTRA_RESET_STATE,			/* Reset positions */
  HA_EXTRA_IGNORE_DUP_KEY,		/* Dup keys don't rollback everything*/
  HA_EXTRA_NO_IGNORE_DUP_KEY,
  HA_EXTRA_PREPARE_FOR_DROP,
  HA_EXTRA_PREPARE_FOR_UPDATE,		/* Remove read cache if problems */
  HA_EXTRA_PRELOAD_BUFFER_SIZE,         /* Set buffer size for preloading */
  /*
    On-the-fly switching between unique and non-unique key inserting.
  */
  HA_EXTRA_CHANGE_KEY_TO_UNIQUE,
  HA_EXTRA_CHANGE_KEY_TO_DUP,
  /*
    When using HA_EXTRA_KEYREAD, overwrite only key member fields and keep 
    other fields intact. When this is off (by default) InnoDB will use memcpy
    to overwrite entire row.
  */
  HA_EXTRA_KEYREAD_PRESERVE_FIELDS,
  HA_EXTRA_MMAP,
  /*
    Ignore if the a tuple is not found, continue processing the
    transaction and ignore that 'row'.  Needed for idempotency
    handling on the slave

    Currently only used by NDB storage engine. Partition handler ignores flag.
  */
  HA_EXTRA_IGNORE_NO_KEY,
  HA_EXTRA_NO_IGNORE_NO_KEY,
  /*
    Mark the table as a log table. For some handlers (e.g. CSV) this results
    in a special locking for the table.
  */
  HA_EXTRA_MARK_AS_LOG_TABLE,
  /*
    Informs handler that write_row() which tries to insert new row into the
    table and encounters some already existing row with same primary/unique
    key can replace old row with new row instead of reporting error (basically
    it informs handler that we do REPLACE instead of simple INSERT).
    Off by default.
  */
  HA_EXTRA_WRITE_CAN_REPLACE,
  HA_EXTRA_WRITE_CANNOT_REPLACE,
  /*
    Inform handler that delete_row()/update_row() cannot batch deletes/updates
    and should perform them immediately. This may be needed when table has 
    AFTER DELETE/UPDATE triggers which access to subject table.
    These flags are reset by the handler::extra(HA_EXTRA_RESET) call.
  */
  HA_EXTRA_DELETE_CANNOT_BATCH,
  HA_EXTRA_UPDATE_CANNOT_BATCH,
  /*
    Inform handler that an "INSERT...ON DUPLICATE KEY UPDATE" will be
    executed. This condition is unset by HA_EXTRA_NO_IGNORE_DUP_KEY.
  */
  HA_EXTRA_INSERT_WITH_UPDATE,
  /* Inform handler that we will do a rename */
  HA_EXTRA_PREPARE_FOR_RENAME,
  /*
    Orders MERGE handler to attach or detach its child tables. Used at
    begin and end of a statement.
  */
  HA_EXTRA_ATTACH_CHILDREN,
  HA_EXTRA_DETACH_CHILDREN
};

/* Compatible option, to be deleted in 6.0 */
#define HA_EXTRA_PREPARE_FOR_DELETE HA_EXTRA_PREPARE_FOR_DROP

	/* The following is parameter to ha_panic() */

enum ha_panic_function {
  HA_PANIC_CLOSE,			/* Close all databases */
  HA_PANIC_WRITE,			/* Unlock and write status */
  HA_PANIC_READ				/* Lock and read keyinfo */
};

	/* The following is parameter to ha_create(); keytypes */

enum ha_base_keytype {
  HA_KEYTYPE_END=0,
  HA_KEYTYPE_TEXT=1,			/* Key is sorted as letters */
  HA_KEYTYPE_BINARY=2,			/* Key is sorted as unsigned chars */
  HA_KEYTYPE_SHORT_INT=3,
  HA_KEYTYPE_LONG_INT=4,
  HA_KEYTYPE_FLOAT=5,
  HA_KEYTYPE_DOUBLE=6,
  HA_KEYTYPE_NUM=7,			/* Not packed num with pre-space */
  HA_KEYTYPE_USHORT_INT=8,
  HA_KEYTYPE_ULONG_INT=9,
  HA_KEYTYPE_LONGLONG=10,
  HA_KEYTYPE_ULONGLONG=11,
  HA_KEYTYPE_INT24=12,
  HA_KEYTYPE_UINT24=13,
  HA_KEYTYPE_INT8=14,
  /* Varchar (0-255 bytes) with length packed with 1 byte */
  HA_KEYTYPE_VARTEXT1=15,               /* Key is sorted as letters */
  HA_KEYTYPE_VARBINARY1=16,             /* Key is sorted as unsigned chars */
  /* Varchar (0-65535 bytes) with length packed with 2 bytes */
  HA_KEYTYPE_VARTEXT2=17,		/* Key is sorted as letters */
  HA_KEYTYPE_VARBINARY2=18,		/* Key is sorted as unsigned chars */
  HA_KEYTYPE_BIT=19
};

#define HA_MAX_KEYTYPE	31		/* Must be log2-1 */

	/* These flags kan be OR:ed to key-flag */

#define HA_NOSAME		 1	/* Set if not dupplicated records */
#define HA_PACK_KEY		 2	/* Pack string key to previous key */
#define HA_AUTO_KEY		 16
#define HA_BINARY_PACK_KEY	 32	/* Packing of all keys to prev key */
#define HA_FULLTEXT		128     /* For full-text search */
#define HA_UNIQUE_CHECK		256	/* Check the key for uniqueness */
#define HA_SPATIAL		1024    /* For spatial search */
#define HA_NULL_ARE_EQUAL	2048	/* NULL in key are cmp as equal */
#define HA_GENERATED_KEY	8192	/* Automaticly generated key */

        /* The combination of the above can be used for key type comparison. */
#define HA_KEYFLAG_MASK (HA_NOSAME | HA_PACK_KEY | HA_AUTO_KEY | \
                         HA_BINARY_PACK_KEY | HA_FULLTEXT | HA_UNIQUE_CHECK | \
                         HA_SPATIAL | HA_NULL_ARE_EQUAL | HA_GENERATED_KEY)

#define HA_KEY_HAS_PART_KEY_SEG 65536   /* Key contains partial segments */

	/* Automatic bits in key-flag */

#define HA_SPACE_PACK_USED	 4	/* Test for if SPACE_PACK used */
#define HA_VAR_LENGTH_KEY	 8
#define HA_NULL_PART_KEY	 64
#define HA_USES_PARSER           16384  /* Fulltext index uses [pre]parser */
#define HA_USES_BLOCK_SIZE	 ((uint) 32768)
#define HA_SORT_ALLOWS_SAME      512    /* Intern bit when sorting records */
#if MYSQL_VERSION_ID < 0x50200
/*
  Key has a part that can have end space.  If this is an unique key
  we have to handle it differently from other unique keys as we can find
  many matching rows for one key (because end space are not compared)
*/
#define HA_END_SPACE_KEY      0 /* was: 4096 */
#else
#error HA_END_SPACE_KEY is obsolete, please remove it
#endif


	/* These flags can be added to key-seg-flag */

#define HA_SPACE_PACK		 1	/* Pack space in key-seg */
#define HA_PART_KEY_SEG		 4	/* Used by MySQL for part-key-cols */
#define HA_VAR_LENGTH_PART	 8
#define HA_NULL_PART		 16
#define HA_BLOB_PART		 32
#define HA_SWAP_KEY		 64
#define HA_REVERSE_SORT		 128	/* Sort key in reverse order */
#define HA_NO_SORT               256 /* do not bother sorting on this keyseg */
/*
  End space in unique/varchar are considered equal. (Like 'a' and 'a ')
  Only needed for internal temporary tables.
*/
#define HA_END_SPACE_ARE_EQUAL	 512
#define HA_BIT_PART		1024

	/* optionbits for database */
#define HA_OPTION_PACK_RECORD		1
#define HA_OPTION_PACK_KEYS		2
#define HA_OPTION_COMPRESS_RECORD	4
#define HA_OPTION_LONG_BLOB_PTR		8 /* new ISAM format */
#define HA_OPTION_TMP_TABLE		16
#define HA_OPTION_CHECKSUM		32
#define HA_OPTION_DELAY_KEY_WRITE	64
#define HA_OPTION_NO_PACK_KEYS		128  /* Reserved for MySQL */
#define HA_OPTION_CREATE_FROM_ENGINE    256
#define HA_OPTION_RELIES_ON_SQL_LAYER   512
#define HA_OPTION_NULL_FIELDS		1024
#define HA_OPTION_PAGE_CHECKSUM		2048
#define HA_OPTION_TEMP_COMPRESS_RECORD	((uint) 16384)	/* set by isamchk */
#define HA_OPTION_READ_ONLY_DATA	((uint) 32768)	/* Set by isamchk */

	/* Bits in flag to create() */

#define HA_DONT_TOUCH_DATA	1	/* Don't empty datafile (isamchk) */
#define HA_PACK_RECORD		2	/* Request packed record format */
#define HA_CREATE_TMP_TABLE	4
#define HA_CREATE_CHECKSUM	8
#define HA_CREATE_KEEP_FILES	16      /* don't overwrite .MYD and MYI */
#define HA_CREATE_PAGE_CHECKSUM	32
#define HA_CREATE_DELAY_KEY_WRITE 64
#define HA_CREATE_RELIES_ON_SQL_LAYER 128

/*
  The following flags (OR-ed) are passed to handler::info() method.
  The method copies misc handler information out of the storage engine
  to data structures accessible from MySQL

  Same flags are also passed down to mi_status, myrg_status, etc.
*/

/* this one is not used */
#define HA_STATUS_POS            1
/*
  assuming the table keeps shared actual copy of the 'info' and
  local, possibly outdated copy, the following flag means that
  it should not try to get the actual data (locking the shared structure)
  slightly outdated version will suffice
*/
#define HA_STATUS_NO_LOCK        2
/* update the time of the last modification (in handler::update_time) */
#define HA_STATUS_TIME           4
/*
  update the 'constant' part of the info:
  handler::max_data_file_length, max_index_file_length, create_time
  sortkey, ref_length, block_size, data_file_name, index_file_name.
  handler::table->s->keys_in_use, keys_for_keyread, rec_per_key
*/
#define HA_STATUS_CONST          8
/*
  update the 'variable' part of the info:
  handler::records, deleted, data_file_length, index_file_length,
  delete_length, check_time, mean_rec_length
*/
#define HA_STATUS_VARIABLE      16
/*
  get the information about the key that caused last duplicate value error
  update handler::errkey and handler::dupp_ref
  see handler::get_dup_key()
*/
#define HA_STATUS_ERRKEY        32
/*
  update handler::auto_increment_value
*/
#define HA_STATUS_AUTO          64

/*
  Errorcodes given by handler functions

  opt_sum_query() assumes these codes are > 1
  Do not add error numbers before HA_ERR_FIRST.
  If necessary to add lower numbers, change HA_ERR_FIRST accordingly.
*/
#define HA_ERR_FIRST            120     /* Copy of first error nr.*/

#define HA_ERR_KEY_NOT_FOUND	120	/* Didn't find key on read or update */
#define HA_ERR_FOUND_DUPP_KEY	121	/* Dupplicate key on write */
#define HA_ERR_INTERNAL_ERROR   122     /* Internal error */
#define HA_ERR_RECORD_CHANGED	123	/* Uppdate with is recoverable */
#define HA_ERR_WRONG_INDEX	124	/* Wrong index given to function */
#define HA_ERR_CRASHED		126	/* Indexfile is crashed */
#define HA_ERR_WRONG_IN_RECORD	127	/* Record-file is crashed */
#define HA_ERR_OUT_OF_MEM	128	/* Record-file is crashed */
#define HA_ERR_NOT_A_TABLE      130     /* not a MYI file - no signature */
#define HA_ERR_WRONG_COMMAND	131	/* Command not supported */
#define HA_ERR_OLD_FILE		132	/* old databasfile */
#define HA_ERR_NO_ACTIVE_RECORD 133	/* No record read in update() */
#define HA_ERR_RECORD_DELETED	134	/* A record is not there */
#define HA_ERR_RECORD_FILE_FULL 135	/* No more room in file */
#define HA_ERR_INDEX_FILE_FULL	136	/* No more room in file */
#define HA_ERR_END_OF_FILE	137	/* end in next/prev/first/last */
#define HA_ERR_UNSUPPORTED	138	/* unsupported extension used */
#define HA_ERR_TO_BIG_ROW	139	/* Too big row */
#define HA_WRONG_CREATE_OPTION	140	/* Wrong create option */
#define HA_ERR_FOUND_DUPP_UNIQUE 141	/* Dupplicate unique on write */
#define HA_ERR_UNKNOWN_CHARSET	 142	/* Can't open charset */
#define HA_ERR_WRONG_MRG_TABLE_DEF 143  /* conflicting tables in MERGE */
#define HA_ERR_CRASHED_ON_REPAIR 144	/* Last (automatic?) repair failed */
#define HA_ERR_CRASHED_ON_USAGE  145	/* Table must be repaired */
#define HA_ERR_LOCK_WAIT_TIMEOUT 146
#define HA_ERR_LOCK_TABLE_FULL   147
#define HA_ERR_READ_ONLY_TRANSACTION 148 /* Updates not allowed */
#define HA_ERR_LOCK_DEADLOCK	 149
#define HA_ERR_CANNOT_ADD_FOREIGN 150    /* Cannot add a foreign key constr. */
#define HA_ERR_NO_REFERENCED_ROW 151     /* Cannot add a child row */
#define HA_ERR_ROW_IS_REFERENCED 152     /* Cannot delete a parent row */
#define HA_ERR_NO_SAVEPOINT	 153     /* No savepoint with that name */
#define HA_ERR_NON_UNIQUE_BLOCK_SIZE 154 /* Non unique key block size */
#define HA_ERR_NO_SUCH_TABLE     155  /* The table does not exist in engine */
#define HA_ERR_TABLE_EXIST       156  /* The table existed in storage engine */
#define HA_ERR_NO_CONNECTION     157  /* Could not connect to storage engine */
/* NULLs are not supported in spatial index */
#define HA_ERR_NULL_IN_SPATIAL   158
#define HA_ERR_TABLE_DEF_CHANGED 159  /* The table changed in storage engine */
/* There's no partition in table for given value */
#define HA_ERR_NO_PARTITION_FOUND 160
#define HA_ERR_RBR_LOGGING_FAILED 161  /* Row-based binlogging of row failed */
#define HA_ERR_DROP_INDEX_FK      162  /* Index needed in foreign key constr */
/*
  Upholding foreign key constraints would lead to a duplicate key error
  in some other table.
*/
#define HA_ERR_FOREIGN_DUPLICATE_KEY 163
/* The table changed in storage engine */
#define HA_ERR_TABLE_NEEDS_UPGRADE 164
#define HA_ERR_TABLE_READONLY      165   /* The table is not writable */

#define HA_ERR_AUTOINC_READ_FAILED 166   /* Failed to get next autoinc value */
#define HA_ERR_AUTOINC_ERANGE    167     /* Failed to set row autoinc value */
#define HA_ERR_GENERIC           168     /* Generic error */
/* row not actually updated: new values same as the old values */
#define HA_ERR_RECORD_IS_THE_SAME 169
/* It is not possible to log this statement */
#define HA_ERR_LOGGING_IMPOSSIBLE 170    /* It is not possible to log this
                                            statement */
#define HA_ERR_CORRUPT_EVENT      171    /* The event was corrupt, leading to
                                            illegal data being read */
#define HA_ERR_NEW_FILE	          172	 /* New file format */
#define HA_ERR_ROWS_EVENT_APPLY   173    /* The event could not be processed
                                            no other hanlder error happened */
#define HA_ERR_INITIALIZATION     174    /* Error during initialization */
#define HA_ERR_FILE_TOO_SHORT	  175	 /* File too short */
#define HA_ERR_WRONG_CRC	  176	 /* Wrong CRC on page */
#define HA_ERR_TOO_MANY_CONCURRENT_TRXS 177 /*Too many active concurrent transactions */
#define HA_ERR_LAST               177    /* Copy of last error nr */

/* Number of different errors */
#define HA_ERR_ERRORS            (HA_ERR_LAST - HA_ERR_FIRST + 1)

	/* Other constants */

#define HA_NAMELEN 64			/* Max length of saved filename */
#define NO_SUCH_KEY (~(uint)0)          /* used as a key no. */

typedef ulong key_part_map;
#define HA_WHOLE_KEY  (~(key_part_map)0)

	/* Intern constants in databases */

	/* bits in _search */
#define SEARCH_FIND	1
#define SEARCH_NO_FIND	2
#define SEARCH_SAME	4
#define SEARCH_BIGGER	8
#define SEARCH_SMALLER	16
#define SEARCH_SAVE_BUFF	32
#define SEARCH_UPDATE	64
#define SEARCH_PREFIX	128
#define SEARCH_LAST	256
#define MBR_CONTAIN     512
#define MBR_INTERSECT   1024
#define MBR_WITHIN      2048
#define MBR_DISJOINT    4096
#define MBR_EQUAL       8192
#define MBR_DATA        16384
#define SEARCH_NULL_ARE_EQUAL 32768	/* NULL in keys are equal */
#define SEARCH_NULL_ARE_NOT_EQUAL 65536	/* NULL in keys are not equal */

	/* bits in opt_flag */
#define QUICK_USED	1
#define READ_CACHE_USED	2
#define READ_CHECK_USED 4
#define KEY_READ_USED	8
#define WRITE_CACHE_USED 16
#define OPT_NO_ROWS	32

	/* bits in update */
#define HA_STATE_CHANGED	1	/* Database has changed */
#define HA_STATE_AKTIV		2	/* Has a current record */
#define HA_STATE_WRITTEN	4	/* Record is written */
#define HA_STATE_DELETED	8
#define HA_STATE_NEXT_FOUND	16	/* Next found record (record before) */
#define HA_STATE_PREV_FOUND	32	/* Prev found record (record after) */
#define HA_STATE_NO_KEY		64	/* Last read didn't find record */
#define HA_STATE_KEY_CHANGED	128
#define HA_STATE_WRITE_AT_END	256	/* set in _ps_find_writepos */
#define HA_STATE_BUFF_SAVED	512	/* If current keybuff is info->buff */
#define HA_STATE_ROW_CHANGED	1024	/* To invalide ROW cache */
#define HA_STATE_EXTEND_BLOCK	2048
#define HA_STATE_RNEXT_SAME	4096	/* rnext_same occupied lastkey2 */

/* myisampack expects no more than 32 field types. */
enum en_fieldtype {
  FIELD_LAST=-1,FIELD_NORMAL,FIELD_SKIP_ENDSPACE,FIELD_SKIP_PRESPACE,
  FIELD_SKIP_ZERO,FIELD_BLOB,FIELD_CONSTANT,FIELD_INTERVALL,FIELD_ZERO,
  FIELD_VARCHAR,FIELD_CHECK,
  FIELD_enum_val_count
};

enum data_file_type {
  STATIC_RECORD, DYNAMIC_RECORD, COMPRESSED_RECORD, BLOCK_RECORD
};

/* For key ranges */

#define NO_MIN_RANGE	1
#define NO_MAX_RANGE	2
#define NEAR_MIN	4
#define NEAR_MAX	8
#define UNIQUE_RANGE	16
#define EQ_RANGE	32
#define NULL_RANGE	64
#define GEOM_FLAG      128
#define SKIP_RANGE     256

typedef struct st_key_range
{
  const uchar *key;
  uint length;
  key_part_map keypart_map;
  enum ha_rkey_function flag;
} key_range;

typedef struct st_key_multi_range
{
  key_range start_key;
  key_range end_key;
  char  *ptr;                 /* Free to use by caller (ptr to row etc) */
  uint  range_flag;           /* key range flags see above */
} KEY_MULTI_RANGE;


/* For number of records */
#ifdef BIG_TABLES
#define rows2double(A)	ulonglong2double(A)
typedef my_off_t	ha_rows;
#else
#define rows2double(A)	(double) (A)
typedef ulong		ha_rows;
#endif

#define HA_POS_ERROR	(~ (ha_rows) 0)
#define HA_OFFSET_ERROR	(~ (my_off_t) 0)

#if SYSTEM_SIZEOF_OFF_T == 4
#define MAX_FILE_SIZE	INT_MAX32
#else
#define MAX_FILE_SIZE	LONGLONG_MAX
#endif

#define HA_VARCHAR_PACKLENGTH(field_length) ((field_length) < 256 ? 1 :2)

/* invalidator function reference for Query Cache */
typedef void (* invalidator_by_filename)(const char * filename);

#endif /* _my_base_h */
