/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: log.h,v 11.19 2001/01/11 18:19:52 bostic Exp $
 */

#ifndef _LOG_H_
#define	_LOG_H_

struct __db_log;	typedef struct __db_log DB_LOG;
struct __fname;		typedef struct __fname FNAME;
struct __hdr;		typedef struct __hdr HDR;
struct __log;		typedef struct __log LOG;
struct __log_persist;	typedef struct __log_persist LOGP;

#define	LFPREFIX	"log."		/* Log file name prefix. */
#define	LFNAME		"log.%010d"	/* Log file name template. */
#define	LFNAME_V1	"log.%05d"	/* Log file name template, rev 1. */

#define	LG_MAX_DEFAULT		(10 * MEGABYTE)	/* 10 MB. */
#define	LG_BSIZE_DEFAULT	(32 * 1024)	/* 32 KB. */
#define	LG_BASE_REGION_SIZE	(480 * 1024)	/* 480 KB. */

/*
 * The per-process table that maps log file-id's to DB structures.
 */
typedef	struct __db_entry {
	TAILQ_HEAD(dblist, __db) dblist; /* Associated DB structures. */
	u_int32_t refcount;		/* Reference counted. */
	u_int32_t count;		/* Number of ops on a deleted db. */
	int	  deleted;		/* File was not found during open. */
} DB_ENTRY;

/*
 * DB_LOG
 *	Per-process log structure.
 */
struct __db_log {
/*
 * These fields need to be protected for multi-threaded support.
 *
 * !!!
 * As this structure is allocated in per-process memory, the mutex may need
 * to be stored elsewhere on architectures unable to support mutexes in heap
 * memory, e.g., HP/UX 9.
 */
	MUTEX	  *mutexp;		/* Mutex for thread protection. */

	DB_ENTRY *dbentry;		/* Recovery file-id mapping. */
#define	DB_GROW_SIZE	64
	int32_t dbentry_cnt;		/* Entries.  Grows by DB_GROW_SIZE. */

/*
 * These fields are always accessed while the region lock is held, so they do
 * not have to be protected by the thread lock as well, OR, they are only used
 * when threads are not being used, i.e. most cursor operations are disallowed
 * on threaded logs.
 */
	u_int32_t lfname;		/* Log file "name". */
	DB_FH	  lfh;			/* Log file handle. */

	DB_LSN	  c_lsn;		/* Cursor: current LSN. */
	DBT	  c_dbt;		/* Cursor: return DBT structure. */
	DB_FH	  c_fh;			/* Cursor: file handle. */
	FILE	  *c_fp;		/* Cursor: file pointer. */
	u_int32_t c_off;		/* Cursor: previous record offset. */
	u_int32_t c_len;		/* Cursor: current record length. */
	u_int32_t r_file;		/* Cursor: current read file */
	u_int32_t r_off;		/* Cursor: offset of read buffer. */
	u_int32_t r_size;		/* Cursor: size of data in read buf. */

	u_int8_t *bufp;			/* Region buffer. */
	u_int8_t *readbufp;		/* Read buffer. */

/* These fields are not protected. */
	DB_ENV	 *dbenv;		/* Reference to error information. */
	REGINFO	  reginfo;		/* Region information. */

/*
 * These fields are used by XA; since XA forbids threaded execution, these
 * do not have to be protected.
 */
	void	*xa_info;		/* Committed transaction list that
					 * has to be carried between calls
					 * to xa_recover. */
	DB_LSN	xa_lsn;			/* Position of an XA recovery scan. */
	DB_LSN	xa_first;		/* LSN to which we need to roll back
					   for this XA recovery scan. */

#define	DBLOG_RECOVER		0x01	/* We are in recovery. */
#define	DBLOG_FORCE_OPEN	0x02	/* Force the db open even
					 * if it appears to be deleted.
					 */
	u_int32_t flags;
};

/*
 * HDR --
 *	Log record header.
 */
struct __hdr {
	u_int32_t prev;			/* Previous offset. */
	u_int32_t cksum;		/* Current checksum. */
	u_int32_t len;			/* Current length. */
};

struct __log_persist {
	u_int32_t magic;		/* DB_LOGMAGIC */
	u_int32_t version;		/* DB_LOGVERSION */

	u_int32_t lg_max;		/* Maximum file size. */
	int	  mode;			/* Log file mode. */
};

/*
 * LOG --
 *	Shared log region.  One of these is allocated in shared memory,
 *	and describes the log.
 */
struct __log {
	LOGP	  persist;		/* Persistent information. */

	SH_TAILQ_HEAD(__fq) fq;		/* List of file names. */

	/*
	 * The lsn LSN is the file offset that we're about to write and which
	 * we will return to the user.
	 */
	DB_LSN	  lsn;			/* LSN at current file offset. */

	/*
	 * The s_lsn LSN is the last LSN that we know is on disk, not just
	 * written, but synced.
	 */
	DB_LSN	  s_lsn;		/* LSN of the last sync. */

	u_int32_t len;			/* Length of the last record. */

	u_int32_t w_off;		/* Current write offset in the file. */

	DB_LSN	  chkpt_lsn;		/* LSN of the last checkpoint. */
	time_t	  chkpt;		/* Time of the last checkpoint. */

	DB_LOG_STAT stat;		/* Log statistics. */

	/*
	 * The f_lsn LSN is the LSN (returned to the user) that "owns" the
	 * first byte of the buffer.  If the record associated with the LSN
	 * spans buffers, it may not reflect the physical file location of
	 * the first byte of the buffer.
	 */
	DB_LSN	  f_lsn;		/* LSN of first byte in the buffer. */
	size_t	  b_off;		/* Current offset in the buffer. */

	roff_t	  buffer_off;		/* Log buffer offset. */
	u_int32_t buffer_size;		/* Log buffer size. */
};

/*
 * FNAME --
 *	File name and id.
 */
struct __fname {
	SH_TAILQ_ENTRY q;		/* File name queue. */

	u_int16_t ref;			/* Reference count. */
	u_int16_t locked;		/* Table is locked. */

	int32_t id;			/* Logging file id. */
	DBTYPE	  s_type;		/* Saved DB type. */

	roff_t	  name_off;		/* Name offset. */
	db_pgno_t meta_pgno;		/* Page number of the meta page. */
	u_int8_t  ufid[DB_FILE_ID_LEN];	/* Unique file id. */
};

/* File open/close register log record opcodes. */
#define	LOG_CHECKPOINT	1		/* Checkpoint: file name/id dump. */
#define	LOG_CLOSE	2		/* File close. */
#define	LOG_OPEN	3		/* File open. */

#define	CHECK_LSN(redo, cmp, lsn, prev)					\
	DB_ASSERT(!DB_REDO(redo) || (cmp) >= 0);			\
	if (DB_REDO(redo) && (cmp) < 0) {				\
		__db_err(dbenv,						\
	"Log sequence error: page LSN %lu:%lu; previous LSN %lu %lu",	\
		    (u_long)(lsn)->file, (u_long)(lsn)->offset,		\
		    (u_long)(prev)->file, (u_long)(prev)->offset);	\
		goto out;						\
	}

/*
 * Status codes indicating the validity of a log file examined by
 * __log_valid().
 */
typedef enum {
	DB_LV_INCOMPLETE,
	DB_LV_NORMAL,
	DB_LV_OLD_READABLE,
	DB_LV_OLD_UNREADABLE
} logfile_validity;

#include "log_auto.h"
#include "log_ext.h"
#endif /* _LOG_H_ */
