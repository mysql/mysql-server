/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996-2002
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: txn.h,v 11.43 2002/08/29 14:22:19 margo Exp $
 */

#ifndef	_TXN_H_
#define	_TXN_H_

#include "dbinc/xa.h"

/* Operation parameters to the delayed commit processing code. */
typedef enum {
	TXN_REMOVE,		/* Remove a file. */
	TXN_TRADE,		/* Trade lockers. */
	TXN_TRADED		/* Already traded; downgrade lock. */
} TXN_EVENT_T;

struct __db_txnregion;	typedef struct __db_txnregion DB_TXNREGION;

/*
 * !!!
 * TXN_MINIMUM = (DB_LOCK_MAXID + 1) but this makes compilers complain.
 */
#define	TXN_MINIMUM	0x80000000
#define	TXN_MAXIMUM	0xffffffff	/* Maximum number of txn ids. */
#define	TXN_INVALID	0		/* Invalid transaction ID. */

#define	DEF_MAX_TXNS	20		/* Default max transactions. */

/*
 * Internal data maintained in shared memory for each transaction.
 */
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
#define	TXN_COLLECTED		0x1
#define	TXN_RESTORED		0x2
	u_int32_t flags;		/* collected during txn_recover */

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
	u_int8_t xid[XIDDATASIZE];	/* XA global transaction id */
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
	DB_MUTEX	*mutexp;	/* Lock list of active transactions
					 * (including the content of each
					 * TXN_DETAIL structure on the list).
					 */
					/* List of active transactions. */
	TAILQ_HEAD(_chain, __db_txn)	txn_chain;
	u_int32_t	 n_discards;	/* Number of txns discarded. */

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
	u_int32_t	cur_maxid;	/* current max unused id. */
	DB_LSN		last_ckp;	/* lsn of the last checkpoint */
	time_t		time_ckp;	/* time of last checkpoint */
	u_int32_t	logtype;	/* type of logging */
	u_int32_t	locktype;	/* lock type */
	DB_TXN_STAT	stat;		/* Statistics for txns. */

#define	TXN_IN_RECOVERY	 0x01		/* environment is being recovered */
	u_int32_t	flags;
					/* active TXN list */
	SH_TAILQ_HEAD(__active) active_txn;
#ifdef HAVE_MUTEX_SYSTEM_RESOURCES
#define	TXN_MAINT_SIZE	(sizeof(roff_t) * DB_MAX_HANDLES)

	roff_t		maint_off;	/* offset of region maintenance info */
#endif
};

/*
 * Log record types.  Note that these are *not* alphabetical.  This is
 * intentional so that we don't change the meaning of values between
 * software upgrades. EXPECTED, UNEXPECTED, IGNORE, NOTFOUND and OK
 * are used in the
 * txnlist functions.
 */
#define	TXN_OK		0
#define	TXN_COMMIT	1
#define	TXN_PREPARE	2
#define	TXN_ABORT	3
#define	TXN_NOTFOUND	4
#define	TXN_IGNORE	5
#define	TXN_EXPECTED	6
#define	TXN_UNEXPECTED	7

#include "dbinc_auto/txn_auto.h"
#include "dbinc_auto/txn_ext.h"
#include "dbinc_auto/xa_ext.h"
#endif /* !_TXN_H_ */
