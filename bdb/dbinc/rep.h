/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2002
 *	Sleepycat Software.  All rights reserved.
 */

#ifndef _REP_H_
#define	_REP_H_

#define	REP_ALIVE	1	/* I am alive message. */
#define	REP_ALIVE_REQ	2	/* Request for alive messages. */
#define	REP_ALL_REQ	3	/* Request all log records greater than LSN. */
#define	REP_ELECT	4	/* Indicates that all listeners should */
				/* begin master election */
#define	REP_FILE	6	/* Page of a database file. */
#define	REP_FILE_REQ	7	/* Request for a database file. */
#define	REP_LOG		8	/* Log record. */
#define	REP_LOG_MORE	9	/* There are more log records to request. */
#define	REP_LOG_REQ	10	/* Request for a log record. */
#define	REP_MASTER_REQ	11	/* Who is the master */
#define	REP_NEWCLIENT	12	/* Announces the presence of a new client. */
#define	REP_NEWFILE	13	/* Announce a log file change. */
#define	REP_NEWMASTER	14	/* Announces who the master is. */
#define	REP_NEWSITE	15	/* Announces that a site has heard from a new
				 * site; like NEWCLIENT, but indirect.  A
				 * NEWCLIENT message comes directly from the new
				 * client while a NEWSITE comes indirectly from
				 * someone who heard about a NEWSITE.
				 */
#define	REP_PAGE	16	/* Database page. */
#define	REP_PAGE_REQ	17	/* Request for a database page. */
#define	REP_PLIST	18	/* Database page list. */
#define	REP_PLIST_REQ	19	/* Request for a page list. */
#define	REP_VERIFY	20	/* A log record for verification. */
#define	REP_VERIFY_FAIL	21	/* The client is outdated. */
#define	REP_VERIFY_REQ	22	/* Request for a log record to verify. */
#define	REP_VOTE1	23	/* Send out your information for an election. */
#define	REP_VOTE2	24	/* Send a "you are master" vote. */

/* Used to consistently designate which messages ought to be received where. */
#define	MASTER_ONLY(dbenv)	\
	if (!F_ISSET(dbenv, DB_ENV_REP_MASTER)) return (EINVAL)

#define	CLIENT_ONLY(dbenv)	\
	if (!F_ISSET(dbenv, DB_ENV_REP_CLIENT)) return (EINVAL)

#define	ANYSITE(dbenv)

/* Shared replication structure. */

typedef struct __rep {
	/*
	 * Due to alignment constraints on some architectures (e.g. HP-UX),
	 * DB_MUTEXes must be the first element of shalloced structures,
	 * and as a corollary there can be only one per structure.  Thus,
	 * db_mutex_off points to a mutex in a separately-allocated chunk.
	 */
	DB_MUTEX	mutex;		/* Region lock. */
	roff_t		db_mutex_off;	/* Client database mutex. */
	u_int32_t	tally_off;	/* Offset of the tally region. */
	int		eid;		/* Environment id. */
	int		master_id;	/* ID of the master site. */
	u_int32_t	gen;		/* Replication generation number */
	int		asites;		/* Space allocated for sites. */
	int		nsites;		/* Number of sites in group. */
	int		priority;	/* My priority in an election. */
	u_int32_t	gbytes;		/* Limit on data sent in single... */
	u_int32_t	bytes;		/* __rep_process_message call. */
#define	DB_REP_REQUEST_GAP	4
#define	DB_REP_MAX_GAP		128
	u_int32_t	request_gap;	/* # of records to receive before we
					 * request a missing log record. */
	u_int32_t	max_gap;	/* Maximum number of records before
					 * requesting a missing log record. */

	/* Vote tallying information. */
	int		sites;		/* Sites heard from. */
	int		winner;		/* Current winner. */
	int		w_priority;	/* Winner priority. */
	u_int32_t	w_gen;		/* Winner generation. */
	DB_LSN		w_lsn;		/* Winner LSN. */
	int		w_tiebreaker;	/* Winner tiebreaking value. */
	int		votes;		/* Number of votes for this site. */

	/* Statistics. */
	DB_REP_STAT	stat;

#define	REP_F_EPHASE1	0x01		/* In phase 1 of election. */
#define	REP_F_EPHASE2	0x02		/* In phase 2 of election. */
#define	REP_F_LOGSONLY	0x04		/* Log-site only; cannot be upgraded. */
#define	REP_F_MASTER	0x08		/* Master replica. */
#define	REP_F_RECOVER	0x10
#define	REP_F_UPGRADE	0x20		/* Upgradeable replica. */
#define	REP_ISCLIENT	(REP_F_UPGRADE | REP_F_LOGSONLY)
	u_int32_t	flags;
} REP;

#define	IN_ELECTION(R)		F_ISSET((R), REP_F_EPHASE1 | REP_F_EPHASE2)
#define	ELECTION_DONE(R)	F_CLR((R), REP_F_EPHASE1 | REP_F_EPHASE2)

/*
 * Per-process replication structure.
 */
struct __db_rep {
	DB_MUTEX	*mutexp;

	DB_MUTEX	*db_mutexp;	/* Mutex for bookkeeping database. */
	DB		*rep_db;	/* Bookkeeping database. */

	REP		*region;	/* In memory structure. */
	int		(*rep_send)	/* Send function. */
			    __P((DB_ENV *,
			    const DBT *, const DBT *, int, u_int32_t));
};

/*
 * Control structure for replication communication infrastructure.
 *
 * Note that the version information should be at the beginning of the
 * structure, so that we can rearrange the rest of it while letting the
 * version checks continue to work.  DB_REPVERSION should be revved any time
 * the rest of the structure changes.
 */
typedef struct __rep_control {
#define	DB_REPVERSION	1
	u_int32_t	rep_version;	/* Replication version number. */
	u_int32_t	log_version;	/* Log version number. */

	DB_LSN		lsn;		/* Log sequence number. */
	u_int32_t	rectype;	/* Message type. */
	u_int32_t	gen;		/* Generation number. */
	u_int32_t	flags;		/* log_put flag value. */
} REP_CONTROL;

/* Election vote information. */
typedef struct __rep_vote {
	int	priority;		/* My site's priority. */
	int	nsites;			/* Number of sites I've been in
					 * communication with. */
	int	tiebreaker;		/* Tie-breaking quasi-random int. */
} REP_VOTE_INFO;

/*
 * This structure takes care of representing a transaction.
 * It holds all the records, sorted by page number so that
 * we can obtain locks and apply updates in a deadlock free
 * order.
 */
typedef struct __lsn_page {
	DB_LSN		lsn;
	u_int32_t	fid;
	DB_LOCK_ILOCK	pgdesc;
#define	LSN_PAGE_NOLOCK		0x0001	/* No lock necessary for log rec. */
	u_int32_t	flags;
} LSN_PAGE;

typedef struct __txn_recs {
	int		npages;
	int		nalloc;
	LSN_PAGE	*array;
	u_int32_t	txnid;
	u_int32_t	lockid;
} TXN_RECS;

typedef struct __lsn_collection {
	int nlsns;
	int nalloc;
	DB_LSN *array;
} LSN_COLLECTION;

/*
 * This is used by the page-prep routines to do the lock_vec call to
 * apply the updates for a single transaction or a collection of
 * transactions.
 */
typedef struct _linfo {
	int		n;
	DB_LOCKREQ	*reqs;
	DBT		*objs;
} linfo_t;

#include "dbinc_auto/rep_ext.h"
#endif	/* !_REP_H_ */
