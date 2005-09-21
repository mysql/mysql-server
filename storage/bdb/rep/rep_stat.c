/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2001-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: rep_stat.c,v 1.155 2004/09/29 15:36:38 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

#include <string.h>
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/db_am.h"
#include "dbinc/log.h"

#ifdef HAVE_STATISTICS
static int __rep_print_all __P((DB_ENV *, u_int32_t));
static int __rep_print_stats __P((DB_ENV *, u_int32_t));
static int __rep_stat __P((DB_ENV *, DB_REP_STAT **, u_int32_t));

/*
 * __rep_stat_pp --
 *	DB_ENV->rep_stat pre/post processing.
 *
 * PUBLIC: int __rep_stat_pp __P((DB_ENV *, DB_REP_STAT **, u_int32_t));
 */
int
__rep_stat_pp(dbenv, statp, flags)
	DB_ENV *dbenv;
	DB_REP_STAT **statp;
	u_int32_t flags;
{
	int ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->rep_handle, "DB_ENV->rep_stat", DB_INIT_REP);

	if ((ret = __db_fchk(dbenv,
	    "DB_ENV->rep_stat", flags, DB_STAT_CLEAR)) != 0)
		return (ret);

	return (__rep_stat(dbenv, statp, flags));
}

/*
 * __rep_stat --
 *	DB_ENV->rep_stat.
 */
static int
__rep_stat(dbenv, statp, flags)
	DB_ENV *dbenv;
	DB_REP_STAT **statp;
	u_int32_t flags;
{
	DB_LOG *dblp;
	DB_REP *db_rep;
	DB_REP_STAT *stats;
	LOG *lp;
	REP *rep;
	u_int32_t queued;
	int dolock, ret;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	dblp = dbenv->lg_handle;
	lp = dblp->reginfo.primary;

	*statp = NULL;

	/* Allocate a stat struct to return to the user. */
	if ((ret = __os_umalloc(dbenv, sizeof(DB_REP_STAT), &stats)) != 0)
		return (ret);

	/*
	 * Read without holding the lock.  If we are in client recovery, we
	 * copy just the stats struct so we won't block.  We only copy out
	 * those stats that don't require acquiring any mutex.
	 */
	dolock = FLD_ISSET(rep->flags, REP_F_RECOVER_MASK) ? 0 : 1;
	memcpy(stats, &rep->stat, sizeof(*stats));

	/* Copy out election stats. */
	if (IN_ELECTION_TALLY(rep)) {
		if (F_ISSET(rep, REP_F_EPHASE1))
			stats->st_election_status = 1;
		else if (F_ISSET(rep, REP_F_EPHASE2))
			stats->st_election_status = 2;

		stats->st_election_nsites = rep->sites;
		stats->st_election_cur_winner = rep->winner;
		stats->st_election_priority = rep->w_priority;
		stats->st_election_gen = rep->w_gen;
		stats->st_election_lsn = rep->w_lsn;
		stats->st_election_votes = rep->votes;
		stats->st_election_tiebreaker = rep->w_tiebreaker;
	}

	/* Copy out other info that's protected by the rep mutex. */
	stats->st_env_id = rep->eid;
	stats->st_env_priority = rep->priority;
	stats->st_nsites = rep->nsites;
	stats->st_master = rep->master_id;
	stats->st_gen = rep->gen;
	stats->st_egen = rep->egen;

	if (F_ISSET(rep, REP_F_MASTER))
		stats->st_status = DB_REP_MASTER;
	else if (F_ISSET(rep, REP_F_CLIENT))
		stats->st_status = DB_REP_CLIENT;
	else
		stats->st_status = 0;

	if (LF_ISSET(DB_STAT_CLEAR)) {
		queued = rep->stat.st_log_queued;
		memset(&rep->stat, 0, sizeof(rep->stat));
		rep->stat.st_log_queued = rep->stat.st_log_queued_total =
		    rep->stat.st_log_queued_max = queued;
	}

	/*
	 * Log-related replication info is stored in the log system and
	 * protected by the log region lock.
	 */
	if (dolock)
		MUTEX_LOCK(dbenv, db_rep->db_mutexp);
	if (F_ISSET(rep, REP_F_CLIENT)) {
		stats->st_next_lsn = lp->ready_lsn;
		stats->st_waiting_lsn = lp->waiting_lsn;
		stats->st_next_pg = rep->ready_pg;
		stats->st_waiting_pg = rep->waiting_pg;
	} else {
		if (F_ISSET(rep, REP_F_MASTER))
			stats->st_next_lsn = lp->lsn;
		else
			ZERO_LSN(stats->st_next_lsn);
		ZERO_LSN(stats->st_waiting_lsn);
	}
	if (dolock)
		MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);

	*statp = stats;
	return (0);
}

/*
 * __rep_stat_print_pp --
 *	DB_ENV->rep_stat_print pre/post processing.
 *
 * PUBLIC: int __rep_stat_print_pp __P((DB_ENV *, u_int32_t));
 */
int
__rep_stat_print_pp(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	int ret;

	PANIC_CHECK(dbenv);
	ENV_REQUIRES_CONFIG(dbenv,
	    dbenv->rep_handle, "DB_ENV->rep_stat_print", DB_INIT_REP);

	if ((ret = __db_fchk(dbenv, "DB_ENV->rep_stat_print",
	    flags, DB_STAT_ALL | DB_STAT_CLEAR)) != 0)
		return (ret);

	return (__rep_stat_print(dbenv, flags));
}

/*
 * __rep_stat_print --
 *	DB_ENV->rep_stat_print method.
 *
 * PUBLIC: int __rep_stat_print __P((DB_ENV *, u_int32_t));
 */
int
__rep_stat_print(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	u_int32_t orig_flags;
	int ret;

	orig_flags = flags;
	LF_CLR(DB_STAT_CLEAR);
	if (flags == 0 || LF_ISSET(DB_STAT_ALL)) {
		ret = __rep_print_stats(dbenv, orig_flags);
		if (flags == 0 || ret != 0)
			return (ret);
	}

	if (LF_ISSET(DB_STAT_ALL) &&
	    (ret = __rep_print_all(dbenv, orig_flags)) != 0)
		return (ret);

	return (0);
}

/*
 * __rep_print_stats --
 *	Print out default statistics.
 */
static int
__rep_print_stats(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	DB_REP_STAT *sp;
	int is_client, ret;
	char *p;

	if ((ret = __rep_stat(dbenv, &sp, flags)) != 0)
		return (ret);

	if (LF_ISSET(DB_STAT_ALL))
		__db_msg(dbenv, "Default replication region information:");
	is_client = 0;
	switch (sp->st_status) {
	case DB_REP_MASTER:
		__db_msg(dbenv,
		    "Environment configured as a replication master");
		break;
	case DB_REP_CLIENT:
		__db_msg(dbenv,
		    "Environment configured as a replication client");
		is_client = 1;
		break;
	default:
		__db_msg(dbenv,
		    "Environment not configured for replication");
		break;
	}

	__db_msg(dbenv, "%lu/%lu\t%s",
	    (u_long)sp->st_next_lsn.file, (u_long)sp->st_next_lsn.offset,
	    is_client ? "Next LSN expected" : "Next LSN to be used");
	__db_msg(dbenv, "%lu/%lu\t%s",
	    (u_long)sp->st_waiting_lsn.file, (u_long)sp->st_waiting_lsn.offset,
	    sp->st_waiting_lsn.file == 0 ?
	    "Not waiting for any missed log records" :
	    "LSN of first log record we have after missed log records");

	__db_dl(dbenv, "Next page number expected.", (u_long)sp->st_next_pg);
	p = sp->st_waiting_pg == PGNO_INVALID ?
	    "Not waiting for any missed pages." :
	    "Page number of first page we have after missed pages.";
	__db_msg(dbenv, "%lu\t%s", (u_long)sp->st_waiting_pg, p);
	__db_dl(dbenv, "Number of duplicate master conditions detected.",
	    (u_long)sp->st_dupmasters);
	if (sp->st_env_id != DB_EID_INVALID)
		__db_dl(dbenv, "Current environment ID", (u_long)sp->st_env_id);
	else
		__db_msg(dbenv, "No current environment ID");
	__db_dl(dbenv,
	    "Current environment priority", (u_long)sp->st_env_priority);
	__db_dl(dbenv, "Current generation number", (u_long)sp->st_gen);
	__db_dl(dbenv,
	    "Current election generation number", (u_long)sp->st_egen);
	__db_dl(dbenv, "Number of duplicate log records received",
	    (u_long)sp->st_log_duplicated);
	__db_dl(dbenv, "Number of log records currently queued",
	    (u_long)sp->st_log_queued);
	__db_dl(dbenv, "Maximum number of log records ever queued at once",
	    (u_long)sp->st_log_queued_max);
	__db_dl(dbenv, "Total number of log records queued",
	    (u_long)sp->st_log_queued_total);
	__db_dl(dbenv,
	    "Number of log records received and appended to the log",
	    (u_long)sp->st_log_records);
	__db_dl(dbenv, "Number of log records missed and requested",
	    (u_long)sp->st_log_requested);
	if (sp->st_master != DB_EID_INVALID)
		__db_dl(dbenv, "Current master ID", (u_long)sp->st_master);
	else
		__db_msg(dbenv, "No current master ID");
	__db_dl(dbenv, "Number of times the master has changed",
	    (u_long)sp->st_master_changes);
	__db_dl(dbenv,
	    "Number of messages received with a bad generation number",
	    (u_long)sp->st_msgs_badgen);
	__db_dl(dbenv, "Number of messages received and processed",
	    (u_long)sp->st_msgs_processed);
	__db_dl(dbenv, "Number of messages ignored due to pending recovery",
	    (u_long)sp->st_msgs_recover);
	__db_dl(dbenv, "Number of failed message sends",
	    (u_long)sp->st_msgs_send_failures);
	__db_dl(dbenv, "Number of messages sent", (u_long)sp->st_msgs_sent);
	__db_dl(dbenv,
	    "Number of new site messages received", (u_long)sp->st_newsites);
	__db_dl(dbenv,
	    "Number of environments believed to be in the replication group",
	    (u_long)sp->st_nsites);
	__db_dl(dbenv, "Transmission limited", (u_long)sp->st_nthrottles);
	__db_dl(dbenv, "Number of outdated conditions detected",
	    (u_long)sp->st_outdated);
	__db_dl(dbenv, "Number of duplicate page records received",
	    (u_long)sp->st_pg_duplicated);
	__db_dl(dbenv, "Number of page records received and added to databases",
	    (u_long)sp->st_pg_records);
	__db_dl(dbenv, "Number of page records missed and requested",
	    (u_long)sp->st_pg_requested);
	if (sp->st_startup_complete == 0)
		__db_msg(dbenv, "Startup incomplete");
	else
		__db_msg(dbenv, "Startup complete");
	__db_dl(dbenv,
	    "Number of transactions applied", (u_long)sp->st_txns_applied);

	__db_dl(dbenv, "Number of elections held", (u_long)sp->st_elections);
	__db_dl(dbenv,
	    "Number of elections won", (u_long)sp->st_elections_won);

	if (sp->st_election_status == 0)
		__db_msg(dbenv, "No election in progress");
	else {
		__db_dl(dbenv, "Current election phase",
		    (u_long)sp->st_election_status);
		__db_dl(dbenv, "Election winner",
		    (u_long)sp->st_election_cur_winner);
		__db_dl(dbenv, "Election generation number",
		    (u_long)sp->st_election_gen);
		__db_msg(dbenv, "%lu/%lu\tMaximum LSN of election winner",
		    (u_long)sp->st_election_lsn.file,
		    (u_long)sp->st_election_lsn.offset);
		__db_dl(dbenv,
		    "Number of sites expected to participate in elections",
		    (u_long)sp->st_election_nsites);
		__db_dl(dbenv, "Number of votes needed to win an election",
		    (u_long)sp->st_election_nvotes);
		__db_dl(dbenv,
		    "Election priority", (u_long)sp->st_election_priority);
		__db_dl(dbenv, "Election tiebreaker value",
		    (u_long)sp->st_election_tiebreaker);
		__db_dl(dbenv, "Votes received this election round",
		    (u_long)sp->st_election_votes);
	}

	__os_ufree(dbenv, sp);

	return (0);
}

/*
 * __rep_print_all --
 *	Display debugging replication region statistics.
 */
static int
__rep_print_all(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	static const FN rep_fn[] = {
		{ REP_F_CLIENT,		"REP_F_CLIENT" },
		{ REP_F_EPHASE1,	"REP_F_EPHASE1" },
		{ REP_F_EPHASE2,	"REP_F_EPHASE2" },
		{ REP_F_MASTER,		"REP_F_MASTER" },
		{ REP_F_MASTERELECT,	"REP_F_MASTERELECT" },
		{ REP_F_NOARCHIVE,	"REP_F_NOARCHIVE" },
		{ REP_F_READY,		"REP_F_READY" },
		{ REP_F_RECOVER_LOG,	"REP_F_RECOVER_LOG" },
		{ REP_F_RECOVER_PAGE,	"REP_F_RECOVER_PAGE" },
		{ REP_F_RECOVER_UPDATE,	"REP_F_RECOVER_UPDATE" },
		{ REP_F_RECOVER_VERIFY,	"REP_F_RECOVER_VERIFY" },
		{ REP_F_TALLY,		"REP_F_TALLY" },
		{ 0,			NULL }
	};
	static const FN dbrep_fn[] = {
		{ DBREP_OPENFILES,	"DBREP_OPENFILES" },
		{ 0,			NULL }
	};
	DB_LOG *dblp;
	DB_REP *db_rep;
	LOG *lp;
	REGENV *renv;
	REGINFO *infop;
	REP *rep;

	db_rep = dbenv->rep_handle;
	rep = db_rep->region;
	infop = dbenv->reginfo;
	renv = infop->primary;

	__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
	__db_msg(dbenv, "DB_REP handle information:");
	__db_print_mutex(dbenv, NULL,
	    db_rep->rep_mutexp, "Replication region mutex", flags);
	__db_print_mutex(dbenv, NULL,
	    db_rep->db_mutexp, "Bookkeeping database mutex", flags);

	if (db_rep->rep_db == NULL)
		STAT_ISSET("Bookkeeping database", db_rep->rep_db);
	else
		(void)__db_stat_print(db_rep->rep_db, flags);

	__db_prflags(dbenv, NULL, db_rep->flags, dbrep_fn, NULL, "\tFlags");

	__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
	__db_msg(dbenv, "REP handle information:");
	__db_print_mutex(dbenv, NULL, &rep->mutex, "REP mutex", flags);

	STAT_LONG("Environment ID", rep->eid);
	STAT_LONG("Master environment ID", rep->master_id);
	STAT_ULONG("Election generation", rep->egen);
	STAT_ULONG("Election generation number", rep->gen);
	STAT_ULONG("Last generation number in log", rep->recover_gen);
	STAT_LONG("Space allocated for sites", rep->asites);
	STAT_LONG("Sites in group", rep->nsites);
	STAT_LONG("Votes needed for election", rep->nvotes);
	STAT_LONG("Priority in election", rep->priority);
	__db_dlbytes(dbenv, "Limit on data sent in a single call",
	    rep->gbytes, (u_long)0, rep->bytes);
	STAT_ULONG("Request gap", rep->request_gap);
	STAT_ULONG("Maximum gap", rep->max_gap);

	STAT_LONG("Thread is in rep_elect", rep->elect_th);
	STAT_ULONG("Callers in rep_proc_msg", rep->msg_th);
	STAT_LONG("Thread is in rep_start", rep->start_th);
	STAT_ULONG("Library handle count", rep->handle_cnt);
	STAT_ULONG("Multi-step operation count", rep->op_cnt);
	STAT_LONG("Running recovery", rep->in_recovery);
	__db_msg(dbenv, "%.24s\tRecovery timestamp",
	    renv->rep_timestamp == 0 ? "0" : ctime(&renv->rep_timestamp));

	STAT_LONG("Sites heard from", rep->sites);
	STAT_LONG("Current winner", rep->winner);
	STAT_LONG("Winner priority", rep->w_priority);
	STAT_ULONG("Winner generation", rep->w_gen);
	STAT_LSN("Winner LSN", &rep->w_lsn);
	STAT_LONG("Winner tiebreaker", rep->w_tiebreaker);
	STAT_LONG("Votes for this site", rep->votes);

	__db_prflags(dbenv, NULL, rep->flags, rep_fn, NULL, "\tFlags");

	__db_msg(dbenv, "%s", DB_GLOBAL(db_line));
	__db_msg(dbenv, "LOG replication information:");
	MUTEX_LOCK(dbenv, db_rep->db_mutexp);
	dblp = dbenv->lg_handle;
	lp = (LOG *)dblp->reginfo.primary;
	STAT_LSN("First log record after a gap", &lp->waiting_lsn);
	STAT_LSN("LSN waiting to verify", &lp->verify_lsn);
	STAT_LSN("Maximum LSN requested", &lp->max_wait_lsn);
	STAT_ULONG("Records to wait before requesting", lp->wait_recs);
	STAT_ULONG("Records received while waiting", lp->rcvd_recs);
	STAT_LSN("Next LSN expected", &lp->ready_lsn);
	MUTEX_UNLOCK(dbenv, db_rep->db_mutexp);

	return (0);
}

#else /* !HAVE_STATISTICS */

int
__rep_stat_pp(dbenv, statp, flags)
	DB_ENV *dbenv;
	DB_REP_STAT **statp;
	u_int32_t flags;
{
	COMPQUIET(statp, NULL);
	COMPQUIET(flags, 0);

	return (__db_stat_not_built(dbenv));
}

int
__rep_stat_print_pp(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	COMPQUIET(flags, 0);

	return (__db_stat_not_built(dbenv));
}
#endif
