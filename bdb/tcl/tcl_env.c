/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: tcl_env.c,v 11.33 2001/01/11 18:19:55 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <tcl.h>
#endif

#include "db_int.h"
#include "tcl_db.h"

/*
 * Prototypes for procedures defined later in this file:
 */
static void	_EnvInfoDelete __P((Tcl_Interp *, DBTCL_INFO *));

/*
 * PUBLIC: int env_Cmd __P((ClientData, Tcl_Interp *, int, Tcl_Obj * CONST*));
 *
 * env_Cmd --
 *	Implements the "env" command.
 */
int
env_Cmd(clientData, interp, objc, objv)
	ClientData clientData;          /* Env handle */
	Tcl_Interp *interp;             /* Interpreter */
	int objc;                       /* How many arguments? */
	Tcl_Obj *CONST objv[];          /* The argument objects */
{
	static char *envcmds[] = {
		"close",
		"lock_detect",
		"lock_id",
		"lock_get",
		"lock_stat",
		"lock_vec",
		"log_archive",
		"log_compare",
		"log_file",
		"log_flush",
		"log_get",
		"log_put",
		"log_register",
		"log_stat",
		"log_unregister",
		"mpool",
		"mpool_stat",
		"mpool_sync",
		"mpool_trickle",
		"mutex",
#if	CONFIG_TEST
		"test",
#endif
		"txn",
		"txn_checkpoint",
		"txn_stat",
		"verbose",
		NULL
	};
	enum envcmds {
		ENVCLOSE,
		ENVLKDETECT,
		ENVLKID,
		ENVLKGET,
		ENVLKSTAT,
		ENVLKVEC,
		ENVLOGARCH,
		ENVLOGCMP,
		ENVLOGFILE,
		ENVLOGFLUSH,
		ENVLOGGET,
		ENVLOGPUT,
		ENVLOGREG,
		ENVLOGSTAT,
		ENVLOGUNREG,
		ENVMP,
		ENVMPSTAT,
		ENVMPSYNC,
		ENVTRICKLE,
		ENVMUTEX,
#if	CONFIG_TEST
		ENVTEST,
#endif
		ENVTXN,
		ENVTXNCKP,
		ENVTXNSTAT,
		ENVVERB
	};
	DBTCL_INFO *envip;
	DB_ENV *envp;
	Tcl_Obj *res;
	u_int32_t newval;
	int cmdindex, result, ret;

	Tcl_ResetResult(interp);
	envp = (DB_ENV *)clientData;
	envip = _PtrToInfo((void *)envp);
	result = TCL_OK;

	if (objc <= 1) {
		Tcl_WrongNumArgs(interp, 1, objv, "command cmdargs");
		return (TCL_ERROR);
	}
	if (envp == NULL) {
		Tcl_SetResult(interp, "NULL env pointer", TCL_STATIC);
		return (TCL_ERROR);
	}
	if (envip == NULL) {
		Tcl_SetResult(interp, "NULL env info pointer", TCL_STATIC);
		return (TCL_ERROR);
	}

	/*
	 * Get the command name index from the object based on the berkdbcmds
	 * defined above.
	 */
	if (Tcl_GetIndexFromObj(interp, objv[1], envcmds, "command",
	    TCL_EXACT, &cmdindex) != TCL_OK)
		return (IS_HELP(objv[1]));
	res = NULL;
	switch ((enum envcmds)cmdindex) {
	case ENVCLOSE:
		/*
		 * No args for this.  Error if there are some.
		 */
		if (objc > 2) {
			Tcl_WrongNumArgs(interp, 2, objv, NULL);
			return (TCL_ERROR);
		}
		/*
		 * Any transactions will be aborted, and an mpools
		 * closed automatically.  We must delete any txn
		 * and mp widgets we have here too for this env.
		 * NOTE: envip is freed when we come back from
		 * this function.  Set it to NULL to make sure no
		 * one tries to use it later.
		 */
		_EnvInfoDelete(interp, envip);
		envip = NULL;
		_debug_check();
		ret = envp->close(envp, 0);
		result = _ReturnSetup(interp, ret, "env close");
		break;
	case ENVLKDETECT:
		result = tcl_LockDetect(interp, objc, objv, envp);
		break;
	case ENVLKSTAT:
		result = tcl_LockStat(interp, objc, objv, envp);
		break;
	case ENVLKID:
		/*
		 * No args for this.  Error if there are some.
		 */
		if (objc > 2) {
			Tcl_WrongNumArgs(interp, 2, objv, NULL);
			return (TCL_ERROR);
		}
		_debug_check();
		ret = lock_id(envp, &newval);
		result = _ReturnSetup(interp, ret, "lock_id");
		if (result == TCL_OK)
			res = Tcl_NewIntObj((int)newval);
		break;
	case ENVLKGET:
		result = tcl_LockGet(interp, objc, objv, envp);
		break;
	case ENVLKVEC:
		result = tcl_LockVec(interp, objc, objv, envp);
		break;
	case ENVLOGARCH:
		result = tcl_LogArchive(interp, objc, objv, envp);
		break;
	case ENVLOGCMP:
		result = tcl_LogCompare(interp, objc, objv);
		break;
	case ENVLOGFILE:
		result = tcl_LogFile(interp, objc, objv, envp);
		break;
	case ENVLOGFLUSH:
		result = tcl_LogFlush(interp, objc, objv, envp);
		break;
	case ENVLOGGET:
		result = tcl_LogGet(interp, objc, objv, envp);
		break;
	case ENVLOGPUT:
		result = tcl_LogPut(interp, objc, objv, envp);
		break;
	case ENVLOGREG:
		result = tcl_LogRegister(interp, objc, objv, envp);
		break;
	case ENVLOGUNREG:
		result = tcl_LogUnregister(interp, objc, objv, envp);
		break;
	case ENVLOGSTAT:
		result = tcl_LogStat(interp, objc, objv, envp);
		break;
	case ENVMPSTAT:
		result = tcl_MpStat(interp, objc, objv, envp);
		break;
	case ENVMPSYNC:
		result = tcl_MpSync(interp, objc, objv, envp);
		break;
	case ENVTRICKLE:
		result = tcl_MpTrickle(interp, objc, objv, envp);
		break;
	case ENVMP:
		result = tcl_Mp(interp, objc, objv, envp, envip);
		break;
	case ENVTXNCKP:
		result = tcl_TxnCheckpoint(interp, objc, objv, envp);
		break;
	case ENVTXNSTAT:
		result = tcl_TxnStat(interp, objc, objv, envp);
		break;
	case ENVTXN:
		result = tcl_Txn(interp, objc, objv, envp, envip);
		break;
	case ENVMUTEX:
		result = tcl_Mutex(interp, objc, objv, envp, envip);
		break;
#if	CONFIG_TEST
	case ENVTEST:
		result = tcl_EnvTest(interp, objc, objv, envp);
		break;
#endif
	case ENVVERB:
		/*
		 * Two args for this.  Error if different.
		 */
		if (objc != 4) {
			Tcl_WrongNumArgs(interp, 2, objv, NULL);
			return (TCL_ERROR);
		}
		result = tcl_EnvVerbose(interp, envp, objv[2], objv[3]);
		break;
	}
	/*
	 * Only set result if we have a res.  Otherwise, lower
	 * functions have already done so.
	 */
	if (result == TCL_OK && res)
		Tcl_SetObjResult(interp, res);
	return (result);
}

/*
 * PUBLIC: int tcl_EnvRemove __P((Tcl_Interp *, int, Tcl_Obj * CONST*,
 * PUBLIC:      DB_ENV *, DBTCL_INFO *));
 *
 * tcl_EnvRemove --
 */
int
tcl_EnvRemove(interp, objc, objv, envp, envip)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *envp;			/* Env pointer */
	DBTCL_INFO *envip;		/* Info pointer */
{
	static char *envremopts[] = {
		"-data_dir",
		"-force",
		"-home",
		"-log_dir",
		"-server",
		"-tmp_dir",
		"-use_environ",
		"-use_environ_root",
		NULL
	};
	enum envremopts {
		ENVREM_DATADIR,
		ENVREM_FORCE,
		ENVREM_HOME,
		ENVREM_LOGDIR,
		ENVREM_SERVER,
		ENVREM_TMPDIR,
		ENVREM_USE_ENVIRON,
		ENVREM_USE_ENVIRON_ROOT
	};
	DB_ENV *e;
	u_int32_t cflag, flag, forceflag;
	int i, optindex, result, ret;
	char *datadir, *home, *logdir, *server, *tmpdir;

	result = TCL_OK;
	cflag = flag = forceflag = 0;
	home = NULL;
	datadir = logdir = tmpdir = NULL;
	server = NULL;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 2, objv, "?args?");
		return (TCL_ERROR);
	}

	i = 2;
	while (i < objc) {
		if (Tcl_GetIndexFromObj(interp, objv[i], envremopts, "option",
		    TCL_EXACT, &optindex) != TCL_OK) {
			result = IS_HELP(objv[i]);
			goto error;
		}
		i++;
		switch ((enum envremopts)optindex) {
		case ENVREM_FORCE:
			forceflag |= DB_FORCE;
			break;
		case ENVREM_HOME:
			/* Make sure we have an arg to check against! */
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-home dir?");
				result = TCL_ERROR;
				break;
			}
			home = Tcl_GetStringFromObj(objv[i++], NULL);
			break;
		case ENVREM_SERVER:
			/* Make sure we have an arg to check against! */
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-server name?");
				result = TCL_ERROR;
				break;
			}
			server = Tcl_GetStringFromObj(objv[i++], NULL);
			cflag = DB_CLIENT;
			break;
		case ENVREM_USE_ENVIRON:
			flag |= DB_USE_ENVIRON;
			break;
		case ENVREM_USE_ENVIRON_ROOT:
			flag |= DB_USE_ENVIRON_ROOT;
			break;
		case ENVREM_DATADIR:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "-data_dir dir");
				result = TCL_ERROR;
				break;
			}
			datadir = Tcl_GetStringFromObj(objv[i++], NULL);
			break;
		case ENVREM_LOGDIR:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "-log_dir dir");
				result = TCL_ERROR;
				break;
			}
			logdir = Tcl_GetStringFromObj(objv[i++], NULL);
			break;
		case ENVREM_TMPDIR:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "-tmp_dir dir");
				result = TCL_ERROR;
				break;
			}
			tmpdir = Tcl_GetStringFromObj(objv[i++], NULL);
			break;
		}
		/*
		 * If, at any time, parsing the args we get an error,
		 * bail out and return.
		 */
		if (result != TCL_OK)
			goto error;
	}

	/*
	 * If envp is NULL, we don't have an open env and we need to open
	 * one of the user.  Don't bother with the info stuff.
	 */
	if (envp == NULL) {
		if ((ret = db_env_create(&e, cflag)) != 0) {
			result = _ReturnSetup(interp, ret, "db_env_create");
			goto error;
		}
		if (server != NULL) {
			ret = e->set_server(e, server, 0, 0, 0);
			result = _ReturnSetup(interp, ret, "set_server");
			if (result != TCL_OK)
				goto error;
		}
		if (datadir != NULL) {
			_debug_check();
			ret = e->set_data_dir(e, datadir);
			result = _ReturnSetup(interp, ret, "set_data_dir");
			if (result != TCL_OK)
				goto error;
		}
		if (logdir != NULL) {
			_debug_check();
			ret = e->set_lg_dir(e, logdir);
			result = _ReturnSetup(interp, ret, "set_log_dir");
			if (result != TCL_OK)
				goto error;
		}
		if (tmpdir != NULL) {
			_debug_check();
			ret = e->set_tmp_dir(e, tmpdir);
			result = _ReturnSetup(interp, ret, "set_tmp_dir");
			if (result != TCL_OK)
				goto error;
		}
	} else {
		/*
		 * We have to clean up any info associated with this env,
		 * regardless of the result of the remove so do it first.
		 * NOTE: envip is freed when we come back from this function.
		 */
		_EnvInfoDelete(interp, envip);
		envip = NULL;
		e = envp;
	}

	flag |= forceflag;
	/*
	 * When we get here we have parsed all the args.  Now remove
	 * the environment.
	 */
	_debug_check();
	ret = e->remove(e, home, flag);
	result = _ReturnSetup(interp, ret, "env remove");
error:
	return (result);
}

static void
_EnvInfoDelete(interp, envip)
	Tcl_Interp *interp;		/* Tcl Interpreter */
	DBTCL_INFO *envip;		/* Info for env */
{
	DBTCL_INFO *nextp, *p;

	/*
	 * Before we can delete the environment info, we must close
	 * any open subsystems in this env.  We will:
	 * 1.  Abort any transactions (which aborts any nested txns).
	 * 2.  Close any mpools (which will put any pages itself).
	 * 3.  Put any locks.
	 * 4.  Close the error file.
	 */
	for (p = LIST_FIRST(&__db_infohead); p != NULL; p = nextp) {
		/*
		 * Check if this info structure "belongs" to this
		 * env.  If so, remove its commands and info structure.
		 * We do not close/abort/whatever here, because we
		 * don't want to replicate DB behavior.
		 */
		 if (p->i_parent == envip) {
			switch (p->i_type) {
			case I_TXN:
				_TxnInfoDelete(interp, p);
				break;
			case I_MP:
				_MpInfoDelete(interp, p);
				break;
			default:
				Tcl_SetResult(interp,
				    "_EnvInfoDelete: bad info type",
				    TCL_STATIC);
				break;
			}
			nextp = LIST_NEXT(p, entries);
			(void)Tcl_DeleteCommand(interp, p->i_name);
			_DeleteInfo(p);
		} else
			nextp = LIST_NEXT(p, entries);
	}
	(void)Tcl_DeleteCommand(interp, envip->i_name);
	_DeleteInfo(envip);
}

/*
 * PUBLIC: int tcl_EnvVerbose __P((Tcl_Interp *, DB_ENV *, Tcl_Obj *,
 * PUBLIC:    Tcl_Obj *));
 *
 * tcl_EnvVerbose --
 */
int
tcl_EnvVerbose(interp, envp, which, onoff)
	Tcl_Interp *interp;		/* Interpreter */
	DB_ENV *envp;			/* Env pointer */
	Tcl_Obj *which;			/* Which subsystem */
	Tcl_Obj *onoff;			/* On or off */
{
	static char *verbwhich[] = {
		"chkpt",
		"deadlock",
		"recovery",
		"wait",
		NULL
	};
	enum verbwhich {
		ENVVERB_CHK,
		ENVVERB_DEAD,
		ENVVERB_REC,
		ENVVERB_WAIT
	};
	static char *verbonoff[] = {
		"off",
		"on",
		NULL
	};
	enum verbonoff {
		ENVVERB_OFF,
		ENVVERB_ON
	};
	int on, optindex, ret;
	u_int32_t wh;

	if (Tcl_GetIndexFromObj(interp, which, verbwhich, "option",
	    TCL_EXACT, &optindex) != TCL_OK)
		return (IS_HELP(which));

	switch ((enum verbwhich)optindex) {
	case ENVVERB_CHK:
		wh = DB_VERB_CHKPOINT;
		break;
	case ENVVERB_DEAD:
		wh = DB_VERB_DEADLOCK;
		break;
	case ENVVERB_REC:
		wh = DB_VERB_RECOVERY;
		break;
	case ENVVERB_WAIT:
		wh = DB_VERB_WAITSFOR;
		break;
	default:
		return (TCL_ERROR);
	}
	if (Tcl_GetIndexFromObj(interp, onoff, verbonoff, "option",
	    TCL_EXACT, &optindex) != TCL_OK)
		return (IS_HELP(onoff));
	switch ((enum verbonoff)optindex) {
	case ENVVERB_OFF:
		on = 0;
		break;
	case ENVVERB_ON:
		on = 1;
		break;
	default:
		return (TCL_ERROR);
	}
	ret = envp->set_verbose(envp, wh, on);
	return (_ReturnSetup(interp, ret, "env set verbose"));
}

#if	CONFIG_TEST
/*
 * PUBLIC: int tcl_EnvTest __P((Tcl_Interp *, int, Tcl_Obj * CONST*, DB_ENV *));
 *
 * tcl_EnvTest --
 */
int
tcl_EnvTest(interp, objc, objv, envp)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *envp;			/* Env pointer */
{
	static char *envtestcmd[] = {
		"abort",
		"copy",
		NULL
	};
	enum envtestcmd {
		ENVTEST_ABORT,
		ENVTEST_COPY
	};
	static char *envtestat[] = {
		"none",
		"preopen",
		"prerename",
		"postlog",
		"postlogmeta",
		"postopen",
		"postrename",
		"postsync",
		NULL
	};
	enum envtestat {
		ENVTEST_NONE,
		ENVTEST_PREOPEN,
		ENVTEST_PRERENAME,
		ENVTEST_POSTLOG,
		ENVTEST_POSTLOGMETA,
		ENVTEST_POSTOPEN,
		ENVTEST_POSTRENAME,
		ENVTEST_POSTSYNC
	};
	int *loc, optindex, result, testval;

	result = TCL_OK;

	if (objc != 4) {
		Tcl_WrongNumArgs(interp, 2, objv, "abort|copy location");
		return (TCL_ERROR);
	}

	/*
	 * This must be the "copy" or "abort" portion of the command.
	 */
	if (Tcl_GetIndexFromObj(interp, objv[2], envtestcmd, "command",
	    TCL_EXACT, &optindex) != TCL_OK) {
		result = IS_HELP(objv[2]);
		return (result);
	}
	switch ((enum envtestcmd)optindex) {
	case ENVTEST_ABORT:
		loc = &envp->test_abort;
		break;
	case ENVTEST_COPY:
		loc = &envp->test_copy;
		break;
	default:
		Tcl_SetResult(interp, "Illegal store location", TCL_STATIC);
		return (TCL_ERROR);
	}

	/*
	 * This must be the location portion of the command.
	 */
	if (Tcl_GetIndexFromObj(interp, objv[3], envtestat, "location",
	    TCL_EXACT, &optindex) != TCL_OK) {
		result = IS_HELP(objv[3]);
		return (result);
	}
	switch ((enum envtestat)optindex) {
	case ENVTEST_NONE:
		testval = 0;
		break;
	case ENVTEST_PREOPEN:
		testval = DB_TEST_PREOPEN;
		break;
	case ENVTEST_PRERENAME:
		testval = DB_TEST_PRERENAME;
		break;
	case ENVTEST_POSTLOG:
		testval = DB_TEST_POSTLOG;
		break;
	case ENVTEST_POSTLOGMETA:
		testval = DB_TEST_POSTLOGMETA;
		break;
	case ENVTEST_POSTOPEN:
		testval = DB_TEST_POSTOPEN;
		break;
	case ENVTEST_POSTRENAME:
		testval = DB_TEST_POSTRENAME;
		break;
	case ENVTEST_POSTSYNC:
		testval = DB_TEST_POSTSYNC;
		break;
	default:
		Tcl_SetResult(interp, "Illegal test location", TCL_STATIC);
		return (TCL_ERROR);
	}

	*loc = testval;
	Tcl_SetResult(interp, "0", TCL_STATIC);
	return (result);
}
#endif
