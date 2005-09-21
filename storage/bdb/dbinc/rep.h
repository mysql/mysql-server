/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2004
 *	Sleepycat Software.  All rights reserved.
 */

#ifndef _REP_H_
#define	_REP_H_

#include "dbinc_auto/rep_auto.h"

#define	REP_ALIVE	1	/* I am alive message. */
#define	REP_ALIVE_REQ	2	/* Request for alive messages. */
#define	REP_ALL_REQ	3	/* Request all log records greater than LSN. */
#define	REP_DUPMASTER	4	/* Duplicate master detected; propagate. */
#define	REP_FILE	5	/* Page of a database file. NOTUSED */
#define	REP_FILE_FAIL	6	/* File requested does not exist. */
#define	REP_FILE_REQ	7	/* Request for a database file. NOTUSED */
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
#define	REP_PAGE_FAIL	17	/* Requested page does not exist. */
#define	REP_PAGE_MORE	18	/* There are more pages to request. */
#define	REP_PAGE_REQ	19	/* Request for a database page. */
#define	REP_UPDATE	20	/* Environment hotcopy information. */
#define	REP_UPDATE_REQ	21	/* Request for hotcopy information. */
#define	REP_VERIFY	22	/* A log record for verification. */
#define	REP_VERIFY_FAIL	23	/* The client is outdated. */
#define	REP_VERIFY_REQ	24	/* Request for a log record to verify. */
#define	REP_VOTE1	25	/* Send out your information for an election. */
#define	REP_VOTE2	26	/* Send a "you are master" vote. */

/*
 * REP_PRINT_MESSAGE
 *	A function to print a debugging message.
 *
 * RPRINT
 *	A macro for debug printing.  Takes as an arg the arg set for __db_msg.
 *
 * !!! This function assumes a local DB_MSGBUF variable called 'mb'.
 */
#ifdef DIAGNOSTIC
#define	REP_PRINT_MESSAGE(dbenv, eid, rp, str)				\
	__rep_print_message(dbenv, eid, rp, str)
#define RPRINT(e, r, x) do {						\
	if (FLD_ISSET((e)->verbose, DB_VERB_REPLICATION)) {		\
		DB_MSGBUF_INIT(&mb);					\
		if ((e)->db_errpfx == NULL) {				\
			if (F_ISSET((r), REP_F_CLIENT))			\
				__db_msgadd((e), &mb, "CLIENT: ");	\
			else if (F_ISSET((r), REP_F_MASTER))		\
				__db_msgadd((e), &mb, "MASTER: ");	\
			else						\
				__db_msgadd((e), &mb, "REP_UNDEF: ");	\
		} else							\
			__db_msgadd((e), &mb, "%s: ",(e)->db_errpfx);	\
		__db_msgadd x;						\
		DB_MSGBUF_FLUSH((e), &mb);				\
	}								\
} while (0)	
#else
#define	REP_PRINT_MESSAGE(dbenv, eid, rp, str)
#define RPRINT(e, r, x)
#endif

/*
 * Election gen file name
 *	The file contains an egen number for an election this client
 * has NOT participated in.  I.e. it is the number of a future
 * election.  We create it when we create the rep region, if it
 * doesn't already exist and initialize egen to 1.  If it does
 * exist, we read it when we create the rep region.  We write it
 * immediately before sending our VOTE1 in an election.  That way,
 * if a client has ever sent a vote for any election, the file is
 * already going to be updated to reflect a future election,
 * should it crash.
 */
#define	REP_EGENNAME	"__db.rep.egen"

/*
 * Database types for __rep_client_dbinit
 */
typedef enum {
	REP_DB,		/* Log record database. */
	REP_PG		/* Pg database. */
} repdb_t;

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
	roff_t		tally_off;	/* Offset of the tally region. */
	roff_t		v2tally_off;	/* Offset of the vote2 tally region. */
	int		eid;		/* Environment id. */
	int		master_id;	/* ID of the master site. */
	u_int32_t	egen;		/* Replication election generation. */
	u_int32_t	gen;		/* Replication generation number. */
	u_int32_t	recover_gen;	/* Last generation number in log. */
	int		asites;		/* Space allocated for sites. */
	int		nsites;		/* Number of sites in group. */
	int		nvotes;		/* Number of votes needed. */
	int		priority;	/* My priority in an election. */
	u_int32_t	gbytes;		/* Limit on data sent in single... */
	u_int32_t	bytes;		/* __rep_process_message call. */
#define	DB_REP_REQUEST_GAP	4
#define	DB_REP_MAX_GAP		128
	u_int32_t	request_gap;	/* # of records to receive before we
					 * request a missing log record. */
	u_int32_t	max_gap;	/* Maximum number of records before
					 * requesting a missing log record. */
	/* Status change information */
	int		elect_th;	/* A thread is in rep_elect. */
	u_int32_t	msg_th;		/* Number of callers in rep_proc_msg. */
	int		start_th;	/* A thread is in rep_start. */
	u_int32_t	handle_cnt;	/* Count of handles in library. */
	u_int32_t	op_cnt;		/* Multi-step operation count.*/
	int		in_recovery;	/* Running recovery now. */

	/* Backup information. */
	int		nfiles;		/* Number of files we have info on. */
	int		curfile;	/* Current file we're getting. */
	__rep_fileinfo_args	*curinfo;	/* Current file info ptr. */
	void		*finfo;		/* Current file info buffer. */
	void		*nextinfo;	/* Next file info buffer. */
	void		*originfo;	/* Original file info buffer. */
	DB_LSN		first_lsn;	/* Earliest LSN we need. */
	DB_LSN		last_lsn;	/* Latest LSN we need. */
	db_pgno_t	ready_pg;	/* Next pg expected. */
	db_pgno_t	waiting_pg;	/* First pg after gap. */
	db_pgno_t	max_wait_pg;	/* Maximum pg requested. */
	u_int32_t	npages;		/* Num of pages rcvd for this file. */
	DB_MPOOLFILE	*file_mpf;	/* Mpoolfile for in-mem database. */
	DB		*file_dbp;	/* This file's page info. */
	DB		*queue_dbp;	/* Dbp for a queue file. */

	/* Vote tallying information. */
	int		sites;		/* Sites heard from. */
	int		winner;		/* Current winner. */
	int		w_priority;	/* Winner priority. */
	u_int32_t	w_gen;		/* Winner generation. */
	DB_LSN		w_lsn;		/* Winner LSN. */
	u_int32_t	w_tiebreaker;	/* Winner tiebreaking value. */
	int		votes;		/* Number of votes for this site. */

	/* Statistics. */
	DB_REP_STAT	stat;

#define	REP_F_CLIENT		0x00001		/* Client replica. */
#define	REP_F_EPHASE1		0x00002		/* In phase 1 of election. */
#define	REP_F_EPHASE2		0x00004		/* In phase 2 of election. */
#define	REP_F_MASTER		0x00008		/* Master replica. */
#define	REP_F_MASTERELECT	0x00010		/* Master elect */
#define	REP_F_NOARCHIVE		0x00020		/* Rep blocks log_archive */
#define	REP_F_READY		0x00040		/* Wait for txn_cnt to be 0. */
#define	REP_F_RECOVER_LOG	0x00080		/* In recovery - log. */
#define	REP_F_RECOVER_PAGE	0x00100		/* In recovery - pages. */
#define	REP_F_RECOVER_UPDATE	0x00200		/* In recovery - files. */
#define	REP_F_RECOVER_VERIFY	0x00400		/* In recovery - verify. */
#define	REP_F_TALLY		0x00800		/* Tallied vote before elect. */
	u_int32_t	flags;
} REP;

/*
 * Recovery flag mask to easily check any/all recovery bits.  That is
 * REP_F_READY and all REP_F_RECOVER*.  This must change if the values
 * of the flags change.
 */
#define	REP_F_RECOVER_MASK					\
    (REP_F_READY | REP_F_RECOVER_LOG | REP_F_RECOVER_PAGE |	\
     REP_F_RECOVER_UPDATE | REP_F_RECOVER_VERIFY)

#define	IN_ELECTION(R)		F_ISSET((R), REP_F_EPHASE1 | REP_F_EPHASE2)
#define	IN_ELECTION_TALLY(R) \
	F_ISSET((R), REP_F_EPHASE1 | REP_F_EPHASE2 | REP_F_TALLY)
#define	IS_REP_MASTER(dbenv)						\
	(REP_ON(dbenv) && ((DB_REP *)(dbenv)->rep_handle)->region &&	\
	    F_ISSET(((REP *)((DB_REP *)(dbenv)->rep_handle)->region),	\
	    REP_F_MASTER))

#define	IS_REP_CLIENT(dbenv)						\
	(REP_ON(dbenv) && ((DB_REP *)(dbenv)->rep_handle)->region &&	\
	    F_ISSET(((REP *)((DB_REP *)(dbenv)->rep_handle)->region),	\
	    REP_F_CLIENT))

#define	IS_CLIENT_PGRECOVER(dbenv)					\
	(IS_REP_CLIENT(dbenv) &&					\
	    F_ISSET(((REP *)((DB_REP *)(dbenv)->rep_handle)->region),   \
	    REP_F_RECOVER_PAGE))

/*
 * Macros to figure out if we need to do replication pre/post-amble
 * processing.
 */
#define	IS_REPLICATED(E, D)						\
	(!F_ISSET((D), DB_AM_RECOVER | DB_AM_REPLICATION) &&		\
	REP_ON(E) && ((DB_REP *)((E)->rep_handle))->region != NULL &&	\
	((DB_REP *)((E)->rep_handle))->region->flags != 0)

#define	IS_ENV_REPLICATED(E) (REP_ON(E) &&		\
	((DB_REP *)((E)->rep_handle))->region != NULL &&		\
	((DB_REP *)((E)->rep_handle))->region->flags != 0)

/*
 * Per-process replication structure.
 *
 * There are 2 mutexes used in replication.
 * 1.  rep_mutexp - This protects the fields of the rep region above.
 * 2.  db_mutexp - This protects the per-process flags, and bookkeeping
 * database and all of the components that maintain it.  Those
 * components include the following fields in the log region (see log.h):
 *	a. ready_lsn
 *	b. waiting_lsn
 *	c. verify_lsn
 *	d. wait_recs
 *	e. rcvd_recs
 *	f. max_wait_lsn
 * These fields in the log region are NOT protected by the log
 * region lock at all.
 *
 * Note that the per-process flags should truly be protected by a
 * special per-process thread mutex, but it is currently set in so
 * isolated a manner that it didn't make sense to do so and in most
 * case we're already holding the db_mutexp anyway.
 *
 * The lock ordering protocol is that db_mutexp must be acquired
 * first and then either rep_mutexp, or the log region mutex may
 * be acquired if necessary.
 */
struct __db_rep {
	DB_MUTEX	*rep_mutexp;	/* Mutex for rep region */

	DB_MUTEX	*db_mutexp;	/* Mutex for bookkeeping database. */
	DB		*rep_db;	/* Bookkeeping database. */

	REP		*region;	/* In memory structure. */
#define	DBREP_OPENFILES		0x0001	/* This handle has opened files. */
	u_int32_t	flags;		/* per-process flags. */
};

/*
 * Control structure for replication communication infrastructure.
 *
 * Note that the version information should be at the beginning of the
 * structure, so that we can rearrange the rest of it while letting the
 * version checks continue to work.  DB_REPVERSION should be revved any time
 * the rest of the structure changes or when the message numbers change.
 */
typedef struct __rep_control {
#define	DB_REPVERSION	2
	u_int32_t	rep_version;	/* Replication version number. */
	u_int32_t	log_version;	/* Log version number. */

	DB_LSN		lsn;		/* Log sequence number. */
	u_int32_t	rectype;	/* Message type. */
	u_int32_t	gen;		/* Generation number. */
	u_int32_t	flags;		/* log_put flag value. */
} REP_CONTROL;

/* Election vote information. */
typedef struct __rep_vote {
	u_int32_t	egen;		/* Election generation. */
	int		nsites;		/* Number of sites I've been in
					 * communication with. */
	int		nvotes;		/* Number of votes needed to win. */
	int		priority;	/* My site's priority. */
	u_int32_t	tiebreaker;	/* Tie-breaking quasi-random value. */
} REP_VOTE_INFO;

typedef struct __rep_vtally {
	u_int32_t	egen;		/* Voter's election generation. */
	int		eid;		/* Voter's ID. */
} REP_VTALLY;

/*
 * This structure takes care of representing a transaction.
 * It holds all the records, sorted by page number so that
 * we can obtain locks and apply updates in a deadlock free
 * order.
 */
typedef struct __lsn_collection {
	u_int nlsns;
	u_int nalloc;
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
