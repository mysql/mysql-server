/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: tcl_log.c,v 11.21 2000/11/30 00:58:45 ubell Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#include <tcl.h>
#endif

#include "db_int.h"
#include "tcl_db.h"

/*
 * tcl_LogArchive --
 *
 * PUBLIC: int tcl_LogArchive __P((Tcl_Interp *, int,
 * PUBLIC:    Tcl_Obj * CONST*, DB_ENV *));
 */
int
tcl_LogArchive(interp, objc, objv, envp)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *envp;			/* Environment pointer */
{
	static char *archopts[] = {
		"-arch_abs",	"-arch_data",	"-arch_log",
		NULL
	};
	enum archopts {
		ARCH_ABS,	ARCH_DATA,	ARCH_LOG
	};
	Tcl_Obj *fileobj, *res;
	u_int32_t flag;
	int i, optindex, result, ret;
	char **file, **list;

	result = TCL_OK;
	flag = 0;
	/*
	 * Get the flag index from the object based on the options
	 * defined above.
	 */
	i = 2;
	while (i < objc) {
		if (Tcl_GetIndexFromObj(interp, objv[i],
		    archopts, "option", TCL_EXACT, &optindex) != TCL_OK)
			return (IS_HELP(objv[i]));
		i++;
		switch ((enum archopts)optindex) {
		case ARCH_ABS:
			flag |= DB_ARCH_ABS;
			break;
		case ARCH_DATA:
			flag |= DB_ARCH_DATA;
			break;
		case ARCH_LOG:
			flag |= DB_ARCH_LOG;
			break;
		}
	}
	_debug_check();
	list = NULL;
	ret = log_archive(envp, &list, flag, NULL);
	result = _ReturnSetup(interp, ret, "log archive");
	if (result == TCL_OK) {
		res = Tcl_NewListObj(0, NULL);
		for (file = list; file != NULL && *file != NULL; file++) {
			fileobj = Tcl_NewStringObj(*file, strlen(*file));
			result = Tcl_ListObjAppendElement(interp, res, fileobj);
			if (result != TCL_OK)
				break;
		}
		Tcl_SetObjResult(interp, res);
	}
	if (list != NULL)
		__os_free(list, 0);
	return (result);
}

/*
 * tcl_LogCompare --
 *
 * PUBLIC: int tcl_LogCompare __P((Tcl_Interp *, int,
 * PUBLIC:    Tcl_Obj * CONST*));
 */
int
tcl_LogCompare(interp, objc, objv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
{
	DB_LSN lsn0, lsn1;
	Tcl_Obj *res;
	int result, ret;

	result = TCL_OK;
	/*
	 * No flags, must be 4 args.
	 */
	if (objc != 4) {
		Tcl_WrongNumArgs(interp, 2, objv, "lsn1 lsn2");
		return (TCL_ERROR);
	}

	result = _GetLsn(interp, objv[2], &lsn0);
	if (result == TCL_ERROR)
		return (result);
	result = _GetLsn(interp, objv[3], &lsn1);
	if (result == TCL_ERROR)
		return (result);

	_debug_check();
	ret = log_compare(&lsn0, &lsn1);
	res = Tcl_NewIntObj(ret);
	Tcl_SetObjResult(interp, res);
	return (result);
}

/*
 * tcl_LogFile --
 *
 * PUBLIC: int tcl_LogFile __P((Tcl_Interp *, int,
 * PUBLIC:    Tcl_Obj * CONST*, DB_ENV *));
 */
int
tcl_LogFile(interp, objc, objv, envp)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *envp;			/* Environment pointer */
{
	DB_LSN lsn;
	Tcl_Obj *res;
	size_t len;
	int result, ret;
	char *name;

	result = TCL_OK;
	/*
	 * No flags, must be 3 args.
	 */
	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "lsn");
		return (TCL_ERROR);
	}

	result = _GetLsn(interp, objv[2], &lsn);
	if (result == TCL_ERROR)
		return (result);

	len = MSG_SIZE;
	ret = ENOMEM;
	name = NULL;
	while (ret == ENOMEM) {
		if (name != NULL)
			__os_free(name, len/2);
		ret = __os_malloc(envp, len, NULL, &name);
		if (ret != 0) {
			Tcl_SetResult(interp, db_strerror(ret), TCL_STATIC);
			break;
		}
		_debug_check();
		ret = log_file(envp, &lsn, name, len);
		len *= 2;
	}
	result = _ReturnSetup(interp, ret, "log_file");
	if (ret == 0) {
		res = Tcl_NewStringObj(name, strlen(name));
		Tcl_SetObjResult(interp, res);
	}

	if (name != NULL)
		__os_free(name, len/2);

	return (result);
}

/*
 * tcl_LogFlush --
 *
 * PUBLIC: int tcl_LogFlush __P((Tcl_Interp *, int,
 * PUBLIC:    Tcl_Obj * CONST*, DB_ENV *));
 */
int
tcl_LogFlush(interp, objc, objv, envp)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *envp;			/* Environment pointer */
{
	DB_LSN lsn, *lsnp;
	int result, ret;

	result = TCL_OK;
	/*
	 * No flags, must be 2 or 3 args.
	 */
	if (objc > 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "?lsn?");
		return (TCL_ERROR);
	}

	if (objc == 3) {
		lsnp = &lsn;
		result = _GetLsn(interp, objv[2], &lsn);
		if (result == TCL_ERROR)
			return (result);
	} else
		lsnp = NULL;

	_debug_check();
	ret = log_flush(envp, lsnp);
	result = _ReturnSetup(interp, ret, "log_flush");
	return (result);
}

/*
 * tcl_LogGet --
 *
 * PUBLIC: int tcl_LogGet __P((Tcl_Interp *, int,
 * PUBLIC:    Tcl_Obj * CONST*, DB_ENV *));
 */
int
tcl_LogGet(interp, objc, objv, envp)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *envp;			/* Environment pointer */
{
	static char *loggetopts[] = {
		"-checkpoint",	"-current",	"-first",
		"-last",	"-next",	"-prev",
		"-set",
		NULL
	};
	enum loggetopts {
		LOGGET_CKP,	LOGGET_CUR,	LOGGET_FIRST,
		LOGGET_LAST,	LOGGET_NEXT,	LOGGET_PREV,
		LOGGET_SET
	};
	DB_LSN lsn;
	DBT data;
	Tcl_Obj *dataobj, *lsnlist, *myobjv[2], *res;
	u_int32_t flag;
	int i, myobjc, optindex, result, ret;

	result = TCL_OK;
	flag = 0;
	if (objc < 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "?-args? lsn");
		return (TCL_ERROR);
	}

	/*
	 * Get the command name index from the object based on the options
	 * defined above.
	 */
	i = 2;
	while (i < objc) {
		if (Tcl_GetIndexFromObj(interp, objv[i],
		    loggetopts, "option", TCL_EXACT, &optindex) != TCL_OK)
			return (IS_HELP(objv[i]));
		i++;
		switch ((enum loggetopts)optindex) {
		case LOGGET_CKP:
			FLAG_CHECK(flag);
			flag |= DB_CHECKPOINT;
			break;
		case LOGGET_CUR:
			FLAG_CHECK(flag);
			flag |= DB_CURRENT;
			break;
		case LOGGET_FIRST:
			FLAG_CHECK(flag);
			flag |= DB_FIRST;
			break;
		case LOGGET_LAST:
			FLAG_CHECK(flag);
			flag |= DB_LAST;
			break;
		case LOGGET_NEXT:
			FLAG_CHECK(flag);
			flag |= DB_NEXT;
			break;
		case LOGGET_PREV:
			FLAG_CHECK(flag);
			flag |= DB_PREV;
			break;
		case LOGGET_SET:
			FLAG_CHECK(flag);
			flag |= DB_SET;
			if (i == objc) {
				Tcl_WrongNumArgs(interp, 2, objv, "?-set lsn?");
				result = TCL_ERROR;
				break;
			}
			result = _GetLsn(interp, objv[i++], &lsn);
			break;
		}
	}

	if (result == TCL_ERROR)
		return (result);

	memset(&data, 0, sizeof(data));
	data.flags |= DB_DBT_MALLOC;
	_debug_check();
	ret = log_get(envp, &lsn, &data, flag);
	res = Tcl_NewListObj(0, NULL);
	result = _ReturnSetup(interp, ret, "log_get");
	if (ret == 0) {
		/*
		 * Success.  Set up return list as {LSN data} where LSN
		 * is a sublist {file offset}.
		 */
		myobjc = 2;
		myobjv[0] = Tcl_NewIntObj(lsn.file);
		myobjv[1] = Tcl_NewIntObj(lsn.offset);
		lsnlist = Tcl_NewListObj(myobjc, myobjv);
		if (lsnlist == NULL) {
			if (data.data != NULL)
				__os_free(data.data, data.size);
			return (TCL_ERROR);
		}
		result = Tcl_ListObjAppendElement(interp, res, lsnlist);
		dataobj = Tcl_NewStringObj(data.data, data.size);
		result = Tcl_ListObjAppendElement(interp, res, dataobj);
	}
	if (data.data != NULL)
		__os_free(data.data, data.size);

	if (result == TCL_OK)
		Tcl_SetObjResult(interp, res);
	return (result);
}

/*
 * tcl_LogPut --
 *
 * PUBLIC: int tcl_LogPut __P((Tcl_Interp *, int,
 * PUBLIC:    Tcl_Obj * CONST*, DB_ENV *));
 */
int
tcl_LogPut(interp, objc, objv, envp)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *envp;			/* Environment pointer */
{
	static char *logputopts[] = {
		"-checkpoint",	"-curlsn",	"-flush",
		NULL
	};
	enum logputopts {
		LOGPUT_CKP,	LOGPUT_CUR,	LOGPUT_FLUSH
	};
	DB_LSN lsn;
	DBT data;
	Tcl_Obj *intobj, *res;
	u_int32_t flag;
	int itmp, optindex, result, ret;

	result = TCL_OK;
	flag = 0;
	if (objc < 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "?-args? record");
		return (TCL_ERROR);
	}

	/*
	 * Data/record must be the last arg.
	 */
	memset(&data, 0, sizeof(data));
	data.data = Tcl_GetByteArrayFromObj(objv[objc-1], &itmp);
	data.size = itmp;

	/*
	 * Get the command name index from the object based on the options
	 * defined above.
	 */
	if (objc == 4) {
		if (Tcl_GetIndexFromObj(interp, objv[2],
		    logputopts, "option", TCL_EXACT, &optindex) != TCL_OK) {
			return (IS_HELP(objv[2]));
		}
		switch ((enum logputopts)optindex) {
		case LOGPUT_CKP:
			flag = DB_CHECKPOINT;
			break;
		case LOGPUT_CUR:
			flag = DB_CURLSN;
			break;
		case LOGPUT_FLUSH:
			flag = DB_FLUSH;
			break;
		}
	}

	if (result == TCL_ERROR)
		return (result);

	_debug_check();
	ret = log_put(envp, &lsn, &data, flag);
	result = _ReturnSetup(interp, ret, "log_put");
	if (result == TCL_ERROR)
		return (result);
	res = Tcl_NewListObj(0, NULL);
	intobj = Tcl_NewIntObj(lsn.file);
	result = Tcl_ListObjAppendElement(interp, res, intobj);
	intobj = Tcl_NewIntObj(lsn.offset);
	result = Tcl_ListObjAppendElement(interp, res, intobj);
	Tcl_SetObjResult(interp, res);
	return (result);
}

/*
 * tcl_LogRegister --
 *
 * PUBLIC: int tcl_LogRegister __P((Tcl_Interp *, int,
 * PUBLIC:    Tcl_Obj * CONST*, DB_ENV *));
 */
int
tcl_LogRegister(interp, objc, objv, envp)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *envp;			/* Environment pointer */
{
	DB *dbp;
	Tcl_Obj *res;
	int result, ret;
	char *arg, msg[MSG_SIZE];

	result = TCL_OK;
	if (objc != 4) {
		Tcl_WrongNumArgs(interp, 2, objv, "db filename");
		return (TCL_ERROR);
	}
	/*
	 * First comes the DB.
	 */
	arg = Tcl_GetStringFromObj(objv[2], NULL);
	dbp = NAME_TO_DB(arg);
	if (dbp == NULL) {
		snprintf(msg, MSG_SIZE,
		    "LogRegister: Invalid db: %s\n", arg);
		Tcl_SetResult(interp, msg, TCL_VOLATILE);
		return (TCL_ERROR);
	}

	/*
	 * Next is the filename.
	 */
	arg = Tcl_GetStringFromObj(objv[3], NULL);

	_debug_check();
	ret = log_register(envp, dbp, arg);
	result = _ReturnSetup(interp, ret, "log_register");
	if (result == TCL_OK) {
		res = Tcl_NewIntObj((int)dbp->log_fileid);
		Tcl_SetObjResult(interp, res);
	}
	return (result);
}

/*
 * tcl_LogStat --
 *
 * PUBLIC: int tcl_LogStat __P((Tcl_Interp *, int,
 * PUBLIC:    Tcl_Obj * CONST*, DB_ENV *));
 */
int
tcl_LogStat(interp, objc, objv, envp)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *envp;			/* Environment pointer */
{
	DB_LOG_STAT *sp;
	Tcl_Obj *res;
	int result, ret;

	result = TCL_OK;
	/*
	 * No args for this.  Error if there are some.
	 */
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 2, objv, NULL);
		return (TCL_ERROR);
	}
	_debug_check();
	ret = log_stat(envp, &sp, NULL);
	result = _ReturnSetup(interp, ret, "log stat");
	if (result == TCL_ERROR)
		return (result);

	/*
	 * Have our stats, now construct the name value
	 * list pairs and free up the memory.
	 */
	res = Tcl_NewObj();
	/*
	 * MAKE_STAT_LIST assumes 'res' and 'error' label.
	 */
	MAKE_STAT_LIST("Magic", sp->st_magic);
	MAKE_STAT_LIST("Log file Version", sp->st_version);
	MAKE_STAT_LIST("Region size", sp->st_regsize);
	MAKE_STAT_LIST("Log file mode", sp->st_mode);
	MAKE_STAT_LIST("Log record cache size", sp->st_lg_bsize);
	MAKE_STAT_LIST("Maximum log file size", sp->st_lg_max);
	MAKE_STAT_LIST("Mbytes written", sp->st_w_mbytes);
	MAKE_STAT_LIST("Bytes written (over Mb)", sp->st_w_bytes);
	MAKE_STAT_LIST("Mbytes written since checkpoint", sp->st_wc_mbytes);
	MAKE_STAT_LIST("Bytes written (over Mb) since checkpoint",
	    sp->st_wc_bytes);
	MAKE_STAT_LIST("Times log written", sp->st_wcount);
	MAKE_STAT_LIST("Times log written because cache filled up",
	    sp->st_wcount_fill);
	MAKE_STAT_LIST("Times log flushed", sp->st_scount);
	MAKE_STAT_LIST("Current log file number", sp->st_cur_file);
	MAKE_STAT_LIST("Current log file offset", sp->st_cur_offset);
	MAKE_STAT_LIST("Number of region lock waits", sp->st_region_wait);
	MAKE_STAT_LIST("Number of region lock nowaits", sp->st_region_nowait);
	Tcl_SetObjResult(interp, res);
error:
	__os_free(sp, sizeof(*sp));
	return (result);
}

/*
 * tcl_LogUnregister --
 *
 * PUBLIC: int tcl_LogUnregister __P((Tcl_Interp *, int,
 * PUBLIC:    Tcl_Obj * CONST*, DB_ENV *));
 */
int
tcl_LogUnregister(interp, objc, objv, envp)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *envp;			/* Environment pointer */
{
	DB *dbp;
	char *arg, msg[MSG_SIZE];
	int result, ret;

	result = TCL_OK;
	/*
	 * 1 arg for this.  Error if more or less.
	 */
	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, NULL);
		return (TCL_ERROR);
	}
	arg = Tcl_GetStringFromObj(objv[2], NULL);
	dbp = NAME_TO_DB(arg);
	if (dbp == NULL) {
		snprintf(msg, MSG_SIZE,
		    "log_unregister: Invalid db identifier: %s\n", arg);
		Tcl_SetResult(interp, msg, TCL_VOLATILE);
		return (TCL_ERROR);
	}
	_debug_check();
	ret = log_unregister(envp, dbp);
	result = _ReturnSetup(interp, ret, "log_unregister");

	return (result);
}
