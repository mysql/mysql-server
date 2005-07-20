/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: tcl_env.c,v 11.121 2004/10/07 16:48:39 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#include <tcl.h>
#endif

#include "db_int.h"
#include "dbinc/db_shash.h"
#include "dbinc/lock.h"
#include "dbinc/txn.h"
#include "dbinc/tcl_db.h"

/*
 * Prototypes for procedures defined later in this file:
 */
static void _EnvInfoDelete __P((Tcl_Interp *, DBTCL_INFO *));
static int  env_DbRemove __P((Tcl_Interp *, int, Tcl_Obj * CONST*, DB_ENV *));
static int  env_DbRename __P((Tcl_Interp *, int, Tcl_Obj * CONST*, DB_ENV *));
static int  env_GetFlags __P((Tcl_Interp *, int, Tcl_Obj * CONST*, DB_ENV *));
static int  env_GetOpenFlag
		__P((Tcl_Interp *, int, Tcl_Obj * CONST*, DB_ENV *));
static int  env_GetLockDetect
		__P((Tcl_Interp *, int, Tcl_Obj * CONST*, DB_ENV *));
static int  env_GetTimeout __P((Tcl_Interp *, int, Tcl_Obj * CONST*, DB_ENV *));
static int  env_GetVerbose __P((Tcl_Interp *, int, Tcl_Obj * CONST*, DB_ENV *));

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
	static const char *envcmds[] = {
#ifdef CONFIG_TEST
		"attributes",
		"errfile",
		"errpfx",
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
		"set_flags",
		"test",
		"txn_id_set",
		"txn_recover",
		"txn_stat",
		"txn_timeout",
		"verbose",
#endif
		"close",
		"dbremove",
		"dbrename",
		"get_cachesize",
		"get_data_dirs",
		"get_encrypt_flags",
		"get_errpfx",
		"get_flags",
		"get_home",
		"get_lg_bsize",
		"get_lg_dir",
		"get_lg_max",
		"get_lg_regionmax",
		"get_lk_detect",
		"get_lk_max_lockers",
		"get_lk_max_locks",
		"get_lk_max_objects",
		"get_mp_max_openfd",
		"get_mp_max_write",
		"get_mp_mmapsize",
		"get_open_flags",
		"get_rep_limit",
		"get_shm_key",
		"get_tas_spins",
		"get_timeout",
		"get_tmp_dir",
		"get_tx_max",
		"get_tx_timestamp",
		"get_verbose",
		"txn",
		"txn_checkpoint",
		NULL
	};
	enum envcmds {
#ifdef CONFIG_TEST
		ENVATTR,
		ENVERRFILE,
		ENVERRPFX,
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
		ENVSETFLAGS,
		ENVTEST,
		ENVTXNSETID,
		ENVTXNRECOVER,
		ENVTXNSTAT,
		ENVTXNTIMEOUT,
		ENVVERB,
#endif
		ENVCLOSE,
		ENVDBREMOVE,
		ENVDBRENAME,
		ENVGETCACHESIZE,
		ENVGETDATADIRS,
		ENVGETENCRYPTFLAGS,
		ENVGETERRPFX,
		ENVGETFLAGS,
		ENVGETHOME,
		ENVGETLGBSIZE,
		ENVGETLGDIR,
		ENVGETLGMAX,
		ENVGETLGREGIONMAX,
		ENVGETLKDETECT,
		ENVGETLKMAXLOCKERS,
		ENVGETLKMAXLOCKS,
		ENVGETLKMAXOBJECTS,
		ENVGETMPMAXOPENFD,
		ENVGETMPMAXWRITE,
		ENVGETMPMMAPSIZE,
		ENVGETOPENFLAG,
		ENVGETREPLIMIT,
		ENVGETSHMKEY,
		ENVGETTASSPINS,
		ENVGETTIMEOUT,
		ENVGETTMPDIR,
		ENVGETTXMAX,
		ENVGETTXTIMESTAMP,
		ENVGETVERBOSE,
		ENVTXN,
		ENVTXNCKP
	};
	DBTCL_INFO *envip;
	DB_ENV *dbenv;
	Tcl_Obj *res, *myobjv[3];
	char newname[MSG_SIZE];
	int cmdindex, i, intvalue1, intvalue2, ncache, result, ret;
	u_int32_t bytes, gbytes, value;
	size_t size;
	long shm_key;
	time_t timeval;
	const char *strval, **dirs;
#ifdef CONFIG_TEST
	DBTCL_INFO *logcip;
	DB_LOGC *logc;
	char *strarg;
	u_int32_t lockid;
	long newval, otherval;
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
#ifdef CONFIG_TEST
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
		ret = dbenv->lock_id(dbenv, &lockid);
		result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "lock_id");
		if (result == TCL_OK)
			res = Tcl_NewWideIntObj((Tcl_WideInt)lockid);
		break;
	case ENVLKFREEID:
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 3, objv, NULL);
			return (TCL_ERROR);
		}
		result = Tcl_GetLongFromObj(interp, objv[2], &newval);
		if (result != TCL_OK)
			return (result);
		ret = dbenv->lock_id_free(dbenv, (u_int32_t)newval);
		result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "lock id_free");
		break;
	case ENVLKSETID:
		if (objc != 4) {
			Tcl_WrongNumArgs(interp, 4, objv, "current max");
			return (TCL_ERROR);
		}
		result = Tcl_GetLongFromObj(interp, objv[2], &newval);
		if (result != TCL_OK)
			return (result);
		result = Tcl_GetLongFromObj(interp, objv[3], &otherval);
		if (result != TCL_OK)
			return (result);
		ret = __lock_id_set(dbenv,
		    (u_int32_t)newval, (u_int32_t)otherval);
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
				(void)Tcl_CreateObjCommand(interp, newname,
				    (Tcl_ObjCmdProc *)logc_Cmd,
				    (ClientData)logc, NULL);
				res = NewStringObj(newname, strlen(newname));
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
		res = Tcl_NewLongObj((long)dbenv->cl_id);
		break;
	case ENVTXNSETID:
		if (objc != 4) {
			Tcl_WrongNumArgs(interp, 4, objv, "current max");
			return (TCL_ERROR);
		}
		result = Tcl_GetLongFromObj(interp, objv[2], &newval);
		if (result != TCL_OK)
			return (result);
		result = Tcl_GetLongFromObj(interp, objv[3], &otherval);
		if (result != TCL_OK)
			return (result);
		ret = __txn_id_set(dbenv,
		    (u_int32_t)newval, (u_int32_t)otherval);
		result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "txn setid");
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
	case ENVERRFILE:
		/*
		 * One args for this.  Error if different.
		 */
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "errfile");
			return (TCL_ERROR);
		}
		strarg = Tcl_GetStringFromObj(objv[2], NULL);
		tcl_EnvSetErrfile(interp, dbenv, envip, strarg);
		result = TCL_OK;
		break;
	case ENVERRPFX:
		/*
		 * One args for this.  Error if different.
		 */
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "pfx");
			return (TCL_ERROR);
		}
		strarg = Tcl_GetStringFromObj(objv[2], NULL);
		result = tcl_EnvSetErrpfx(interp, dbenv, envip, strarg);
		break;
	case ENVSETFLAGS:
		/*
		 * Two args for this.  Error if different.
		 */
		if (objc != 4) {
			Tcl_WrongNumArgs(interp, 2, objv, "which on|off");
			return (TCL_ERROR);
		}
		result = tcl_EnvSetFlags(interp, dbenv, objv[2], objv[3]);
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
	case ENVGETCACHESIZE:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, NULL);
			return (TCL_ERROR);
		}
		ret = dbenv->get_cachesize(dbenv, &gbytes, &bytes, &ncache);
		if ((result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "env get_cachesize")) == TCL_OK) {
			myobjv[0] = Tcl_NewLongObj((long)gbytes);
			myobjv[1] = Tcl_NewLongObj((long)bytes);
			myobjv[2] = Tcl_NewLongObj((long)ncache);
			res = Tcl_NewListObj(3, myobjv);
		}
		break;
	case ENVGETDATADIRS:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, NULL);
			return (TCL_ERROR);
		}
		ret = dbenv->get_data_dirs(dbenv, &dirs);
		if ((result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "env get_data_dirs")) == TCL_OK) {
			res = Tcl_NewListObj(0, NULL);
			for (i = 0; result == TCL_OK && dirs[i] != NULL; i++)
				result = Tcl_ListObjAppendElement(interp, res,
				    NewStringObj(dirs[i], strlen(dirs[i])));
		}
		break;
	case ENVGETENCRYPTFLAGS:
		result = tcl_EnvGetEncryptFlags(interp, objc, objv, dbenv);
		break;
	case ENVGETERRPFX:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, NULL);
			return (TCL_ERROR);
		}
		dbenv->get_errpfx(dbenv, &strval);
		res = NewStringObj(strval, strlen(strval));
		break;
	case ENVGETFLAGS:
		result = env_GetFlags(interp, objc, objv, dbenv);
		break;
	case ENVGETHOME:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, NULL);
			return (TCL_ERROR);
		}
		ret = dbenv->get_home(dbenv, &strval);
		if ((result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "env get_home")) == TCL_OK)
			res = NewStringObj(strval, strlen(strval));
		break;
	case ENVGETLGBSIZE:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, NULL);
			return (TCL_ERROR);
		}
		ret = dbenv->get_lg_bsize(dbenv, &value);
		if ((result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "env get_lg_bsize")) == TCL_OK)
			res = Tcl_NewLongObj((long)value);
		break;
	case ENVGETLGDIR:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, NULL);
			return (TCL_ERROR);
		}
		ret = dbenv->get_lg_dir(dbenv, &strval);
		if ((result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "env get_lg_dir")) == TCL_OK)
			res = NewStringObj(strval, strlen(strval));
		break;
	case ENVGETLGMAX:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, NULL);
			return (TCL_ERROR);
		}
		ret = dbenv->get_lg_max(dbenv, &value);
		if ((result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "env get_lg_max")) == TCL_OK)
			res = Tcl_NewLongObj((long)value);
		break;
	case ENVGETLGREGIONMAX:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, NULL);
			return (TCL_ERROR);
		}
		ret = dbenv->get_lg_regionmax(dbenv, &value);
		if ((result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "env get_lg_regionmax")) == TCL_OK)
			res = Tcl_NewLongObj((long)value);
		break;
	case ENVGETLKDETECT:
		result = env_GetLockDetect(interp, objc, objv, dbenv);
		break;
	case ENVGETLKMAXLOCKERS:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, NULL);
			return (TCL_ERROR);
		}
		ret = dbenv->get_lk_max_lockers(dbenv, &value);
		if ((result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "env get_lk_max_lockers")) == TCL_OK)
			res = Tcl_NewLongObj((long)value);
		break;
	case ENVGETLKMAXLOCKS:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, NULL);
			return (TCL_ERROR);
		}
		ret = dbenv->get_lk_max_locks(dbenv, &value);
		if ((result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "env get_lk_max_locks")) == TCL_OK)
			res = Tcl_NewLongObj((long)value);
		break;
	case ENVGETLKMAXOBJECTS:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, NULL);
			return (TCL_ERROR);
		}
		ret = dbenv->get_lk_max_objects(dbenv, &value);
		if ((result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "env get_lk_max_objects")) == TCL_OK)
			res = Tcl_NewLongObj((long)value);
		break;
	case ENVGETMPMAXOPENFD:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, NULL);
			return (TCL_ERROR);
		}
		ret = dbenv->get_mp_max_openfd(dbenv, &intvalue1);
		if ((result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "env get_mp_max_openfd")) == TCL_OK)
			res = Tcl_NewIntObj(intvalue1);
		break;
	case ENVGETMPMAXWRITE:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, NULL);
			return (TCL_ERROR);
		}
		ret = dbenv->get_mp_max_write(dbenv, &intvalue1, &intvalue2);
		if ((result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "env get_mp_max_write")) == TCL_OK) {
			myobjv[0] = Tcl_NewIntObj(intvalue1);
			myobjv[1] = Tcl_NewIntObj(intvalue2);
			res = Tcl_NewListObj(2, myobjv);
		}
		break;
	case ENVGETMPMMAPSIZE:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, NULL);
			return (TCL_ERROR);
		}
		ret = dbenv->get_mp_mmapsize(dbenv, &size);
		if ((result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "env get_mp_mmapsize")) == TCL_OK)
			res = Tcl_NewLongObj((long)size);
		break;
	case ENVGETOPENFLAG:
		result = env_GetOpenFlag(interp, objc, objv, dbenv);
		break;
	case ENVGETREPLIMIT:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, NULL);
			return (TCL_ERROR);
		}
		ret = dbenv->get_rep_limit(dbenv, &gbytes, &bytes);
		if ((result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "env get_rep_limit")) == TCL_OK) {
			myobjv[0] = Tcl_NewLongObj((long)gbytes);
			myobjv[1] = Tcl_NewLongObj((long)bytes);
			res = Tcl_NewListObj(2, myobjv);
		}
		break;
	case ENVGETSHMKEY:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, NULL);
			return (TCL_ERROR);
		}
		ret = dbenv->get_shm_key(dbenv, &shm_key);
		if ((result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "env shm_key")) == TCL_OK)
			res = Tcl_NewLongObj(shm_key);
		break;
	case ENVGETTASSPINS:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, NULL);
			return (TCL_ERROR);
		}
		ret = dbenv->get_tas_spins(dbenv, &value);
		if ((result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "env get_tas_spins")) == TCL_OK)
			res = Tcl_NewLongObj((long)value);
		break;
	case ENVGETTIMEOUT:
		result = env_GetTimeout(interp, objc, objv, dbenv);
		break;
	case ENVGETTMPDIR:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, NULL);
			return (TCL_ERROR);
		}
		ret = dbenv->get_tmp_dir(dbenv, &strval);
		if ((result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "env get_tmp_dir")) == TCL_OK)
			res = NewStringObj(strval, strlen(strval));
		break;
	case ENVGETTXMAX:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, NULL);
			return (TCL_ERROR);
		}
		ret = dbenv->get_tx_max(dbenv, &value);
		if ((result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "env get_tx_max")) == TCL_OK)
			res = Tcl_NewLongObj((long)value);
		break;
	case ENVGETTXTIMESTAMP:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, NULL);
			return (TCL_ERROR);
		}
		ret = dbenv->get_tx_timestamp(dbenv, &timeval);
		if ((result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "env get_tx_timestamp")) == TCL_OK)
			res = Tcl_NewLongObj((long)timeval);
		break;
	case ENVGETVERBOSE:
		result = env_GetVerbose(interp, objc, objv, dbenv);
		break;
	case ENVTXN:
		result = tcl_Txn(interp, objc, objv, dbenv, envip);
		break;
	case ENVTXNCKP:
		result = tcl_TxnCheckpoint(interp, objc, objv, dbenv);
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
	static const char *envremopts[] = {
#ifdef CONFIG_TEST
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
#ifdef CONFIG_TEST
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
#ifdef CONFIG_TEST
		case ENVREM_SERVER:
			/* Make sure we have an arg to check against! */
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-server name?");
				result = TCL_ERROR;
				break;
			}
			server = Tcl_GetStringFromObj(objv[i++], NULL);
			cflag = DB_RPCCLIENT;
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
#ifdef CONFIG_TEST
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
			case I_DB:
			case I_DBC:
			case I_ENV:
			case I_LOCK:
			case I_LOGC:
			case I_MUTEX:
			case I_NDBM:
			case I_PG:
			case I_SEQ:
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

#ifdef CONFIG_TEST
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
	static const char *verbwhich[] = {
		"deadlock",
		"recovery",
		"rep",
		"wait",
		NULL
	};
	enum verbwhich {
		ENVVERB_DEAD,
		ENVVERB_REC,
		ENVVERB_REP,
		ENVVERB_WAIT
	};
	static const char *verbonoff[] = {
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

#ifdef CONFIG_TEST
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
	myobj = NewStringObj("-home", strlen("-home"));
	if ((result = Tcl_ListObjAppendElement(interp,
	    retlist, myobj)) != TCL_OK)
		goto err;
	myobj = NewStringObj(dbenv->db_home, strlen(dbenv->db_home));
	if ((result = Tcl_ListObjAppendElement(interp,
	    retlist, myobj)) != TCL_OK)
		goto err;
	if (CDB_LOCKING(dbenv)) {
		myobj = NewStringObj("-cdb", strlen("-cdb"));
		if ((result = Tcl_ListObjAppendElement(interp,
		    retlist, myobj)) != TCL_OK)
			goto err;
	}
	if (CRYPTO_ON(dbenv)) {
		myobj = NewStringObj("-crypto", strlen("-crypto"));
		if ((result = Tcl_ListObjAppendElement(interp,
		    retlist, myobj)) != TCL_OK)
			goto err;
	}
	if (LOCKING_ON(dbenv)) {
		myobj = NewStringObj("-lock", strlen("-lock"));
		if ((result = Tcl_ListObjAppendElement(interp,
		    retlist, myobj)) != TCL_OK)
			goto err;
	}
	if (LOGGING_ON(dbenv)) {
		myobj = NewStringObj("-log", strlen("-log"));
		if ((result = Tcl_ListObjAppendElement(interp,
		    retlist, myobj)) != TCL_OK)
			goto err;
	}
	if (MPOOL_ON(dbenv)) {
		myobj = NewStringObj("-mpool", strlen("-mpool"));
		if ((result = Tcl_ListObjAppendElement(interp,
		    retlist, myobj)) != TCL_OK)
			goto err;
	}
	if (RPC_ON(dbenv)) {
		myobj = NewStringObj("-rpc", strlen("-rpc"));
		if ((result = Tcl_ListObjAppendElement(interp,
		    retlist, myobj)) != TCL_OK)
			goto err;
	}
	if (REP_ON(dbenv)) {
		myobj = NewStringObj("-rep", strlen("-rep"));
		if ((result = Tcl_ListObjAppendElement(interp,
		    retlist, myobj)) != TCL_OK)
			goto err;
	}
	if (TXN_ON(dbenv)) {
		myobj = NewStringObj("-txn", strlen("-txn"));
		if ((result = Tcl_ListObjAppendElement(interp,
		    retlist, myobj)) != TCL_OK)
			goto err;
	}
	Tcl_SetObjResult(interp, retlist);
err:
	return (result);
}

/*
 * PUBLIC: int tcl_EnvSetFlags __P((Tcl_Interp *, DB_ENV *, Tcl_Obj *,
 * PUBLIC:    Tcl_Obj *));
 *
 * tcl_EnvSetFlags --
 *	Set flags in an env.
 */
int
tcl_EnvSetFlags(interp, dbenv, which, onoff)
	Tcl_Interp *interp;		/* Interpreter */
	DB_ENV *dbenv;			/* Env pointer */
	Tcl_Obj *which;			/* Which subsystem */
	Tcl_Obj *onoff;			/* On or off */
{
	static const char *sfwhich[] = {
		"-auto_commit",
		"-direct_db",
		"-direct_log",
		"-dsync_log",
		"-log_inmemory",
		"-log_remove",
		"-nolock",
		"-nommap",
		"-nopanic",
		"-nosync",
		"-overwrite",
		"-panic",
		"-wrnosync",
		NULL
	};
	enum sfwhich {
		ENVSF_AUTOCOMMIT,
		ENVSF_DIRECTDB,
		ENVSF_DIRECTLOG,
		ENVSF_DSYNCLOG,
		ENVSF_LOG_INMEMORY,
		ENVSF_LOG_REMOVE,
		ENVSF_NOLOCK,
		ENVSF_NOMMAP,
		ENVSF_NOPANIC,
		ENVSF_NOSYNC,
		ENVSF_OVERWRITE,
		ENVSF_PANIC,
		ENVSF_WRNOSYNC
	};
	static const char *sfonoff[] = {
		"off",
		"on",
		NULL
	};
	enum sfonoff {
		ENVSF_OFF,
		ENVSF_ON
	};
	int on, optindex, ret;
	u_int32_t wh;

	if (Tcl_GetIndexFromObj(interp, which, sfwhich, "option",
	    TCL_EXACT, &optindex) != TCL_OK)
		return (IS_HELP(which));

	switch ((enum sfwhich)optindex) {
	case ENVSF_AUTOCOMMIT:
		wh = DB_AUTO_COMMIT;
		break;
	case ENVSF_DIRECTDB:
		wh = DB_DIRECT_DB;
		break;
	case ENVSF_DIRECTLOG:
		wh = DB_DIRECT_LOG;
		break;
	case ENVSF_DSYNCLOG:
		wh = DB_DSYNC_LOG;
		break;
	case ENVSF_LOG_INMEMORY:
		wh = DB_LOG_INMEMORY;
		break;
	case ENVSF_LOG_REMOVE:
		wh = DB_LOG_AUTOREMOVE;
		break;
	case ENVSF_NOLOCK:
		wh = DB_NOLOCKING;
		break;
	case ENVSF_NOMMAP:
		wh = DB_NOMMAP;
		break;
	case ENVSF_NOSYNC:
		wh = DB_TXN_NOSYNC;
		break;
	case ENVSF_NOPANIC:
		wh = DB_NOPANIC;
		break;
	case ENVSF_PANIC:
		wh = DB_PANIC_ENVIRONMENT;
		break;
	case ENVSF_OVERWRITE:
		wh = DB_OVERWRITE;
		break;
	case ENVSF_WRNOSYNC:
		wh = DB_TXN_WRITE_NOSYNC;
		break;
	default:
		return (TCL_ERROR);
	}
	if (Tcl_GetIndexFromObj(interp, onoff, sfonoff, "option",
	    TCL_EXACT, &optindex) != TCL_OK)
		return (IS_HELP(onoff));
	switch ((enum sfonoff)optindex) {
	case ENVSF_OFF:
		on = 0;
		break;
	case ENVSF_ON:
		on = 1;
		break;
	default:
		return (TCL_ERROR);
	}
	ret = dbenv->set_flags(dbenv, wh, on);
	return (_ReturnSetup(interp, ret, DB_RETOK_STD(ret),
	    "env set verbose"));
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
	static const char *envtestcmd[] = {
		"abort",
		"check",
		"copy",
		NULL
	};
	enum envtestcmd {
		ENVTEST_ABORT,
		ENVTEST_CHECK,
		ENVTEST_COPY
	};
	static const char *envtestat[] = {
		"electinit",
		"electvote1",
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
		ENVTEST_ELECTVOTE1,
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
	 * This must be the "check", "copy" or "abort" portion of the command.
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
	case ENVTEST_CHECK:
		loc = &dbenv->test_check;
		if (Tcl_GetIntFromObj(interp, objv[3], &testval) != TCL_OK) {
			result = IS_HELP(objv[3]);
			return (result);
		}
		goto done;
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
	case ENVTEST_ELECTVOTE1:
		DB_ASSERT(loc == &dbenv->test_abort);
		testval = DB_TEST_ELECTVOTE1;
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
done:
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
	static const char *envdbrem[] = {
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
			if ((ret = __os_malloc(
			    dbenv, (size_t)subdblen + 1, &subdb)) != 0) {
				Tcl_SetResult(interp,
				    db_strerror(ret), TCL_STATIC);
				return (0);
			}
			memcpy(subdb, subdbtmp, (size_t)subdblen);
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
	static const char *envdbmv[] = {
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
			if ((ret = __os_malloc(
			    dbenv, (size_t)subdblen + 1, &subdb)) != 0) {
				Tcl_SetResult(interp,
				    db_strerror(ret), TCL_STATIC);
				return (0);
			}
			memcpy(subdb, subdbtmp, (size_t)subdblen);
			subdb[subdblen] = '\0';
		}
		subdbtmp = Tcl_GetByteArrayFromObj(objv[i++], &newlen);
		if ((ret = __os_malloc(
		    dbenv, (size_t)newlen + 1, &newname)) != 0) {
			Tcl_SetResult(interp,
			    db_strerror(ret), TCL_STATIC);
			return (0);
		}
		memcpy(newname, subdbtmp, (size_t)newlen);
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

/*
 * env_GetFlags --
 *	Implements the ENV->get_flags command.
 */
static int
env_GetFlags(interp, objc, objv, dbenv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *dbenv;
{
	int i, ret, result;
	u_int32_t flags;
	char buf[512];
	Tcl_Obj *res;

	static const struct {
		u_int32_t flag;
		char *arg;
	} open_flags[] = {
		{ DB_AUTO_COMMIT, "-auto_commit" },
		{ DB_CDB_ALLDB, "-cdb_alldb" },
		{ DB_DIRECT_DB, "-direct_db" },
		{ DB_DIRECT_LOG, "-direct_log" },
		{ DB_DSYNC_LOG, "-dsync_log" },
		{ DB_LOG_AUTOREMOVE, "-log_remove" },
		{ DB_LOG_INMEMORY, "-log_inmemory" },
		{ DB_NOLOCKING, "-nolock" },
		{ DB_NOMMAP, "-nommap" },
		{ DB_NOPANIC, "-nopanic" },
		{ DB_OVERWRITE, "-overwrite" },
		{ DB_PANIC_ENVIRONMENT, "-panic" },
		{ DB_REGION_INIT, "-region_init" },
		{ DB_TXN_NOSYNC, "-nosync" },
		{ DB_TXN_WRITE_NOSYNC, "-wrnosync" },
		{ DB_YIELDCPU, "-yield" },
		{ 0, NULL }
	};

	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, NULL);
		return (TCL_ERROR);
	}

	ret = dbenv->get_flags(dbenv, &flags);
	if ((result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
	    "env get_flags")) == TCL_OK) {
		buf[0] = '\0';

		for (i = 0; open_flags[i].flag != 0; i++)
			if (LF_ISSET(open_flags[i].flag)) {
				if (strlen(buf) > 0)
					(void)strncat(buf, " ", sizeof(buf));
				(void)strncat(
				    buf, open_flags[i].arg, sizeof(buf));
			}

		res = NewStringObj(buf, strlen(buf));
		Tcl_SetObjResult(interp, res);
	}

	return (result);
}

/*
 * env_GetOpenFlag --
 *	Implements the ENV->get_open_flags command.
 */
static int
env_GetOpenFlag(interp, objc, objv, dbenv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *dbenv;
{
	int i, ret, result;
	u_int32_t flags;
	char buf[512];
	Tcl_Obj *res;

	static const struct {
		u_int32_t flag;
		char *arg;
	} open_flags[] = {
		{ DB_INIT_CDB, "-cdb" },
		{ DB_INIT_LOCK, "-lock" },
		{ DB_INIT_LOG, "-log" },
		{ DB_INIT_MPOOL, "-mpool" },
		{ DB_INIT_TXN, "-txn" },
		{ DB_RECOVER, "-recover" },
		{ DB_RECOVER_FATAL, "-recover_fatal" },
		{ DB_USE_ENVIRON, "-use_environ" },
		{ DB_USE_ENVIRON_ROOT, "-use_environ_root" },
		{ DB_CREATE, "-create" },
		{ DB_LOCKDOWN, "-lockdown" },
		{ DB_PRIVATE, "-private" },
		{ DB_SYSTEM_MEM, "-system_mem" },
		{ DB_THREAD, "-thread" },
		{ 0, NULL }
	};

	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, NULL);
		return (TCL_ERROR);
	}

	ret = dbenv->get_open_flags(dbenv, &flags);
	if ((result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
	    "env get_open_flags")) == TCL_OK) {
		buf[0] = '\0';

		for (i = 0; open_flags[i].flag != 0; i++)
			if (LF_ISSET(open_flags[i].flag)) {
				if (strlen(buf) > 0)
					(void)strncat(buf, " ", sizeof(buf));
				(void)strncat(
				    buf, open_flags[i].arg, sizeof(buf));
			}

		res = NewStringObj(buf, strlen(buf));
		Tcl_SetObjResult(interp, res);
	}

	return (result);
}

/*
 * PUBLIC: int tcl_EnvGetEncryptFlags __P((Tcl_Interp *, int, Tcl_Obj * CONST*,
 * PUBLIC:      DB_ENV *));
 *
 * tcl_EnvGetEncryptFlags --
 *	Implements the ENV->get_encrypt_flags command.
 */
int
tcl_EnvGetEncryptFlags(interp, objc, objv, dbenv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *dbenv;			/* Database pointer */
{
	int i, ret, result;
	u_int32_t flags;
	char buf[512];
	Tcl_Obj *res;

	static const struct {
		u_int32_t flag;
		char *arg;
	} encrypt_flags[] = {
		{ DB_ENCRYPT_AES, "-encryptaes" },
		{ 0, NULL }
	};

	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, NULL);
		return (TCL_ERROR);
	}

	ret = dbenv->get_encrypt_flags(dbenv, &flags);
	if ((result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
	    "env get_encrypt_flags")) == TCL_OK) {
		buf[0] = '\0';

		for (i = 0; encrypt_flags[i].flag != 0; i++)
			if (LF_ISSET(encrypt_flags[i].flag)) {
				if (strlen(buf) > 0)
					(void)strncat(buf, " ", sizeof(buf));
				(void)strncat(
				    buf, encrypt_flags[i].arg, sizeof(buf));
			}

		res = NewStringObj(buf, strlen(buf));
		Tcl_SetObjResult(interp, res);
	}

	return (result);
}

/*
 * env_GetLockDetect --
 *	Implements the ENV->get_lk_detect command.
 */
static int
env_GetLockDetect(interp, objc, objv, dbenv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *dbenv;
{
	int i, ret, result;
	u_int32_t lk_detect;
	const char *answer;
	Tcl_Obj *res;
	static const struct {
		u_int32_t flag;
		char *name;
	} lk_detect_returns[] = {
		{ DB_LOCK_DEFAULT, "default" },
		{ DB_LOCK_EXPIRE, "expire" },
		{ DB_LOCK_MAXLOCKS, "maxlocks" },
		{ DB_LOCK_MAXWRITE, "maxwrite" },
		{ DB_LOCK_MINLOCKS, "minlocks" },
		{ DB_LOCK_MINWRITE, "minwrite" },
		{ DB_LOCK_OLDEST, "oldest" },
		{ DB_LOCK_RANDOM, "random" },
		{ DB_LOCK_YOUNGEST, "youngest" },
		{ 0, NULL }
	};

	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, NULL);
		return (TCL_ERROR);
	}
	ret = dbenv->get_lk_detect(dbenv, &lk_detect);
	if ((result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
	    "env get_lk_detect")) == TCL_OK) {
		answer = "unknown";
		for (i = 0; lk_detect_returns[i].flag != 0; i++)
			if (lk_detect == lk_detect_returns[i].flag)
				answer = lk_detect_returns[i].name;

		res = NewStringObj(answer, strlen(answer));
		Tcl_SetObjResult(interp, res);
	}

	return (result);
}

/*
 * env_GetTimeout --
 *	Implements the ENV->get_timeout command.
 */
static int
env_GetTimeout(interp, objc, objv, dbenv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *dbenv;
{
	static const struct {
		u_int32_t flag;
		char *arg;
	} timeout_flags[] = {
		{ DB_SET_TXN_TIMEOUT, "txn" },
		{ DB_SET_LOCK_TIMEOUT, "lock" },
		{ 0, NULL }
	};
	Tcl_Obj *res;
	db_timeout_t timeout;
	u_int32_t which;
	int i, ret, result;
	const char *arg;

	COMPQUIET(timeout, 0);

	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, NULL);
		return (TCL_ERROR);
	}

	arg = Tcl_GetStringFromObj(objv[2], NULL);
	which = 0;
	for (i = 0; timeout_flags[i].flag != 0; i++)
		if (strcmp(arg, timeout_flags[i].arg) == 0)
			which = timeout_flags[i].flag;
	if (which == 0) {
		ret = EINVAL;
		goto err;
	}

	ret = dbenv->get_timeout(dbenv, &timeout, which);
err:	if ((result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
	    "env get_timeout")) == TCL_OK) {
		res = Tcl_NewLongObj((long)timeout);
		Tcl_SetObjResult(interp, res);
	}

	return (result);
}

/*
 * env_GetVerbose --
 *	Implements the ENV->get_open_flags command.
 */
static int
env_GetVerbose(interp, objc, objv, dbenv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *dbenv;
{
	static const struct {
		u_int32_t flag;
		char *arg;
	} verbose_flags[] = {
		{ DB_VERB_DEADLOCK, "deadlock" },
		{ DB_VERB_RECOVERY, "recovery" },
		{ DB_VERB_REPLICATION, "rep" },
		{ DB_VERB_WAITSFOR, "wait" },
		{ 0, NULL }
	};
	Tcl_Obj *res;
	u_int32_t which;
	int i, onoff, ret, result;
	const char *arg, *answer;

	COMPQUIET(onoff, 0);

	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, NULL);
		return (TCL_ERROR);
	}

	arg = Tcl_GetStringFromObj(objv[2], NULL);
	which = 0;
	for (i = 0; verbose_flags[i].flag != 0; i++)
		if (strcmp(arg, verbose_flags[i].arg) == 0)
			which = verbose_flags[i].flag;
	if (which == 0) {
		ret = EINVAL;
		goto err;
	}

	ret = dbenv->get_verbose(dbenv, which, &onoff);
err:	if ((result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
	    "env get_timeout")) == 0) {
		answer = onoff ? "on" : "off";
		res = NewStringObj(answer, strlen(answer));
		Tcl_SetObjResult(interp, res);
	}

	return (result);
}

/*
 * PUBLIC: void tcl_EnvSetErrfile __P((Tcl_Interp *, DB_ENV *, DBTCL_INFO *,
 * PUBLIC:    char *));
 *
 * tcl_EnvSetErrfile --
 *	Implements the ENV->set_errfile command.
 */
void
tcl_EnvSetErrfile(interp, dbenv, ip, errf)
	Tcl_Interp *interp;		/* Interpreter */
	DB_ENV *dbenv;			/* Database pointer */
	DBTCL_INFO *ip;			/* Our internal info */
	char *errf;
{
	COMPQUIET(interp, NULL);
	/*
	 * If the user already set one, free it.
	 */
	if (ip->i_err != NULL && ip->i_err != stdout &&
	    ip->i_err != stderr)
		(void)fclose(ip->i_err);
	if (strcmp(errf, "/dev/stdout") == 0)
		ip->i_err = stdout;
	else if (strcmp(errf, "/dev/stderr") == 0)
		ip->i_err = stderr;
	else
		ip->i_err = fopen(errf, "a");
	if (ip->i_err != NULL)
		dbenv->set_errfile(dbenv, ip->i_err);
}

/*
 * PUBLIC: int tcl_EnvSetErrpfx __P((Tcl_Interp *, DB_ENV *, DBTCL_INFO *,
 * PUBLIC:    char *));
 *
 * tcl_EnvSetErrpfx --
 *	Implements the ENV->set_errpfx command.
 */
int
tcl_EnvSetErrpfx(interp, dbenv, ip, pfx)
	Tcl_Interp *interp;		/* Interpreter */
	DB_ENV *dbenv;			/* Database pointer */
	DBTCL_INFO *ip;			/* Our internal info */
	char *pfx;
{
	int result, ret;

	/*
	 * Assume success.  The only thing that can fail is
	 * the __os_strdup.
	 */
	result = TCL_OK;
	Tcl_SetResult(interp, "0", TCL_STATIC);
	/*
	 * If the user already set one, free it.
	 */
	if (ip->i_errpfx != NULL)
		__os_free(dbenv, ip->i_errpfx);
	if ((ret = __os_strdup(dbenv, pfx, &ip->i_errpfx)) != 0) {
		result = _ReturnSetup(interp, ret,
		    DB_RETOK_STD(ret), "__os_strdup");
		ip->i_errpfx = NULL;
	}
	if (ip->i_errpfx != NULL)
		dbenv->set_errpfx(dbenv, ip->i_errpfx);
	return (result);
}
