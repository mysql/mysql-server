/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: qam.h,v 11.26 2001/01/11 18:19:52 bostic Exp $
 */

/*
 * QAM data elements: a status field and the data.
 */
typedef struct _qamdata {
	u_int8_t  flags;	/* 00: delete bit. */
#define	QAM_VALID	0x01
#define	QAM_SET		0x02
	u_int8_t  data[1];	/* Record. */
} QAMDATA;

struct __queue;		typedef struct __queue QUEUE;
struct __qcursor;	typedef struct __qcursor QUEUE_CURSOR;

struct __qcursor {
	/* struct __dbc_internal */
	__DBC_INTERNAL

	/* Queue private part */

	/* Per-thread information: queue private. */
	db_recno_t	 recno;		/* Current record number. */

	u_int32_t	 flags;
};

/*
 * The in-memory, per-tree queue data structure.
 */

typedef struct __mpfarray {
	u_int32_t n_extent;		/* Number of extents in table. */
	u_int32_t low_extent;		/* First extent open. */
	u_int32_t hi_extent;		/* Last extent open. */
	struct __qmpf {
		int pinref;
		DB_MPOOLFILE *mpf;
	} *mpfarray;			 /* Array of open extents. */
} MPFARRAY;

struct __queue {
	db_pgno_t q_meta;		/* Database meta-data page. */
	db_pgno_t q_root;		/* Database root page. */

	int	  re_pad;		/* Fixed-length padding byte. */
	u_int32_t re_len;		/* Length for fixed-length records. */
	u_int32_t rec_page;		/* records per page */
	u_int32_t page_ext;		/* Pages per extent */
	MPFARRAY array1, array2;	/* File arrays. */
	DB_MPOOL_FINFO finfo;		/* Initialized info struct. */
	DB_PGINFO pginfo;		/* Initialized pginfo struct. */
	DBT pgcookie;			/* Initialized pgcookie. */
	char *path;			/* Space allocated to file pathname. */
	char *name;			/* The name of the file. */
	char *dir;			/* The dir of the file. */
	int mode;			/* Mode to open extents. */
};

/* Format for queue extent names. */
#define	QUEUE_EXTENT "%s/__dbq.%s.%d"

typedef struct __qam_filelist {
	DB_MPOOLFILE *mpf;
	u_int32_t id;
} QUEUE_FILELIST;

/*
 * Caculate the page number of a recno
 *
 * Number of records per page =
 *	Divide the available space on the page by the record len + header.
 *
 * Page number for record =
 *	divide the physical record number by the records per page
 *	add the root page number
 *      For now the root page will always be 1, but we might want to change
 *	in the future (e.g. multiple fixed len queues per file).
 *
 * Index of record on page =
 *	physical record number, less the logical pno times records/page
 */
#define	CALC_QAM_RECNO_PER_PAGE(dbp)					\
    (((dbp)->pgsize - sizeof(QPAGE)) /					\
    ALIGN(((QUEUE *)(dbp)->q_internal)->re_len +			\
    sizeof(QAMDATA) - SSZA(QAMDATA, data), sizeof(u_int32_t)))

#define	QAM_RECNO_PER_PAGE(dbp)	(((QUEUE*)(dbp)->q_internal)->rec_page)

#define	QAM_RECNO_PAGE(dbp, recno)					\
    (((QUEUE *)(dbp)->q_internal)->q_root				\
    + (((recno) - 1) / QAM_RECNO_PER_PAGE(dbp)))

#define	QAM_RECNO_INDEX(dbp, pgno, recno)				\
    (((recno) - 1) - (QAM_RECNO_PER_PAGE(dbp)				\
    * (pgno - ((QUEUE *)(dbp)->q_internal)->q_root)))

#define	QAM_GET_RECORD(dbp, page, index)				\
    ((QAMDATA *)((u_int8_t *)(page) +					\
    sizeof(QPAGE) + (ALIGN(sizeof(QAMDATA) - SSZA(QAMDATA, data) +	\
    ((QUEUE *)(dbp)->q_internal)->re_len, sizeof(u_int32_t)) * index)))

#define	QAM_AFTER_CURRENT(meta, recno)					\
    ((recno) > (meta)->cur_recno &&					\
    ((meta)->first_recno <= (meta)->cur_recno || (recno) < (meta)->first_recno))

#define	QAM_BEFORE_FIRST(meta, recno)					\
    ((recno) < (meta)->first_recno &&					\
    ((meta->first_recno <= (meta)->cur_recno || (recno) > (meta)->cur_recno)))

#define	QAM_NOT_VALID(meta, recno)					\
    (recno == RECNO_OOB ||						\
	QAM_BEFORE_FIRST(meta, recno) || QAM_AFTER_CURRENT(meta, recno))

/*
 * Log opcodes for the mvptr routine.
 */
#define	QAM_SETFIRST		0x01
#define	QAM_SETCUR		0x02

/*
 * Parameter to __qam_position.
 */
typedef enum {
	QAM_READ,
	QAM_WRITE,
	QAM_CONSUME
} qam_position_mode;

typedef enum {
	QAM_PROBE_GET,
	QAM_PROBE_PUT,
	QAM_PROBE_MPF
} qam_probe_mode;

#define	__qam_fget(dbp, pgnoaddr, flags, addrp) \
	__qam_fprobe(dbp, *pgnoaddr, addrp, QAM_PROBE_GET, flags)

#define	__qam_fput(dbp, pageno, addrp, flags) \
	__qam_fprobe(dbp, pageno, addrp, QAM_PROBE_PUT, flags)

#include "qam_auto.h"
#include "qam_ext.h"
