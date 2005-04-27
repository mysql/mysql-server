/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2002
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: tcl_env.c,v 11.84 2002/08/06 06:21:03 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#include <tcl.h>
#endif

#include "db_int.h"
#include "dbinc/tcl_db.h"

/*
 * Prototypes for procedures defined later in this file:
 */
static void _EnvInfoDelete __P((Tcl_Interp *, DBTCL_INFO *));
static int  env_DbRemove __P((Tcl_Interp *, int, Tcl_Obj * CONST*, DB_ENV *));
static int  env_DbRename __P((Tcl_Interp *, int, Tcl_Obj * CONST*, DB_ENV *));

/*
 * PUBLIC: int env_Cmd __P((ClientData, Tcl_Interp *, int, Tcl_Obj * CONST*));
 *
 * env_Cmd --
 *	Implements the "env" command.
 */
int
env_Cmd(clientData, interp, objc, objv)
	ClientData clientData;		/* Env handle */
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
{
	static char *envcmds[] = {
#if CONFIG_TEST
		"attributes",
		"lock_detect",
		"lock_id",
		"lock_id_free",
		"lock_id_set",
		"lock_get",
		"lock_stat",
		"lock_timeout",
		"lock_vec",
		"log_archive",
		"log_compare",
		"log_cursor",
		"log_file",
		"log_flush",
		"log_get",
		"log_put",
		"log_stat",
		"mpool",
		"mpool_stat",
		"mpool_sync",
		"mpool_trickle",
		"mutex",
		"rep_elect",
		"rep_flush",
		"rep_limit",
		"rep_process_message",
		"rep_request",
		"rep_start",
		"rep_stat",
		"rpcid",
		"test",
		"txn_checkpoint",
		"txn_id_set",
		"txn_recover",
		"txn_stat",
		"txn_timeout",
		"verbose",
#endif
		"close",
		"dbremove",
		"dbrename",
		"txn",
		NULL
	};
	enum envcmds {
#if CONFIG_TEST
		ENVATTR,
		ENVLKDETECT,
		ENVLKID,
		ENVLKFREEID,
		ENVLKSETID,
		ENVLKGET,
		ENVLKSTAT,
		ENVLKTIMEOUT,
		ENVLKVEC,
		ENVLOGARCH,
		ENVLOGCMP,
		ENVLOGCURSOR,
		ENVLOGFILE,
		ENVLOGFLUSH,
		ENVLOGGET,
		ENVLOGPUT,
		ENVLOGSTAT,
		ENVMP,
		ENVMPSTAT,
		ENVMPSYNC,
		ENVTRICKLE,
		ENVMUTEX,
		ENVREPELECT,
		ENVREPFLUSH,
		ENVREPLIMIT,
		ENVREPPROCMESS,
		ENVREPREQUEST,
		ENVREPSTART,
		ENVREPSTAT,
		ENVRPCID,
		ENVTEST,
		ENVTXNCKP,
		ENVTXNSETID,
		ENVTXNRECOVER,
		ENVTXNSTAT,
		ENVTXNTIMEOUT,
		ENVVERB,
#endif
		ENVCLOSE,
		ENVDBREMOVE,
		ENVDBRENAME,
		ENVTXN
	};
	DBTCL_INFO *envip, *logcip;
	DB_ENV *dbenv;
	DB_LOGC *logc;
	Tcl_Obj *res;
	char newname[MSG_SIZE];
	int cmdindex, result, ret;
	u_int32_t newval;
#if CONFIG_TEST
	u_int32_t otherval;
#endif

	Tcl_ResetResult(interp);
	dbenv = (DB_ENV *)clientData;
	envip = _PtrToInfo((void *)dbenv);
	result = TCL_OK;
	memset(newname, 0, MSG_SIZE);

	if (objc <= 1) {
		Tcl_WrongNumArgs(interp, 1, objv, "command cmdargs");
		return (TCL_ERROR);
	}
	if (dbenv == NULL) {
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
#if CONFIG_TEST
	case ENVLKDETECT:
		result = tcl_LockDetect(interp, objc, objv, dbenv);
		break;
	case ENVLKSTAT:
		result = tcl_LockStat(interp, objc, objv, dbenv);
		break;
	case ENVLKTIMEOUT:
		result = tcl_LockTimeout(interp, objc, objv, dbenv);
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
		ret = dbenv->lock_id(dbenv, &newval);
		result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "lock_id");
		if (result == TCL_OK)
			res = Tcl_NewLongObj((long)newval);
		break;
	case ENVLKFREEID:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 3, objv, NULL);
			return (TCL_ERROR);
		}
		result = Tcl_GetLongFromObj(interp, objv[2], (long *)&newval);
		if (result != TCL_OK)
			return (result);
		ret = dbenv->lock_id_free(dbenv, newval);
		result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "lock id_free");
		break;
	case ENVLKSETID:
		if (objc != 4) {
			Tcl_WrongNumArgs(interp, 4, objv, "current max");
			return (TCL_ERROR);
		}
		result = Tcl_GetLongFromObj(interp, objv[2], (long *)&newval);
		if (result != TCL_OK)
			return (result);
		result = Tcl_GetLongFromObj(interp, objv[3], (long *)&otherval);
		if (result != TCL_OK)
			return (result);
		ret = dbenv->lock_id_set(dbenv, newval, otherval);
		result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "lock id_free");
		break;
	case ENVLKGET:
		result = tcl_LockGet(interp, objc, objv, dbenv);
		break;
	case ENVLKVEC:
		result = tcl_LockVec(interp, objc, objv, dbenv);
		break;
	case ENVLOGARCH:
		result = tcl_LogArchive(interp, objc, objv, dbenv);
		break;
	case ENVLOGCMP:
		result = tcl_LogCompare(interp, objc, objv);
		break;
	case ENVLOGCURSOR:
		snprintf(newname, sizeof(newname),
		    "%s.logc%d", envip->i_name, envip->i_envlogcid);
		logcip = _NewInfo(interp, NULL, newname, I_LOGC);
		if (logcip != NULL) {
			ret = dbenv->log_cursor(dbenv, &logc, 0);
			if (ret == 0) {
				result = TCL_OK;
				envip->i_envlogcid++;
				/*
				 * We do NOT want to set i_parent to
				 * envip here because log cursors are
				 * not "tied" to the env.  That is, they
				 * are NOT closed if the env is closed.
				 */
				Tcl_CreateObjCommand(interp, newname,
				    (Tcl_ObjCmdProc *)logc_Cmd,
				    (ClientData)logc, NULL);
				res =
				    Tcl_NewStringObj(newname, strlen(newname));
				_SetInfoData(logcip, logc);
			} else {
				_DeleteInfo(logcip);
				result = _ErrorSetup(interp, ret, "log cursor");
			}
		} else {
			Tcl_SetResult(interp,
			    "Could not set up info", TCL_STATIC);
			result = TCL_ERROR;
		}
		break;
	case ENVLOGFILE:
		result = tcl_LogFile(interp, objc, objv, dbenv);
		break;
	case ENVLOGFLUSH:
		result = tcl_LogFlush(interp, objc, objv, dbenv);
		break;
	case ENVLOGGET:
		result = tcl_LogGet(interp, objc, objv, dbenv);
		break;
	case ENVLOGPUT:
		result = tcl_LogPut(interp, objc, objv, dbenv);
		break;
	case ENVLOGSTAT:
		result = tcl_LogStat(interp, objc, objv, dbenv);
		break;
	case ENVMPSTAT:
		result = tcl_MpStat(interp, objc, objv, dbenv);
		break;
	case ENVMPSYNC:
		result = tcl_MpSync(interp, objc, objv, dbenv);
		break;
	case ENVTRICKLE:
		result = tcl_MpTrickle(interp, objc, objv, dbenv);
		break;
	case ENVMP:
		result = tcl_Mp(interp, objc, objv, dbenv, envip);
		break;
	case ENVREPELECT:
		result = tcl_RepElect(interp, objc, objv, dbenv);
		break;
	case ENVREPFLUSH:
		result = tcl_RepFlush(interp, objc, objv, dbenv);
		break;
	case ENVREPLIMIT:
		result = tcl_RepLimit(interp, objc, objv, dbenv);
		break;
	case ENVREPPROCMESS:
		result = tcl_RepProcessMessage(interp, objc, objv, dbenv);
		break;
	case ENVREPREQUEST:
		result = tcl_RepRequest(interp, objc, objv, dbenv);
		break;
	case ENVREPSTART:
		result = tcl_RepStart(interp, objc, objv, dbenv);
		break;
	case ENVREPSTAT:
		result = tcl_RepStat(interp, objc, objv, dbenv);
		break;
	case ENVRPCID:
		/*
		 * No args for this.  Error if there are some.
		 */
		if (objc > 2) {
			Tcl_WrongNumArgs(interp, 2, objv, NULL);
			return (TCL_ERROR);
		}
		/*
		 * !!! Retrieve the client ID from the dbp handle directly.
		 * This is for testing purposes only.  It is dbp-private data.
		 */
		res = Tcl_NewLongObj(dbenv->cl_id);
		break;
	case ENVTXNCKP:
		result = tcl_TxnCheckpoint(interp, objc, objv, dbenv);
		break;
	case ENVTXNSETID:
		if (objc != 4) {
			Tcl_WrongNumArgs(interp, 4, objv, "current max");
			return (TCL_ERROR);
		}
		result = Tcl_GetLongFromObj(interp, objv[2], (long *)&newval);
		if (result != TCL_OK)
			return (result);
		result = Tcl_GetLongFromObj(interp, objv[3], (long *)&otherval);
		if (result != TCL_OK)
			return (result);
		ret = dbenv->txn_id_set(dbenv, newval, otherval);
		result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "lock id_free");
		break;
	case ENVTXNRECOVER:
		result = tcl_TxnRecover(interp, objc, objv, dbenv, envip);
		break;
	case ENVTXNSTAT:
		result = tcl_TxnStat(interp, objc, objv, dbenv);
		break;
	case ENVTXNTIMEOUT:
		result = tcl_TxnTimeout(interp, objc, objv, dbenv);
		break;
	case ENVMUTEX:
		result = tcl_Mutex(interp, objc, objv, dbenv, envip);
		break;
	case ENVATTR:
		result = tcl_EnvAttr(interp, objc, objv, dbenv);
		break;
	case ENVTEST:
		result = tcl_EnvTest(interp, objc, objv, dbenv);
		break;
	case ENVVERB:
		/*
		 * Two args for this.  Error if different.
		 */
		if (objc != 4) {
			Tcl_WrongNumArgs(interp, 2, objv, NULL);
			return (TCL_ERROR);
		}
		result = tcl_EnvVerbose(interp, dbenv, objv[2], objv[3]);
		break;
#endif
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
		_debug_check();
		ret = dbenv->close(dbenv, 0);
		result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "env close");
		_EnvInfoDelete(interp, envip);
		envip = NULL;
		break;
	case ENVDBREMOVE:
		result = env_DbRemove(interp, objc, objv, dbenv);
		break;
	case ENVDBRENAME:
		result = env_DbRename(interp, objc, objv, dbenv);
		break;
	case ENVTXN:
		result = tcl_Txn(interp, objc, objv, dbenv, envip);
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
tcl_EnvRemove(interp, objc, objv, dbenv, envip)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *dbenv;			/* Env pointer */
	DBTCL_INFO *envip;		/* Info pointer */
{
	static char *envremopts[] = {
#if CONFIG_TEST
		"-overwrite",
		"-server",
#endif
		"-data_dir",
		"-encryptaes",
		"-encryptany",
		"-force",
		"-home",
		"-log_dir",
		"-tmp_dir",
		"-use_environ",
		"-use_environ_root",
		NULL
	};
	enum envremopts {
#if CONFIG_TEST
		ENVREM_OVERWRITE,
		ENVREM_SERVER,
#endif
		ENVREM_DATADIR,
		ENVREM_ENCRYPT_AES,
		ENVREM_ENCRYPT_ANY,
		ENVREM_FORCE,
		ENVREM_HOME,
		ENVREM_LOGDIR,
		ENVREM_TMPDIR,
		ENVREM_USE_ENVIRON,
		ENVREM_USE_ENVIRON_ROOT
	};
	DB_ENV *e;
	u_int32_t cflag, enc_flag, flag, forceflag, sflag;
	int i, optindex, result, ret;
	char *datadir, *home, *logdir, *passwd, *server, *tmpdir;

	result = TCL_OK;
	cflag = flag = forceflag = sflag = 0;
	home = NULL;
	passwd = NULL;
	datadir = logdir = tmpdir = NULL;
	server = NULL;
	enc_flag = 0;

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
#if CONFIG_TEST
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
#endif
		case ENVREM_ENCRYPT_AES:
			/* Make sure we have an arg to check against! */
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-encryptaes passwd?");
				result = TCL_ERROR;
				break;
			}
			passwd = Tcl_GetStringFromObj(objv[i++], NULL);
			enc_flag = DB_ENCRYPT_AES;
			break;
		case ENVREM_ENCRYPT_ANY:
			/* Make sure we have an arg to check against! */
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-encryptany passwd?");
				result = TCL_ERROR;
				break;
			}
			passwd = Tcl_GetStringFromObj(objv[i++], NULL);
			enc_flag = 0;
			break;
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
#if CONFIG_TEST
		case ENVREM_OVERWRITE:
			sflag |= DB_OVERWRITE;
			break;
#endif
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
	 * If dbenv is NULL, we don't have an open env and we need to open
	 * one of the user.  Don't bother with the info stuff.
	 */
	if (dbenv == NULL) {
		if ((ret = db_env_create(&e, cflag)) != 0) {
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "db_env_create");
			goto error;
		}
		if (server != NULL) {
			_debug_check();
			ret = e->set_rpc_server(e, NULL, server, 0, 0, 0);
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "set_rpc_server");
			if (result != TCL_OK)
				goto error;
		}
		if (datadir != NULL) {
			_debug_check();
			ret = e->set_data_dir(e, datadir);
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "set_data_dir");
			if (result != TCL_OK)
				goto error;
		}
		if (logdir != NULL) {
			_debug_check();
			ret = e->set_lg_dir(e, logdir);
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "set_log_dir");
			if (result != TCL_OK)
				goto error;
		}
		if (tmpdir != NULL) {
			_debug_check();
			ret = e->set_tmp_dir(e, tmpdir);
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "set_tmp_dir");
			if (result != TCL_OK)
				goto error;
		}
		if (passwd != NULL) {
			ret = e->set_encrypt(e, passwd, enc_flag);
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "set_encrypt");
		}
		if (sflag != 0 && (ret = e->set_flags(e, sflag, 1)) != 0) {
			_debug_check();
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "set_flags");
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
		e = dbenv;
	}

	flag |= forceflag;
	/*
	 * When we get here we have parsed all the args.  Now remove
	 * the environment.
	 */
	_debug_check();
	ret = e->remove(e, home, flag);
	result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
	    "env remove");
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
	 * 3.  Put any locks and close log cursors.
	 * 4.  Close the error file.
	 */
	for (p = LIST_FIRST(&__db_infohead); p != NULL; p = nextp) {
		/*
		 * Check if this info structure "belongs" to this
		 * env.  If so, remove its commands and info structure.
		 * We do not close/abort/whatever here, because we
		 * don't want to replicate DB behavior.
		 *
		 * NOTE:  Only those types that can nest need to be
		 * itemized in the switch below.  That is txns and mps.
		 * Other types like log cursors and locks will just
		 * get cleaned up here.
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

#if CONFIG_TEST
/*
 * PUBLIC: int tcl_EnvVerbose __P((Tcl_Interp *, DB_ENV *, Tcl_Obj *,
 * PUBLIC:    Tcl_Obj *));
 *
 * tcl_EnvVerbose --
 */
int
tcl_EnvVerbose(interp, dbenv, which, onoff)
	Tcl_Interp *interp;		/* Interpreter */
	DB_ENV *dbenv;			/* Env pointer */
	Tcl_Obj *which;			/* Which subsystem */
	Tcl_Obj *onoff;			/* On or off */
{
	static char *verbwhich[] = {
		"chkpt",
		"deadlock",
		"recovery",
		"rep",
		"wait",
		NULL
	};
	enum verbwhich {
		ENVVERB_CHK,
		ENVVERB_DEAD,
		ENVVERB_REC,
		ENVVERB_REP,
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
	case ENVVERB_REP:
		wh = DB_VERB_REPLICATION;
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
	ret = dbenv->set_verbose(dbenv, wh, on);
	return (_ReturnSetup(interp, ret, DB_RETOK_STD(ret),
	    "env set verbose"));
}
#endif

#if	CONFIG_TEST
/*
 * PUBLIC: int tcl_EnvAttr __P((Tcl_Interp *, int, Tcl_Obj * CONST*, DB_ENV *));
 *
 * tcl_EnvAttr --
 *	Return a list of the env's attributes
 */
int
tcl_EnvAttr(interp, objc, objv, dbenv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *dbenv;			/* Env pointer */
{
	int result;
	Tcl_Obj *myobj, *retlist;

	result = TCL_OK;

	if (objc > 2) {
		Tcl_WrongNumArgs(interp, 2, objv, NULL);
		return (TCL_ERROR);
	}
	retlist = Tcl_NewListObj(0, NULL);
	/*
	 * XXX
	 * We peek at the dbenv to determine what subsystems
	 * we have available in this env.
	 */
	myobj = Tcl_NewStringObj("-home", strlen("-home"));
	if ((result = Tcl_ListObjAppendElement(interp,
	    retlist, myobj)) != TCL_OK)
		goto err;
	myobj = Tcl_NewStringObj(dbenv->db_home, strlen(dbenv->db_home));
	if ((result = Tcl_ListObjAppendElement(interp,
	    retlist, myobj)) != TCL_OK)
		goto err;
	if (CDB_LOCKING(dbenv)) {
		myobj = Tcl_NewStringObj("-cdb", strlen("-cdb"));
		if ((result = Tcl_ListObjAppendElement(interp,
		    retlist, myobj)) != TCL_OK)
			goto err;
	}
	if (CRYPTO_ON(dbenv)) {
		myobj = Tcl_NewStringObj("-crypto", strlen("-crypto"));
		if ((result = Tcl_ListObjAppendElement(interp,
		    retlist, myobj)) != TCL_OK)
			goto err;
	}
	if (LOCKING_ON(dbenv)) {
		myobj = Tcl_NewStringObj("-lock", strlen("-lock"));
		if ((result = Tcl_ListObjAppendElement(interp,
		    retlist, myobj)) != TCL_OK)
			goto err;
	}
	if (LOGGING_ON(dbenv)) {
		myobj = Tcl_NewStringObj("-log", strlen("-log"));
		if ((result = Tcl_ListObjAppendElement(interp,
		    retlist, myobj)) != TCL_OK)
			goto err;
	}
	if (MPOOL_ON(dbenv)) {
		myobj = Tcl_NewStringObj("-mpool", strlen("-mpool"));
		if ((result = Tcl_ListObjAppendElement(interp,
		    retlist, myobj)) != TCL_OK)
			goto err;
	}
	if (RPC_ON(dbenv)) {
		myobj = Tcl_NewStringObj("-rpc", strlen("-rpc"));
		if ((result = Tcl_ListObjAppendElement(interp,
		    retlist, myobj)) != TCL_OK)
			goto err;
	}
	if (TXN_ON(dbenv)) {
		myobj = Tcl_NewStringObj("-txn", strlen("-txn"));
		if ((result = Tcl_ListObjAppendElement(interp,
		    retlist, myobj)) != TCL_OK)
			goto err;
	}
	Tcl_SetObjResult(interp, retlist);
err:
	return (result);
}

/*
 * PUBLIC: int tcl_EnvTest __P((Tcl_Interp *, int, Tcl_Obj * CONST*, DB_ENV *));
 *
 * tcl_EnvTest --
 */
int
tcl_EnvTest(interp, objc, objv, dbenv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *dbenv;			/* Env pointer */
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
		"electinit",
		"electsend",
		"electvote1",
		"electvote2",
		"electwait1",
		"electwait2",
		"none",
		"predestroy",
		"preopen",
		"postdestroy",
		"postlog",
		"postlogmeta",
		"postopen",
		"postsync",
		"subdb_lock",
		NULL
	};
	enum envtestat {
		ENVTEST_ELECTINIT,
		ENVTEST_ELECTSEND,
		ENVTEST_ELECTVOTE1,
		ENVTEST_ELECTVOTE2,
		ENVTEST_ELECTWAIT1,
		ENVTEST_ELECTWAIT2,
		ENVTEST_NONE,
		ENVTEST_PREDESTROY,
		ENVTEST_PREOPEN,
		ENVTEST_POSTDESTROY,
		ENVTEST_POSTLOG,
		ENVTEST_POSTLOGMETA,
		ENVTEST_POSTOPEN,
		ENVTEST_POSTSYNC,
		ENVTEST_SUBDB_LOCKS
	};
	int *loc, optindex, result, testval;

	result = TCL_OK;
	loc = NULL;

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
		loc = &dbenv->test_abort;
		break;
	case ENVTEST_COPY:
		loc = &dbenv->test_copy;
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
	case ENVTEST_ELECTINIT:
		DB_ASSERT(loc == &dbenv->test_abort);
		testval = DB_TEST_ELECTINIT;
		break;
	case ENVTEST_ELECTSEND:
		DB_ASSERT(loc == &dbenv->test_abort);
		testval = DB_TEST_ELECTSEND;
		break;
	case ENVTEST_ELECTVOTE1:
		DB_ASSERT(loc == &dbenv->test_abort);
		testval = DB_TEST_ELECTVOTE1;
		break;
	case ENVTEST_ELECTVOTE2:
		DB_ASSERT(loc == &dbenv->test_abort);
		testval = DB_TEST_ELECTVOTE2;
		break;
	case ENVTEST_ELECTWAIT1:
		DB_ASSERT(loc == &dbenv->test_abort);
		testval = DB_TEST_ELECTWAIT1;
		break;
	case ENVTEST_ELECTWAIT2:
		DB_ASSERT(loc == &dbenv->test_abort);
		testval = DB_TEST_ELECTWAIT2;
		break;
	case ENVTEST_NONE:
		testval = 0;
		break;
	case ENVTEST_PREOPEN:
		testval = DB_TEST_PREOPEN;
		break;
	case ENVTEST_PREDESTROY:
		testval = DB_TEST_PREDESTROY;
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
	case ENVTEST_POSTDESTROY:
		testval = DB_TEST_POSTDESTROY;
		break;
	case ENVTEST_POSTSYNC:
		testval = DB_TEST_POSTSYNC;
		break;
	case ENVTEST_SUBDB_LOCKS:
		DB_ASSERT(loc == &dbenv->test_abort);
		testval = DB_TEST_SUBDB_LOCKS;
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

/*
 * env_DbRemove --
 *	Implements the ENV->dbremove command.
 */
static int
env_DbRemove(interp, objc, objv, dbenv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *dbenv;
{
	static char *envdbrem[] = {
		"-auto_commit",
		"-txn",
		"--",
		NULL
	};
	enum envdbrem {
		TCL_EDBREM_COMMIT,
		TCL_EDBREM_TXN,
		TCL_EDBREM_ENDARG
	};
	DB_TXN *txn;
	u_int32_t flag;
	int endarg, i, optindex, result, ret, subdblen;
	u_char *subdbtmp;
	char *arg, *db, *subdb, msg[MSG_SIZE];

	txn = NULL;
	result = TCL_OK;
	subdbtmp = NULL;
	db = subdb = NULL;
	endarg = 0;
	flag = 0;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 2, objv, "?args? filename ?database?");
		return (TCL_ERROR);
	}

	/*
	 * We must first parse for the environment flag, since that
	 * is needed for db_create.  Then create the db handle.
	 */
	i = 2;
	while (i < objc) {
		if (Tcl_GetIndexFromObj(interp, objv[i], envdbrem,
		    "option", TCL_EXACT, &optindex) != TCL_OK) {
			arg = Tcl_GetStringFromObj(objv[i], NULL);
			if (arg[0] == '-') {
				result = IS_HELP(objv[i]);
				goto error;
			} else
				Tcl_ResetResult(interp);
			break;
		}
		i++;
		switch ((enum envdbrem)optindex) {
		case TCL_EDBREM_COMMIT:
			flag |= DB_AUTO_COMMIT;
			break;
		case TCL_EDBREM_TXN:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv, "?-txn id?");
				result = TCL_ERROR;
				break;
			}
			arg = Tcl_GetStringFromObj(objv[i++], NULL);
			txn = NAME_TO_TXN(arg);
			if (txn == NULL) {
				snprintf(msg, MSG_SIZE,
				    "env dbremove: Invalid txn %s\n", arg);
				Tcl_SetResult(interp, msg, TCL_VOLATILE);
				return (TCL_ERROR);
			}
			break;
		case TCL_EDBREM_ENDARG:
			endarg = 1;
			break;
		}
		/*
		 * If, at any time, parsing the args we get an error,
		 * bail out and return.
		 */
		if (result != TCL_OK)
			goto error;
		if (endarg)
			break;
	}
	if (result != TCL_OK)
		goto error;
	/*
	 * Any args we have left, (better be 1 or 2 left) are
	 * file names. If there is 1, a db name, if 2 a db and subdb name.
	 */
	if ((i != (objc - 1)) || (i != (objc - 2))) {
		/*
		 * Dbs must be NULL terminated file names, but subdbs can
		 * be anything.  Use Strings for the db name and byte
		 * arrays for the subdb.
		 */
		db = Tcl_GetStringFromObj(objv[i++], NULL);
		if (i != objc) {
			subdbtmp =
			    Tcl_GetByteArrayFromObj(objv[i++], &subdblen);
			if ((ret = __os_malloc(dbenv, subdblen + 1,
			    &subdb)) != 0) {
				Tcl_SetResult(interp,
				    db_strerror(ret), TCL_STATIC);
				return (0);
			}
			memcpy(subdb, subdbtmp, subdblen);
			subdb[subdblen] = '\0';
		}
	} else {
		Tcl_WrongNumArgs(interp, 2, objv, "?args? filename ?database?");
		result = TCL_ERROR;
		goto error;
	}
	ret = dbenv->dbremove(dbenv, txn, db, subdb, flag);
	result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
	    "env dbremove");
error:
	if (subdb)
		__os_free(dbenv, subdb);
	return (result);
}

/*
 * env_DbRename --
 *	Implements the ENV->dbrename command.
 */
static int
env_DbRename(interp, objc, objv, dbenv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *dbenv;
{
	static char *envdbmv[] = {
		"-auto_commit",
		"-txn",
		"--",
		NULL
	};
	enum envdbmv {
		TCL_EDBMV_COMMIT,
		TCL_EDBMV_TXN,
		TCL_EDBMV_ENDARG
	};
	DB_TXN *txn;
	u_int32_t flag;
	int endarg, i, newlen, optindex, result, ret, subdblen;
	u_char *subdbtmp;
	char *arg, *db, *newname, *subdb, msg[MSG_SIZE];

	txn = NULL;
	result = TCL_OK;
	subdbtmp = NULL;
	db = newname = subdb = NULL;
	endarg = 0;
	flag = 0;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 3, objv,
		    "?args? filename ?database? ?newname?");
		return (TCL_ERROR);
	}

	/*
	 * We must first parse for the environment flag, since that
	 * is needed for db_create.  Then create the db handle.
	 */
	i = 2;
	while (i < objc) {
		if (Tcl_GetIndexFromObj(interp, objv[i], envdbmv,
		    "option", TCL_EXACT, &optindex) != TCL_OK) {
			arg = Tcl_GetStringFromObj(objv[i], NULL);
			if (arg[0] == '-') {
				result = IS_HELP(objv[i]);
				goto error;
			} else
				Tcl_ResetResult(interp);
			break;
		}
		i++;
		switch ((enum envdbmv)optindex) {
		case TCL_EDBMV_COMMIT:
			flag |= DB_AUTO_COMMIT;
			break;
		case TCL_EDBMV_TXN:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv, "?-txn id?");
				result = TCL_ERROR;
				break;
			}
			arg = Tcl_GetStringFromObj(objv[i++], NULL);
			txn = NAME_TO_TXN(arg);
			if (txn == NULL) {
				snprintf(msg, MSG_SIZE,
				    "env dbrename: Invalid txn %s\n", arg);
				Tcl_SetResult(interp, msg, TCL_VOLATILE);
				return (TCL_ERROR);
			}
			break;
		case TCL_EDBMV_ENDARG:
			endarg = 1;
			break;
		}
		/*
		 * If, at any time, parsing the args we get an error,
		 * bail out and return.
		 */
		if (result != TCL_OK)
			goto error;
		if (endarg)
			break;
	}
	if (result != TCL_OK)
		goto error;
	/*
	 * Any args we have left, (better be 2 or 3 left) are
	 * file names. If there is 2, a db name, if 3 a db and subdb name.
	 */
	if ((i != (objc - 2)) || (i != (objc - 3))) {
		/*
		 * Dbs must be NULL terminated file names, but subdbs can
		 * be anything.  Use Strings for the db name and byte
		 * arrays for the subdb.
		 */
		db = Tcl_GetStringFromObj(objv[i++], NULL);
		if (i == objc - 2) {
			subdbtmp =
			    Tcl_GetByteArrayFromObj(objv[i++], &subdblen);
			if ((ret = __os_malloc(dbenv, subdblen + 1,
			    &subdb)) != 0) {
				Tcl_SetResult(interp,
				    db_strerror(ret), TCL_STATIC);
				return (0);
			}
			memcpy(subdb, subdbtmp, subdblen);
			subdb[subdblen] = '\0';
		}
		subdbtmp =
		    Tcl_GetByteArrayFromObj(objv[i++], &newlen);
		if ((ret = __os_malloc(dbenv, newlen + 1,
		    &newname)) != 0) {
			Tcl_SetResult(interp,
			    db_strerror(ret), TCL_STATIC);
			return (0);
		}
		memcpy(newname, subdbtmp, newlen);
		newname[newlen] = '\0';
	} else {
		Tcl_WrongNumArgs(interp, 3, objv,
		    "?args? filename ?database? ?newname?");
		result = TCL_ERROR;
		goto error;
	}
	ret = dbenv->dbrename(dbenv, txn, db, subdb, newname, flag);
	result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
	    "env dbrename");
error:
	if (subdb)
		__os_free(dbenv, subdb);
	if (newname)
		__os_free(dbenv, newname);
	return (result);
}
