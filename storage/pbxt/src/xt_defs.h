/* Copyright (c) 2005 PrimeBase Technologies GmbH
 *
 * PrimeBase XT
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Author: Paul McCullagh
 *
 * H&G2JCtL
 */
#ifndef __xt_defs_h__
#define __xt_defs_h__

#ifdef XT_WIN
#include "win_inttypes.h"
#else
#include <inttypes.h>
#endif
#include <sys/types.h>
#include <assert.h>
#include <stddef.h>
#include <string.h>

//#include "pthread_xt.h"

#ifdef DEBUG
//#define DEBUG_LOG_DELETE
#endif

/* the following macros are used to quote compile-time numeric 
 * constants into strings, e.g. __LINE__ 
 */
#define _QUOTE(x) #x
#define QUOTE(x) _QUOTE(x)

/* ----------------------------------------------------------------------
 * CRASH DEBUGGING
 */

/* Define this if crash debug should be on by default:
 * pbxt_crash_debug set to TRUE by default.
 * It can be turned off by creating a file called 'no-debug'
 * in the pbxt database.
 * It can be turned on by defining the file 'crash-debug'
 * in the pbxt database.
 */
//#define XT_CRASH_DEBUG

/* These are the things crash debug will do: */
/* Create a core dump (windows only): */
#define XT_COREDUMP

/* Backup the datadir before recovery after a crash: */
//#define XT_BACKUP_BEFORE_RECOVERY

/* Keep this number of transaction logs around
 * for analysis after a crash.
 */
#define XT_NUMBER_OF_LOGS_TO_SAVE		5

/* ----------------------------------------------------------------------
 * GENERIC GLOBAL TYPES
 */

#ifdef XT_WIN

#define xtInt1			__int8
#define xtInt2			__int16
#define xtInt4			__int32
#define xtInt8			__int64

#define xtWord1			unsigned __int8
#define xtWord2			unsigned __int16
#define xtWord4			unsigned __int32
#define xtWord8			unsigned __int64

#ifndef PATH_MAX
#define PATH_MAX		MAX_PATH
#endif
#ifndef NAME_MAX
#define NAME_MAX		MAX_PATH
#endif

/* XT actually assumes that off_t is 8 bytes: */
#define off_t			xtWord8

#else // XT_WIN

#define xtInt1			int8_t
#define xtInt2			int16_t
#define xtInt4			int32_t
#define xtInt8			int64_t

#ifdef XT_SOLARIS
#define u_int8_t		uint8_t
#define u_int16_t		uint16_t
#define u_int32_t		uint32_t
#define u_int64_t		uint64_t
#endif

#define xtWord1			u_int8_t
#define xtWord2			u_int16_t
#define xtWord4			u_int32_t
#define xtWord8			u_int64_t

#endif // XT_WIN

/* A pointer sized word value: */
#define xtWordPS		ptrdiff_t

#define XT_MAX_INT_1	((xtInt1) 0x7F)
#define XT_MIN_INT_1	((xtInt1) 0x80)
#define XT_MAX_INT_2	((xtInt2) 0x7FFF)
#define XT_MIN_INT_2	((xtInt2) 0x8000)
#define XT_MAX_INT_4	((xtInt4) 0x7FFFFFFF)
#define XT_MIN_INT_4	((xtInt4) 0x80000000)

#define xtReal4			float
#define xtReal8			double

#ifndef u_int
#define u_int			unsigned int				/* Assumed at least 4 bytes long! */
#define u_long			unsigned long				/* Assumed at least 4 bytes long! */
#endif
#define llong			long long				/* Assumed at least 8 bytes long! */
#define u_llong			unsigned long long			/* Assumed at least 8 bytes long! */

#define c_char			const char

#ifndef NULL
#define NULL			0
#endif

#define xtPublic

#define xtBool			int
#ifndef TRUE
#define TRUE			1
#endif
#ifndef FALSE
#define FALSE			0
#endif

/* Additional return codes: */
#define XT_MAYBE		2
#define XT_ERR			-1
#define XT_NEW			-2
#define XT_RETRY		-3
#define XT_REREAD		-4

#ifdef OK
#undef OK
#endif
#define OK				TRUE

#ifdef FAILED
#undef FAILED
#endif
#define FAILED			FALSE

typedef xtWord1			XTDiskValue1[1];	
typedef xtWord1			XTDiskValue2[2];	
typedef xtWord1			XTDiskValue3[3];	
typedef xtWord1			XTDiskValue4[4];	
typedef xtWord1			XTDiskValue6[6];	
typedef xtWord1			XTDiskValue8[8];	

#ifdef DEBUG
#define XT_VAR_LENGTH	100
#else
#define XT_VAR_LENGTH	1
#endif

typedef struct XTPathStr {
	char				ps_path[XT_VAR_LENGTH];
} *XTPathStrPtr;

//#define XT_UNUSED(x)		x __attribute__((__unused__))
#define XT_UNUSED(x)

/* Only used when DEBUG is on: */
#ifdef DEBUG
#define XT_NDEBUG_UNUSED(x)	x
#else
//#define XT_NDEBUG_UNUSED(x)	x __attribute__((__unused__))
#define XT_NDEBUG_UNUSED(x)
#endif

/* ----------------------------------------------------------------------
 * MAIN CONSTANTS
 */

/*
 * Define if there should only be one database per server instance:
 */
#define XT_USE_GLOBAL_DB

/*
 * The rollover size is the write limit of a log file.
 * After this size is reached, a thread will start a
 * new log.
 *
 * However, logs can grow much larger than this size.
 * The reason is, a transaction single transaction
 * may not span more than one data log file.
 *
 * This means the log rollover size is actually a
 * minimum size.
 */

#ifdef DEBUG
//#define XT_USE_GLOBAL_DEBUG_SIZES
#endif

/*
 * I believe the MySQL limit is 16. This limit is currently only used for
 * BLOB streaming.
 */
#define XT_MAX_COLS_PER_INDEX			32

/*
 * The maximum number of tables that can be created in a PBXT
 * database. The amount is based on the fact that XT creates
 * about 5 files per table in the database, and also
 * uses directory listing to find tables.
 */
#define XT_MAX_TABLES					10000

/*
 * When the amount of garbage in the file is greater than the
 * garbage threshold, then compactor is activated.
 */
#define XT_GARBAGE_THRESHOLD			((double) 50.0)

/* A record that does not contain blobs will be handled as a fixed
 * length record if its maximum size is less than this amount,
 * regardless of the size of the VARCHAR fields it contains.
 */
#define XT_TAB_MIN_VAR_REC_LENGTH		320

/* No record in the data handle file may exceed this size: */
#define XT_TAB_MAX_FIX_REC_LENGTH		(16 * 1024)

/* No record in the data handle file may exceed this size, if
 * AVG_ROW_LENGTH is set.
 */
#define XT_TAB_MAX_FIX_REC_LENGTH_SPEC	(64 * 1024)

/*
 * Determines the page size of the indexes. The value is given
 * in shifts of 1 to the left (e.g. 1 << 11 == 2048,
 * 1 << 12 == 4096).
 *
 * PMC: Note the performance of sysbench is better with 11
 * than with 12.
 *
 * InnoDB uses 16K pages:
 * 1 << 14 == 16384.
 */
#define XT_INDEX_PAGE_SHIFTS			14

/* The number of RW locks used to scatter locks on the rows
 * of a table. The locks are only help for a short time during which
 * the row list is scanned.
 *
 * For more details see [(9)].
 * 223, 1019, 3613
 */
#define XT_ROW_RWLOCKS					1019
//#define XT_ROW_RWLOCKS					223

/*
 * These are the number of row lock "slots" per table.
 * Row locks are taken on UPDATE/DELETE or SELECT FOR UPDATE.
 */
#define XT_ROW_LOCK_COUNT				(XT_ROW_RWLOCKS * 91)

/*
 * The size of index write buffer. Must be at least as large as the
 * largest index page, plus overhead.
 */
#define XT_INDEX_WRITE_BUFFER_SIZE		(1024 * 1024)

/* This is the time in seconds that a open table in the open
 * table pool must be on the free list before it
 * is actually freed from the pool.
 *
 * This is to reduce the affect from MySQL with a very low
 * table cache size, which causes tables to be openned and
 * closed very rapidly.
 */
#define XT_OPEN_TABLE_FREE_TIME			30

/* Define this in order to use memory mapped files
 * (record and row pointer files only).
 *
 * This makes no difference in sysbench R/W performance
 * test.
 */
//#define XT_USE_ROW_REC_MMAP_FILES

/* Define this if sequential scan should load data into the 
 * record cache.
 *
 * This is the way InnoDB behaves.
 */
#define XT_SEQ_SCAN_LOADS_CACHE

/* Define this in order to use direct I/O on index files: */
/* NOTE: DO NOT ENABLE!
 * {DIRECT-IO}
 * It currently does not work, because of changes to the inde
 * cache.
 */
//#define XT_USE_DIRECT_IO_ON_INDEX

/*
 * Define this variable if PBXT should do lazy deleting in indexes
 * Note, even if the variable is not defined, PBXT will handle
 * lazy deleted items in an index.
 *
 * NOTE: This can cause significant degrade of index scan speed.
 * 25% on sysbench readonly index scan tests.
 */
//#define XT_USE_LAZY_DELETE

/*
 * Define this variable if a connection should wait for the
 * sweeper to clean up previous transactions executed by the
 * connection, before continuing.
 *
 * The number of transactions that the sweeper is aload to
 * lag can be dynamic, but there is a limit (XT_MAX_XACT_BEHIND)
 */
#define XT_WAIT_FOR_CLEANUP

/*
 * This seems to be the optimal value, at least according to
 * sysbench/sysbench run --test=oltp --num-threads=128 --max-requests=50000 --mysql-user=root 
 * --oltp-table-size=100000 --oltp-table-name=sb_pbxt --mysql-engine-trx=yes
 *
 * Using 8, 16 and 128 threads.
 */
#define XT_MAX_XACT_BEHIND				2

/* {NO-ACTION-BUG}
 * Define this to implement NO ACTION correctly
 * NOTE: this does not work currently because of a bug
 * in MySQL
 *
 * The bug prevent returning of an error in external_lock()
 * on statement end. In this case an assertion fails.
 *
 * set storage_engine = pbxt;
 * DROP TABLE IF EXISTS t4,t3,t2,t1;
 * CREATE TABLE t1 (s1 INT PRIMARY KEY);
 * CREATE TABLE t2 (s1 INT PRIMARY KEY, FOREIGN KEY (s1) REFERENCES t1 (s1) ON DELETE NO ACTION);
 * 
 * INSERT INTO t1 VALUES (1);
 * INSERT INTO t2 VALUES (1);
 * 
 * begin;
 * INSERT INTO t1 VALUES (2);
 * DELETE FROM t1 where s1 = 1;
 * <-- Assertion fails here because this DELETE returns
 * an error from external_lock()
 */
//#define XT_IMPLEMENT_NO_ACTION

/* ----------------------------------------------------------------------
 * GLOBAL CONSTANTS
 */

#define XT_INDEX_PAGE_SIZE				(1 << XT_INDEX_PAGE_SHIFTS)
#define XT_INDEX_PAGE_MASK				(XT_INDEX_PAGE_SIZE - 1)

/* The index file uses direct I/O. This is the minimum block.
 * size that can be used when doing direct I/O.
 */
#define XT_BLOCK_SIZE_FOR_DIRECT_IO		512

/*
 * The header is currently a fixed size, so the information must
 * fit in this block!
 *
 * This must also be a multiple of XT_INDEX_MIN_BLOCK_SIZE
 */
#define XT_INDEX_HEAD_SIZE				(XT_BLOCK_SIZE_FOR_DIRECT_IO * 8)		// 4K

#define XT_IDENTIFIER_CHAR_COUNT		64

#define XT_IDENTIFIER_NAME_SIZE			((XT_IDENTIFIER_CHAR_COUNT * 3) + 1)	// The identifier length as UTF-8
#define XT_TABLE_NAME_SIZE				((XT_IDENTIFIER_CHAR_COUNT * 5) + 1)	// The maximum length of a file name that has been normalized

#define XT_ADD_PTR(p, l)				((void *) ((char *) (p) + (l)))

/* ----------------------------------------------------------------------
 * DEFINES DEPENDENT ON  CONSTANTS
 */

#ifdef XT_USE_ROW_REC_MMAP_FILES

#define XT_ROW_REC_FILE_PTR						XTMapFilePtr
#define XT_PWRITE_RR_FILE						xt_pwrite_fmap
#define XT_PREAD_RR_FILE						xt_pread_fmap
#define XT_FLUSH_RR_FILE						xt_flush_fmap
#define XT_CLOSE_RR_FILE_NS						xt_close_fmap_ns

#define XT_LOCK_MEMORY_PTR(x, f, a, s, v, c)	do { x = xt_lock_fmap_ptr(f, a, s, v, c); } while (0)
#define XT_UNLOCK_MEMORY_PTR(f, d, e, v)		do { xt_unlock_fmap_ptr(f, v); d = NULL; } while (0)

#else

#define XT_ROW_REC_FILE_PTR						XTOpenFilePtr
#define XT_PWRITE_RR_FILE						xt_pwrite_file
#define XT_PREAD_RR_FILE						xt_pread_file
#define XT_FLUSH_RR_FILE						xt_flush_file
#define XT_CLOSE_RR_FILE_NS						xt_close_file_ns

#define XT_LOCK_MEMORY_PTR(x, f, a, s, v, c)	do { if (!xt_lock_file_ptr(f, &x, a, s, v, c)) x = NULL; } while (0)
#define XT_UNLOCK_MEMORY_PTR(f, d, e, v)		do { if (e) { xt_unlock_file_ptr(f, d, v); d = NULL; } } while (0)

#endif

/* ----------------------------------------------------------------------
 * DEBUG SIZES!
 * Reduce the thresholds to make things happen faster.
 */

#ifdef XT_USE_GLOBAL_DEBUG_SIZES

//#undef XT_ROW_RWLOCKS
//#define XT_ROW_RWLOCKS				2

//#undef XT_TAB_MIN_VAR_REC_LENGTH
//#define XT_TAB_MIN_VAR_REC_LENGTH		20

//#undef XT_ROW_LOCK_COUNT
//#define XT_ROW_LOCK_COUNT				(XT_ROW_RWLOCKS * 2)

//#undef XT_INDEX_PAGE_SHIFTS
//#define XT_INDEX_PAGE_SHIFTS			8	// 256
//#undef XT_BLOCK_SIZE_FOR_DIRECT_IO
//#define XT_BLOCK_SIZE_FOR_DIRECT_IO	256

//#undef XT_INDEX_WRITE_BUFFER_SIZE
//#define XT_INDEX_WRITE_BUFFER_SIZE	(40 * 1024)

#endif

/* ----------------------------------------------------------------------
 * BYTE ORDER
 */

/*
 * Byte order on the disk is little endian! This is the byte order of the i386.
 * Little endian byte order starts with the least significant byte.
 *
 * The reason for choosing this byte order for the disk is 2-fold:
 * Firstly the i386 is the cheapest and fasted platform today.
 * Secondly the i386, unlike RISK chips (with big endian) can address
 * memory that is not aligned!
 *
 * Since the disk image of PrimeBase XT is not aligned, the second point
 * is significant. A RISK chip needs to access it byte-wise, so we might as
 * well do the byte swapping at the same time.
 *
 * The macros below are of 4 general types:
 *
 * GET/SET - Get and set 1,2,4,8 byte values (short, int, long, etc).
 * Values are swapped only on big endian platforms. This makes these
 * functions very efficient on little-endian platforms.
 *
 * COPY - Transfer data without swapping regardless of platform. This
 * function is a bit more efficient on little-endian platforms
 * because alignment is not an issue.
 *
 * MOVE - Similar to get and set, but the deals with memory instead
 * of values. Since no swapping is done on little-endian platforms
 * this function is identical to COPY on little-endian platforms.
 *
 * SWAP - Transfer and swap data regardless of the platform type.
 * Aligment is not assumed.
 *
 * The DISK component of the macro names indicates that alignment of
 * the value cannot be assumed.
 *
 */
#if BYTE_ORDER == BIG_ENDIAN
/* The native order of the machine is big endian. Since the native disk
 * disk order of XT is little endian, all data to and from disk
 * must be swapped.
 */
#define XT_SET_DISK_1(d, s)		((d)[0] = (xtWord1) (s))

#define XT_SET_DISK_2(d, s)		do { (d)[0] = (xtWord1)  (((xtWord2) (s))        & 0xFF); (d)[1] = (xtWord1) ((((xtWord2) (s)) >> 8 ) & 0xFF); } while (0)

#define XT_SET_DISK_3(d, s)		do { (d)[0] = (xtWord1)  (((xtWord4) (s))        & 0xFF); (d)[1] = (xtWord1) ((((xtWord4) (s)) >> 8 ) & 0xFF); \
									 (d)[2] = (xtWord1) ((((xtWord4) (s)) >> 16) & 0xFF); } while (0)

#define XT_SET_DISK_4(d, s)		do { (d)[0] = (xtWord1)  (((xtWord4) (s))        & 0xFF); (d)[1] = (xtWord1) ((((xtWord4) (s)) >> 8 ) & 0xFF); \
									 (d)[2] = (xtWord1) ((((xtWord4) (s)) >> 16) & 0xFF); (d)[3] = (xtWord1) ((((xtWord4) (s)) >> 24) & 0xFF); } while (0)

#define XT_SET_DISK_6(d, s)		do { (d)[0] = (xtWord1)  (((xtWord8) (s))        & 0xFF); (d)[1] = (xtWord1) ((((xtWord8) (s)) >> 8 ) & 0xFF); \
									 (d)[2] = (xtWord1) ((((xtWord8) (s)) >> 16) & 0xFF); (d)[3] = (xtWord1) ((((xtWord8) (s)) >> 24) & 0xFF); \
									 (d)[4] = (xtWord1) ((((xtWord8) (s)) >> 32) & 0xFF); (d)[5] = (xtWord1) ((((xtWord8) (s)) >> 40) & 0xFF); } while (0)

#define XT_SET_DISK_8(d, s)		do { (d)[0] = (xtWord1)  (((xtWord8) (s))        & 0xFF); (d)[1] = (xtWord1) ((((xtWord8) (s)) >> 8 ) & 0xFF); \
									 (d)[2] = (xtWord1) ((((xtWord8) (s)) >> 16) & 0xFF); (d)[3] = (xtWord1) ((((xtWord8) (s)) >> 24) & 0xFF); \
									 (d)[4] = (xtWord1) ((((xtWord8) (s)) >> 32) & 0xFF); (d)[5] = (xtWord1) ((((xtWord8) (s)) >> 40) & 0xFF); \
									 (d)[6] = (xtWord1) ((((xtWord8) (s)) >> 48) & 0xFF); (d)[7] = (xtWord1) ((((xtWord8) (s)) >> 56) & 0xFF); } while (0)

#define XT_GET_DISK_1(s)		((s)[0])

#define XT_GET_DISK_2(s)		((xtWord2) (((xtWord2) (s)[0]) | (((xtWord2) (s)[1]) << 8)))

#define XT_GET_DISK_3(s)		((xtWord4) (((xtWord4) (s)[0]) | (((xtWord4) (s)[1]) << 8) | (((xtWord4) (s)[2]) << 16)))

#define XT_GET_DISK_4(s)		(((xtWord4) (s)[0])        | (((xtWord4) (s)[1]) << 8 ) | \
								(((xtWord4) (s)[2]) << 16) | (((xtWord4) (s)[3]) << 24))

#define XT_GET_DISK_6(s)		(((xtWord8) (s)[0])        | (((xtWord8) (s)[1]) << 8 ) | \
								(((xtWord8) (s)[2]) << 16) | (((xtWord8) (s)[3]) << 24) | \
								(((xtWord8) (s)[4]) << 32) | (((xtWord8) (s)[5]) << 40))

#define XT_GET_DISK_8(s)		(((xtWord8) (s)[0])        | (((xtWord8) (s)[1]) << 8 ) | \
								(((xtWord8) (s)[2]) << 16) | (((xtWord8) (s)[3]) << 24) | \
								(((xtWord8) (s)[4]) << 32) | (((xtWord8) (s)[5]) << 40) | \
								(((xtWord8) (s)[6]) << 48) | (((xtWord8) (s)[7]) << 56))

/* Move will copy memory, and swap the bytes on a big endian machine.
 * On a little endian machine it is the same as COPY.
 */
#define XT_MOVE_DISK_1(d, s)	((d)[0] = (s)[0])
#define XT_MOVE_DISK_2(d, s)	do { (d)[0] = (s)[1]; (d)[1] = (s)[0]; } while (0)
#define XT_MOVE_DISK_3(d, s)	do { (d)[0] = (s)[2]; (d)[1] = (s)[1]; (d)[2] = (s)[0]; } while (0)
#define XT_MOVE_DISK_4(d, s)	do { (d)[0] = (s)[3]; (d)[1] = (s)[2]; (d)[2] = (s)[1]; (d)[3] = (s)[0]; } while (0)
#define XT_MOVE_DISK_8(d, s)	do { (d)[0] = (s)[7]; (d)[1] = (s)[6]; \
									 (d)[2] = (s)[5]; (d)[3] = (s)[4]; \
									 (d)[4] = (s)[3]; (d)[5] = (s)[2]; \
									 (d)[6] = (s)[1]; (d)[7] = (s)[0]; } while (0)

/*
 * Copy just copies the number of bytes assuming the data is not alligned.
 */
#define XT_COPY_DISK_1(d, s)	(d)[0] = s
#define XT_COPY_DISK_2(d, s)	do { (d)[0] = (s)[0]; (d)[1] = (s)[1]; } while (0)
#define XT_COPY_DISK_3(d, s)	do { (d)[0] = (s)[0]; (d)[1] = (s)[1]; (d)[2] = (s)[2]; } while (0)
#define XT_COPY_DISK_4(d, s)	do { (d)[0] = (s)[0]; (d)[1] = (s)[1]; (d)[2] = (s)[2]; (d)[3] = (s)[3]; } while (0)
#define XT_COPY_DISK_6(d, s)	memcpy(&((d)[0]), &((s)[0]), 6)
#define XT_COPY_DISK_8(d, s)	memcpy(&((d)[0]), &((s)[0]), 8)
#define XT_COPY_DISK_10(d, s)	memcpy(&((d)[0]), &((s)[0]), 10)

#define XT_SET_NULL_DISK_1(d)	XT_SET_DISK_1(d, 0)
#define XT_SET_NULL_DISK_2(d)	do { (d)[0] = 0; (d)[1] = 0; } while (0)
#define XT_SET_NULL_DISK_4(d)	do { (d)[0] = 0; (d)[1] = 0; (d)[2] = 0; (d)[3] = 0; } while (0)
#define XT_SET_NULL_DISK_6(d)	do { (d)[0] = 0; (d)[1] = 0; (d)[2] = 0; (d)[3] = 0; (d)[4] = 0; (d)[5] = 0; } while (0)
#define XT_SET_NULL_DISK_8(d)	do { (d)[0] = 0; (d)[1] = 0; (d)[2] = 0; (d)[3] = 0; (d)[4] = 0; (d)[5] = 0; (d)[6] = 0; (d)[7] = 0; } while (0)

#define XT_IS_NULL_DISK_1(d)	(!(XT_GET_DISK_1(d)))
#define XT_IS_NULL_DISK_4(d)	(!(d)[0] && !(d)[1] && !(d)[2] && !(d)[3])
#define XT_IS_NULL_DISK_8(d)	(!(d)[0] && !(d)[1] && !(d)[2] && !(d)[3] && !(d)[4] && !(d)[5] && !(d)[6] && !(7)[3])

#define XT_EQ_DISK_4(d, s)		((d)[0] == (s)[0] && (d)[1] == (s)[1] && (d)[2] == (s)[2] && (d)[3] == (s)[3])
#define XT_EQ_DISK_8(d, s)		((d)[0] == (s)[0] && (d)[1] == (s)[1] && (d)[2] == (s)[2] && (d)[3] == (s)[3] && \
								(d)[4] == (s)[4] && (d)[5] == (s)[5] && (d)[6] == (s)[6] && (d)[7] == (s)[7])

#define XT_IS_FF_DISK_4(d)		((d)[0] == 0xFF && (d)[1] == 0xFF && (d)[2] == 0xFF && (d)[3] == 0xFF)
#else
/*
 * The native order of the machine is little endian. This means the data to
 * and from disk need not be swapped. In addition to this, since
 * the i386 can access non-aligned memory we are not required to
 * handle the data byte-for-byte.
 */
#define XT_SET_DISK_1(d, s)		((d)[0] = (xtWord1) (s))
#define XT_SET_DISK_2(d, s)		(*((xtWord2 *) &((d)[0])) = (xtWord2) (s))
#define XT_SET_DISK_3(d, s)		do { (*((xtWord2 *) &((d)[0])) = (xtWord2) (s));  *((xtWord1 *) &((d)[2])) = (xtWord1) (((xtWord4) (s)) >> 16); } while (0)
#define XT_SET_DISK_4(d, s)		(*((xtWord4 *) &((d)[0])) = (xtWord4) (s))
#define XT_SET_DISK_6(d, s)		do { *((xtWord4 *) &((d)[0])) = (xtWord4) (s); *((xtWord2 *) &((d)[4])) = (xtWord2) (((xtWord8) (s)) >> 32); } while (0)
#define XT_SET_DISK_8(d, s)		(*((xtWord8 *) &((d)[0])) = (xtWord8) (s))

#define XT_GET_DISK_1(s)		((s)[0])
#define XT_GET_DISK_2(s)		*((xtWord2 *) &((s)[0]))
#define XT_GET_DISK_3(s)		((xtWord4) *((xtWord2 *) &((s)[0])) | (((xtWord4) *((xtWord1 *) &((s)[2]))) << 16))
#define XT_GET_DISK_4(s)		*((xtWord4 *) &((s)[0]))
#define XT_GET_DISK_6(s)		((xtWord8) *((xtWord4 *) &((s)[0])) | (((xtWord8) *((xtWord2 *) &((s)[4]))) << 32))
#define XT_GET_DISK_8(s)		*((xtWord8 *) &((s)[0]))

#define XT_MOVE_DISK_1(d, s)	((d)[0] = (s)[0])
#define XT_MOVE_DISK_2(d, s)	XT_COPY_DISK_2(d, s)
#define XT_MOVE_DISK_3(d, s)	XT_COPY_DISK_3(d, s)
#define XT_MOVE_DISK_4(d, s)	XT_COPY_DISK_4(d, s)
#define XT_MOVE_DISK_8(d, s)	XT_COPY_DISK_8(d, s)

#define XT_COPY_DISK_1(d, s)	(d)[0] = s
#define XT_COPY_DISK_2(d, s)	(*((xtWord2 *) &((d)[0])) = (*((xtWord2 *) &((s)[0]))))
#define XT_COPY_DISK_3(d, s)	do { *((xtWord2 *) &((d)[0])) = *((xtWord2 *) &((s)[0])); (d)[2] = (s)[2]; } while (0)
#define XT_COPY_DISK_4(d, s)	(*((xtWord4 *) &((d)[0])) = (*((xtWord4 *) &((s)[0]))))
#define XT_COPY_DISK_6(d, s)	do { *((xtWord4 *) &((d)[0])) = *((xtWord4 *) &((s)[0])); *((xtWord2 *) &((d)[4])) = *((xtWord2 *) &((s)[4])); } while (0)
#define XT_COPY_DISK_8(d, s)	(*((xtWord8 *) &(d[0])) = (*((xtWord8 *) &((s)[0]))))
#define XT_COPY_DISK_10(d, s)	memcpy(&((d)[0]), &((s)[0]), 10)

#define XT_SET_NULL_DISK_1(d)	XT_SET_DISK_1(d, 0)
#define XT_SET_NULL_DISK_2(d)	XT_SET_DISK_2(d, 0)
#define XT_SET_NULL_DISK_3(d)	XT_SET_DISK_3(d, 0)
#define XT_SET_NULL_DISK_4(d)	XT_SET_DISK_4(d, 0L)
#define XT_SET_NULL_DISK_6(d)	XT_SET_DISK_6(d, 0LL)
#define XT_SET_NULL_DISK_8(d)	XT_SET_DISK_8(d, 0LL)

#define XT_IS_NULL_DISK_1(d)	(!(XT_GET_DISK_1(d)))
#define XT_IS_NULL_DISK_2(d)	(!(XT_GET_DISK_2(d)))
#define XT_IS_NULL_DISK_3(d)	(!(XT_GET_DISK_3(d)))
#define XT_IS_NULL_DISK_4(d)	(!(XT_GET_DISK_4(d)))
#define XT_IS_NULL_DISK_8(d)	(!(XT_GET_DISK_8(d)))

#define XT_EQ_DISK_4(d, s)		(XT_GET_DISK_4(d) == XT_GET_DISK_4(s))
#define XT_EQ_DISK_8(d, s)		(XT_GET_DISK_8(d) == XT_GET_DISK_8(s))

#define XT_IS_FF_DISK_4(d)		(XT_GET_DISK_4(d) == 0xFFFFFFFF)
#endif

#define XT_CMP_DISK_4(a, b)		((xtInt4) XT_GET_DISK_4(a) - (xtInt4) XT_GET_DISK_4(b))
#define XT_CMP_DISK_8(d, s)		memcmp(&((d)[0]), &((s)[0]), 8)
//#define XT_CMP_DISK_8(d, s)		(XT_CMP_DISK_4((d).h_number_4, (s).h_number_4) == 0 ? XT_CMP_DISK_4((d).h_file_4, (s).h_file_4) : XT_CMP_DISK_4((d).h_number_4, (s).h_number_4))

#define XT_SWAP_DISK_2(d, s)	do { (d)[0] = (s)[1]; (d)[1] = (s)[0]; } while (0)
#define XT_SWAP_DISK_3(d, s)	do { (d)[0] = (s)[2]; (d)[1] = (s)[1]; (d)[2] = (s)[0]; } while (0)
#define XT_SWAP_DISK_4(d, s)	do { (d)[0] = (s)[3]; (d)[1] = (s)[2]; (d)[2] = (s)[1]; (d)[3] = (s)[0]; } while (0)
#define XT_SWAP_DISK_8(d, s)	do { (d)[0] = (s)[7]; (d)[1] = (s)[6]; (d)[2] = (s)[5]; (d)[3] = (s)[4]; \
									 (d)[4] = (s)[3]; (d)[5] = (s)[2]; (d)[6] = (s)[1]; (d)[7] = (s)[0]; } while (0)

/* ----------------------------------------------------------------------
 *  GLOBAL APPLICATION TYPES & MACROS
 */

struct XTThread;

typedef void (*XTFreeFunc)(struct XTThread *self, void *thunk, void *item);
typedef int (*XTCompareFunc)(struct XTThread *self, register const void *thunk, register const void *a, register const void *b);

/* Log ID and offset: */
#define xtLogID					xtWord4
#define xtLogOffset				off_t

#define xtDatabaseID			xtWord4
#define xtTableID				xtWord4
#define xtOpSeqNo				xtWord4
#define xtXactID				xtWord4
#define xtThreadID				xtWord4

#ifdef DEBUG
//#define XT_USE_NODE_ID_STRUCT
#endif

#ifdef XT_USE_NODE_ID_STRUCT
typedef struct xtIndexNodeID {
	xtWord4						x;
} xtIndexNodeID;
#define XT_NODE_TEMP			xtWord4 xt_node_temp
#define	XT_NODE_ID(a)			(a).x
#define	XT_RET_NODE_ID(a)		*((xtIndexNodeID *) &(xt_node_temp = (a)))
#else
#define XT_NODE_TEMP			
#define xtIndexNodeID			xtWord4
#define	XT_NODE_ID(a)			a
#define	XT_RET_NODE_ID(a)		((xtIndexNodeID) (a))
#endif

/* Row, Record ID and Record offsets: */
#define xtRowID					xtWord4
#define xtRecordID				xtWord4				/* NOTE: Record offset == header-size + record-id * record-size! */
#define xtRefID					xtWord4				/* Must be big enough to contain a xtRowID and a xtRecordID! */
#define xtRecOffset				off_t
#define	xtDiskRecordID4			XTDiskValue4
#ifdef XT_WIN
#define xtProcID				DWORD
#else
#define xtProcID				pid_t
#endif

#define XT_ROW_ID_SIZE			4
#define XT_RECORD_ID_SIZE		4
#define XT_REF_ID_SIZE			4					/* max(XT_ROW_ID_SIZE, XT_RECORD_ID_SIZE) */
#define XT_RECORD_OFFS_SIZE		4
#define XT_RECORD_REF_SIZE		(XT_RECORD_ID_SIZE + XT_ROW_ID_SIZE)
#define XT_CHECKSUM4_REC(x)		(x)

#define XT_XACT_ID_SIZE			4
#define XT_CHECKSUM4_XACT(x)	(x)

#ifdef XT_WIN
#define __FUNC__				__FUNCTION__
#elif defined(XT_SOLARIS)
#define __FUNC__				"__func__"
#else
#define __FUNC__				__PRETTY_FUNCTION__
#endif

/* ----------------------------------------------------------------------
 * GLOBAL VARIABLES
 */

extern bool					pbxt_inited;
extern xtBool				pbxt_ignore_case;
extern const char			*pbxt_extensions[];
extern xtBool				pbxt_crash_debug;


/* ----------------------------------------------------------------------
 * DRIZZLE MAPPINGS VARIABLES
 */

#ifdef DRIZZLED
/* Drizzle is stuck at this level: */
#define MYSQL_VERSION_ID					60005

#define TABLE_LIST							TableList
#define TABLE								Table
#define THD									Session
#define MYSQL_THD							Session *
#define THR_THD								THR_Session
#define STRUCT_TABLE						class Table
#define TABLE_SHARE							TableShare

#define MYSQL_TYPE_STRING					DRIZZLE_TYPE_VARCHAR
#define MYSQL_TYPE_VARCHAR					DRIZZLE_TYPE_VARCHAR
#define MYSQL_TYPE_LONGLONG					DRIZZLE_TYPE_LONGLONG
#define MYSQL_TYPE_BLOB						DRIZZLE_TYPE_BLOB
#define MYSQL_TYPE_ENUM						DRIZZLE_TYPE_ENUM
#define MYSQL_TYPE_LONG						DRIZZLE_TYPE_LONG
#define MYSQL_PLUGIN_VAR_HEADER				DRIZZLE_PLUGIN_VAR_HEADER
#define MYSQL_SYSVAR_STR					DRIZZLE_SYSVAR_STR
#define MYSQL_SYSVAR_INT					DRIZZLE_SYSVAR_INT
#define MYSQL_SYSVAR						DRIZZLE_SYSVAR
#define MYSQL_STORAGE_ENGINE_PLUGIN			DRIZZLE_STORAGE_ENGINE_PLUGIN
#define MYSQL_INFORMATION_SCHEMA_PLUGIN		DRIZZLE_INFORMATION_SCHEMA_PLUGIN
#define memcpy_fixed						memcpy
#define bfill(m, len, ch)					memset(m, ch, len)

#define mx_tmp_use_all_columns(x, y)		(x)->use_all_columns(y)
#define mx_tmp_restore_column_map(x, y)		(x)->restore_column_map(y)
#define MX_BIT_FAST_TEST_AND_SET(x, y)		bitmap_test_and_set(x, y)

#define MX_TABLE_TYPES_T					handler::Table_flags
#define MX_UINT8_T							uint8_t
#define MX_ULONG_T							uint32_t
#define MX_ULONGLONG_T						uint64_t
#define MX_LONGLONG_T						uint64_t
#define MX_CHARSET_INFO						struct charset_info_st
#define MX_CONST_CHARSET_INFO				const struct charset_info_st			
#define MX_CONST							const

#define my_bool								bool
#define int16								int16_t
#define int32								int32_t
#define uint16								uint16_t
#define uint32								uint32_t
#define uchar								unsigned char
#define longlong							int64_t
#define ulonglong							uint64_t

#define HAVE_LONG_LONG

#define my_malloc(x, y)						malloc(x)
#define my_free(x, y)						free(x)

#define HA_CAN_SQL_HANDLER					0
#define HA_CAN_INSERT_DELAYED				0
#define HA_BINLOG_ROW_CAPABLE				0
#define HA_BINLOG_STMT_CAPABLE				0
#define HA_CACHE_TBL_TRANSACT				0

#define max									cmax
#define min									cmin

#define NullS								NULL

#define thd_charset							session_charset
#define thd_query							session_query
#define thd_slave_thread					session_slave_thread
#define thd_non_transactional_update		session_non_transactional_update
#define thd_binlog_format					session_binlog_format
#define thd_mark_transaction_to_rollback	session_mark_transaction_to_rollback
#define thd_ha_data							session_ha_data
#define current_thd							current_session
#define thd_sql_command						session_sql_command
#define thd_test_options					session_test_options
#define thd_killed							session_killed
#define thd_tx_isolation					session_tx_isolation
#define thd_in_lock_tables					session_in_lock_tables
#define thd_tablespace_op					session_tablespace_op
#define thd_alloc							session_alloc
#define thd_make_lex_string					session_make_lex_string
#define column_bitmaps_signal()

#define my_pthread_setspecific_ptr(T, V)	pthread_setspecific(T, (void*) (V))

#define mysql_real_data_home				drizzle_real_data_home

#define mi_int4store(T,A)   { uint32_t def_temp= (uint32_t) (A);\
                              ((unsigned char*) (T))[3]= (unsigned char) (def_temp);\
                              ((unsigned char*) (T))[2]= (unsigned char) (def_temp >> 8);\
                              ((unsigned char*) (T))[1]= (unsigned char) (def_temp >> 16);\
                              ((unsigned char*) (T))[0]= (unsigned char) (def_temp >> 24); }

#define mi_uint4korr(A) ((uint32_t) (((uint32_t) (((const unsigned char*) (A))[3])) +\
                                   (((uint32_t) (((const unsigned char*) (A))[2])) << 8) +\
                                   (((uint32_t) (((const unsigned char*) (A))[1])) << 16) +\
                                   (((uint32_t) (((const unsigned char*) (A))[0])) << 24)))

class PBXTStorageEngine;
typedef PBXTStorageEngine handlerton;

#else // DRIZZLED
/* The MySQL case: */
#if MYSQL_VERSION_ID >= 60008
#define STRUCT_TABLE						struct TABLE
#else
#define STRUCT_TABLE						struct st_table
#endif

#define mx_tmp_use_all_columns				dbug_tmp_use_all_columns
#define mx_tmp_restore_column_map(x, y)		dbug_tmp_restore_column_map((x)->read_set, y)
#define MX_BIT_FAST_TEST_AND_SET(x, y)		bitmap_fast_test_and_set(x, y)

#define MX_TABLE_TYPES_T					ulonglong
#define MX_UINT8_T							uint8
#define MX_ULONG_T							ulong
#define MX_ULONGLONG_T						ulonglong
#define MX_LONGLONG_T						longlong
#define MX_CHARSET_INFO						CHARSET_INFO
#define MX_CONST_CHARSET_INFO				struct charset_info_st			
#define MX_CONST							

#endif // DRIZZLED

#define MX_BITMAP							MY_BITMAP
#define MX_BIT_SIZE()						n_bits
#define MX_BIT_IS_SUBSET(x, y)				bitmap_is_subset(x, y)
#define MX_BIT_SET(x, y)					bitmap_set_bit(x, y)

#ifndef XT_SCAN_CORE_DEFINED
#define XT_SCAN_CORE_DEFINED
xtBool	xt_mm_scan_core(void);
#endif

//#define DEBUG_LOCK_QUEUE

#endif
