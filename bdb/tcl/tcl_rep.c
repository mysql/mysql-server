/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: tcl_rep.c,v 11.85 2002/08/06 04:45:44 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#include <tcl.h>
#endif

#include "db_int.h"
#include "dbinc/tcl_db.h"

#if CONFIG_TEST
/*
 * tcl_RepElect --
 *	Call DB_ENV->rep_elect().
 *
 * PUBLIC: int tcl_RepElect
 * PUBLIC:     __P((Tcl_Interp *, int, Tcl_Obj * CONST *, DB_ENV *));
 */
int
tcl_RepElect(interp, objc, objv, dbenv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *dbenv;			/* Environment pointer */
{
	int eid, nsites, pri, result, ret;
	u_int32_t timeout;

	if (objc != 5) {
		Tcl_WrongNumArgs(interp, 5, objv, "nsites pri timeout");
		return (TCL_ERROR);
	}

	if ((result = Tcl_GetIntFromObj(interp, objv[2], &nsites)) != TCL_OK)
		return (result);
	if ((result = Tcl_GetIntFromObj(interp, objv[3], &pri)) != TCL_OK)
		return (result);
	if ((result = _GetUInt32(interp, objv[4], &timeout)) != TCL_OK)
		return (result);

	_debug_check();
	if ((ret = dbenv->rep_elect(dbenv, nsites, pri, timeout, &eid)) != 0)
		return (_ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "env rep_elect"));

	Tcl_SetObjResult(interp, Tcl_NewIntObj(eid));

	return (TCL_OK);
}
#endif

#if CONFIG_TEST
/*
 * tcl_RepFlush --
 *	Call DB_ENV->rep_flush().
 *
 * PUBLIC: int tcl_RepFlush
 * PUBLIC:     __P((Tcl_Interp *, int, Tcl_Obj * CONST *, DB_ENV *));
 */
int
tcl_RepFlush(interp, objc, objv, dbenv)
	Tcl_Interp *interp;
	int objc;
	Tcl_Obj *CONST objv[];
	DB_ENV *dbenv;
{
	int ret;

	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 2, objv, "");
		return TCL_ERROR;
	}

	_debug_check();
	ret = dbenv->rep_flush(dbenv);
	return (_ReturnSetup(interp, ret, DB_RETOK_STD(ret), "env rep_flush"));
}
#endif
#if CONFIG_TEST
/*
 * tcl_RepLimit --
 *	Call DB_ENV->set_rep_limit().
 *
 * PUBLIC: int tcl_RepLimit
 * PUBLIC:     __P((Tcl_Interp *, int, Tcl_Obj * CONST *, DB_ENV *));
 */
int
tcl_RepLimit(interp, objc, objv, dbenv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *dbenv;			/* Environment pointer */
{
	int result, ret;
	u_int32_t bytes, gbytes;

	if (objc != 4) {
		Tcl_WrongNumArgs(interp, 4, objv, "gbytes bytes");
		return (TCL_ERROR);
	}

	if ((result = _GetUInt32(interp, objv[2], &gbytes)) != TCL_OK)
		return (result);
	if ((result = _GetUInt32(interp, objv[3], &bytes)) != TCL_OK)
		return (result);

	_debug_check();
	if ((ret = dbenv->set_rep_limit(dbenv, gbytes, bytes)) != 0)
		return (_ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "env set_rep_limit"));

	return (_ReturnSetup(interp,
	    ret, DB_RETOK_STD(ret), "env set_rep_limit"));
}
#endif

#if CONFIG_TEST
/*
 * tcl_RepRequest --
 *	Call DB_ENV->set_rep_request().
 *
 * PUBLIC: int tcl_RepRequest
 * PUBLIC:     __P((Tcl_Interp *, int, Tcl_Obj * CONST *, DB_ENV *));
 */
int
tcl_RepRequest(interp, objc, objv, dbenv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *dbenv;			/* Environment pointer */
{
	int result, ret;
	u_int32_t min, max;

	if (objc != 4) {
		Tcl_WrongNumArgs(interp, 4, objv, "min max");
		return (TCL_ERROR);
	}

	if ((result = _GetUInt32(interp, objv[2], &min)) != TCL_OK)
		return (result);
	if ((result = _GetUInt32(interp, objv[3], &max)) != TCL_OK)
		return (result);

	_debug_check();
	if ((ret = dbenv->set_rep_request(dbenv, min, max)) != 0)
		return (_ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "env set_rep_request"));

	return (_ReturnSetup(interp,
	    ret, DB_RETOK_STD(ret), "env set_rep_request"));
}
#endif

#if CONFIG_TEST
/*
 * tcl_RepStart --
 *	Call DB_ENV->rep_start().
 *
 * PUBLIC: int tcl_RepStart
 * PUBLIC:     __P((Tcl_Interp *, int, Tcl_Obj * CONST *, DB_ENV *));
 *
 *	Note that this normally can/should be achieved as an argument to
 * berkdb env, but we need to test forcible upgrading of clients, which
 * involves calling this on an open environment handle.
 */
int
tcl_RepStart(interp, objc, objv, dbenv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *dbenv;
{
	static char *tclrpstrt[] = {
		"-client",
		"-master",
		NULL
	};
	enum tclrpstrt {
		TCL_RPSTRT_CLIENT,
		TCL_RPSTRT_MASTER
	};
	char *arg;
	int i, optindex, ret;
	u_int32_t flag;

	flag = 0;

	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 3, objv, "[-master/-client]");
		return (TCL_ERROR);
	}

	i = 2;
	while (i < objc) {
		if (Tcl_GetIndexFromObj(interp, objv[i], tclrpstrt,
		    "option", TCL_EXACT, &optindex) != TCL_OK) {
			arg = Tcl_GetStringFromObj(objv[i], NULL);
			if (arg[0] == '-')
				return (IS_HELP(objv[i]));
			else
				Tcl_ResetResult(interp);
			break;
		}
		i++;
		switch ((enum tclrpstrt)optindex) {
		case TCL_RPSTRT_CLIENT:
			flag |= DB_REP_CLIENT;
			break;
		case TCL_RPSTRT_MASTER:
			flag |= DB_REP_MASTER;
			break;
		}
	}

	_debug_check();
	ret = dbenv->rep_start(dbenv, NULL, flag);
	return (_ReturnSetup(interp, ret, DB_RETOK_STD(ret), "env rep_start"));
}
#endif

#if CONFIG_TEST
/*
 * tcl_RepProcessMessage --
 *	Call DB_ENV->rep_process_message().
 *
 * PUBLIC: int tcl_RepProcessMessage
 * PUBLIC:     __P((Tcl_Interp *, int, Tcl_Obj * CONST *, DB_ENV *));
 */
int
tcl_RepProcessMessage(interp, objc, objv, dbenv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *dbenv;			/* Environment pointer */
{
	DBT control, rec;
	Tcl_Obj *res;
	void *ctmp, *rtmp;
	int eid;
	int freectl, freerec, result, ret;

	if (objc != 5) {
		Tcl_WrongNumArgs(interp, 5, objv, "id control rec");
		return (TCL_ERROR);
	}
	freectl = freerec = 0;

	memset(&control, 0, sizeof(control));
	memset(&rec, 0, sizeof(rec));

	if ((result = Tcl_GetIntFromObj(interp, objv[2], &eid)) != TCL_OK)
		return (result);

	ret = _CopyObjBytes(interp, objv[3], &ctmp,
	    &control.size, &freectl);
	if (ret != 0) {
		result = _ReturnSetup(interp, ret,
		    DB_RETOK_REPPMSG(ret), "rep_proc_msg");
		return (result);
	}
	control.data = ctmp;
	ret = _CopyObjBytes(interp, objv[4], &rtmp,
	    &rec.size, &freerec);
	if (ret != 0) {
		result = _ReturnSetup(interp, ret,
		    DB_RETOK_REPPMSG(ret), "rep_proc_msg");
		goto out;
	}
	rec.data = rtmp;
	_debug_check();
	ret = dbenv->rep_process_message(dbenv, &control, &rec, &eid);
	result = _ReturnSetup(interp, ret, DB_RETOK_REPPMSG(ret),
	    "env rep_process_message");

	/*
	 * If we have a new master, return its environment ID.
	 *
	 * XXX
	 * We should do something prettier to differentiate success
	 * from an env ID, and figure out how to represent HOLDELECTION.
	 */
	if (result == TCL_OK && ret == DB_REP_NEWMASTER) {
		res = Tcl_NewIntObj(eid);
		Tcl_SetObjResult(interp, res);
	}
out:
	if (freectl)
		(void)__os_free(NULL, ctmp);
	if (freerec)
		(void)__os_free(NULL, rtmp);

	return (result);
}
#endif

#if CONFIG_TEST
/*
 * tcl_RepStat --
 *	Call DB_ENV->rep_stat().
 *
 * PUBLIC: int tcl_RepStat
 * PUBLIC:     __P((Tcl_Interp *, int, Tcl_Obj * CONST *, DB_ENV *));
 */
int
tcl_RepStat(interp, objc, objv, dbenv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *dbenv;
{
	DB_REP_STAT *sp;
	Tcl_Obj *myobjv[2], *res, *thislist, *lsnlist;
	u_int32_t flag;
	int myobjc, result, ret;
	char *arg;

	result = TCL_OK;
	flag = 0;

	if (objc > 3) {
		Tcl_WrongNumArgs(interp, 2, objv, NULL);
		return (TCL_ERROR);
	}
	if (objc == 3) {
		arg = Tcl_GetStringFromObj(objv[2], NULL);
		if (strcmp(arg, "-clear") == 0)
			flag = DB_STAT_CLEAR;
		else {
			Tcl_SetResult(interp,
			    "db stat: unknown arg", TCL_STATIC);
			return (TCL_ERROR);
		}
	}

	_debug_check();
	ret = dbenv->rep_stat(dbenv, &sp, flag);
	result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
	    "rep stat");
	if (result == TCL_ERROR)
		return (result);

	/*
	 * Have our stats, now construct the name value
	 * list pairs and free up the memory.
	 */
	res = Tcl_NewObj();
	/*
	 * MAKE_STAT_* assumes 'res' and 'error' label.
	 */
	MAKE_STAT_LSN("Next LSN expected", &sp->st_next_lsn);
	MAKE_STAT_LSN("First missed LSN", &sp->st_waiting_lsn);
	MAKE_STAT_LIST("Duplicate master conditions", sp->st_dupmasters);
	MAKE_STAT_LIST("Environment ID", sp->st_env_id);
	MAKE_STAT_LIST("Environment priority", sp->st_env_priority);
	MAKE_STAT_LIST("Generation number", sp->st_gen);
	MAKE_STAT_LIST("Duplicate log records received", sp->st_log_duplicated);
	MAKE_STAT_LIST("Current log records queued", sp->st_log_queued);
	MAKE_STAT_LIST("Maximum log records queued", sp->st_log_queued_max);
	MAKE_STAT_LIST("Total log records queued", sp->st_log_queued_total);
	MAKE_STAT_LIST("Log records received", sp->st_log_records);
	MAKE_STAT_LIST("Log records requested", sp->st_log_requested);
	MAKE_STAT_LIST("Master environment ID", sp->st_master);
	MAKE_STAT_LIST("Master changes", sp->st_master_changes);
	MAKE_STAT_LIST("Messages with bad generation number",
	    sp->st_msgs_badgen);
	MAKE_STAT_LIST("Messages processed", sp->st_msgs_processed);
	MAKE_STAT_LIST("Messages ignored for recovery", sp->st_msgs_recover);
	MAKE_STAT_LIST("Message send failures", sp->st_msgs_send_failures);
	MAKE_STAT_LIST("Messages sent", sp->st_msgs_sent);
	MAKE_STAT_LIST("New site messages", sp->st_newsites);
	MAKE_STAT_LIST("Transmission limited", sp->st_nthrottles);
	MAKE_STAT_LIST("Outdated conditions", sp->st_outdated);
	MAKE_STAT_LIST("Transactions applied", sp->st_txns_applied);
	MAKE_STAT_LIST("Elections held", sp->st_elections);
	MAKE_STAT_LIST("Elections won", sp->st_elections_won);
	MAKE_STAT_LIST("Election phase", sp->st_election_status);
	MAKE_STAT_LIST("Election winner", sp->st_election_cur_winner);
	MAKE_STAT_LIST("Election generation number", sp->st_election_gen);
	MAKE_STAT_LSN("Election max LSN", &sp->st_election_lsn);
	MAKE_STAT_LIST("Election sites", sp->st_election_nsites);
	MAKE_STAT_LIST("Election priority", sp->st_election_priority);
	MAKE_STAT_LIST("Election tiebreaker", sp->st_election_tiebreaker);
	MAKE_STAT_LIST("Election votes", sp->st_election_votes);

	Tcl_SetObjResult(interp, res);
error:
	free(sp);
	return (result);
}
#endif
