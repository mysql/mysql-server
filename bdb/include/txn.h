/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: txn.h,v 11.12 2001/01/02 17:23:39 margo Exp $
 */

#ifndef	_TXN_H_
#define	_TXN_H_

#include "xa.h"

struct __db_txnmgr;	typedef struct __db_txnmgr DB_TXNMGR;
struct __db_txnregion;	typedef struct __db_txnregion DB_TXNREGION;

/*
 * !!!
 * TXN_MINIMUM = (DB_LOCK_MAXID + 1) but this makes compilers complain.
 */
#define	TXN_MINIMUM	0x80000000
#define	TXN_INVALID	0xffffffff	/* Maximum number of txn ids. */
#define	TXN_INVALID_ID	0		/* Invalid transaction ID. */

#define	DEF_MAX_TXNS	20		/* Default max transactions. */

/* The structure allocated for every transaction. */
struct __db_txn {
	DB_TXNMGR	*mgrp;		/* Pointer to transaction manager. */
	DB_TXN		*parent;	/* Pointer to transaction's parent. */
	DB_LSN		last_lsn;	/* Lsn of last log write. */
	u_int32_t	txnid;		/* Unique transaction id. */
	roff_t		off;		/* Detail structure within region. */
	TAILQ_ENTRY(__db_txn) links;	/* Links transactions off manager. */
	TAILQ_HEAD(__kids, __db_txn) kids; /* Child transactions. */
	TAILQ_ENTRY(__db_txn) klinks;	/* Links child transactions. */
	u_int32_t	cursors;	/* Number of cursors open for txn */

#define	TXN_CHILDCOMMIT	0x01		/* Transaction that has committed. */
#define	TXN_MALLOC	0x02		/* Structure allocated by TXN system. */
#define	TXN_NOSYNC	0x04		/* Do not sync on prepare and commit. */
#define	TXN_NOWAIT	0x08		/* Do not wait on locks. */
#define	TXN_SYNC	0x10		/* Sync on prepare and commit. */
	u_int32_t	flags;
};

/*
 * Internal data maintained in shared memory for each transaction.
 */
typedef char DB_XID[XIDDATASIZE];

typedef struct __txn_detail {
	u_int32_t txnid;		/* current transaction id
					   used to link free list also */
	DB_LSN	last_lsn;		/* last lsn written for this txn */
	DB_LSN	begin_lsn;		/* lsn of begin record */
	roff_t	parent;			/* Offset of transaction's parent. */

#define	TXN_RUNNING		1
#define	TXN_ABORTED		2
#define	TXN_PREPARED		3
#define	TXN_COMMITTED		4
	u_int32_t status;		/* status of the transaction */

	SH_TAILQ_ENTRY	links;		/* free/active list */

#define	TXN_XA_ABORTED		1
#define	TXN_XA_DEADLOCKED	2
#define	TXN_XA_ENDED		3
#define	TXN_XA_PREPARED		4
#define	TXN_XA_STARTED		5
#define	TXN_XA_SUSPENDED	6
	u_int32_t xa_status;		/* XA status */

	/*
	 * XID (xid_t) structure: because these fields are logged, the
	 * sizes have to be explicit.
	 */
	DB_XID xid;			/* XA global transaction id */
	u_int32_t bqual;		/* bqual_length from XID */
	u_int32_t gtrid;		/* gtrid_length from XID */
	int32_t format;			/* XA format */
} TXN_DETAIL;

/*
 * DB_TXNMGR --
 *	The transaction manager encapsulates the transaction system.
 */
struct __db_txnmgr {
/*
 * These fields need to be protected for multi-threaded support.
 *
 * !!!
 * As this structure is allocated in per-process memory, the mutex may need
 * to be stored elsewhere on architectures unable to support mutexes in heap
 * memory, e.g., HP/UX 9.
 */
	MUTEX		*mutexp;	/* Lock list of active transactions
					 * (including the content of each
					 * TXN_DETAIL structure on the list).
					 */
					/* List of active transactions. */
	TAILQ_HEAD(_chain, __db_txn)	txn_chain;

/* These fields are never updated after creation, and so not protected. */
	DB_ENV		*dbenv;		/* Environment. */
	REGINFO		 reginfo;	/* Region information. */
};

/*
 * DB_TXNREGION --
 *	The primary transaction data structure in the shared memory region.
 */
struct __db_txnregion {
	u_int32_t	maxtxns;	/* maximum number of active TXNs */
	u_int32_t	last_txnid;	/* last transaction id given out */
	DB_LSN		pending_ckp;	/* last checkpoint did not finish */
	DB_LSN		last_ckp;	/* lsn of the last checkpoint */
	time_t		time_ckp;	/* time of last checkpoint */
	u_int32_t	logtype;	/* type of logging */
	u_int32_t	locktype;	/* lock type */
	u_int32_t	naborts;	/* number of aborted TXNs */
	u_int32_t	ncommits;	/* number of committed TXNs */
	u_int32_t	nbegins;	/* number of begun TXNs */
	u_int32_t	nactive;	/* number of active TXNs */
	u_int32_t	maxnactive;	/* maximum number of active TXNs */
					/* active TXN list */
	SH_TAILQ_HEAD(__active) active_txn;
};

/*
 * Make the region large enough to hold N transaction detail structures
 * plus some space to hold thread handles and the beginning of the shalloc
 * region.
 */
#define	TXN_REGION_SIZE(N)						\
	(sizeof(DB_TXNREGION) + N * sizeof(TXN_DETAIL) + 1000)

/*
 * Log record types.
 */
#define	TXN_COMMIT	1
#define	TXN_PREPARE	2

#include "txn_auto.h"
#include "txn_ext.h"

#include "xa_ext.h"
#endif /* !_TXN_H_ */
