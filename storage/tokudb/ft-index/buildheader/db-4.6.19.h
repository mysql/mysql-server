/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
/*
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996,2007 Oracle.  All rights reserved.
 *
 * $Id$
 *
 * db.h include file layout:
 *	General.
 *	Database Environment.
 *	Locking subsystem.
 *	Logging subsystem.
 *	Shared buffer cache (mpool) subsystem.
 *	Transaction subsystem.
 *	Access methods.
 *	Access method cursors.
 *	Dbm/Ndbm, Hsearch historic interfaces.
 */

#ifndef _DB_H_
#define	_DB_H_

#ifndef	__NO_SYSTEM_INCLUDES
#include <sys/types.h>
#include <inttypes.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#endif


#if defined(__cplusplus)
extern "C" {
#endif


#undef __P
#define	__P(protos)	protos

/*
 * Berkeley DB version information.
 */
#define	DB_VERSION_MAJOR	4
#define	DB_VERSION_MINOR	6
#define	DB_VERSION_PATCH	19
#define	DB_VERSION_STRING	"Berkeley DB 4.6.19: (August 10, 2007)"

/*
 * !!!
 * Berkeley DB uses specifically sized types.  If they're not provided by
 * the system, typedef them here.
 *
 * We protect them against multiple inclusion using __BIT_TYPES_DEFINED__,
 * as does BIND and Kerberos, since we don't know for sure what #include
 * files the user is using.
 *
 * !!!
 * We also provide the standard u_int, u_long etc., if they're not provided
 * by the system.
 */
#ifndef	__BIT_TYPES_DEFINED__
#define	__BIT_TYPES_DEFINED__







#endif






/*
 * Missing ANSI types.
 *
 * uintmax_t --
 * Largest unsigned type, used to align structures in memory.  We don't store
 * floating point types in structures, so integral types should be sufficient
 * (and we don't have to worry about systems that store floats in other than
 * power-of-2 numbers of bytes).  Additionally this fixes compilers that rewrite
 * structure assignments and ANSI C memcpy calls to be in-line instructions
 * that happen to require alignment.
 *
 * uintptr_t --
 * Unsigned type that's the same size as a pointer.  There are places where
 * DB modifies pointers by discarding the bottom bits to guarantee alignment.
 * We can't use uintmax_t, it may be larger than the pointer, and compilers
 * get upset about that.  So far we haven't run on any machine where there's
 * no unsigned type the same size as a pointer -- here's hoping.
 */










/*
 * Sequences are only available on machines with 64-bit integral types.
 */
typedef int64_t db_seq_t;

/* Thread and process identification. */
typedef pthread_t db_threadid_t;

/* Basic types that are exported or quasi-exported. */
typedef	uint32_t	db_pgno_t;	/* Page number type. */
typedef	uint16_t	db_indx_t;	/* Page offset type. */
#define	DB_MAX_PAGES	0xffffffff	/* >= # of pages in a file */

typedef	uint32_t	db_recno_t;	/* Record number type. */
#define	DB_MAX_RECORDS	0xffffffff	/* >= # of records in a tree */

typedef uint32_t	db_timeout_t;	/* Type of a timeout. */

/*
 * Region offsets are the difference between a pointer in a region and the
 * region's base address.  With private environments, both addresses are the
 * result of calling malloc, and we can't assume anything about what malloc
 * will return, so region offsets have to be able to hold differences between
 * arbitrary pointers.
 */
typedef	uintptr_t	roff_t;

/*
 * Forward structure declarations, so we can declare pointers and
 * applications can get type checking.
 */
struct __db;		typedef struct __db DB;
struct __db_bt_stat;	typedef struct __db_bt_stat DB_BTREE_STAT;
struct __db_cipher;	typedef struct __db_cipher DB_CIPHER;
struct __db_compact;	typedef struct __db_compact DB_COMPACT;
struct __db_dbt;	typedef struct __db_dbt DBT;
struct __db_env;	typedef struct __db_env DB_ENV;
struct __db_h_stat;	typedef struct __db_h_stat DB_HASH_STAT;
struct __db_ilock;	typedef struct __db_ilock DB_LOCK_ILOCK;
struct __db_lock_stat;	typedef struct __db_lock_stat DB_LOCK_STAT;
struct __db_lock_hstat;	typedef struct __db_lock_hstat DB_LOCK_HSTAT;
struct __db_lock_u;	typedef struct __db_lock_u DB_LOCK;
struct __db_locker;	typedef struct __db_locker DB_LOCKER;
struct __db_lockreq;	typedef struct __db_lockreq DB_LOCKREQ;
struct __db_locktab;	typedef struct __db_locktab DB_LOCKTAB;
struct __db_log;	typedef struct __db_log DB_LOG;
struct __db_log_cursor;	typedef struct __db_log_cursor DB_LOGC;
struct __db_log_stat;	typedef struct __db_log_stat DB_LOG_STAT;
struct __db_lsn;	typedef struct __db_lsn DB_LSN;
struct __db_mpool;	typedef struct __db_mpool DB_MPOOL;
struct __db_mpool_fstat;typedef struct __db_mpool_fstat DB_MPOOL_FSTAT;
struct __db_mpool_stat;	typedef struct __db_mpool_stat DB_MPOOL_STAT;
struct __db_mpoolfile;	typedef struct __db_mpoolfile DB_MPOOLFILE;
struct __db_mutex_stat;	typedef struct __db_mutex_stat DB_MUTEX_STAT;
struct __db_mutex_t;	typedef struct __db_mutex_t DB_MUTEX;
struct __db_mutexmgr;	typedef struct __db_mutexmgr DB_MUTEXMGR;
struct __db_preplist;	typedef struct __db_preplist DB_PREPLIST;
struct __db_qam_stat;	typedef struct __db_qam_stat DB_QUEUE_STAT;
struct __db_rep;	typedef struct __db_rep DB_REP;
struct __db_rep_stat;	typedef struct __db_rep_stat DB_REP_STAT;
struct __db_repmgr_site; \
			typedef struct __db_repmgr_site DB_REPMGR_SITE;
struct __db_repmgr_stat; \
			typedef struct __db_repmgr_stat DB_REPMGR_STAT;
struct __db_seq_record; typedef struct __db_seq_record DB_SEQ_RECORD;
struct __db_seq_stat;	typedef struct __db_seq_stat DB_SEQUENCE_STAT;
struct __db_sequence;	typedef struct __db_sequence DB_SEQUENCE;
struct __db_txn;	typedef struct __db_txn DB_TXN;
struct __db_txn_active;	typedef struct __db_txn_active DB_TXN_ACTIVE;
struct __db_txn_stat;	typedef struct __db_txn_stat DB_TXN_STAT;
struct __db_txnmgr;	typedef struct __db_txnmgr DB_TXNMGR;
struct __dbc;		typedef struct __dbc DBC;
struct __dbc_internal;	typedef struct __dbc_internal DBC_INTERNAL;
struct __fh_t;		typedef struct __fh_t DB_FH;
struct __fname;		typedef struct __fname FNAME;
struct __key_range;	typedef struct __key_range DB_KEY_RANGE;
struct __mpoolfile;	typedef struct __mpoolfile MPOOLFILE;

/* Key/data structure -- a Data-Base Thang. */
struct __db_dbt {
	void	 *data;			/* Key/data */
	uint32_t size;			/* key/data length */

	uint32_t ulen;			/* RO: length of user buffer. */
	uint32_t dlen;			/* RO: get/put record length. */
	uint32_t doff;			/* RO: get/put record offset. */

	void *app_data;

#define	DB_DBT_APPMALLOC	0x001	/* Callback allocated memory. */
#define	DB_DBT_DUPOK		0x002	/* Insert if duplicate. */
#define	DB_DBT_ISSET		0x004	/* Lower level calls set value. */
#define	DB_DBT_MALLOC		0x008	/* Return in malloc'd memory. */
#define	DB_DBT_MULTIPLE		0x010	/* References multiple records. */
#define	DB_DBT_PARTIAL		0x020	/* Partial put/get. */
#define	DB_DBT_REALLOC		0x040	/* Return in realloc'd memory. */
#define	DB_DBT_USERCOPY		0x080	/* Use the user-supplied callback. */
#define	DB_DBT_USERMEM		0x100	/* Return in user's memory. */
	uint32_t flags;
};

/*
 * Common flags --
 *	Interfaces which use any of these common flags should never have
 *	interface specific flags in this range.
 */
#define	DB_CREATE	      0x0000001	/* Create file as necessary. */
#define	DB_DURABLE_UNKNOWN    0x0000002 /* Durability on open (internal). */
#define	DB_FORCE	      0x0000004	/* Force (anything). */
#define	DB_MULTIVERSION	      0x0000008 /* Multiversion concurrency control. */
#define	DB_NOMMAP	      0x0000010	/* Don't mmap underlying file. */
#define	DB_RDONLY	      0x0000020	/* Read-only (O_RDONLY). */
#define	DB_RECOVER	      0x0000040	/* Run normal recovery. */
#define	DB_THREAD	      0x0000080	/* Applications are threaded. */
#define	DB_TRUNCATE	      0x0000100	/* Discard existing DB (O_TRUNC). */
#define	DB_TXN_NOSYNC	      0x0000200	/* Do not sync log on commit. */
#define	DB_TXN_NOWAIT	      0x0000400	/* Do not wait for locks. */
#define	DB_TXN_NOT_DURABLE    0x0000800	/* Do not log changes. */
#define	DB_TXN_WRITE_NOSYNC   0x0001000	/* Write the log but don't sync. */
#define	DB_SPARE_FLAG         0x0002000	/* Spare. */

/*
 * Common flags --
 *	Interfaces which use any of these common flags should never have
 *	interface specific flags in this range.
 *
 * DB_AUTO_COMMIT:
 *	DB_ENV->set_flags, DB->open
 *      (Note: until the 4.3 release, legal to DB->associate, DB->del,
 *	DB->put, DB->remove, DB->rename and DB->truncate, and others.)
 * DB_READ_COMMITTED:
 *	DB->cursor, DB->get, DB->join, DBcursor->get, DB_ENV->txn_begin
 * DB_READ_UNCOMMITTED:
 *	DB->cursor, DB->get, DB->join, DB->open, DBcursor->get,
 *	DB_ENV->txn_begin
 * DB_TXN_SNAPSHOT:
 *	DB_ENV->set_flags, DB_ENV->txn_begin, DB->cursor
 *
 * !!!
 * The DB_READ_COMMITTED and DB_READ_UNCOMMITTED bit masks can't be changed
 * without also changing the masks for the flags that can be OR'd into DB
 * access method and cursor operation values.
 */
#define	DB_IGNORE_LEASE	      0x01000000/* Ignore leases. */
#define	DB_AUTO_COMMIT	      0x02000000/* Implied transaction. */

#define	DB_READ_COMMITTED     0x04000000/* Degree 2 isolation. */
#define	DB_DEGREE_2	      0x04000000/*	Historic name. */

#define	DB_READ_UNCOMMITTED   0x08000000/* Degree 1 isolation. */
#define	DB_DIRTY_READ	      0x08000000/*	Historic name. */

#define	DB_TXN_SNAPSHOT	      0x10000000/* Snapshot isolation. */

/*
 * Flags common to db_env_create and db_create.
 */
#define	DB_CXX_NO_EXCEPTIONS  0x0000001	/* C++: return error values. */

/*
 * Flags private to db_env_create.
 *	   Shared flags up to 0x0000001 */
#define	DB_RPCCLIENT	      0x0000002	/* An RPC client environment. */

/*
 * Flags private to db_create.
 *	   Shared flags up to 0x0000001 */
#define	DB_XA_CREATE	      0x0000002	/* Open in an XA environment. */

/*
 * Flags shared by DB_ENV->remove and DB_ENV->open.
 *	   Shared flags up to 0x0002000 */
#define	DB_USE_ENVIRON	      0x0004000	/* Use the environment. */
#define	DB_USE_ENVIRON_ROOT   0x0008000	/* Use the environment if root. */
/*
 * Flags private to DB_ENV->open.
 */
#define	DB_INIT_CDB	      0x0010000	/* Concurrent Access Methods. */
#define	DB_INIT_LOCK	      0x0020000	/* Initialize locking. */
#define	DB_INIT_LOG	      0x0040000	/* Initialize logging. */
#define	DB_INIT_MPOOL	      0x0080000	/* Initialize mpool. */
#define	DB_INIT_REP	      0x0100000	/* Initialize replication. */
#define	DB_INIT_TXN	      0x0200000	/* Initialize transactions. */
#define	DB_LOCKDOWN	      0x0400000	/* Lock memory into physical core. */
#define	DB_PRIVATE	      0x0800000	/* DB_ENV is process local. */
#define	DB_RECOVER_FATAL      0x1000000	/* Run catastrophic recovery. */
#define	DB_REGISTER	      0x2000000	/* Multi-process registry. */
#define	DB_SYSTEM_MEM	      0x4000000	/* Use system-backed memory. */

#define	DB_JOINENV	      0x0	/* Compatibility. */

/*
 * Flags private to DB->open.
 *	   Shared flags up to 0x0002000 */
#define	DB_EXCL		      0x0004000	/* Exclusive open (O_EXCL). */
#define	DB_FCNTL_LOCKING      0x0008000	/* UNDOC: fcntl(2) locking. */
#define	DB_NO_AUTO_COMMIT     0x0010000	/* Override env-wide AUTOCOMMIT. */
#define	DB_RDWRMASTER	      0x0020000	/* UNDOC: allow subdb master open R/W */
#define	DB_WRITEOPEN	      0x0040000	/* UNDOC: open with write lock. */

/*
 * Flags private to DB->associate.
 *	   Shared flags up to 0x0002000 */
#define	DB_IMMUTABLE_KEY      0x0004000	/* Secondary key is immutable. */
/*	      Shared flags at 0x1000000 */

/*
 * Flags private to DB_ENV->txn_begin.
 *	   Shared flags up to 0x0002000 */
#define	DB_TXN_SYNC	      0x0004000	/* Always sync log on commit. */
#define	DB_TXN_WAIT	      0x0008000	/* Always wait for locks in this TXN. */

/*
 * Flags private to DB_ENV->txn_checkpoint.
 *	   Shared flags up to 0x0002000 */
#define	DB_CKP_INTERNAL	      0x0004000	/* Internally generated checkpoint. */

/*
 * Flags private to DB_ENV->set_encrypt.
 */
#define	DB_ENCRYPT_AES	      0x0000001	/* AES, assumes SHA1 checksum */

/*
 * Flags private to DB_ENV->set_flags.
 *	   Shared flags up to 0x00002000 */
#define	DB_CDB_ALLDB	      0x00004000/* Set CDB locking per environment. */
#define	DB_DIRECT_DB	      0x00008000/* Don't buffer databases in the OS. */
#define	DB_DIRECT_LOG	      0x00010000/* Don't buffer log files in the OS. */
#define	DB_DSYNC_DB	      0x00020000/* Set O_DSYNC on the databases. */
#define	DB_DSYNC_LOG	      0x00040000/* Set O_DSYNC on the log. */
#define	DB_LOG_AUTOREMOVE     0x00080000/* Automatically remove log files. */
#define	DB_LOG_INMEMORY       0x00100000/* Store logs in buffers in memory. */
#define	DB_NOLOCKING	      0x00200000/* Set locking/mutex behavior. */
#define	DB_NOPANIC	      0x00400000/* Set panic state per DB_ENV. */
#define	DB_OVERWRITE	      0x00800000/* Overwrite unlinked region files. */
#define	DB_PANIC_ENVIRONMENT  0x01000000/* Set panic state per environment. */
/*	      Shared flags at 0x02000000 */
/*	      Shared flags at 0x04000000 */
/*	      Shared flags at 0x08000000 */
/*	      Shared flags at 0x10000000 */
#define	DB_REGION_INIT	      0x20000000/* Page-fault regions on open. */
#define	DB_TIME_NOTGRANTED    0x40000000/* Return NOTGRANTED on timeout. */
#define	DB_YIELDCPU	      0x80000000/* Yield the CPU (a lot). */

/*
 * Flags private to DB->set_feedback's callback.
 */
#define	DB_UPGRADE	      0x0000001	/* Upgrading. */
#define	DB_VERIFY	      0x0000002	/* Verifying. */

/*
 * Flags private to DB->compact.
 *	   Shared flags up to 0x00002000
 */
#define	DB_FREELIST_ONLY      0x00004000 /* Just sort and truncate. */
#define	DB_FREE_SPACE         0x00008000 /* Free space . */
#define	DB_COMPACT_FLAGS      \
      (DB_FREELIST_ONLY | DB_FREE_SPACE)

/*
 * Flags private to DB_MPOOLFILE->open.
 *	   Shared flags up to 0x0002000 */
#define	DB_DIRECT	      0x0004000	/* Don't buffer the file in the OS. */
#define	DB_EXTENT	      0x0008000	/* internal: dealing with an extent. */
#define	DB_ODDFILESIZE	      0x0010000	/* Truncate file to N * pgsize. */

/*
 * Flags private to DB->set_flags.
 *	   Shared flags up to 0x00002000 */
#define	DB_CHKSUM	      0x00004000 /* Do checksumming */
#define	DB_DUP		      0x00008000 /* Btree, Hash: duplicate keys. */
#define	DB_DUPSORT	      0x00010000 /* Btree, Hash: duplicate keys. */
#define	DB_ENCRYPT	      0x00020000 /* Btree, Hash: duplicate keys. */
#define	DB_INORDER	      0x00040000 /* Queue: strict ordering on consume */
#define	DB_RECNUM	      0x00080000 /* Btree: record numbers. */
#define	DB_RENUMBER	      0x00100000 /* Recno: renumber on insert/delete. */
#define	DB_REVSPLITOFF	      0x00200000 /* Btree: turn off reverse splits. */
#define	DB_SNAPSHOT	      0x00400000 /* Recno: snapshot the input. */

/*
 * Flags private to the DB_ENV->stat_print, DB->stat and DB->stat_print methods.
 */
#define	DB_FAST_STAT	      0x0000001 /* Don't traverse the database. */
#define	DB_STAT_ALL	      0x0000002	/* Print: Everything. */
#define	DB_STAT_CLEAR	      0x0000004	/* Clear stat after returning values. */
#define	DB_STAT_LOCK_CONF     0x0000008	/* Print: Lock conflict matrix. */
#define	DB_STAT_LOCK_LOCKERS  0x0000010	/* Print: Lockers. */
#define	DB_STAT_LOCK_OBJECTS  0x0000020	/* Print: Lock objects. */
#define	DB_STAT_LOCK_PARAMS   0x0000040	/* Print: Lock parameters. */
#define	DB_STAT_MEMP_HASH     0x0000080	/* Print: Mpool hash buckets. */
#define	DB_STAT_NOERROR       0x0000100 /* Internal: continue on error. */
#define	DB_STAT_SUBSYSTEM     0x0000200 /* Print: Subsystems too. */

/*
 * Flags private to DB->join.
 */
#define	DB_JOIN_NOSORT	      0x0000001	/* Don't try to optimize join. */

/*
 * Flags private to DB->verify.
 */
#define	DB_AGGRESSIVE	      0x0000001	/* Salvage whatever could be data.*/
#define	DB_NOORDERCHK	      0x0000002	/* Skip sort order/hashing check. */
#define	DB_ORDERCHKONLY	      0x0000004	/* Only perform the order check. */
#define	DB_PR_PAGE	      0x0000008	/* Show page contents (-da). */
#define	DB_PR_RECOVERYTEST    0x0000010	/* Recovery test (-dr). */
#define	DB_PRINTABLE	      0x0000020	/* Use printable format for salvage. */
#define	DB_SALVAGE	      0x0000040	/* Salvage what looks like data. */
#define	DB_UNREF	      0x0000080	/* Report unreferenced pages. */
/*
 * !!!
 * These must not go over 0x8000, or they will collide with the flags
 * used by __bam_vrfy_subtree.
 */

/*
 * Flags private to DB->rep_set_transport's send callback.
 */
#define	DB_REP_ANYWHERE	      0x0000001	/* Message can be serviced anywhere. */
#define	DB_REP_NOBUFFER	      0x0000002	/* Do not buffer this message. */
#define	DB_REP_PERMANENT      0x0000004	/* Important--app. may want to flush. */
#define	DB_REP_REREQUEST      0x0000008	/* This msg already been requested. */

/*******************************************************
 * Mutexes.
 *******************************************************/
typedef uint32_t	db_mutex_t;

/*
 * Flag arguments for DbEnv.mutex_alloc, DbEnv.is_alive and for the
 * DB_MUTEX structure.
 */
#define	DB_MUTEX_ALLOCATED	0x01	/* Mutex currently allocated. */
#define	DB_MUTEX_LOCKED		0x02	/* Mutex currently locked. */
#define	DB_MUTEX_LOGICAL_LOCK	0x04	/* Mutex backs a database lock. */
#define	DB_MUTEX_PROCESS_ONLY	0x08	/* Mutex private to a process. */
#define	DB_MUTEX_SELF_BLOCK	0x10	/* Must be able to block self. */

struct __db_mutex_stat {
	/* The following fields are maintained in the region's copy. */
	uint32_t st_mutex_align;	/* Mutex alignment */
	uint32_t st_mutex_tas_spins;	/* Mutex test-and-set spins */
	uint32_t st_mutex_cnt;		/* Mutex count */
	uint32_t st_mutex_free;	/* Available mutexes */
	uint32_t st_mutex_inuse;	/* Mutexes in use */
	uint32_t st_mutex_inuse_max;	/* Maximum mutexes ever in use */

	/* The following fields are filled-in from other places. */
#ifndef __TEST_DB_NO_STATISTICS
	uint32_t st_region_wait;	/* Region lock granted after wait. */
	uint32_t st_region_nowait;	/* Region lock granted without wait. */
	roff_t	  st_regsize;		/* Region size. */
#endif
};

/* This is the length of the buffer passed to DB_ENV->thread_id_string() */
#define	DB_THREADID_STRLEN	128

/*******************************************************
 * Locking.
 *******************************************************/
#define	DB_LOCKVERSION	1

#define	DB_FILE_ID_LEN		20	/* Unique file ID length. */

/*
 * Deadlock detector modes; used in the DB_ENV structure to configure the
 * locking subsystem.
 */
#define	DB_LOCK_NORUN		0
#define	DB_LOCK_DEFAULT		1	/* Default policy. */
#define	DB_LOCK_EXPIRE		2	/* Only expire locks, no detection. */
#define	DB_LOCK_MAXLOCKS	3	/* Select locker with max locks. */
#define	DB_LOCK_MAXWRITE	4	/* Select locker with max writelocks. */
#define	DB_LOCK_MINLOCKS	5	/* Select locker with min locks. */
#define	DB_LOCK_MINWRITE	6	/* Select locker with min writelocks. */
#define	DB_LOCK_OLDEST		7	/* Select oldest locker. */
#define	DB_LOCK_RANDOM		8	/* Select random locker. */
#define	DB_LOCK_YOUNGEST	9	/* Select youngest locker. */

/* Flag values for lock_vec(), lock_get(). */
#define	DB_LOCK_ABORT		0x001	/* Internal: Lock during abort. */
#define	DB_LOCK_NOWAIT		0x002	/* Don't wait on unavailable lock. */
#define	DB_LOCK_RECORD		0x004	/* Internal: record lock. */
#define	DB_LOCK_SET_TIMEOUT	0x008	/* Internal: set lock timeout. */
#define	DB_LOCK_SWITCH		0x010	/* Internal: switch existing lock. */
#define	DB_LOCK_UPGRADE		0x020	/* Internal: upgrade existing lock. */

/* Flag values for DbEnv.set_timeout. */
#define	DB_SET_LOCK_TIMEOUT	1	/* Set lock timeout */
#define	DB_SET_TXN_NOW		2	/* Timeout lock now (internal) */
#define	DB_SET_TXN_TIMEOUT	3	/* Set transaction timeout */

/*
 * Simple R/W lock modes and for multi-granularity intention locking.
 *
 * !!!
 * These values are NOT random, as they are used as an index into the lock
 * conflicts arrays, i.e., DB_LOCK_IWRITE must be == 3, and DB_LOCK_IREAD
 * must be == 4.
 */
typedef enum {
	DB_LOCK_NG=0,			/* Not granted. */
	DB_LOCK_READ=1,			/* Shared/read. */
	DB_LOCK_WRITE=2,		/* Exclusive/write. */
	DB_LOCK_WAIT=3,			/* Wait for event */
	DB_LOCK_IWRITE=4,		/* Intent exclusive/write. */
	DB_LOCK_IREAD=5,		/* Intent to share/read. */
	DB_LOCK_IWR=6,			/* Intent to read and write. */
	DB_LOCK_READ_UNCOMMITTED=7,	/* Degree 1 isolation. */
	DB_LOCK_WWRITE=8		/* Was Written. */
} db_lockmode_t;

/*
 * Request types.
 */
typedef enum {
	DB_LOCK_DUMP=0,			/* Display held locks. */
	DB_LOCK_GET=1,			/* Get the lock. */
	DB_LOCK_GET_TIMEOUT=2,		/* Get lock with a timeout. */
	DB_LOCK_INHERIT=3,		/* Pass locks to parent. */
	DB_LOCK_PUT=4,			/* Release the lock. */
	DB_LOCK_PUT_ALL=5,		/* Release locker's locks. */
	DB_LOCK_PUT_OBJ=6,		/* Release locker's locks on obj. */
	DB_LOCK_PUT_READ=7,		/* Release locker's read locks. */
	DB_LOCK_TIMEOUT=8,		/* Force a txn to timeout. */
	DB_LOCK_TRADE=9,		/* Trade locker ids on a lock. */
	DB_LOCK_UPGRADE_WRITE=10	/* Upgrade writes for dirty reads. */
} db_lockop_t;

/*
 * Status of a lock.
 */
typedef enum  {
	DB_LSTAT_ABORTED=1,		/* Lock belongs to an aborted txn. */
	DB_LSTAT_EXPIRED=2,		/* Lock has expired. */
	DB_LSTAT_FREE=3,		/* Lock is unallocated. */
	DB_LSTAT_HELD=4,		/* Lock is currently held. */
	DB_LSTAT_PENDING=5,		/* Lock was waiting and has been
					 * promoted; waiting for the owner
					 * to run and upgrade it to held. */
	DB_LSTAT_WAITING=6		/* Lock is on the wait queue. */
}db_status_t;

/* Lock statistics structure. */
struct __db_lock_stat {
	uint32_t st_id;		/* Last allocated locker ID. */
	uint32_t st_cur_maxid;		/* Current maximum unused ID. */
	uint32_t st_maxlocks;		/* Maximum number of locks in table. */
	uint32_t st_maxlockers;	/* Maximum num of lockers in table. */
	uint32_t st_maxobjects;	/* Maximum num of objects in table. */
	int	  st_nmodes;		/* Number of lock modes. */
	uint32_t st_nlockers;		/* Current number of lockers. */
#ifndef __TEST_DB_NO_STATISTICS
	uint32_t st_nlocks;		/* Current number of locks. */
	uint32_t st_maxnlocks;		/* Maximum number of locks so far. */
	uint32_t st_maxnlockers;	/* Maximum number of lockers so far. */
	uint32_t st_nobjects;		/* Current number of objects. */
	uint32_t st_maxnobjects;	/* Maximum number of objects so far. */
	uint32_t st_nrequests;		/* Number of lock gets. */
	uint32_t st_nreleases;		/* Number of lock puts. */
	uint32_t st_nupgrade;		/* Number of lock upgrades. */
	uint32_t st_ndowngrade;	/* Number of lock downgrades. */
	uint32_t st_lock_wait;		/* Lock conflicts w/ subsequent wait */
	uint32_t st_lock_nowait;	/* Lock conflicts w/o subsequent wait */
	uint32_t st_ndeadlocks;	/* Number of lock deadlocks. */
	db_timeout_t st_locktimeout;	/* Lock timeout. */
	uint32_t st_nlocktimeouts;	/* Number of lock timeouts. */
	db_timeout_t st_txntimeout;	/* Transaction timeout. */
	uint32_t st_ntxntimeouts;	/* Number of transaction timeouts. */
	uint32_t st_objs_wait;		/* Object lock granted after wait. */
	uint32_t st_objs_nowait;	/* Object lock granted without wait. */
	uint32_t st_lockers_wait;	/* Locker lock granted after wait. */
	uint32_t st_lockers_nowait;	/* Locker lock granted without wait. */
	uint32_t st_locks_wait;	/* Lock lock granted after wait. */
	uint32_t st_locks_nowait;	/* Lock lock granted without wait. */
	uint32_t st_region_wait;	/* Region lock granted after wait. */
	uint32_t st_region_nowait;	/* Region lock granted without wait. */
	uint32_t st_hash_len;		/* Max length of bucket. */
	roff_t	  st_regsize;		/* Region size. */
#endif
};

struct __db_lock_hstat {
	uint32_t st_nrequests;		/* Number of lock gets. */
	uint32_t st_nreleases;		/* Number of lock puts. */
	uint32_t st_nupgrade;		/* Number of lock upgrades. */
	uint32_t st_ndowngrade;	/* Number of lock downgrades. */
	uint32_t st_lock_wait;		/* Lock conflicts w/ subsequent wait */
	uint32_t st_lock_nowait;	/* Lock conflicts w/o subsequent wait */
	uint32_t st_nlocktimeouts;	/* Number of lock timeouts. */
	uint32_t st_ntxntimeouts;	/* Number of transaction timeouts. */
	uint32_t st_hash_len;		/* Max length of bucket. */
};

/*
 * DB_LOCK_ILOCK --
 *	Internal DB access method lock.
 */
struct __db_ilock {
	db_pgno_t pgno;			/* Page being locked. */
	uint8_t fileid[DB_FILE_ID_LEN];/* File id. */
#define	DB_HANDLE_LOCK	1
#define	DB_RECORD_LOCK	2
#define	DB_PAGE_LOCK	3
	uint32_t type;			/* Type of lock. */
};

/*
 * DB_LOCK --
 *	The structure is allocated by the caller and filled in during a
 *	lock_get request (or a lock_vec/DB_LOCK_GET).
 */
struct __db_lock_u {
	roff_t		off;		/* Offset of the lock in the region */
	uint32_t	ndx;		/* Index of the object referenced by
					 * this lock; used for locking. */
	uint32_t	gen;		/* Generation number of this lock. */
	db_lockmode_t	mode;		/* mode of this lock. */
};

/* Lock request structure. */
struct __db_lockreq {
	db_lockop_t	 op;		/* Operation. */
	db_lockmode_t	 mode;		/* Requested mode. */
	db_timeout_t	 timeout;	/* Time to expire lock. */
	DBT		*obj;		/* Object being locked. */
	DB_LOCK		 lock;		/* Lock returned. */
};

/*******************************************************
 * Logging.
 *******************************************************/
#define	DB_LOGVERSION	13		/* Current log version. */
#define	DB_LOGOLDVER	8		/* Oldest log version supported. */
#define	DB_LOGMAGIC	0x040988

/* Flag values for DB_ENV->log_archive(). */
#define	DB_ARCH_ABS	0x001		/* Absolute pathnames. */
#define	DB_ARCH_DATA	0x002		/* Data files. */
#define	DB_ARCH_LOG	0x004		/* Log files. */
#define	DB_ARCH_REMOVE	0x008	/* Remove log files. */

/* Flag values for DB_ENV->log_put(). */
#define	DB_FLUSH		0x001	/* Flush data to disk (public). */
#define	DB_LOG_CHKPNT		0x002	/* Flush supports a checkpoint */
#define	DB_LOG_COMMIT		0x004	/* Flush supports a commit */
#define	DB_LOG_NOCOPY		0x008	/* Don't copy data */
#define	DB_LOG_NOT_DURABLE	0x010	/* Do not log; keep in memory */
#define	DB_LOG_WRNOSYNC		0x020	/* Write, don't sync log_put */

/*
 * A DB_LSN has two parts, a fileid which identifies a specific file, and an
 * offset within that file.  The fileid is an unsigned 4-byte quantity that
 * uniquely identifies a file within the log directory -- currently a simple
 * counter inside the log.  The offset is also an unsigned 4-byte value.  The
 * log manager guarantees the offset is never more than 4 bytes by switching
 * to a new log file before the maximum length imposed by an unsigned 4-byte
 * offset is reached.
 */
struct __db_lsn {
	uint32_t	file;		/* File ID. */
	uint32_t	offset;		/* File offset. */
};

/*
 * Application-specified log record types start at DB_user_BEGIN, and must not
 * equal or exceed DB_debug_FLAG.
 *
 * DB_debug_FLAG is the high-bit of the uint32_t that specifies a log record
 * type.  If the flag is set, it's a log record that was logged for debugging
 * purposes only, even if it reflects a database change -- the change was part
 * of a non-durable transaction.
 */
#define	DB_user_BEGIN		10000
#define	DB_debug_FLAG		0x80000000

/*
 * DB_LOGC --
 *	Log cursor.
 */
struct __db_log_cursor {
	DB_ENV	 *dbenv;		/* Enclosing dbenv. */

	DB_FH	 *fhp;			/* File handle. */
	DB_LSN	  lsn;			/* Cursor: LSN */
	uint32_t len;			/* Cursor: record length */
	uint32_t prev;			/* Cursor: previous record's offset */

	DBT	  dbt;			/* Return DBT. */
	DB_LSN    p_lsn;		/* Persist LSN. */
	uint32_t p_version;		/* Persist version. */

	uint8_t *bp;			/* Allocated read buffer. */
	uint32_t bp_size;		/* Read buffer length in bytes. */
	uint32_t bp_rlen;		/* Read buffer valid data length. */
	DB_LSN	  bp_lsn;		/* Read buffer first byte LSN. */

	uint32_t bp_maxrec;		/* Max record length in the log file. */

	/* DB_LOGC PUBLIC HANDLE LIST BEGIN */
	int (*close) __P((DB_LOGC *, uint32_t));
	int (*get) __P((DB_LOGC *, DB_LSN *, DBT *, uint32_t));
	int (*version) __P((DB_LOGC *, uint32_t *, uint32_t));
	/* DB_LOGC PUBLIC HANDLE LIST END */

#define	DB_LOG_DISK		0x01	/* Log record came from disk. */
#define	DB_LOG_LOCKED		0x02	/* Log region already locked */
#define	DB_LOG_SILENT_ERR	0x04	/* Turn-off error messages. */
	uint32_t flags;
};

/* Log statistics structure. */
struct __db_log_stat {
	uint32_t st_magic;		/* Log file magic number. */
	uint32_t st_version;		/* Log file version number. */
	int	  st_mode;		/* Log file permissions mode. */
	uint32_t st_lg_bsize;		/* Log buffer size. */
	uint32_t st_lg_size;		/* Log file size. */
	uint32_t st_wc_bytes;		/* Bytes to log since checkpoint. */
	uint32_t st_wc_mbytes;		/* Megabytes to log since checkpoint. */
#ifndef __TEST_DB_NO_STATISTICS
	uint32_t st_record;		/* Records entered into the log. */
	uint32_t st_w_bytes;		/* Bytes to log. */
	uint32_t st_w_mbytes;		/* Megabytes to log. */
	uint32_t st_wcount;		/* Total I/O writes to the log. */
	uint32_t st_wcount_fill;	/* Overflow writes to the log. */
	uint32_t st_rcount;		/* Total I/O reads from the log. */
	uint32_t st_scount;		/* Total syncs to the log. */
	uint32_t st_region_wait;	/* Region lock granted after wait. */
	uint32_t st_region_nowait;	/* Region lock granted without wait. */
	uint32_t st_cur_file;		/* Current log file number. */
	uint32_t st_cur_offset;	/* Current log file offset. */
	uint32_t st_disk_file;		/* Known on disk log file number. */
	uint32_t st_disk_offset;	/* Known on disk log file offset. */
	uint32_t st_maxcommitperflush;	/* Max number of commits in a flush. */
	uint32_t st_mincommitperflush;	/* Min number of commits in a flush. */
	roff_t	  st_regsize;		/* Region size. */
#endif
};

/*
 * We need to record the first log record of a transaction.  For user
 * defined logging this macro returns the place to put that information,
 * if it is need in rlsnp, otherwise it leaves it unchanged.  We also
 * need to track the last record of the transaction, this returns the
 * place to put that info.
 */
#define	DB_SET_TXN_LSNP(txn, blsnp, llsnp)		\
	((txn)->set_txn_lsnp(txn, blsnp, llsnp))

/*******************************************************
 * Shared buffer cache (mpool).
 *******************************************************/
/* Flag values for DB_MPOOLFILE->get. */
#define	DB_MPOOL_CREATE		0x001	/* Create a page. */
#define	DB_MPOOL_DIRTY		0x002	/* Get page for an update. */
#define	DB_MPOOL_EDIT		0x004	/* Modify without copying. */
#define	DB_MPOOL_FREE		0x008	/* Free page if present. */
#define	DB_MPOOL_LAST		0x010	/* Return the last page. */
#define	DB_MPOOL_NEW		0x020	/* Create a new page. */

/* Undocumented flag value for DB_MPOOLFILE->close. */
#define	DB_MPOOL_DISCARD	0x001	/* Discard file. */

/* Flags values for DB_MPOOLFILE->set_flags. */
#define	DB_MPOOL_NOFILE		0x001	/* Never open a backing file. */
#define	DB_MPOOL_UNLINK		0x002	/* Unlink the file on last close. */

/* Priority values for DB_MPOOLFILE->{put,set_priority}. */
typedef enum {
	DB_PRIORITY_UNCHANGED=0,
	DB_PRIORITY_VERY_LOW=1,
	DB_PRIORITY_LOW=2,
	DB_PRIORITY_DEFAULT=3,
	DB_PRIORITY_HIGH=4,
	DB_PRIORITY_VERY_HIGH=5
} DB_CACHE_PRIORITY;

/* Per-process DB_MPOOLFILE information. */
struct __db_mpoolfile {
	DB_FH	  *fhp;			/* Underlying file handle. */

	/*
	 * !!!
	 * The ref, pinref and q fields are protected by the region lock.
	 */
	uint32_t  ref;			/* Reference count. */

	uint32_t pinref;		/* Pinned block reference count. */

	/*
	 * !!!
	 * Explicit representations of structures from queue.h.
	 * TAILQ_ENTRY(__db_mpoolfile) q;
	 */
	struct {
		struct __db_mpoolfile *tqe_next;
		struct __db_mpoolfile **tqe_prev;
	} q;				/* Linked list of DB_MPOOLFILE's. */

	/*
	 * !!!
	 * The rest of the fields (with the exception of the MP_FLUSH flag)
	 * are not thread-protected, even when they may be modified at any
	 * time by the application.  The reason is the DB_MPOOLFILE handle
	 * is single-threaded from the viewpoint of the application, and so
	 * the only fields needing to be thread-protected are those accessed
	 * by checkpoint or sync threads when using DB_MPOOLFILE structures
	 * to flush buffers from the cache.
	 */
	DB_ENV	       *dbenv;		/* Overlying DB_ENV. */
	MPOOLFILE      *mfp;		/* Underlying MPOOLFILE. */

	uint32_t	clear_len;	/* Cleared length on created pages. */
	uint8_t			/* Unique file ID. */
			fileid[DB_FILE_ID_LEN];
	int		ftype;		/* File type. */
	int32_t		lsn_offset;	/* LSN offset in page. */
	uint32_t	gbytes, bytes;	/* Maximum file size. */
	DBT	       *pgcookie;	/* Byte-string passed to pgin/pgout. */
	int32_t		priority;	/* Cache priority. */

	void	       *addr;		/* Address of mmap'd region. */
	size_t		len;		/* Length of mmap'd region. */

	uint32_t	config_flags;	/* Flags to DB_MPOOLFILE->set_flags. */

	/* DB_MPOOLFILE PUBLIC HANDLE LIST BEGIN */
	int (*close) __P((DB_MPOOLFILE *, uint32_t));
	int (*get)
	    __P((DB_MPOOLFILE *, db_pgno_t *, DB_TXN *, uint32_t, void *));
	int (*get_clear_len) __P((DB_MPOOLFILE *, uint32_t *));
	int (*get_fileid) __P((DB_MPOOLFILE *, uint8_t *));
	int (*get_flags) __P((DB_MPOOLFILE *, uint32_t *));
	int (*get_ftype) __P((DB_MPOOLFILE *, int *));
	int (*get_last_pgno) __P((DB_MPOOLFILE *, db_pgno_t *));
	int (*get_lsn_offset) __P((DB_MPOOLFILE *, int32_t *));
	int (*get_maxsize) __P((DB_MPOOLFILE *, uint32_t *, uint32_t *));
	int (*get_pgcookie) __P((DB_MPOOLFILE *, DBT *));
	int (*get_priority) __P((DB_MPOOLFILE *, DB_CACHE_PRIORITY *));
	int (*open) __P((DB_MPOOLFILE *, const char *, uint32_t, int, size_t));
	int (*put) __P((DB_MPOOLFILE *, void *, DB_CACHE_PRIORITY, uint32_t));
	int (*set_clear_len) __P((DB_MPOOLFILE *, uint32_t));
	int (*set_fileid) __P((DB_MPOOLFILE *, uint8_t *));
	int (*set_flags) __P((DB_MPOOLFILE *, uint32_t, int));
	int (*set_ftype) __P((DB_MPOOLFILE *, int));
	int (*set_lsn_offset) __P((DB_MPOOLFILE *, int32_t));
	int (*set_maxsize) __P((DB_MPOOLFILE *, uint32_t, uint32_t));
	int (*set_pgcookie) __P((DB_MPOOLFILE *, DBT *));
	int (*set_priority) __P((DB_MPOOLFILE *, DB_CACHE_PRIORITY));
	int (*sync) __P((DB_MPOOLFILE *));
	/* DB_MPOOLFILE PUBLIC HANDLE LIST END */

	/*
	 * MP_FILEID_SET, MP_OPEN_CALLED and MP_READONLY do not need to be
	 * thread protected because they are initialized before the file is
	 * linked onto the per-process lists, and never modified.
	 *
	 * MP_FLUSH is thread protected because it is potentially read/set by
	 * multiple threads of control.
	 */
#define	MP_FILEID_SET	0x001		/* Application supplied a file ID. */
#define	MP_FLUSH	0x002		/* Was opened to flush a buffer. */
#define	MP_MULTIVERSION	0x004		/* Opened for multiversion access. */
#define	MP_OPEN_CALLED	0x008		/* File opened. */
#define	MP_READONLY	0x010		/* File is readonly. */
	uint32_t  flags;
};

/* Mpool statistics structure. */
struct __db_mpool_stat {
	uint32_t st_gbytes;		/* Total cache size: GB. */
	uint32_t st_bytes;		/* Total cache size: B. */
	uint32_t st_ncache;		/* Number of cache regions. */
	uint32_t st_max_ncache;	/* Maximum number of regions. */
	size_t	  st_mmapsize;		/* Maximum file size for mmap. */
	int	  st_maxopenfd;		/* Maximum number of open fd's. */
	int	  st_maxwrite;		/* Maximum buffers to write. */
	db_timeout_t st_maxwrite_sleep;	/* Sleep after writing max buffers. */
	uint32_t st_pages;		/* Total number of pages. */
#ifndef __TEST_DB_NO_STATISTICS
	uint32_t st_map;		/* Pages from mapped files. */
	uint32_t st_cache_hit;		/* Pages found in the cache. */
	uint32_t st_cache_miss;	/* Pages not found in the cache. */
	uint32_t st_page_create;	/* Pages created in the cache. */
	uint32_t st_page_in;		/* Pages read in. */
	uint32_t st_page_out;		/* Pages written out. */
	uint32_t st_ro_evict;		/* Clean pages forced from the cache. */
	uint32_t st_rw_evict;		/* Dirty pages forced from the cache. */
	uint32_t st_page_trickle;	/* Pages written by memp_trickle. */
	uint32_t st_page_clean;	/* Clean pages. */
	uint32_t st_page_dirty;	/* Dirty pages. */
	uint32_t st_hash_buckets;	/* Number of hash buckets. */
	uint32_t st_hash_searches;	/* Total hash chain searches. */
	uint32_t st_hash_longest;	/* Longest hash chain searched. */
	uint32_t st_hash_examined;	/* Total hash entries searched. */
	uint32_t st_hash_nowait;	/* Hash lock granted with nowait. */
	uint32_t st_hash_wait;		/* Hash lock granted after wait. */
	uint32_t st_hash_max_nowait;	/* Max hash lock granted with nowait. */
	uint32_t st_hash_max_wait;	/* Max hash lock granted after wait. */
	uint32_t st_region_nowait;	/* Region lock granted with nowait. */
	uint32_t st_region_wait;	/* Region lock granted after wait. */
	uint32_t st_mvcc_frozen;		/* Buffers frozen. */
	uint32_t st_mvcc_thawed;		/* Buffers thawed. */
	uint32_t st_mvcc_freed;		/* Frozen buffers freed. */
	uint32_t st_alloc;		/* Number of page allocations. */
	uint32_t st_alloc_buckets;	/* Buckets checked during allocation. */
	uint32_t st_alloc_max_buckets;	/* Max checked during allocation. */
	uint32_t st_alloc_pages;	/* Pages checked during allocation. */
	uint32_t st_alloc_max_pages;	/* Max checked during allocation. */
	uint32_t st_io_wait;		/* Thread waited on buffer I/O. */
	roff_t	  st_regsize;		/* Region size. */
#endif
};

/* Mpool file statistics structure. */
struct __db_mpool_fstat {
	char *file_name;		/* File name. */
	uint32_t st_pagesize;		/* Page size. */
#ifndef __TEST_DB_NO_STATISTICS
	uint32_t st_map;		/* Pages from mapped files. */
	uint32_t st_cache_hit;		/* Pages found in the cache. */
	uint32_t st_cache_miss;	/* Pages not found in the cache. */
	uint32_t st_page_create;	/* Pages created in the cache. */
	uint32_t st_page_in;		/* Pages read in. */
	uint32_t st_page_out;		/* Pages written out. */
#endif
};

/*******************************************************
 * Transactions and recovery.
 *******************************************************/
#define	DB_TXNVERSION	1

typedef enum {
	DB_TXN_ABORT=0,			/* Public. */
	DB_TXN_APPLY=1,			/* Public. */
	DB_TXN_BACKWARD_ALLOC=2,	/* Internal. */
	DB_TXN_BACKWARD_ROLL=3,		/* Public. */
	DB_TXN_FORWARD_ROLL=4,		/* Public. */
	DB_TXN_OPENFILES=5,		/* Internal. */
	DB_TXN_POPENFILES=6,		/* Internal. */
	DB_TXN_PRINT=7			/* Public. */
} db_recops;

/*
 * BACKWARD_ALLOC is used during the forward pass to pick up any aborted
 * allocations for files that were created during the forward pass.
 * The main difference between _ALLOC and _ROLL is that the entry for
 * the file not exist during the rollforward pass.
 */
#define	DB_UNDO(op)	((op) == DB_TXN_ABORT ||			\
		(op) == DB_TXN_BACKWARD_ROLL || (op) == DB_TXN_BACKWARD_ALLOC)
#define	DB_REDO(op)	((op) == DB_TXN_FORWARD_ROLL || (op) == DB_TXN_APPLY)

struct __db_txn {
	DB_TXNMGR	*mgrp;		/* Pointer to transaction manager. */
	DB_TXN		*parent;	/* Pointer to transaction's parent. */

	uint32_t	txnid;		/* Unique transaction id. */
	char		*name;		/* Transaction name. */
	DB_LOCKER	*locker;	/* Locker for this txn. */

	db_threadid_t	tid;		/* Thread id for use in MT XA. */
	void		*td;		/* Detail structure within region. */
	db_timeout_t	lock_timeout;	/* Timeout for locks for this txn. */
	db_timeout_t	expire;		/* Time transaction expires. */
	void		*txn_list;	/* Undo information for parent. */

	/*
	 * !!!
	 * Explicit representations of structures from queue.h.
	 * TAILQ_ENTRY(__db_txn) links;
	 * TAILQ_ENTRY(__db_txn) xalinks;
	 */
	struct {
		struct __db_txn *tqe_next;
		struct __db_txn **tqe_prev;
	} links;			/* Links transactions off manager. */
	struct {
		struct __db_txn *tqe_next;
		struct __db_txn **tqe_prev;
	} xalinks;			/* Links active XA transactions. */

	/*
	 * !!!
	 * Explicit representations of structures from queue.h.
	 * TAILQ_HEAD(__kids, __db_txn) kids;
	 */
	struct __kids {
		struct __db_txn *tqh_first;
		struct __db_txn **tqh_last;
	} kids;

	/*
	 * !!!
	 * Explicit representations of structures from queue.h.
	 * TAILQ_HEAD(__events, __txn_event) events;
	 */
	struct {
		struct __txn_event *tqh_first;
		struct __txn_event **tqh_last;
	} events;			/* Links deferred events. */

	/*
	 * !!!
	 * Explicit representations of structures from queue.h.
	 * STAILQ_HEAD(__logrec, __txn_logrec) logs;
	 */
	struct {
		struct __txn_logrec *stqh_first;
		struct __txn_logrec **stqh_last;
	} logs;				/* Links in memory log records. */

	/*
	 * !!!
	 * Explicit representations of structures from queue.h.
	 * TAILQ_ENTRY(__db_txn) klinks;
	 */
	struct {
		struct __db_txn *tqe_next;
		struct __db_txn **tqe_prev;
	} klinks;

	void	*api_internal;		/* C++ API private. */
	void	*xml_internal;		/* XML API private. */

	uint32_t	cursors;	/* Number of cursors open for txn */

	/* DB_TXN PUBLIC HANDLE LIST BEGIN */
	int	  (*abort) __P((DB_TXN *));
	int	  (*commit) __P((DB_TXN *, uint32_t));
	int	  (*discard) __P((DB_TXN *, uint32_t));
	int	  (*get_name) __P((DB_TXN *, const char **));
	uint32_t (*id) __P((DB_TXN *));
	int	  (*prepare) __P((DB_TXN *, uint8_t *));
	int	  (*set_name) __P((DB_TXN *, const char *));
	int	  (*set_timeout) __P((DB_TXN *, db_timeout_t, uint32_t));
	/* DB_TXN PUBLIC HANDLE LIST END */

	/* DB_TXN PRIVATE HANDLE LIST BEGIN */
	void	  (*set_txn_lsnp) __P((DB_TXN *txn, DB_LSN **, DB_LSN **));
	/* DB_TXN PRIVATE HANDLE LIST END */

#define	TXN_CHILDCOMMIT		0x0001	/* Txn has committed. */
#define	TXN_CDSGROUP		0x0002	/* CDS group handle. */
#define	TXN_COMPENSATE		0x0004	/* Compensating transaction. */
#define	TXN_DEADLOCK		0x0008	/* Txn has deadlocked. */
#define	TXN_LOCKTIMEOUT		0x0010	/* Txn has a lock timeout. */
#define	TXN_MALLOC		0x0020	/* Structure allocated by TXN system. */
#define	TXN_NOSYNC		0x0040	/* Do not sync on prepare and commit. */
#define	TXN_NOWAIT		0x0080	/* Do not wait on locks. */
#define	TXN_PRIVATE		0x0100	/* Txn owned by cursor.. */
#define	TXN_READ_COMMITTED	0x0200	/* Txn has degree 2 isolation. */
#define	TXN_READ_UNCOMMITTED	0x0400	/* Txn has degree 1 isolation. */
#define	TXN_RESTORED		0x0800	/* Txn has been restored. */
#define	TXN_SNAPSHOT		0x1000	/* Snapshot Isolation. */
#define	TXN_SYNC		0x2000	/* Write and sync on prepare/commit. */
#define	TXN_WRITE_NOSYNC	0x4000	/* Write only on prepare/commit. */
	uint32_t	flags;
};

#define	TXN_SYNC_FLAGS (TXN_SYNC | TXN_NOSYNC | TXN_WRITE_NOSYNC)

/*
 * Structure used for two phase commit interface.  Berkeley DB support for two
 * phase commit is compatible with the X/Open XA interface.
 *
 * The XA #define XIDDATASIZE defines the size of a global transaction ID.  We
 * have our own version here (for name space reasons) which must have the same
 * value.
 */
#define	DB_XIDDATASIZE	128
struct __db_preplist {
	DB_TXN	*txn;
	uint8_t gid[DB_XIDDATASIZE];
};

/* Transaction statistics structure. */
struct __db_txn_active {
	uint32_t txnid;		/* Transaction ID */
	uint32_t parentid;		/* Transaction ID of parent */
	pid_t     pid;			/* Process owning txn ID */
	db_threadid_t tid;		/* Thread owning txn ID */

	DB_LSN	  lsn;			/* LSN when transaction began */

	DB_LSN	  read_lsn;		/* Read LSN for MVCC */
	uint32_t mvcc_ref;		/* MVCC reference count */

#define	TXN_ABORTED		1
#define	TXN_COMMITTED		2
#define	TXN_PREPARED		3
#define	TXN_RUNNING		4
	uint32_t status;		/* Status of the transaction */

#define	TXN_XA_ABORTED		1
#define	TXN_XA_DEADLOCKED	2
#define	TXN_XA_ENDED		3
#define	TXN_XA_PREPARED		4
#define	TXN_XA_STARTED		5
#define	TXN_XA_SUSPENDED	6
	uint32_t xa_status;		/* XA status */

	uint8_t  xid[DB_XIDDATASIZE];	/* Global transaction ID */
	char	  name[51];		/* 50 bytes of name, nul termination */
};

struct __db_txn_stat {
	uint32_t st_nrestores;		/* number of restored transactions
					   after recovery. */
#ifndef __TEST_DB_NO_STATISTICS
	DB_LSN	  st_last_ckp;		/* lsn of the last checkpoint */
	time_t	  st_time_ckp;		/* time of last checkpoint */
	uint32_t st_last_txnid;	/* last transaction id given out */
	uint32_t st_maxtxns;		/* maximum txns possible */
	uint32_t st_naborts;		/* number of aborted transactions */
	uint32_t st_nbegins;		/* number of begun transactions */
	uint32_t st_ncommits;		/* number of committed transactions */
	uint32_t st_nactive;		/* number of active transactions */
	uint32_t st_nsnapshot;		/* number of snapshot transactions */
	uint32_t st_maxnactive;	/* maximum active transactions */
	uint32_t st_maxnsnapshot;	/* maximum snapshot transactions */
	DB_TXN_ACTIVE *st_txnarray;	/* array of active transactions */
	uint32_t st_region_wait;	/* Region lock granted after wait. */
	uint32_t st_region_nowait;	/* Region lock granted without wait. */
	roff_t	  st_regsize;		/* Region size. */
#endif
};

/*******************************************************
 * Replication.
 *******************************************************/
/* Special, out-of-band environment IDs. */
#define	DB_EID_BROADCAST	-1
#define	DB_EID_INVALID		-2

/* rep_config flag values. */
#define	DB_REP_CONF_BULK	0x0001	/* Bulk transfer. */
#define	DB_REP_CONF_DELAYCLIENT	0x0002	/* Delay client synchronization. */
#define	DB_REP_CONF_NOAUTOINIT	0x0004	/* No automatic client init. */
#define	DB_REP_CONF_NOWAIT	0x0008	/* Don't wait, return error. */

/*
 * Operation code values for rep_start and/or repmgr_start.  Just one of the
 * following values should be passed in the flags parameter.  (If we ever need
 * additional, independent bit flags for these methods, we can start allocating
 * them from the high-order byte of the flags word, as we currently do elsewhere
 * for DB_AFTER through DB_WRITELOCK and DB_AUTO_COMMIT, etc.)
 */
#define	DB_REP_CLIENT			1
#define	DB_REP_ELECTION			2
#define	DB_REP_MASTER			3

#define	DB_REPFLAGS_MASK	0x000000ff	/* Mask for rep modes. */

#define	DB_REP_DEFAULT_PRIORITY		100

/* Acknowledgement policies. */
#define	DB_REPMGR_ACKS_ALL		1
#define	DB_REPMGR_ACKS_ALL_PEERS	2
#define	DB_REPMGR_ACKS_NONE		3
#define	DB_REPMGR_ACKS_ONE		4
#define	DB_REPMGR_ACKS_ONE_PEER		5
#define	DB_REPMGR_ACKS_QUORUM		6

/* Replication timeout configuration values. */
#define	DB_REP_ACK_TIMEOUT		1	/* RepMgr acknowledgements. */
#define	DB_REP_CHECKPOINT_DELAY		2	/* RepMgr acknowledgements. */
#define	DB_REP_CONNECTION_RETRY		3	/* RepMgr connections. */
#define	DB_REP_ELECTION_RETRY		4	/* RepMgr elect retries. */
#define	DB_REP_ELECTION_TIMEOUT		5	/* Rep normal elections. */
#define	DB_REP_FULL_ELECTION_TIMEOUT	6	/* Rep full elections. */
#define	DB_REP_LEASE_TIMEOUT		7	/* Master leases. */

/* Event notification types. */
#define	DB_EVENT_NO_SUCH_EVENT		0 /* out-of-band sentinel value */
#define	DB_EVENT_PANIC			1
#define	DB_EVENT_REP_CLIENT		2
#define	DB_EVENT_REP_ELECTED		3
#define	DB_EVENT_REP_MASTER		4
#define	DB_EVENT_REP_NEWMASTER		5
#define	DB_EVENT_REP_PERM_FAILED	6
#define	DB_EVENT_REP_STARTUPDONE	7
#define	DB_EVENT_WRITE_FAILED		8

/* Flag value for repmgr_add_remote_site. */
#define	DB_REPMGR_PEER          0x01

/* Replication Manager site status. */
struct __db_repmgr_site {
	int eid;
	char *host;
	u_int port;

#define	DB_REPMGR_CONNECTED	0x01
#define	DB_REPMGR_DISCONNECTED	0x02
	uint32_t status;
};

/* Replication statistics. */
struct __db_rep_stat {
	/* !!!
	 * Many replication statistics fields cannot be protected by a mutex
	 * without an unacceptable performance penalty, since most message
	 * processing is done without the need to hold a region-wide lock.
	 * Fields whose comments end with a '+' may be updated without holding
	 * the replication or log mutexes (as appropriate), and thus may be
	 * off somewhat (or, on unreasonable architectures under unlucky
	 * circumstances, garbaged).
	 */
	uint32_t st_log_queued;	/* Log records currently queued.+ */
	uint32_t st_startup_complete;	/* Site completed client sync-up. */
#ifndef __TEST_DB_NO_STATISTICS
	uint32_t st_status;		/* Current replication status. */
	DB_LSN st_next_lsn;		/* Next LSN to use or expect. */
	DB_LSN st_waiting_lsn;		/* LSN we're awaiting, if any. */
	db_pgno_t st_next_pg;		/* Next pg we expect. */
	db_pgno_t st_waiting_pg;	/* pg we're awaiting, if any. */

	uint32_t st_dupmasters;	/* # of times a duplicate master
					   condition was detected.+ */
	int st_env_id;			/* Current environment ID. */
	int st_env_priority;		/* Current environment priority. */
	uint32_t st_bulk_fills;	/* Bulk buffer fills. */
	uint32_t st_bulk_overflows;	/* Bulk buffer overflows. */
	uint32_t st_bulk_records;	/* Bulk records stored. */
	uint32_t st_bulk_transfers;	/* Transfers of bulk buffers. */
	uint32_t st_client_rerequests;	/* Number of forced rerequests. */
	uint32_t st_client_svc_req;	/* Number of client service requests
					   received by this client. */
	uint32_t st_client_svc_miss;	/* Number of client service requests
					   missing on this client. */
	uint32_t st_gen;		/* Current generation number. */
	uint32_t st_egen;		/* Current election gen number. */
	uint32_t st_log_duplicated;	/* Log records received multiply.+ */
	uint32_t st_log_queued_max;	/* Max. log records queued at once.+ */
	uint32_t st_log_queued_total;	/* Total # of log recs. ever queued.+ */
	uint32_t st_log_records;	/* Log records received and put.+ */
	uint32_t st_log_requested;	/* Log recs. missed and requested.+ */
	int st_master;			/* Env. ID of the current master. */
	uint32_t st_master_changes;	/* # of times we've switched masters. */
	uint32_t st_msgs_badgen;	/* Messages with a bad generation #.+ */
	uint32_t st_msgs_processed;	/* Messages received and processed.+ */
	uint32_t st_msgs_recover;	/* Messages ignored because this site
					   was a client in recovery.+ */
	uint32_t st_msgs_send_failures;/* # of failed message sends.+ */
	uint32_t st_msgs_sent;		/* # of successful message sends.+ */
	uint32_t st_newsites;		/* # of NEWSITE msgs. received.+ */
	int st_nsites;			/* Current number of sites we will
					   assume during elections. */
	uint32_t st_nthrottles;	/* # of times we were throttled. */
	uint32_t st_outdated;		/* # of times we detected and returned
					   an OUTDATED condition.+ */
	uint32_t st_pg_duplicated;	/* Pages received multiply.+ */
	uint32_t st_pg_records;	/* Pages received and stored.+ */
	uint32_t st_pg_requested;	/* Pages missed and requested.+ */
	uint32_t st_txns_applied;	/* # of transactions applied.+ */
	uint32_t st_startsync_delayed;	/* # of STARTSYNC msgs delayed.+ */

	/* Elections generally. */
	uint32_t st_elections;		/* # of elections held.+ */
	uint32_t st_elections_won;	/* # of elections won by this site.+ */

	/* Statistics about an in-progress election. */
	int st_election_cur_winner;	/* Current front-runner. */
	uint32_t st_election_gen;	/* Election generation number. */
	DB_LSN st_election_lsn;		/* Max. LSN of current winner. */
	int st_election_nsites;		/* # of "registered voters". */
	int st_election_nvotes;		/* # of "registered voters" needed. */
	int st_election_priority;	/* Current election priority. */
	int st_election_status;		/* Current election status. */
	uint32_t st_election_tiebreaker;/* Election tiebreaker value. */
	int st_election_votes;		/* Votes received in this round. */
	uint32_t st_election_sec;	/* Last election time seconds. */
	uint32_t st_election_usec;	/* Last election time useconds. */
#endif
};

/* Replication Manager statistics. */
struct __db_repmgr_stat {
	uint32_t st_perm_failed;	/* # of insufficiently ack'ed msgs. */
	uint32_t st_msgs_queued;	/* # msgs queued for network delay. */
	uint32_t st_msgs_dropped;	/* # msgs discarded due to excessive
					   queue length. */
	uint32_t st_connection_drop;	/* Existing connections dropped. */
	uint32_t st_connect_fail;	/* Failed new connection attempts. */
};

/*******************************************************
 * Sequences.
 *******************************************************/
/*
 * The storage record for a sequence.
 */
struct __db_seq_record {
	uint32_t	seq_version;	/* Version size/number. */
#define	DB_SEQ_DEC		0x00000001	/* Decrement sequence. */
#define	DB_SEQ_INC		0x00000002	/* Increment sequence. */
#define	DB_SEQ_RANGE_SET	0x00000004	/* Range set (internal). */
#define	DB_SEQ_WRAP		0x00000008	/* Wrap sequence at min/max. */
#define	DB_SEQ_WRAPPED		0x00000010	/* Just wrapped (internal). */
	uint32_t	flags;		/* Flags. */
	db_seq_t	seq_value;	/* Current value. */
	db_seq_t	seq_max;	/* Max permitted. */
	db_seq_t	seq_min;	/* Min permitted. */
};

/*
 * Handle for a sequence object.
 */
struct __db_sequence {
	DB		*seq_dbp;	/* DB handle for this sequence. */
	db_mutex_t	mtx_seq;	/* Mutex if sequence is threaded. */
	DB_SEQ_RECORD	*seq_rp;	/* Pointer to current data. */
	DB_SEQ_RECORD	seq_record;	/* Data from DB_SEQUENCE. */
	int32_t		seq_cache_size; /* Number of values cached. */
	db_seq_t	seq_last_value;	/* Last value cached. */
	DBT		seq_key;	/* DBT pointing to sequence key. */
	DBT		seq_data;	/* DBT pointing to seq_record. */

	/* API-private structure: used by C++ and Java. */
	void		*api_internal;

	/* DB_SEQUENCE PUBLIC HANDLE LIST BEGIN */
	int		(*close) __P((DB_SEQUENCE *, uint32_t));
	int		(*get) __P((DB_SEQUENCE *,
			      DB_TXN *, int32_t, db_seq_t *, uint32_t));
	int		(*get_cachesize) __P((DB_SEQUENCE *, int32_t *));
	int		(*get_db) __P((DB_SEQUENCE *, DB **));
	int		(*get_flags) __P((DB_SEQUENCE *, uint32_t *));
	int		(*get_key) __P((DB_SEQUENCE *, DBT *));
	int		(*get_range) __P((DB_SEQUENCE *,
			     db_seq_t *, db_seq_t *));
	int		(*initial_value) __P((DB_SEQUENCE *, db_seq_t));
	int		(*open) __P((DB_SEQUENCE *,
			    DB_TXN *, DBT *, uint32_t));
	int		(*remove) __P((DB_SEQUENCE *, DB_TXN *, uint32_t));
	int		(*set_cachesize) __P((DB_SEQUENCE *, int32_t));
	int		(*set_flags) __P((DB_SEQUENCE *, uint32_t));
	int		(*set_range) __P((DB_SEQUENCE *, db_seq_t, db_seq_t));
	int		(*stat) __P((DB_SEQUENCE *,
			    DB_SEQUENCE_STAT **, uint32_t));
	int		(*stat_print) __P((DB_SEQUENCE *, uint32_t));
	/* DB_SEQUENCE PUBLIC HANDLE LIST END */
};

struct __db_seq_stat {
	uint32_t st_wait;		/* Sequence lock granted w/o wait. */
	uint32_t st_nowait;		/* Sequence lock granted after wait. */
	db_seq_t  st_current;		/* Current value in db. */
	db_seq_t  st_value;		/* Current cached value. */
	db_seq_t  st_last_value;	/* Last cached value. */
	db_seq_t  st_min;		/* Minimum value. */
	db_seq_t  st_max;		/* Maximum value. */
	int32_t   st_cache_size;	/* Cache size. */
	uint32_t st_flags;		/* Flag value. */
};

/*******************************************************
 * Access methods.
 *******************************************************/
typedef enum {
	DB_BTREE=1,
	DB_HASH=2,
	DB_RECNO=3,
	DB_QUEUE=4,
	DB_UNKNOWN=5			/* Figure it out on open. */
} DBTYPE;

#define	DB_RENAMEMAGIC	0x030800	/* File has been renamed. */

#define	DB_BTREEVERSION	9		/* Current btree version. */
#define	DB_BTREEOLDVER	8		/* Oldest btree version supported. */
#define	DB_BTREEMAGIC	0x053162

#define	DB_HASHVERSION	9		/* Current hash version. */
#define	DB_HASHOLDVER	7		/* Oldest hash version supported. */
#define	DB_HASHMAGIC	0x061561

#define	DB_QAMVERSION	4		/* Current queue version. */
#define	DB_QAMOLDVER	3		/* Oldest queue version supported. */
#define	DB_QAMMAGIC	0x042253

#define	DB_SEQUENCE_VERSION 2		/* Current sequence version. */
#define	DB_SEQUENCE_OLDVER  1		/* Oldest sequence version supported. */

/*
 * DB access method and cursor operation values.  Each value is an operation
 * code to which additional bit flags are added.
 */
#define	DB_AFTER		 1	/* Dbc.put */
#define	DB_APPEND		 2	/* Db.put */
#define	DB_BEFORE		 3	/* Dbc.put */
#define	DB_CONSUME		 4	/* Db.get */
#define	DB_CONSUME_WAIT		 5	/* Db.get */
#define	DB_CURRENT		 6	/* Dbc.get, Dbc.put, DbLogc.get */
#define	DB_FIRST		 7	/* Dbc.get, DbLogc->get */
#define	DB_GET_BOTH		 8	/* Db.get, Dbc.get */
#define	DB_GET_BOTHC		 9	/* Dbc.get (internal) */
#define	DB_GET_BOTH_RANGE	10	/* Db.get, Dbc.get */
#define	DB_GET_RECNO		11	/* Dbc.get */
#define	DB_JOIN_ITEM		12	/* Dbc.get; don't do primary lookup */
#define	DB_KEYFIRST		13	/* Dbc.put */
#define	DB_KEYLAST		14	/* Dbc.put */
#define	DB_LAST			15	/* Dbc.get, DbLogc->get */
#define	DB_NEXT			16	/* Dbc.get, DbLogc->get */
#define	DB_NEXT_DUP		17	/* Dbc.get */
#define	DB_NEXT_NODUP		18	/* Dbc.get */
#define	DB_NODUPDATA		19	/* Db.put, Dbc.put */
#define	DB_NOOVERWRITE		20	/* Db.put */
#define	DB_NOSYNC		21	/* Db.close */
#define	DB_POSITION		22	/* Dbc.dup */
#define	DB_PREV			23	/* Dbc.get, DbLogc->get */
#define	DB_PREV_DUP		24	/* Dbc.get */
#define	DB_PREV_NODUP		25	/* Dbc.get */
#define	DB_SET			26	/* Dbc.get, DbLogc->get */
#define	DB_SET_RANGE		27	/* Dbc.get */
#define	DB_SET_RECNO		28	/* Db.get, Dbc.get */
#define	DB_UPDATE_SECONDARY	29	/* Dbc.get, Dbc.del (internal) */
#define	DB_WRITECURSOR		30	/* Db.cursor */
#define	DB_WRITELOCK		31	/* Db.cursor (internal) */

/* This has to change when the max opcode hits 255. */
#define	DB_OPFLAGS_MASK	0x000000ff	/* Mask for operations flags. */

/*
 * Masks for flags that can be OR'd into DB access method and cursor
 * operation values.  Three top bits have already been taken:
 *
 * DB_AUTO_COMMIT	0x02000000
 * DB_READ_COMMITTED	0x04000000
 * DB_READ_UNCOMMITTED	0x08000000
 */
#define	DB_MULTIPLE	0x10000000	/* Return multiple data values. */
#define	DB_MULTIPLE_KEY	0x20000000	/* Return multiple data/key pairs. */
#define	DB_RMW		0x40000000	/* Acquire write lock immediately. */

/*
 * DB (user visible) error return codes.
 *
 * !!!
 * We don't want our error returns to conflict with other packages where
 * possible, so pick a base error value that's hopefully not common.  We
 * document that we own the error name space from -30,800 to -30,999.
 */
/* DB (public) error return codes. */
#define	DB_BUFFER_SMALL		(-30999)/* User memory too small for return. */
#define	DB_DONOTINDEX		(-30998)/* "Null" return from 2ndary callbk. */
#define	DB_KEYEMPTY		(-30997)/* Key/data deleted or never created. */
#define	DB_KEYEXIST		(-30996)/* The key/data pair already exists. */
#define	DB_LOCK_DEADLOCK	(-30995)/* Deadlock. */
#define	DB_LOCK_NOTGRANTED	(-30994)/* Lock unavailable. */
#define	DB_LOG_BUFFER_FULL	(-30993)/* In-memory log buffer full. */
#define	DB_NOSERVER		(-30992)/* Server panic return. */
#define	DB_NOSERVER_HOME	(-30991)/* Bad home sent to server. */
#define	DB_NOSERVER_ID		(-30990)/* Bad ID sent to server. */
#define	DB_NOTFOUND		(-30989)/* Key/data pair not found (EOF). */
#define	DB_OLD_VERSION		(-30988)/* Out-of-date version. */
#define	DB_PAGE_NOTFOUND	(-30987)/* Requested page not found. */
#define	DB_REP_DUPMASTER	(-30986)/* There are two masters. */
#define	DB_REP_HANDLE_DEAD	(-30985)/* Rolled back a commit. */
#define	DB_REP_HOLDELECTION	(-30984)/* Time to hold an election. */
#define	DB_REP_IGNORE		(-30983)/* This msg should be ignored.*/
#define	DB_REP_ISPERM		(-30982)/* Cached not written perm written.*/
#define	DB_REP_JOIN_FAILURE	(-30981)/* Unable to join replication group. */
#define	DB_REP_LEASE_EXPIRED	(-30980)/* Master lease has expired. */
#define	DB_REP_LOCKOUT		(-30979)/* API/Replication lockout now. */
#define	DB_REP_NEWSITE		(-30978)/* New site entered system. */
#define	DB_REP_NOTPERM		(-30977)/* Permanent log record not written. */
#define	DB_REP_UNAVAIL		(-30976)/* Site cannot currently be reached. */
#define	DB_RUNRECOVERY		(-30975)/* Panic return. */
#define	DB_SECONDARY_BAD	(-30974)/* Secondary index corrupt. */
#define	DB_VERIFY_BAD		(-30973)/* Verify failed; bad format. */
#define	DB_VERSION_MISMATCH	(-30972)/* Environment version mismatch. */

/* DB (private) error return codes. */
#define	DB_ALREADY_ABORTED	(-30899)
#define	DB_DELETED		(-30898)/* Recovery file marked deleted. */
#define	DB_EVENT_NOT_HANDLED	(-30897)/* Forward event to application. */
#define	DB_NEEDSPLIT		(-30896)/* Page needs to be split. */
#define	DB_REP_BULKOVF		(-30895)/* Rep bulk buffer overflow. */
#define	DB_REP_EGENCHG		(-30894)/* Egen changed while in election. */
#define	DB_REP_LOGREADY		(-30893)/* Rep log ready for recovery. */
#define	DB_REP_NEWMASTER	(-30892)/* We have learned of a new master. */
#define	DB_REP_PAGEDONE		(-30891)/* This page was already done. */
#define	DB_SURPRISE_KID		(-30890)/* Child commit where parent
					   didn't know it was a parent. */
#define	DB_SWAPBYTES		(-30889)/* Database needs byte swapping. */
#define	DB_TIMEOUT		(-30888)/* Timed out waiting for election. */
#define	DB_TXN_CKP		(-30887)/* Encountered ckp record in log. */
#define	DB_VERIFY_FATAL		(-30886)/* DB->verify cannot proceed. */

/* Database handle. */
struct __db {
	/*******************************************************
	 * Public: owned by the application.
	 *******************************************************/
	uint32_t pgsize;		/* Database logical page size. */
	DB_CACHE_PRIORITY priority;	/* Database priority in cache. */

					/* Callbacks. */
	int (*db_append_recno) __P((DB *, DBT *, db_recno_t));
	void (*db_feedback) __P((DB *, int, int));
	int (*dup_compare) __P((DB *, const DBT *, const DBT *));

	void	*app_private;		/* Application-private handle. */

	/*******************************************************
	 * Private: owned by DB.
	 *******************************************************/
	DB_ENV	*dbenv;			/* Backing environment. */

	DBTYPE	 type;			/* DB access method type. */

	DB_MPOOLFILE *mpf;		/* Backing buffer pool. */

	db_mutex_t mutex;		/* Synchronization for free threading */

	char *fname, *dname;		/* File/database passed to DB->open. */
	uint32_t open_flags;		/* Flags passed to DB->open. */

	uint8_t fileid[DB_FILE_ID_LEN];/* File's unique ID for locking. */

	uint32_t adj_fileid;		/* File's unique ID for curs. adj. */

#define	DB_LOGFILEID_INVALID	-1
	FNAME *log_filename;		/* File's naming info for logging. */

	db_pgno_t meta_pgno;		/* Meta page number */
	DB_LOCKER *locker;		/* Locker for handle locking. */
	DB_LOCKER *cur_locker;		/* Current handle lock holder. */
	DB_TXN *cur_txn;		/* Opening transaction. */
	DB_LOCKER *associate_locker;	/* Locker for DB->associate call. */
	DB_LOCK	 handle_lock;		/* Lock held on this handle. */

	u_int	 cl_id;			/* RPC: remote client id. */

	time_t	 timestamp;		/* Handle timestamp for replication. */
	uint32_t fid_gen;		/* Rep generation number for fids. */

	/*
	 * Returned data memory for DB->get() and friends.
	 */
	DBT	 my_rskey;		/* Secondary key. */
	DBT	 my_rkey;		/* [Primary] key. */
	DBT	 my_rdata;		/* Data. */

	/*
	 * !!!
	 * Some applications use DB but implement their own locking outside of
	 * DB.  If they're using fcntl(2) locking on the underlying database
	 * file, and we open and close a file descriptor for that file, we will
	 * discard their locks.  The DB_FCNTL_LOCKING flag to DB->open is an
	 * undocumented interface to support this usage which leaves any file
	 * descriptors we open until DB->close.  This will only work with the
	 * DB->open interface and simple caches, e.g., creating a transaction
	 * thread may open/close file descriptors this flag doesn't protect.
	 * Locking with fcntl(2) on a file that you don't own is a very, very
	 * unsafe thing to do.  'Nuff said.
	 */
	DB_FH	*saved_open_fhp;	/* Saved file handle. */

	/*
	 * Linked list of DBP's, linked from the DB_ENV, used to keep track
	 * of all open db handles for cursor adjustment.
	 *
	 * !!!
	 * Explicit representations of structures from queue.h.
	 * TAILQ_ENTRY(__db) dblistlinks;
	 */
	struct {
		struct __db *tqe_next;
		struct __db **tqe_prev;
	} dblistlinks;

	/*
	 * Cursor queues.
	 *
	 * !!!
	 * Explicit representations of structures from queue.h.
	 * TAILQ_HEAD(__cq_fq, __dbc) free_queue;
	 * TAILQ_HEAD(__cq_aq, __dbc) active_queue;
	 * TAILQ_HEAD(__cq_jq, __dbc) join_queue;
	 */
	struct __cq_fq {
		struct __dbc *tqh_first;
		struct __dbc **tqh_last;
	} free_queue;
	struct __cq_aq {
		struct __dbc *tqh_first;
		struct __dbc **tqh_last;
	} active_queue;
	struct __cq_jq {
		struct __dbc *tqh_first;
		struct __dbc **tqh_last;
	} join_queue;

	/*
	 * Secondary index support.
	 *
	 * Linked list of secondary indices -- set in the primary.
	 *
	 * !!!
	 * Explicit representations of structures from queue.h.
	 * LIST_HEAD(s_secondaries, __db);
	 */
	struct {
		struct __db *lh_first;
	} s_secondaries;

	/*
	 * List entries for secondaries, and reference count of how many
	 * threads are updating this secondary (see Dbc.put).
	 *
	 * !!!
	 * Note that these are synchronized by the primary's mutex, but
	 * filled in in the secondaries.
	 *
	 * !!!
	 * Explicit representations of structures from queue.h.
	 * LIST_ENTRY(__db) s_links;
	 */
	struct {
		struct __db *le_next;
		struct __db **le_prev;
	} s_links;
	uint32_t s_refcnt;

	/* Secondary callback and free functions -- set in the secondary. */
	int	(*s_callback) __P((DB *, const DBT *, const DBT *, DBT *));

	/* Reference to primary -- set in the secondary. */
	DB	*s_primary;

#define	DB_ASSOC_IMMUTABLE_KEY    0x00000001 /* Secondary key is immutable. */

	/* Flags passed to associate -- set in the secondary. */
	uint32_t s_assoc_flags;

	/* API-private structure: used by DB 1.85, C++, Java, Perl and Tcl */
	void	*api_internal;

	/* Subsystem-private structure. */
	void	*bt_internal;		/* Btree/Recno access method. */
	void	*h_internal;		/* Hash access method. */
	void	*q_internal;		/* Queue access method. */
	void	*xa_internal;		/* XA. */

	/* DB PUBLIC HANDLE LIST BEGIN */
	int  (*associate) __P((DB *, DB_TXN *, DB *,
		int (*)(DB *, const DBT *, const DBT *, DBT *), uint32_t));
	int  (*close) __P((DB *, uint32_t));
	int  (*compact) __P((DB *,
		DB_TXN *, DBT *, DBT *, DB_COMPACT *, uint32_t, DBT *));
	int  (*cursor) __P((DB *, DB_TXN *, DBC **, uint32_t));
	int  (*del) __P((DB *, DB_TXN *, DBT *, uint32_t));
	void (*err) __P((DB *, int, const char *, ...));
	void (*errx) __P((DB *, const char *, ...));
	int  (*exists) __P((DB *, DB_TXN *, DBT *, uint32_t));
	int  (*fd) __P((DB *, int *));
	int  (*get) __P((DB *, DB_TXN *, DBT *, DBT *, uint32_t));
	int  (*get_bt_minkey) __P((DB *, uint32_t *));
	int  (*get_byteswapped) __P((DB *, int *));
	int  (*get_cachesize) __P((DB *, uint32_t *, uint32_t *, int *));
	int  (*get_dbname) __P((DB *, const char **, const char **));
	int  (*get_encrypt_flags) __P((DB *, uint32_t *));
	DB_ENV *(*get_env) __P((DB *));
	void (*get_errfile) __P((DB *, FILE **));
	void (*get_errpfx) __P((DB *, const char **));
	int  (*get_flags) __P((DB *, uint32_t *));
	int  (*get_h_ffactor) __P((DB *, uint32_t *));
	int  (*get_h_nelem) __P((DB *, uint32_t *));
	int  (*get_lorder) __P((DB *, int *));
	DB_MPOOLFILE *(*get_mpf) __P((DB *));
	void (*get_msgfile) __P((DB *, FILE **));
	int  (*get_multiple) __P((DB *));
	int  (*get_open_flags) __P((DB *, uint32_t *));
	int  (*get_pagesize) __P((DB *, uint32_t *));
	int  (*get_priority) __P((DB *, DB_CACHE_PRIORITY *));
	int  (*get_q_extentsize) __P((DB *, uint32_t *));
	int  (*get_re_delim) __P((DB *, int *));
	int  (*get_re_len) __P((DB *, uint32_t *));
	int  (*get_re_pad) __P((DB *, int *));
	int  (*get_re_source) __P((DB *, const char **));
	int  (*get_transactional) __P((DB *));
	int  (*get_type) __P((DB *, DBTYPE *));
	int  (*join) __P((DB *, DBC **, DBC **, uint32_t));
	int  (*key_range)
		__P((DB *, DB_TXN *, DBT *, DB_KEY_RANGE *, uint32_t));
	int  (*open) __P((DB *,
		DB_TXN *, const char *, const char *, DBTYPE, uint32_t, int));
	int  (*pget) __P((DB *, DB_TXN *, DBT *, DBT *, DBT *, uint32_t));
	int  (*put) __P((DB *, DB_TXN *, DBT *, DBT *, uint32_t));
	int  (*remove) __P((DB *, const char *, const char *, uint32_t));
	int  (*rename) __P((DB *,
		const char *, const char *, const char *, uint32_t));
	int  (*set_alloc) __P((DB *, void *(*)(size_t),
		void *(*)(void *, size_t), void (*)(void *)));
	int  (*set_append_recno) __P((DB *, int (*)(DB *, DBT *, db_recno_t)));
	int  (*set_bt_compare)
		__P((DB *, int (*)(DB *, const DBT *, const DBT *)));
	int  (*set_bt_minkey) __P((DB *, uint32_t));
	int  (*set_bt_prefix)
		__P((DB *, size_t (*)(DB *, const DBT *, const DBT *)));
	int  (*set_cachesize) __P((DB *, uint32_t, uint32_t, int));
	int  (*set_dup_compare)
		__P((DB *, int (*)(DB *, const DBT *, const DBT *)));
	int  (*set_encrypt) __P((DB *, const char *, uint32_t));
	void (*set_errcall) __P((DB *,
		void (*)(const DB_ENV *, const char *, const char *)));
	void (*set_errfile) __P((DB *, FILE *));
	void (*set_errpfx) __P((DB *, const char *));
	int  (*set_feedback) __P((DB *, void (*)(DB *, int, int)));
	int  (*set_flags) __P((DB *, uint32_t));
	int  (*set_h_compare)
		__P((DB *, int (*)(DB *, const DBT *, const DBT *)));
	int  (*set_h_ffactor) __P((DB *, uint32_t));
	int  (*set_h_hash)
		__P((DB *, uint32_t (*)(DB *, const void *, uint32_t)));
	int  (*set_h_nelem) __P((DB *, uint32_t));
	int  (*set_lorder) __P((DB *, int));
	void (*set_msgcall) __P((DB *, void (*)(const DB_ENV *, const char *)));
	void (*set_msgfile) __P((DB *, FILE *));
	int  (*set_pagesize) __P((DB *, uint32_t));
	int  (*set_paniccall) __P((DB *, void (*)(DB_ENV *, int)));
	int  (*set_priority) __P((DB *, DB_CACHE_PRIORITY));
	int  (*set_q_extentsize) __P((DB *, uint32_t));
	int  (*set_re_delim) __P((DB *, int));
	int  (*set_re_len) __P((DB *, uint32_t));
	int  (*set_re_pad) __P((DB *, int));
	int  (*set_re_source) __P((DB *, const char *));
	int  (*stat) __P((DB *, DB_TXN *, void *, uint32_t));
	int  (*stat_print) __P((DB *, uint32_t));
	int  (*sync) __P((DB *, uint32_t));
	int  (*truncate) __P((DB *, DB_TXN *, uint32_t *, uint32_t));
	int  (*upgrade) __P((DB *, const char *, uint32_t));
	int  (*verify)
		__P((DB *, const char *, const char *, FILE *, uint32_t));
	/* DB PUBLIC HANDLE LIST END */

	/* DB PRIVATE HANDLE LIST BEGIN */
	int  (*dump) __P((DB *, const char *,
		int (*)(void *, const void *), void *, int, int));
	int  (*db_am_remove) __P((DB *, DB_TXN *, const char *, const char *));
	int  (*db_am_rename) __P((DB *, DB_TXN *,
	    const char *, const char *, const char *));
	/* DB PRIVATE HANDLE LIST END */

	/*
	 * Never called; these are a place to save function pointers
	 * so that we can undo an associate.
	 */
	int  (*stored_get) __P((DB *, DB_TXN *, DBT *, DBT *, uint32_t));
	int  (*stored_close) __P((DB *, uint32_t));

#define	DB_OK_BTREE	0x01
#define	DB_OK_HASH	0x02
#define	DB_OK_QUEUE	0x04
#define	DB_OK_RECNO	0x08
	uint32_t	am_ok;		/* Legal AM choices. */

	/*
	 * This field really ought to be an AM_FLAG, but we have
	 * have run out of bits.  If/when we decide to split up
	 * the flags, we can incorporate it.
	 */
	int	 preserve_fid;		/* Do not free fileid on close. */

#define	DB_AM_CHKSUM		0x00000001 /* Checksumming */
#define	DB_AM_COMPENSATE	0x00000002 /* Created by compensating txn */
#define	DB_AM_CREATED		0x00000004 /* Database was created upon open */
#define	DB_AM_CREATED_MSTR	0x00000008 /* Encompassing file was created */
#define	DB_AM_DBM_ERROR		0x00000010 /* Error in DBM/NDBM database */
#define	DB_AM_DELIMITER		0x00000020 /* Variable length delimiter set */
#define	DB_AM_DISCARD		0x00000040 /* Discard any cached pages */
#define	DB_AM_DUP		0x00000080 /* DB_DUP */
#define	DB_AM_DUPSORT		0x00000100 /* DB_DUPSORT */
#define	DB_AM_ENCRYPT		0x00000200 /* Encryption */
#define	DB_AM_FIXEDLEN		0x00000400 /* Fixed-length records */
#define	DB_AM_INMEM		0x00000800 /* In-memory; no sync on close */
#define	DB_AM_INORDER		0x00001000 /* DB_INORDER */
#define	DB_AM_IN_RENAME		0x00002000 /* File is being renamed */
#define	DB_AM_NOT_DURABLE	0x00004000 /* Do not log changes */
#define	DB_AM_OPEN_CALLED	0x00008000 /* DB->open called */
#define	DB_AM_PAD		0x00010000 /* Fixed-length record pad */
#define	DB_AM_PGDEF		0x00020000 /* Page size was defaulted */
#define	DB_AM_RDONLY		0x00040000 /* Database is readonly */
#define	DB_AM_READ_UNCOMMITTED	0x00080000 /* Support degree 1 isolation */
#define	DB_AM_RECNUM		0x00100000 /* DB_RECNUM */
#define	DB_AM_RECOVER		0x00200000 /* DB opened by recovery routine */
#define	DB_AM_RENUMBER		0x00400000 /* DB_RENUMBER */
#define	DB_AM_REVSPLITOFF	0x00800000 /* DB_REVSPLITOFF */
#define	DB_AM_SECONDARY		0x01000000 /* Database is a secondary index */
#define	DB_AM_SNAPSHOT		0x02000000 /* DB_SNAPSHOT */
#define	DB_AM_SUBDB		0x04000000 /* Subdatabases supported */
#define	DB_AM_SWAP		0x08000000 /* Pages need to be byte-swapped */
#define	DB_AM_TXN		0x10000000 /* Opened in a transaction */
#define	DB_AM_VERIFYING		0x20000000 /* DB handle is in the verifier */
	uint32_t orig_flags;		   /* Flags at  open, for refresh */
	uint32_t flags;
};

/*
 * Macros for bulk get.  These are only intended for the C API.
 * For C++, use DbMultiple*Iterator.
 */
#define	DB_MULTIPLE_INIT(pointer, dbt)					\
	(pointer = (uint8_t *)(dbt)->data +				\
	    (dbt)->ulen - sizeof(uint32_t))
#define	DB_MULTIPLE_NEXT(pointer, dbt, retdata, retdlen)		\
	do {								\
		if (*((uint32_t *)(pointer)) == (uint32_t)-1) {	\
			retdata = NULL;					\
			pointer = NULL;					\
			break;						\
		}							\
		retdata = (uint8_t *)					\
		    (dbt)->data + *(uint32_t *)(pointer);		\
		(pointer) = (uint32_t *)(pointer) - 1;			\
		retdlen = *(uint32_t *)(pointer);			\
		(pointer) = (uint32_t *)(pointer) - 1;			\
		if (retdlen == 0 &&					\
		    retdata == (uint8_t *)(dbt)->data)			\
			retdata = NULL;					\
	} while (0)
#define	DB_MULTIPLE_KEY_NEXT(pointer, dbt, retkey, retklen, retdata, retdlen) \
	do {								\
		if (*((uint32_t *)(pointer)) == (uint32_t)-1) {	\
			retdata = NULL;					\
			retkey = NULL;					\
			pointer = NULL;					\
			break;						\
		}							\
		retkey = (uint8_t *)					\
		    (dbt)->data + *(uint32_t *)(pointer);		\
		(pointer) = (uint32_t *)(pointer) - 1;			\
		retklen = *(uint32_t *)(pointer);			\
		(pointer) = (uint32_t *)(pointer) - 1;			\
		retdata = (uint8_t *)					\
		    (dbt)->data + *(uint32_t *)(pointer);		\
		(pointer) = (uint32_t *)(pointer) - 1;			\
		retdlen = *(uint32_t *)(pointer);			\
		(pointer) = (uint32_t *)(pointer) - 1;			\
	} while (0)

#define	DB_MULTIPLE_RECNO_NEXT(pointer, dbt, recno, retdata, retdlen)   \
	do {								\
		if (*((uint32_t *)(pointer)) == (uint32_t)0) {	\
			recno = 0;					\
			retdata = NULL;					\
			pointer = NULL;					\
			break;						\
		}							\
		recno = *(uint32_t *)(pointer);			\
		(pointer) = (uint32_t *)(pointer) - 1;			\
		retdata = (uint8_t *)					\
		    (dbt)->data + *(uint32_t *)(pointer);		\
		(pointer) = (uint32_t *)(pointer) - 1;			\
		retdlen = *(uint32_t *)(pointer);			\
		(pointer) = (uint32_t *)(pointer) - 1;			\
	} while (0)

/*******************************************************
 * Access method cursors.
 *******************************************************/
struct __dbc {
	DB *dbp;			/* Related DB access method. */
	DB_TXN	 *txn;			/* Associated transaction. */
	DB_CACHE_PRIORITY priority;	/* Priority in cache. */

	/*
	 * Active/free cursor queues.
	 *
	 * !!!
	 * Explicit representations of structures from queue.h.
	 * TAILQ_ENTRY(__dbc) links;
	 */
	struct {
		DBC *tqe_next;
		DBC **tqe_prev;
	} links;

	/*
	 * The DBT *'s below are used by the cursor routines to return
	 * data to the user when DBT flags indicate that DB should manage
	 * the returned memory.  They point at a DBT containing the buffer
	 * and length that will be used, and "belonging" to the handle that
	 * should "own" this memory.  This may be a "my_*" field of this
	 * cursor--the default--or it may be the corresponding field of
	 * another cursor, a DB handle, a join cursor, etc.  In general, it
	 * will be whatever handle the user originally used for the current
	 * DB interface call.
	 */
	DBT	 *rskey;		/* Returned secondary key. */
	DBT	 *rkey;			/* Returned [primary] key. */
	DBT	 *rdata;		/* Returned data. */

	DBT	  my_rskey;		/* Space for returned secondary key. */
	DBT	  my_rkey;		/* Space for returned [primary] key. */
	DBT	  my_rdata;		/* Space for returned data. */

	void	 *lref;			/* Reference to default locker. */
	DB_LOCKER *locker;		/* Locker for this operation. */
	DBT	  lock_dbt;		/* DBT referencing lock. */
	DB_LOCK_ILOCK lock;		/* Object to be locked. */
	DB_LOCK	  mylock;		/* CDB lock held on this cursor. */

	u_int	  cl_id;		/* Remote client id. */

	DBTYPE	  dbtype;		/* Cursor type. */

	DBC_INTERNAL *internal;		/* Access method private. */

	/* DBC PUBLIC HANDLE LIST BEGIN */
	int (*close) __P((DBC *));
	int (*count) __P((DBC *, db_recno_t *, uint32_t));
	int (*del) __P((DBC *, uint32_t));
	int (*dup) __P((DBC *, DBC **, uint32_t));
	int (*get) __P((DBC *, DBT *, DBT *, uint32_t));
	int (*get_priority) __P((DBC *, DB_CACHE_PRIORITY *));
	int (*pget) __P((DBC *, DBT *, DBT *, DBT *, uint32_t));
	int (*put) __P((DBC *, DBT *, DBT *, uint32_t));
	int (*set_priority) __P((DBC *, DB_CACHE_PRIORITY));
	/* DBC PUBLIC HANDLE LIST END */

	/* The following are the method names deprecated in the 4.6 release. */
	int (*c_close) __P((DBC *));
	int (*c_count) __P((DBC *, db_recno_t *, uint32_t));
	int (*c_del) __P((DBC *, uint32_t));
	int (*c_dup) __P((DBC *, DBC **, uint32_t));
	int (*c_get) __P((DBC *, DBT *, DBT *, uint32_t));
	int (*c_pget) __P((DBC *, DBT *, DBT *, DBT *, uint32_t));
	int (*c_put) __P((DBC *, DBT *, DBT *, uint32_t));

	/* DBC PRIVATE HANDLE LIST BEGIN */
	int (*am_bulk) __P((DBC *, DBT *, uint32_t));
	int (*am_close) __P((DBC *, db_pgno_t, int *));
	int (*am_del) __P((DBC *));
	int (*am_destroy) __P((DBC *));
	int (*am_get) __P((DBC *, DBT *, DBT *, uint32_t, db_pgno_t *));
	int (*am_put) __P((DBC *, DBT *, DBT *, uint32_t, db_pgno_t *));
	int (*am_writelock) __P((DBC *));
	/* DBC PRIVATE HANDLE LIST END */

/*
 * DBC_DONTLOCK and DBC_RECOVER are used during recovery and transaction
 * abort.  If a transaction is being aborted or recovered then DBC_RECOVER
 * will be set and locking and logging will be disabled on this cursor.  If
 * we are performing a compensating transaction (e.g. free page processing)
 * then DB_DONTLOCK will be set to inhibit locking, but logging will still
 * be required. DB_DONTLOCK is also used if the whole database is locked.
 */
#define	DBC_ACTIVE		0x0001	/* Cursor in use. */
#define	DBC_DONTLOCK		0x0002	/* Don't lock on this cursor. */
#define	DBC_MULTIPLE		0x0004	/* Return Multiple data. */
#define	DBC_MULTIPLE_KEY	0x0008	/* Return Multiple keys and data. */
#define	DBC_OPD			0x0010	/* Cursor references off-page dups. */
#define	DBC_OWN_LID		0x0020	/* Free lock id on destroy. */
#define	DBC_READ_COMMITTED	0x0040	/* Cursor has degree 2 isolation. */
#define	DBC_READ_UNCOMMITTED	0x0080	/* Cursor has degree 1 isolation. */
#define	DBC_RECOVER		0x0100	/* Recovery cursor; don't log/lock. */
#define	DBC_RMW			0x0200	/* Acquire write flag in read op. */
#define	DBC_TRANSIENT		0x0400	/* Cursor is transient. */
#define	DBC_WRITECURSOR		0x0800	/* Cursor may be used to write (CDB). */
#define	DBC_WRITER		0x1000	/* Cursor immediately writing (CDB). */
	uint32_t flags;
};

/* Key range statistics structure */
struct __key_range {
	double less;
	double equal;
	double greater;
};

/* Btree/Recno statistics structure. */
struct __db_bt_stat {
	uint32_t bt_magic;		/* Magic number. */
	uint32_t bt_version;		/* Version number. */
	uint32_t bt_metaflags;		/* Metadata flags. */
	uint32_t bt_nkeys;		/* Number of unique keys. */
	uint32_t bt_ndata;		/* Number of data items. */
	uint32_t bt_pagecnt;		/* Page count. */
	uint32_t bt_pagesize;		/* Page size. */
	uint32_t bt_minkey;		/* Minkey value. */
	uint32_t bt_re_len;		/* Fixed-length record length. */
	uint32_t bt_re_pad;		/* Fixed-length record pad. */
	uint32_t bt_levels;		/* Tree levels. */
	uint32_t bt_int_pg;		/* Internal pages. */
	uint32_t bt_leaf_pg;		/* Leaf pages. */
	uint32_t bt_dup_pg;		/* Duplicate pages. */
	uint32_t bt_over_pg;		/* Overflow pages. */
	uint32_t bt_empty_pg;		/* Empty pages. */
	uint32_t bt_free;		/* Pages on the free list. */
	uint32_t bt_int_pgfree;	/* Bytes free in internal pages. */
	uint32_t bt_leaf_pgfree;	/* Bytes free in leaf pages. */
	uint32_t bt_dup_pgfree;	/* Bytes free in duplicate pages. */
	uint32_t bt_over_pgfree;	/* Bytes free in overflow pages. */
};

struct __db_compact {
	/* Input Parameters. */
	uint32_t	compact_fillpercent;	/* Desired fillfactor: 1-100 */
	db_timeout_t	compact_timeout;	/* Lock timeout. */
	uint32_t	compact_pages;		/* Max pages to process. */
	/* Output Stats. */
	uint32_t	compact_pages_free;	/* Number of pages freed. */
	uint32_t	compact_pages_examine;	/* Number of pages examine. */
	uint32_t	compact_levels;		/* Number of levels removed. */
	uint32_t	compact_deadlock;	/* Number of deadlocks. */
	db_pgno_t	compact_pages_truncated; /* Pages truncated to OS. */
	/* Internal. */
	db_pgno_t	compact_truncate;	/* Page number for truncation */
};

/* Hash statistics structure. */
struct __db_h_stat {
	uint32_t hash_magic;		/* Magic number. */
	uint32_t hash_version;		/* Version number. */
	uint32_t hash_metaflags;	/* Metadata flags. */
	uint32_t hash_nkeys;		/* Number of unique keys. */
	uint32_t hash_ndata;		/* Number of data items. */
	uint32_t hash_pagecnt;		/* Page count. */
	uint32_t hash_pagesize;	/* Page size. */
	uint32_t hash_ffactor;		/* Fill factor specified at create. */
	uint32_t hash_buckets;		/* Number of hash buckets. */
	uint32_t hash_free;		/* Pages on the free list. */
	uint32_t hash_bfree;		/* Bytes free on bucket pages. */
	uint32_t hash_bigpages;	/* Number of big key/data pages. */
	uint32_t hash_big_bfree;	/* Bytes free on big item pages. */
	uint32_t hash_overflows;	/* Number of overflow pages. */
	uint32_t hash_ovfl_free;	/* Bytes free on ovfl pages. */
	uint32_t hash_dup;		/* Number of dup pages. */
	uint32_t hash_dup_free;	/* Bytes free on duplicate pages. */
};

/* Queue statistics structure. */
struct __db_qam_stat {
	uint32_t qs_magic;		/* Magic number. */
	uint32_t qs_version;		/* Version number. */
	uint32_t qs_metaflags;		/* Metadata flags. */
	uint32_t qs_nkeys;		/* Number of unique keys. */
	uint32_t qs_ndata;		/* Number of data items. */
	uint32_t qs_pagesize;		/* Page size. */
	uint32_t qs_extentsize;	/* Pages per extent. */
	uint32_t qs_pages;		/* Data pages. */
	uint32_t qs_re_len;		/* Fixed-length record length. */
	uint32_t qs_re_pad;		/* Fixed-length record pad. */
	uint32_t qs_pgfree;		/* Bytes free in data pages. */
	uint32_t qs_first_recno;	/* First not deleted record. */
	uint32_t qs_cur_recno;		/* Next available record number. */
};

/*******************************************************
 * Environment.
 *******************************************************/
#define	DB_REGION_MAGIC	0x120897	/* Environment magic number. */

/* Database Environment handle. */
struct __db_env {
	/*******************************************************
	 * Public: owned by the application.
	 *******************************************************/
					/* Error message callback. */
	void (*db_errcall) __P((const DB_ENV *, const char *, const char *));
	FILE		*db_errfile;	/* Error message file stream. */
	const char	*db_errpfx;	/* Error message prefix. */

	FILE		*db_msgfile;	/* Statistics message file stream. */
					/* Statistics message callback. */
	void (*db_msgcall) __P((const DB_ENV *, const char *));

					/* Other Callbacks. */
	void (*db_feedback) __P((DB_ENV *, int, int));
	void (*db_paniccall) __P((DB_ENV *, int));
	void (*db_event_func) __P((DB_ENV *, uint32_t, void *));

					/* App-specified alloc functions. */
	void *(*db_malloc) __P((size_t));
	void *(*db_realloc) __P((void *, size_t));
	void (*db_free) __P((void *));

	/* Application callback to copy data to/from a custom data source. */
#define	DB_USERCOPY_GETDATA	0x0001
#define	DB_USERCOPY_SETDATA	0x0002
	int (*dbt_usercopy)
	    __P((DBT *, uint32_t, void *, uint32_t, uint32_t));

	/*
	 * Currently, the verbose list is a bit field with room for 32
	 * entries.  There's no reason that it needs to be limited, if
	 * there are ever more than 32 entries, convert to a bit array.
	 */
#define	DB_VERB_DEADLOCK	0x0001	/* Deadlock detection information. */
#define	DB_VERB_FILEOPS		0x0002	/* Major file operations. */
#define	DB_VERB_FILEOPS_ALL	0x0004	/* All file operations. */
#define	DB_VERB_RECOVERY	0x0008	/* Recovery information. */
#define	DB_VERB_REGISTER	0x0010	/* Dump waits-for table. */
#define	DB_VERB_REPLICATION	0x0020	/* Replication information. */
#define	DB_VERB_WAITSFOR	0x0040	/* Dump waits-for table. */
	uint32_t	 verbose;	/* Verbose output. */

	void		*app_private;	/* Application-private handle. */

	int (*app_dispatch)		/* User-specified recovery dispatch. */
	    __P((DB_ENV *, DBT *, DB_LSN *, db_recops));

	/* Mutexes. */
	uint32_t	mutex_align;	/* Mutex alignment */
	uint32_t	mutex_cnt;	/* Number of mutexes to configure */
	uint32_t	mutex_inc;	/* Number of mutexes to add */
	uint32_t	mutex_tas_spins;/* Test-and-set spin count */

	struct {
		int	  alloc_id;	/* Allocation ID argument */
		uint32_t flags;	/* Flags argument */
	} *mutex_iq;			/* Initial mutexes queue */
	u_int		mutex_iq_next;	/* Count of initial mutexes */
	u_int		mutex_iq_max;	/* Maximum initial mutexes */

	/* Locking. */
	uint8_t	*lk_conflicts;	/* Two dimensional conflict matrix. */
	int		 lk_modes;	/* Number of lock modes in table. */
	uint32_t	 lk_max;	/* Maximum number of locks. */
	uint32_t	 lk_max_lockers;/* Maximum number of lockers. */
	uint32_t	 lk_max_objects;/* Maximum number of locked objects. */
	uint32_t	 lk_detect;	/* Deadlock detect on all conflicts. */
	db_timeout_t	 lk_timeout;	/* Lock timeout period. */

	/* Logging. */
	uint32_t	 lg_bsize;	/* Buffer size. */
	uint32_t	 lg_size;	/* Log file size. */
	uint32_t	 lg_regionmax;	/* Region size. */
	int		 lg_filemode;	/* Log file permission mode. */

	/* Memory pool. */
	u_int		 mp_ncache;	/* Initial number of cache regions. */
	uint32_t	 mp_gbytes;	/* Cache size: GB. */
	uint32_t	 mp_bytes;	/* Cache size: bytes. */
	uint32_t	 mp_max_gbytes;	/* Maximum cache size: GB. */
	uint32_t	 mp_max_bytes;	/* Maximum cache size: bytes. */
	size_t		 mp_mmapsize;	/* Maximum file size for mmap. */
	int		 mp_maxopenfd;	/* Maximum open file descriptors. */
	int		 mp_maxwrite;	/* Maximum buffers to write. */
	db_timeout_t mp_maxwrite_sleep;	/* Sleep after writing max buffers. */

	/* Transactions. */
	uint32_t	 tx_max;	/* Maximum number of transactions. */
	time_t		 tx_timestamp;	/* Recover to specific timestamp. */
	db_timeout_t	 tx_timeout;	/* Timeout for transactions. */

	/* Thread tracking. */
	uint32_t	thr_nbucket;	/* Number of hash buckets. */
	uint32_t	thr_max;	/* Max before garbage collection. */
	void		*thr_hashtab;	/* Hash table of DB_THREAD_INFO. */

	/*******************************************************
	 * Private: owned by DB.
	 *******************************************************/
	db_mutex_t mtx_env;		/* General DbEnv structure mutex. */

	pid_t		pid_cache;	/* Cached process ID. */

					/* User files, paths. */
	char		*db_home;	/* Database home. */
	char		*db_log_dir;	/* Database log file directory. */
	char		*db_tmp_dir;	/* Database tmp file directory. */

	char	       **db_data_dir;	/* Database data file directories. */
	int		 data_cnt;	/* Database data file slots. */
	int		 data_next;	/* Next Database data file slot. */

	int		 db_mode;	/* Default open permissions. */
	int		 dir_mode;	/* Intermediate directory perms. */
	void		*env_lref;	/* Locker in non-threaded handles. */
	uint32_t	 open_flags;	/* Flags passed to DB_ENV->open. */

	void		*reginfo;	/* REGINFO structure reference. */
	DB_FH		*lockfhp;	/* fcntl(2) locking file handle. */

	DB_FH		*registry;	/* DB_REGISTER file handle. */
	uint32_t	registry_off;	/*
					 * Offset of our slot.  We can't use
					 * off_t because its size depends on
					 * build settings.
					 */

					/* Return IDs. */
	void	       (*thread_id) __P((DB_ENV *, pid_t *, db_threadid_t *));
					/* Return if IDs alive. */
	int	       (*is_alive)
			__P((DB_ENV *, pid_t, db_threadid_t, uint32_t));
					/* Format IDs into a string. */
	char	       *(*thread_id_string)
			__P((DB_ENV *, pid_t, db_threadid_t, char *));

	int	      (**recover_dtab)	/* Dispatch table for recover funcs. */
			    __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
	size_t		 recover_dtab_size;
					/* Slots in the dispatch table. */

	void		*cl_handle;	/* RPC: remote client handle. */
	u_int		 cl_id;		/* RPC: remote client env id. */

	int		 db_ref;	/* DB reference count. */

	long		 shm_key;	/* shmget(2) key. */

	/*
	 * List of open file handles for this DB_ENV.  Must be protected
	 * for multi-threaded support.
	 *
	 * !!!
	 * Explicit representation of structure in queue.h.
	 * TAILQ_HEAD(__fdlist, __fh_t);
	 */
	struct __fdlist {
		struct __fh_t *tqh_first;
		struct __fh_t **tqh_last;
	} fdlist;

	/*
	 * List of open DB handles for this DB_ENV, used for cursor
	 * adjustment.  Must be protected for multi-threaded support.
	 *
	 * !!!
	 * Explicit representation of structure in queue.h.
	 * TAILQ_HEAD(__dblist, __db);
	 */
	db_mutex_t mtx_dblist;		/* Mutex. */
	struct __dblist {
		struct __db *tqh_first;
		struct __db **tqh_last;
	} dblist;

	/*
	 * XA support.
	 *
	 * !!!
	 * Explicit representations of structures from queue.h.
	 * TAILQ_ENTRY(__db_env) links;
	 * TAILQ_HEAD(xa_txn, __db_txn);
	 */
	struct {
		struct __db_env *tqe_next;
		struct __db_env **tqe_prev;
	} links;
	struct __xa_txn {	/* XA Active Transactions. */
		struct __db_txn *tqh_first;
		struct __db_txn **tqh_last;
	} xa_txn;
	int		 xa_rmid;	/* XA Resource Manager ID. */

	char		*passwd;	/* Cryptography support. */
	size_t		 passwd_len;
	void		*crypto_handle;	/* Primary handle. */
	db_mutex_t	 mtx_mt;	/* Mersenne Twister mutex. */
	int		 mti;		/* Mersenne Twister index. */
	u_long		*mt;		/* Mersenne Twister state vector. */

	/* API-private structure. */
	void		*api1_internal;	/* C++, Perl API private */
	void		*api2_internal;	/* Java API private */

	DB_LOCKTAB	*lk_handle;	/* Lock handle. */
	DB_LOG		*lg_handle;	/* Log handle. */
	DB_MPOOL	*mp_handle;	/* Mpool handle. */
	DB_MUTEXMGR	*mutex_handle;	/* Mutex handle. */
	DB_REP		*rep_handle;	/* Replication handle. */
	DB_TXNMGR	*tx_handle;	/* Txn handle. */

	/* DB_ENV PUBLIC HANDLE LIST BEGIN */
	int  (*cdsgroup_begin) __P((DB_ENV *, DB_TXN **));
	int  (*close) __P((DB_ENV *, uint32_t));
	int  (*dbremove) __P((DB_ENV *,
		DB_TXN *, const char *, const char *, uint32_t));
	int  (*dbrename) __P((DB_ENV *,
		DB_TXN *, const char *, const char *, const char *, uint32_t));
	void (*err) __P((const DB_ENV *, int, const char *, ...));
	void (*errx) __P((const DB_ENV *, const char *, ...));
	int  (*failchk) __P((DB_ENV *, uint32_t));
	int  (*fileid_reset) __P((DB_ENV *, const char *, uint32_t));
	int  (*get_cachesize) __P((DB_ENV *, uint32_t *, uint32_t *, int *));
	int  (*get_cache_max) __P((DB_ENV *, uint32_t *, uint32_t *));
	int  (*get_data_dirs) __P((DB_ENV *, const char ***));
	int  (*get_encrypt_flags) __P((DB_ENV *, uint32_t *));
	void (*get_errfile) __P((DB_ENV *, FILE **));
	void (*get_errpfx) __P((DB_ENV *, const char **));
	int  (*get_flags) __P((DB_ENV *, uint32_t *));
	int  (*get_home) __P((DB_ENV *, const char **));
	int  (*get_lg_bsize) __P((DB_ENV *, uint32_t *));
	int  (*get_lg_dir) __P((DB_ENV *, const char **));
	int  (*get_lg_filemode) __P((DB_ENV *, int *));
	int  (*get_lg_max) __P((DB_ENV *, uint32_t *));
	int  (*get_lg_regionmax) __P((DB_ENV *, uint32_t *));
	int  (*get_lk_conflicts) __P((DB_ENV *, const uint8_t **, int *));
	int  (*get_lk_detect) __P((DB_ENV *, uint32_t *));
	int  (*get_lk_max_lockers) __P((DB_ENV *, uint32_t *));
	int  (*get_lk_max_locks) __P((DB_ENV *, uint32_t *));
	int  (*get_lk_max_objects) __P((DB_ENV *, uint32_t *));
	int  (*get_mp_max_openfd) __P((DB_ENV *, int *));
	int  (*get_mp_max_write) __P((DB_ENV *, int *, db_timeout_t *));
	int  (*get_mp_mmapsize) __P((DB_ENV *, size_t *));
	void (*get_msgfile) __P((DB_ENV *, FILE **));
	int  (*get_open_flags) __P((DB_ENV *, uint32_t *));
	int  (*get_shm_key) __P((DB_ENV *, long *));
	int  (*get_thread_count) __P((DB_ENV *, uint32_t *));
	int  (*get_timeout) __P((DB_ENV *, db_timeout_t *, uint32_t));
	int  (*get_tmp_dir) __P((DB_ENV *, const char **));
	int  (*get_tx_max) __P((DB_ENV *, uint32_t *));
	int  (*get_tx_timestamp) __P((DB_ENV *, time_t *));
	int  (*get_verbose) __P((DB_ENV *, uint32_t, int *));
	int  (*is_bigendian) __P((void));
	int  (*lock_detect) __P((DB_ENV *, uint32_t, uint32_t, int *));
	int  (*lock_get) __P((DB_ENV *,
		uint32_t, uint32_t, const DBT *, db_lockmode_t, DB_LOCK *));
	int  (*lock_id) __P((DB_ENV *, uint32_t *));
	int  (*lock_id_free) __P((DB_ENV *, uint32_t));
	int  (*lock_put) __P((DB_ENV *, DB_LOCK *));
	int  (*lock_stat) __P((DB_ENV *, DB_LOCK_STAT **, uint32_t));
	int  (*lock_stat_print) __P((DB_ENV *, uint32_t));
	int  (*lock_vec) __P((DB_ENV *,
		uint32_t, uint32_t, DB_LOCKREQ *, int, DB_LOCKREQ **));
	int  (*log_archive) __P((DB_ENV *, char **[], uint32_t));
	int  (*log_cursor) __P((DB_ENV *, DB_LOGC **, uint32_t));
	int  (*log_file) __P((DB_ENV *, const DB_LSN *, char *, size_t));
	int  (*log_flush) __P((DB_ENV *, const DB_LSN *));
	int  (*log_printf) __P((DB_ENV *, DB_TXN *, const char *, ...));
	int  (*log_put) __P((DB_ENV *, DB_LSN *, const DBT *, uint32_t));
	int  (*log_stat) __P((DB_ENV *, DB_LOG_STAT **, uint32_t));
	int  (*log_stat_print) __P((DB_ENV *, uint32_t));
	int  (*lsn_reset) __P((DB_ENV *, const char *, uint32_t));
	int  (*memp_fcreate) __P((DB_ENV *, DB_MPOOLFILE **, uint32_t));
	int  (*memp_register) __P((DB_ENV *, int, int (*)(DB_ENV *,
		db_pgno_t, void *, DBT *), int (*)(DB_ENV *,
		db_pgno_t, void *, DBT *)));
	int  (*memp_stat) __P((DB_ENV *,
		DB_MPOOL_STAT **, DB_MPOOL_FSTAT ***, uint32_t));
	int  (*memp_stat_print) __P((DB_ENV *, uint32_t));
	int  (*memp_sync) __P((DB_ENV *, DB_LSN *));
	int  (*memp_trickle) __P((DB_ENV *, int, int *));
	int  (*mutex_alloc) __P((DB_ENV *, uint32_t, db_mutex_t *));
	int  (*mutex_free) __P((DB_ENV *, db_mutex_t));
	int  (*mutex_get_align) __P((DB_ENV *, uint32_t *));
	int  (*mutex_get_increment) __P((DB_ENV *, uint32_t *));
	int  (*mutex_get_max) __P((DB_ENV *, uint32_t *));
	int  (*mutex_get_tas_spins) __P((DB_ENV *, uint32_t *));
	int  (*mutex_lock) __P((DB_ENV *, db_mutex_t));
	int  (*mutex_set_align) __P((DB_ENV *, uint32_t));
	int  (*mutex_set_increment) __P((DB_ENV *, uint32_t));
	int  (*mutex_set_max) __P((DB_ENV *, uint32_t));
	int  (*mutex_set_tas_spins) __P((DB_ENV *, uint32_t));
	int  (*mutex_stat) __P((DB_ENV *, DB_MUTEX_STAT **, uint32_t));
	int  (*mutex_stat_print) __P((DB_ENV *, uint32_t));
	int  (*mutex_unlock) __P((DB_ENV *, db_mutex_t));
	int  (*open) __P((DB_ENV *, const char *, uint32_t, int));
	int  (*remove) __P((DB_ENV *, const char *, uint32_t));
	int  (*rep_elect) __P((DB_ENV *, int, int, uint32_t));
	int  (*rep_flush) __P((DB_ENV *));
	int  (*rep_get_config) __P((DB_ENV *, uint32_t, int *));
	int  (*rep_get_limit) __P((DB_ENV *, uint32_t *, uint32_t *));
	int  (*rep_get_nsites) __P((DB_ENV *, int *));
	int  (*rep_get_priority) __P((DB_ENV *, int *));
	int  (*rep_get_timeout) __P((DB_ENV *, int, uint32_t *));
	int  (*rep_process_message)
		__P((DB_ENV *, DBT *, DBT *, int, DB_LSN *));
	int  (*rep_set_config) __P((DB_ENV *, uint32_t, int));
	int  (*rep_set_lease) __P((DB_ENV *, uint32_t, uint32_t));
	int  (*rep_set_limit) __P((DB_ENV *, uint32_t, uint32_t));
	int  (*rep_set_nsites) __P((DB_ENV *, int));
	int  (*rep_set_priority) __P((DB_ENV *, int));
	int  (*rep_set_timeout) __P((DB_ENV *, int, db_timeout_t));
	int  (*rep_set_transport) __P((DB_ENV *, int, int (*)(DB_ENV *,
		const DBT *, const DBT *, const DB_LSN *, int, uint32_t)));
	int  (*rep_start) __P((DB_ENV *, DBT *, uint32_t));
	int  (*rep_stat) __P((DB_ENV *, DB_REP_STAT **, uint32_t));
	int  (*rep_stat_print) __P((DB_ENV *, uint32_t));
	int  (*rep_sync) __P((DB_ENV *, uint32_t));
	int  (*repmgr_add_remote_site) __P((DB_ENV *, const char *, u_int,
		int *, uint32_t));
	int  (*repmgr_get_ack_policy) __P((DB_ENV *, int *));
	int  (*repmgr_set_ack_policy) __P((DB_ENV *, int));
	int  (*repmgr_set_local_site) __P((DB_ENV *, const char *, u_int,
		uint32_t));
	int  (*repmgr_site_list) __P((DB_ENV *, u_int *,
		DB_REPMGR_SITE **));
	int  (*repmgr_start) __P((DB_ENV *, int, uint32_t));
	int  (*repmgr_stat) __P((DB_ENV *, DB_REPMGR_STAT **, uint32_t));
	int  (*repmgr_stat_print) __P((DB_ENV *, uint32_t));
	int  (*set_alloc) __P((DB_ENV *, void *(*)(size_t),
		void *(*)(void *, size_t), void (*)(void *)));
	int  (*set_app_dispatch)
		__P((DB_ENV *, int (*)(DB_ENV *, DBT *, DB_LSN *, db_recops)));
	int  (*set_cachesize) __P((DB_ENV *, uint32_t, uint32_t, int));
	int  (*set_cache_max) __P((DB_ENV *, uint32_t, uint32_t));
	int  (*set_data_dir) __P((DB_ENV *, const char *));
	int  (*set_encrypt) __P((DB_ENV *, const char *, uint32_t));
	void (*set_errcall) __P((DB_ENV *,
		void (*)(const DB_ENV *, const char *, const char *)));
	void (*set_errfile) __P((DB_ENV *, FILE *));
	void (*set_errpfx) __P((DB_ENV *, const char *));
	int  (*set_event_notify)
		__P((DB_ENV *, void (*)(DB_ENV *, uint32_t, void *)));
	int  (*set_feedback) __P((DB_ENV *, void (*)(DB_ENV *, int, int)));
	int  (*set_flags) __P((DB_ENV *, uint32_t, int));
	int  (*set_intermediate_dir) __P((DB_ENV *, int, uint32_t));
	int  (*set_isalive) __P((DB_ENV *,
		int (*)(DB_ENV *, pid_t, db_threadid_t, uint32_t)));
	int  (*set_lg_bsize) __P((DB_ENV *, uint32_t));
	int  (*set_lg_dir) __P((DB_ENV *, const char *));
	int  (*set_lg_filemode) __P((DB_ENV *, int));
	int  (*set_lg_max) __P((DB_ENV *, uint32_t));
	int  (*set_lg_regionmax) __P((DB_ENV *, uint32_t));
	int  (*set_lk_conflicts) __P((DB_ENV *, uint8_t *, int));
	int  (*set_lk_detect) __P((DB_ENV *, uint32_t));
	int  (*set_lk_max_lockers) __P((DB_ENV *, uint32_t));
	int  (*set_lk_max_locks) __P((DB_ENV *, uint32_t));
	int  (*set_lk_max_objects) __P((DB_ENV *, uint32_t));
	int  (*set_mp_max_openfd) __P((DB_ENV *, int));
	int  (*set_mp_max_write) __P((DB_ENV *, int, db_timeout_t));
	int  (*set_mp_mmapsize) __P((DB_ENV *, size_t));
	void (*set_msgcall)
		__P((DB_ENV *, void (*)(const DB_ENV *, const char *)));
	void (*set_msgfile) __P((DB_ENV *, FILE *));
	int  (*set_paniccall) __P((DB_ENV *, void (*)(DB_ENV *, int)));
	int  (*set_rep_request) __P((DB_ENV *, uint32_t, uint32_t));
	int  (*set_rpc_server)
		__P((DB_ENV *, void *, const char *, long, long, uint32_t));
	int  (*set_shm_key) __P((DB_ENV *, long));
	int  (*set_thread_count) __P((DB_ENV *, uint32_t));
	int  (*set_thread_id) __P((DB_ENV *,
		void (*)(DB_ENV *, pid_t *, db_threadid_t *)));
	int  (*set_thread_id_string) __P((DB_ENV *,
		char *(*)(DB_ENV *, pid_t, db_threadid_t, char *)));
	int  (*set_timeout) __P((DB_ENV *, db_timeout_t, uint32_t));
	int  (*set_tmp_dir) __P((DB_ENV *, const char *));
	int  (*set_tx_max) __P((DB_ENV *, uint32_t));
	int  (*set_tx_timestamp) __P((DB_ENV *, time_t *));
	int  (*set_verbose) __P((DB_ENV *, uint32_t, int));
	int  (*stat_print) __P((DB_ENV *, uint32_t));
	int  (*txn_begin) __P((DB_ENV *, DB_TXN *, DB_TXN **, uint32_t));
	int  (*txn_checkpoint) __P((DB_ENV *, uint32_t, uint32_t, uint32_t));
	int  (*txn_recover)
		__P((DB_ENV *, DB_PREPLIST *, long, long *, uint32_t));
	int  (*txn_stat) __P((DB_ENV *, DB_TXN_STAT **, uint32_t));
	int  (*txn_stat_print) __P((DB_ENV *, uint32_t));
	/* DB_ENV PUBLIC HANDLE LIST END */

	/* DB_ENV PRIVATE HANDLE LIST BEGIN */
	int  (*prdbt) __P((DBT *,
		int, const char *, void *, int (*)(void *, const void *), int));
	/* DB_ENV PRIVATE HANDLE LIST END */

#define	DB_TEST_ELECTINIT	 1	/* after __rep_elect_init */
#define	DB_TEST_ELECTVOTE1	 2	/* after sending VOTE1 */
#define	DB_TEST_POSTDESTROY	 3	/* after destroy op */
#define	DB_TEST_POSTLOG		 4	/* after logging all pages */
#define	DB_TEST_POSTLOGMETA	 5	/* after logging meta in btree */
#define	DB_TEST_POSTOPEN	 6	/* after __os_open */
#define	DB_TEST_POSTSYNC	 7	/* after syncing the log */
#define	DB_TEST_PREDESTROY	 8	/* before destroy op */
#define	DB_TEST_PREOPEN		 9	/* before __os_open */
#define	DB_TEST_RECYCLE		 10	/* test rep and txn_recycle */
#define	DB_TEST_SUBDB_LOCKS	 11	/* subdb locking tests */
	int		 test_abort;	/* Abort value for testing. */
	int		 test_check;	/* Checkpoint value for testing. */
	int		 test_copy;	/* Copy value for testing. */

#define	DB_ENV_AUTO_COMMIT	0x00000001 /* DB_AUTO_COMMIT. */
#define	DB_ENV_CDB		0x00000002 /* DB_INIT_CDB. */
#define	DB_ENV_CDB_ALLDB	0x00000004 /* CDB environment wide locking. */
#define	DB_ENV_DBLOCAL		0x00000008 /* Environment for a private DB. */
#define	DB_ENV_DIRECT_DB	0x00000010 /* DB_DIRECT_DB set. */
#define	DB_ENV_DIRECT_LOG	0x00000020 /* DB_DIRECT_LOG set. */
#define	DB_ENV_DSYNC_DB		0x00000040 /* DB_DSYNC_DB set. */
#define	DB_ENV_DSYNC_LOG	0x00000080 /* DB_DSYNC_LOG set. */
#define	DB_ENV_LOCKDOWN		0x00000100 /* DB_LOCKDOWN set. */
#define	DB_ENV_LOG_AUTOREMOVE   0x00000200 /* DB_LOG_AUTOREMOVE set. */
#define	DB_ENV_LOG_INMEMORY     0x00000400 /* DB_LOG_INMEMORY set. */
#define	DB_ENV_MULTIVERSION	0x00000800 /* DB_MULTIVERSION set. */
#define	DB_ENV_NOLOCKING	0x00001000 /* DB_NOLOCKING set. */
#define	DB_ENV_NOMMAP		0x00002000 /* DB_NOMMAP set. */
#define	DB_ENV_NOPANIC		0x00004000 /* Okay if panic set. */
#define	DB_ENV_NO_OUTPUT_SET	0x00008000 /* No output channel set. */
#define	DB_ENV_OPEN_CALLED	0x00010000 /* DB_ENV->open called. */
#define	DB_ENV_OVERWRITE	0x00020000 /* DB_OVERWRITE set. */
#define	DB_ENV_PRIVATE		0x00040000 /* DB_PRIVATE set. */
#define	DB_ENV_RECOVER_FATAL	0x00080000 /* Doing fatal recovery in env. */
#define	DB_ENV_REF_COUNTED	0x00100000 /* Region references this handle. */
#define	DB_ENV_REGION_INIT	0x00200000 /* DB_REGION_INIT set. */
#define	DB_ENV_RPCCLIENT	0x00400000 /* DB_RPCCLIENT set. */
#define	DB_ENV_RPCCLIENT_GIVEN	0x00800000 /* User-supplied RPC client struct */
#define	DB_ENV_SYSTEM_MEM	0x01000000 /* DB_SYSTEM_MEM set. */
#define	DB_ENV_THREAD		0x02000000 /* DB_THREAD set. */
#define	DB_ENV_TIME_NOTGRANTED	0x04000000 /* DB_TIME_NOTGRANTED set. */
#define	DB_ENV_TXN_NOSYNC	0x08000000 /* DB_TXN_NOSYNC set. */
#define	DB_ENV_TXN_NOWAIT	0x10000000 /* DB_TXN_NOWAIT set. */
#define	DB_ENV_TXN_SNAPSHOT	0x20000000 /* DB_TXN_SNAPSHOT set. */
#define	DB_ENV_TXN_WRITE_NOSYNC	0x40000000 /* DB_TXN_WRITE_NOSYNC set. */
#define	DB_ENV_YIELDCPU		0x80000000 /* DB_YIELDCPU set. */
	uint32_t flags;
};

#ifndef DB_DBM_HSEARCH
#define	DB_DBM_HSEARCH	0		/* No historic interfaces by default. */
#endif
#if DB_DBM_HSEARCH != 0
/*******************************************************
 * Dbm/Ndbm historic interfaces.
 *******************************************************/
typedef struct __db DBM;

#define	DBM_INSERT	0		/* Flags to dbm_store(). */
#define	DBM_REPLACE	1

/*
 * The DB support for ndbm(3) always appends this suffix to the
 * file name to avoid overwriting the user's original database.
 */
#define	DBM_SUFFIX	".db"

#if defined(_XPG4_2)
typedef struct {
	char *dptr;
	size_t dsize;
} datum;
#else
typedef struct {
	char *dptr;
	int dsize;
} datum;
#endif

/*
 * Translate NDBM calls into DB calls so that DB doesn't step on the
 * application's name space.
 */
#define	dbm_clearerr(a)		__db_ndbm_clearerr(a)
#define	dbm_close(a)		__db_ndbm_close(a)
#define	dbm_delete(a, b)	__db_ndbm_delete(a, b)
#define	dbm_dirfno(a)		__db_ndbm_dirfno(a)
#define	dbm_error(a)		__db_ndbm_error(a)
#define	dbm_fetch(a, b)		__db_ndbm_fetch(a, b)
#define	dbm_firstkey(a)		__db_ndbm_firstkey(a)
#define	dbm_nextkey(a)		__db_ndbm_nextkey(a)
#define	dbm_open(a, b, c)	__db_ndbm_open(a, b, c)
#define	dbm_pagfno(a)		__db_ndbm_pagfno(a)
#define	dbm_rdonly(a)		__db_ndbm_rdonly(a)
#define	dbm_store(a, b, c, d) \
	__db_ndbm_store(a, b, c, d)

/*
 * Translate DBM calls into DB calls so that DB doesn't step on the
 * application's name space.
 *
 * The global variables dbrdonly, dirf and pagf were not retained when 4BSD
 * replaced the dbm interface with ndbm, and are not supported here.
 */
#define	dbminit(a)	__db_dbm_init(a)
#define	dbmclose	__db_dbm_close
#if !defined(__cplusplus)
#define	delete(a)	__db_dbm_delete(a)
#endif
#define	fetch(a)	__db_dbm_fetch(a)
#define	firstkey	__db_dbm_firstkey
#define	nextkey(a)	__db_dbm_nextkey(a)
#define	store(a, b)	__db_dbm_store(a, b)

/*******************************************************
 * Hsearch historic interface.
 *******************************************************/
typedef enum {
	FIND, ENTER
} ACTION;

typedef struct entry {
	char *key;
	char *data;
} ENTRY;

#define	hcreate(a)	__db_hcreate(a)
#define	hdestroy	__db_hdestroy
#define	hsearch(a, b)	__db_hsearch(a, b)

#endif /* DB_DBM_HSEARCH */

#if defined(__cplusplus)
}
#endif


#endif /* !_DB_H_ */

/* DO NOT EDIT: automatically built by dist/s_include. */
#ifndef	_DB_EXT_PROT_IN_
#define	_DB_EXT_PROT_IN_

#if defined(__cplusplus)
extern "C" {
#endif

int db_create __P((DB **, DB_ENV *, uint32_t));
char *db_strerror __P((int));
int db_env_create __P((DB_ENV **, uint32_t));
char *db_version __P((int *, int *, int *));
int log_compare __P((const DB_LSN *, const DB_LSN *));
int db_env_set_func_close __P((int (*)(int)));
int db_env_set_func_dirfree __P((void (*)(char **, int)));
int db_env_set_func_dirlist __P((int (*)(const char *, char ***, int *)));
int db_env_set_func_exists __P((int (*)(const char *, int *)));
int db_env_set_func_free __P((void (*)(void *)));
int db_env_set_func_fsync __P((int (*)(int)));
int db_env_set_func_ftruncate __P((int (*)(int, off_t)));
int db_env_set_func_ioinfo __P((int (*)(const char *, int, uint32_t *, uint32_t *, uint32_t *)));
int db_env_set_func_malloc __P((void *(*)(size_t)));
int db_env_set_func_map __P((int (*)(char *, size_t, int, int, void **)));
int db_env_set_func_pread __P((ssize_t (*)(int, void *, size_t, off_t)));
int db_env_set_func_pwrite __P((ssize_t (*)(int, const void *, size_t, off_t)));
int db_env_set_func_open __P((int (*)(const char *, int, ...)));
int db_env_set_func_read __P((ssize_t (*)(int, void *, size_t)));
int db_env_set_func_realloc __P((void *(*)(void *, size_t)));
int db_env_set_func_rename __P((int (*)(const char *, const char *)));
int db_env_set_func_seek __P((int (*)(int, off_t, int)));
int db_env_set_func_sleep __P((int (*)(u_long, u_long)));
int db_env_set_func_unlink __P((int (*)(const char *)));
int db_env_set_func_unmap __P((int (*)(void *, size_t)));
int db_env_set_func_write __P((ssize_t (*)(int, const void *, size_t)));
int db_env_set_func_yield __P((int (*)(void)));
int db_sequence_create __P((DB_SEQUENCE **, DB *, uint32_t));
#if DB_DBM_HSEARCH != 0
int	 __db_ndbm_clearerr __P((DBM *));
void	 __db_ndbm_close __P((DBM *));
int	 __db_ndbm_delete __P((DBM *, datum));
int	 __db_ndbm_dirfno __P((DBM *));
int	 __db_ndbm_error __P((DBM *));
datum __db_ndbm_fetch __P((DBM *, datum));
datum __db_ndbm_firstkey __P((DBM *));
datum __db_ndbm_nextkey __P((DBM *));
DBM	*__db_ndbm_open __P((const char *, int, int));
int	 __db_ndbm_pagfno __P((DBM *));
int	 __db_ndbm_rdonly __P((DBM *));
int	 __db_ndbm_store __P((DBM *, datum, datum, int));
int	 __db_dbm_close __P((void));
int	 __db_dbm_delete __P((datum));
datum __db_dbm_fetch __P((datum));
datum __db_dbm_firstkey __P((void));
int	 __db_dbm_init __P((char *));
datum __db_dbm_nextkey __P((datum));
int	 __db_dbm_store __P((datum, datum));
#endif
#if DB_DBM_HSEARCH != 0
int __db_hcreate __P((size_t));
ENTRY *__db_hsearch __P((ENTRY, ACTION));
void __db_hdestroy __P((void));
#endif

#if defined(__cplusplus)
}
#endif
#endif /* !_DB_EXT_PROT_IN_ */
