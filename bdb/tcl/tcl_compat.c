/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: tcl_compat.c,v 11.22 2001/01/11 18:19:55 bostic Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <tcl.h>
#endif

#define	DB_DBM_HSEARCH 1

#include "db_int.h"
#include "tcl_db.h"

/*
 * Prototypes for procedures defined later in this file:
 */
static int mutex_Cmd __P((ClientData, Tcl_Interp *, int, Tcl_Obj * CONST*));

/*
 * bdb_HCommand --
 *	Implements h* functions.
 *
 * PUBLIC: int bdb_HCommand __P((Tcl_Interp *, int, Tcl_Obj * CONST*));
 */
int
bdb_HCommand(interp, objc, objv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
{
	static char *hcmds[] = {
		"hcreate",
		"hdestroy",
		"hsearch",
		NULL
	};
	enum hcmds {
		HHCREATE,
		HHDESTROY,
		HHSEARCH
	};
	static char *srchacts[] = {
		"enter",
		"find",
		NULL
	};
	enum srchacts {
		ACT_ENTER,
		ACT_FIND
	};
	ENTRY item, *hres;
	ACTION action;
	int actindex, cmdindex, nelem, result, ret;
	Tcl_Obj *res;

	result = TCL_OK;
	/*
	 * Get the command name index from the object based on the cmds
	 * defined above.  This SHOULD NOT fail because we already checked
	 * in the 'berkdb' command.
	 */
	if (Tcl_GetIndexFromObj(interp,
	    objv[1], hcmds, "command", TCL_EXACT, &cmdindex) != TCL_OK)
		return (IS_HELP(objv[1]));

	res = NULL;
	switch ((enum hcmds)cmdindex) {
	case HHCREATE:
		/*
		 * Must be 1 arg, nelem.  Error if not.
		 */
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "nelem");
			return (TCL_ERROR);
		}
		result = Tcl_GetIntFromObj(interp, objv[2], &nelem);
		if (result == TCL_OK) {
			_debug_check();
			ret = hcreate(nelem) == 0 ? 1: 0;
			_ReturnSetup(interp, ret, "hcreate");
		}
		break;
	case HHSEARCH:
		/*
		 * 3 args for this.  Error if different.
		 */
		if (objc != 5) {
			Tcl_WrongNumArgs(interp, 2, objv, "key data action");
			return (TCL_ERROR);
		}
		item.key = Tcl_GetStringFromObj(objv[2], NULL);
		item.data = Tcl_GetStringFromObj(objv[3], NULL);
		action = 0;
		if (Tcl_GetIndexFromObj(interp, objv[4], srchacts,
		    "action", TCL_EXACT, &actindex) != TCL_OK)
			return (IS_HELP(objv[4]));
		switch ((enum srchacts)actindex) {
		case ACT_FIND:
			action = FIND;
			break;
		case ACT_ENTER:
			action = ENTER;
			break;
		}
		_debug_check();
		hres = hsearch(item, action);
		if (hres == NULL)
			Tcl_SetResult(interp, "-1", TCL_STATIC);
		else if (action == FIND)
			Tcl_SetResult(interp, (char *)hres->data, TCL_STATIC);
		else
			/* action is ENTER */
			Tcl_SetResult(interp, "0", TCL_STATIC);

		break;
	case HHDESTROY:
		/*
		 * No args for this.  Error if there are some.
		 */
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 2, objv, NULL);
			return (TCL_ERROR);
		}
		_debug_check();
		(void)hdestroy();
		res = Tcl_NewIntObj(0);
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
 *
 * bdb_NdbmOpen --
 *	Opens an ndbm database.
 *
 * PUBLIC: #if DB_DBM_HSEARCH != 0
 * PUBLIC: int bdb_NdbmOpen __P((Tcl_Interp *, int, Tcl_Obj * CONST*, DBM **));
 * PUBLIC: #endif
 */
int
bdb_NdbmOpen(interp, objc, objv, dbpp)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DBM **dbpp;			/* Dbm pointer */
{
	static char *ndbopen[] = {
		"-create",
		"-mode",
		"-rdonly",
		"-truncate",
		"--",
		NULL
	};
	enum ndbopen {
		NDB_CREATE,
		NDB_MODE,
		NDB_RDONLY,
		NDB_TRUNC,
		NDB_ENDARG
	};

	u_int32_t open_flags;
	int endarg, i, mode, optindex, read_only, result;
	char *arg, *db;

	result = TCL_OK;
	open_flags = 0;
	endarg = mode = 0;
	read_only = 0;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 2, objv, "?args?");
		return (TCL_ERROR);
	}

	/*
	 * Get the option name index from the object based on the args
	 * defined above.
	 */
	i = 2;
	while (i < objc) {
		if (Tcl_GetIndexFromObj(interp, objv[i], ndbopen, "option",
		    TCL_EXACT, &optindex) != TCL_OK) {
			arg = Tcl_GetStringFromObj(objv[i], NULL);
			if (arg[0] == '-') {
				result = IS_HELP(objv[i]);
				goto error;
			} else
				Tcl_ResetResult(interp);
			break;
		}
		i++;
		switch ((enum ndbopen)optindex) {
		case NDB_CREATE:
			open_flags |= O_CREAT;
			break;
		case NDB_RDONLY:
			read_only = 1;
			break;
		case NDB_TRUNC:
			open_flags |= O_TRUNC;
			break;
		case NDB_MODE:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-mode mode?");
				result = TCL_ERROR;
				break;
			}
			/*
			 * Don't need to check result here because
			 * if TCL_ERROR, the error message is already
			 * set up, and we'll bail out below.  If ok,
			 * the mode is set and we go on.
			 */
			result = Tcl_GetIntFromObj(interp, objv[i++], &mode);
			break;
		case NDB_ENDARG:
			endarg = 1;
			break;
		} /* switch */

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
	 * Any args we have left, (better be 0, or 1 left) is a
	 * file name.  If we have 0, then an in-memory db.  If
	 * there is 1, a db name.
	 */
	db = NULL;
	if (i != objc && i != objc - 1) {
		Tcl_WrongNumArgs(interp, 2, objv, "?args? ?file?");
		result = TCL_ERROR;
		goto error;
	}
	if (i != objc)
		db = Tcl_GetStringFromObj(objv[objc - 1], NULL);

	/*
	 * When we get here, we have already parsed all of our args
	 * and made all our calls to set up the database.  Everything
	 * is okay so far, no errors, if we get here.
	 *
	 * Now open the database.
	 */
	if (read_only)
		open_flags |= O_RDONLY;
	else
		open_flags |= O_RDWR;
	_debug_check();
	if ((*dbpp = dbm_open(db, open_flags, mode)) == NULL) {
		result = _ReturnSetup(interp, Tcl_GetErrno(), "db open");
		goto error;
	}
	return (TCL_OK);

error:
	*dbpp = NULL;
	return (result);
}

/*
 * bdb_DbmCommand --
 *	Implements "dbm" commands.
 *
 * PUBLIC: #if DB_DBM_HSEARCH != 0
 * PUBLIC: int bdb_DbmCommand
 * PUBLIC:     __P((Tcl_Interp *, int, Tcl_Obj * CONST*, int, DBM *));
 * PUBLIC: #endif
 */
int
bdb_DbmCommand(interp, objc, objv, flag, dbm)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	int flag;			/* Which db interface */
	DBM *dbm;			/* DBM pointer */
{
	static char *dbmcmds[] = {
		"dbmclose",
		"dbminit",
		"delete",
		"fetch",
		"firstkey",
		"nextkey",
		"store",
		NULL
	};
	enum dbmcmds {
		DBMCLOSE,
		DBMINIT,
		DBMDELETE,
		DBMFETCH,
		DBMFIRST,
		DBMNEXT,
		DBMSTORE
	};
	static char *stflag[] = {
		"insert",	"replace",
		NULL
	};
	enum stflag {
		STINSERT,	STREPLACE
	};
	datum key, data;
	int cmdindex, stindex, result, ret;
	char *name, *t;

	result = TCL_OK;
	/*
	 * Get the command name index from the object based on the cmds
	 * defined above.  This SHOULD NOT fail because we already checked
	 * in the 'berkdb' command.
	 */
	if (Tcl_GetIndexFromObj(interp,
	    objv[1], dbmcmds, "command", TCL_EXACT, &cmdindex) != TCL_OK)
		return (IS_HELP(objv[1]));

	switch ((enum dbmcmds)cmdindex) {
	case DBMCLOSE:
		/*
		 * No arg for this.  Error if different.
		 */
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 2, objv, NULL);
			return (TCL_ERROR);
		}
		_debug_check();
		if (flag == DBTCL_DBM)
			ret = dbmclose();
		else {
			Tcl_SetResult(interp,
			    "Bad interface flag for command", TCL_STATIC);
			return (TCL_ERROR);
		}
		_ReturnSetup(interp, ret, "dbmclose");
		break;
	case DBMINIT:
		/*
		 * Must be 1 arg - file.
		 */
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "file");
			return (TCL_ERROR);
		}
		name = Tcl_GetStringFromObj(objv[2], NULL);
		if (flag == DBTCL_DBM)
			ret = dbminit(name);
		else {
			Tcl_SetResult(interp, "Bad interface flag for command",
			    TCL_STATIC);
			return (TCL_ERROR);
		}
		_ReturnSetup(interp, ret, "dbminit");
		break;
	case DBMFETCH:
		/*
		 * 1 arg for this.  Error if different.
		 */
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "key");
			return (TCL_ERROR);
		}
		key.dptr = (char *)Tcl_GetByteArrayFromObj(objv[2], &key.dsize);
		_debug_check();
		if (flag == DBTCL_DBM)
			data = fetch(key);
		else if (flag == DBTCL_NDBM)
			data = dbm_fetch(dbm, key);
		else {
			Tcl_SetResult(interp,
			    "Bad interface flag for command", TCL_STATIC);
			return (TCL_ERROR);
		}
		if (data.dptr == NULL ||
		    (ret = __os_malloc(NULL, data.dsize + 1, NULL, &t)) != 0)
			Tcl_SetResult(interp, "-1", TCL_STATIC);
		else {
			memcpy(t, data.dptr, data.dsize);
			t[data.dsize] = '\0';
			Tcl_SetResult(interp, t, TCL_VOLATILE);
			__os_free(t, data.dsize + 1);
		}
		break;
	case DBMSTORE:
		/*
		 * 2 args for this.  Error if different.
		 */
		if (objc != 4 && flag == DBTCL_DBM) {
			Tcl_WrongNumArgs(interp, 2, objv, "key data");
			return (TCL_ERROR);
		}
		if (objc != 5 && flag == DBTCL_NDBM) {
			Tcl_WrongNumArgs(interp, 2, objv, "key data action");
			return (TCL_ERROR);
		}
		key.dptr = (char *)Tcl_GetByteArrayFromObj(objv[2], &key.dsize);
		data.dptr =
		    (char *)Tcl_GetByteArrayFromObj(objv[3], &data.dsize);
		_debug_check();
		if (flag == DBTCL_DBM)
			ret = store(key, data);
		else if (flag == DBTCL_NDBM) {
			if (Tcl_GetIndexFromObj(interp, objv[4], stflag,
			    "flag", TCL_EXACT, &stindex) != TCL_OK)
				return (IS_HELP(objv[4]));
			switch ((enum stflag)stindex) {
			case STINSERT:
				flag = DBM_INSERT;
				break;
			case STREPLACE:
				flag = DBM_REPLACE;
				break;
			}
			ret = dbm_store(dbm, key, data, flag);
		} else {
			Tcl_SetResult(interp,
			    "Bad interface flag for command", TCL_STATIC);
			return (TCL_ERROR);
		}
		_ReturnSetup(interp, ret, "store");
		break;
	case DBMDELETE:
		/*
		 * 1 arg for this.  Error if different.
		 */
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "key");
			return (TCL_ERROR);
		}
		key.dptr = (char *)Tcl_GetByteArrayFromObj(objv[2], &key.dsize);
		_debug_check();
		if (flag == DBTCL_DBM)
			ret = delete(key);
		else if (flag == DBTCL_NDBM)
			ret = dbm_delete(dbm, key);
		else {
			Tcl_SetResult(interp,
			    "Bad interface flag for command", TCL_STATIC);
			return (TCL_ERROR);
		}
		_ReturnSetup(interp, ret, "delete");
		break;
	case DBMFIRST:
		/*
		 * No arg for this.  Error if different.
		 */
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 2, objv, NULL);
			return (TCL_ERROR);
		}
		_debug_check();
		if (flag == DBTCL_DBM)
			key = firstkey();
		else if (flag == DBTCL_NDBM)
			key = dbm_firstkey(dbm);
		else {
			Tcl_SetResult(interp,
			    "Bad interface flag for command", TCL_STATIC);
			return (TCL_ERROR);
		}
		if (key.dptr == NULL ||
		    (ret = __os_malloc(NULL, key.dsize + 1, NULL, &t)) != 0)
			Tcl_SetResult(interp, "-1", TCL_STATIC);
		else {
			memcpy(t, key.dptr, key.dsize);
			t[key.dsize] = '\0';
			Tcl_SetResult(interp, t, TCL_VOLATILE);
			__os_free(t, key.dsize + 1);
		}
		break;
	case DBMNEXT:
		/*
		 * 0 or 1 arg for this.  Error if different.
		 */
		_debug_check();
		if (flag == DBTCL_DBM) {
			if (objc != 3) {
				Tcl_WrongNumArgs(interp, 2, objv, NULL);
				return (TCL_ERROR);
			}
			key.dptr = (char *)
			    Tcl_GetByteArrayFromObj(objv[2], &key.dsize);
			data = nextkey(key);
		} else if (flag == DBTCL_NDBM) {
			if (objc != 2) {
				Tcl_WrongNumArgs(interp, 2, objv, NULL);
				return (TCL_ERROR);
			}
			data = dbm_nextkey(dbm);
		} else {
			Tcl_SetResult(interp,
			    "Bad interface flag for command", TCL_STATIC);
			return (TCL_ERROR);
		}
		if (data.dptr == NULL ||
		    (ret = __os_malloc(NULL, data.dsize + 1, NULL, &t)) != 0)
			Tcl_SetResult(interp, "-1", TCL_STATIC);
		else {
			memcpy(t, data.dptr, data.dsize);
			t[data.dsize] = '\0';
			Tcl_SetResult(interp, t, TCL_VOLATILE);
			__os_free(t, data.dsize + 1);
		}
		break;
	}
	return (result);
}

/*
 * ndbm_Cmd --
 *	Implements the "ndbm" widget.
 *
 * PUBLIC: int ndbm_Cmd __P((ClientData, Tcl_Interp *, int, Tcl_Obj * CONST*));
 */
int
ndbm_Cmd(clientData, interp, objc, objv)
	ClientData clientData;		/* DB handle */
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
{
	static char *ndbcmds[] = {
		"clearerr",
		"close",
		"delete",
		"dirfno",
		"error",
		"fetch",
		"firstkey",
		"nextkey",
		"pagfno",
		"rdonly",
		"store",
		NULL
	};
	enum ndbcmds {
		NDBCLRERR,
		NDBCLOSE,
		NDBDELETE,
		NDBDIRFNO,
		NDBERR,
		NDBFETCH,
		NDBFIRST,
		NDBNEXT,
		NDBPAGFNO,
		NDBRDONLY,
		NDBSTORE
	};
	DBM *dbp;
	DBTCL_INFO *dbip;
	Tcl_Obj *res;
	int cmdindex, result, ret;

	Tcl_ResetResult(interp);
	dbp = (DBM *)clientData;
	dbip = _PtrToInfo((void *)dbp);
	result = TCL_OK;
	if (objc <= 1) {
		Tcl_WrongNumArgs(interp, 1, objv, "command cmdargs");
		return (TCL_ERROR);
	}
	if (dbp == NULL) {
		Tcl_SetResult(interp, "NULL db pointer", TCL_STATIC);
		return (TCL_ERROR);
	}
	if (dbip == NULL) {
		Tcl_SetResult(interp, "NULL db info pointer", TCL_STATIC);
		return (TCL_ERROR);
	}

	/*
	 * Get the command name index from the object based on the dbcmds
	 * defined above.
	 */
	if (Tcl_GetIndexFromObj(interp,
	    objv[1], ndbcmds, "command", TCL_EXACT, &cmdindex) != TCL_OK)
		return (IS_HELP(objv[1]));

	res = NULL;
	switch ((enum ndbcmds)cmdindex) {
	case NDBCLOSE:
		_debug_check();
		dbm_close(dbp);
		(void)Tcl_DeleteCommand(interp, dbip->i_name);
		_DeleteInfo(dbip);
		res = Tcl_NewIntObj(0);
		break;
	case NDBDELETE:
	case NDBFETCH:
	case NDBFIRST:
	case NDBNEXT:
	case NDBSTORE:
		result = bdb_DbmCommand(interp, objc, objv, DBTCL_NDBM, dbp);
		break;
	case NDBCLRERR:
		/*
		 * No args for this.  Error if there are some.
		 */
		if (objc > 2) {
			Tcl_WrongNumArgs(interp, 2, objv, NULL);
			return (TCL_ERROR);
		}
		_debug_check();
		ret = dbm_clearerr(dbp);
		if (ret)
			_ReturnSetup(interp, ret, "clearerr");
		else
			res = Tcl_NewIntObj(ret);
		break;
	case NDBDIRFNO:
		/*
		 * No args for this.  Error if there are some.
		 */
		if (objc > 2) {
			Tcl_WrongNumArgs(interp, 2, objv, NULL);
			return (TCL_ERROR);
		}
		_debug_check();
		ret = dbm_dirfno(dbp);
		res = Tcl_NewIntObj(ret);
		break;
	case NDBPAGFNO:
		/*
		 * No args for this.  Error if there are some.
		 */
		if (objc > 2) {
			Tcl_WrongNumArgs(interp, 2, objv, NULL);
			return (TCL_ERROR);
		}
		_debug_check();
		ret = dbm_pagfno(dbp);
		res = Tcl_NewIntObj(ret);
		break;
	case NDBERR:
		/*
		 * No args for this.  Error if there are some.
		 */
		if (objc > 2) {
			Tcl_WrongNumArgs(interp, 2, objv, NULL);
			return (TCL_ERROR);
		}
		_debug_check();
		ret = dbm_error(dbp);
		Tcl_SetErrno(ret);
		Tcl_SetResult(interp, Tcl_PosixError(interp), TCL_STATIC);
		break;
	case NDBRDONLY:
		/*
		 * No args for this.  Error if there are some.
		 */
		if (objc > 2) {
			Tcl_WrongNumArgs(interp, 2, objv, NULL);
			return (TCL_ERROR);
		}
		_debug_check();
		ret = dbm_rdonly(dbp);
		if (ret)
			_ReturnSetup(interp, ret, "rdonly");
		else
			res = Tcl_NewIntObj(ret);
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
 * bdb_RandCommand --
 *	Implements rand* functions.
 *
 * PUBLIC: int bdb_RandCommand __P((Tcl_Interp *, int, Tcl_Obj * CONST*));
 */
int
bdb_RandCommand(interp, objc, objv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
{
	static char *rcmds[] = {
		"rand",	"random_int",	"srand",
		NULL
	};
	enum rcmds {
		RRAND, RRAND_INT, RSRAND
	};
	long t;
	int cmdindex, hi, lo, result, ret;
	Tcl_Obj *res;
	char msg[MSG_SIZE];

	result = TCL_OK;
	/*
	 * Get the command name index from the object based on the cmds
	 * defined above.  This SHOULD NOT fail because we already checked
	 * in the 'berkdb' command.
	 */
	if (Tcl_GetIndexFromObj(interp,
	    objv[1], rcmds, "command", TCL_EXACT, &cmdindex) != TCL_OK)
		return (IS_HELP(objv[1]));

	res = NULL;
	switch ((enum rcmds)cmdindex) {
	case RRAND:
		/*
		 * Must be 0 args.  Error if different.
		 */
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 2, objv, NULL);
			return (TCL_ERROR);
		}
		ret = rand();
		res = Tcl_NewIntObj(ret);
		break;
	case RRAND_INT:
		/*
		 * Must be 4 args.  Error if different.
		 */
		if (objc != 4) {
			Tcl_WrongNumArgs(interp, 2, objv, "lo hi");
			return (TCL_ERROR);
		}
		result = Tcl_GetIntFromObj(interp, objv[2], &lo);
		if (result != TCL_OK)
			break;
		result = Tcl_GetIntFromObj(interp, objv[3], &hi);
		if (result == TCL_OK) {
#ifndef RAND_MAX
#define	RAND_MAX	0x7fffffff
#endif
			t = rand();
			if (t > RAND_MAX) {
				snprintf(msg, MSG_SIZE,
				    "Max random is higher than %ld\n",
				    (long)RAND_MAX);
				Tcl_SetResult(interp, msg, TCL_VOLATILE);
				result = TCL_ERROR;
				break;
			}
			_debug_check();
			ret = (int)(((double)t / ((double)(RAND_MAX) + 1)) *
			    (hi - lo + 1));
			ret += lo;
			res = Tcl_NewIntObj(ret);
		}
		break;
	case RSRAND:
		/*
		 * Must be 1 arg.  Error if different.
		 */
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "seed");
			return (TCL_ERROR);
		}
		result = Tcl_GetIntFromObj(interp, objv[2], &lo);
		if (result == TCL_OK) {
			srand((u_int)lo);
			res = Tcl_NewIntObj(0);
		}
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
 *
 * tcl_Mutex --
 *	Opens an env mutex.
 *
 * PUBLIC: int tcl_Mutex __P((Tcl_Interp *, int, Tcl_Obj * CONST*, DB_ENV *,
 * PUBLIC:    DBTCL_INFO *));
 */
int
tcl_Mutex(interp, objc, objv, envp, envip)
	Tcl_Interp *interp;             /* Interpreter */
	int objc;                       /* How many arguments? */
	Tcl_Obj *CONST objv[];          /* The argument objects */
	DB_ENV *envp;			/* Environment pointer */
	DBTCL_INFO *envip;		/* Info pointer */
{
	DBTCL_INFO *ip;
	Tcl_Obj *res;
	_MUTEX_DATA *md;
	int i, mode, nitems, result, ret;
	char newname[MSG_SIZE];

	md = NULL;
	result = TCL_OK;
	mode = nitems = ret = 0;
	memset(newname, 0, MSG_SIZE);

	if (objc != 4) {
		Tcl_WrongNumArgs(interp, 2, objv, "mode nitems");
		return (TCL_ERROR);
	}
	result = Tcl_GetIntFromObj(interp, objv[2], &mode);
	if (result != TCL_OK)
		return (TCL_ERROR);
	result = Tcl_GetIntFromObj(interp, objv[3], &nitems);
	if (result != TCL_OK)
		return (TCL_ERROR);

	snprintf(newname, sizeof(newname),
	    "%s.mutex%d", envip->i_name, envip->i_envmutexid);
	ip = _NewInfo(interp, NULL, newname, I_MUTEX);
	if (ip == NULL) {
		Tcl_SetResult(interp, "Could not set up info",
		    TCL_STATIC);
		return (TCL_ERROR);
	}
	/*
	 * Set up mutex.
	 */
	/*
	 * Map in the region.
	 *
	 * XXX
	 * We don't bother doing this "right", i.e., using the shalloc
	 * functions, just grab some memory knowing that it's correctly
	 * aligned.
	 */
	_debug_check();
	if (__os_calloc(NULL, 1, sizeof(_MUTEX_DATA), &md) != 0)
		goto posixout;
	md->env = envp;
	md->n_mutex = nitems;
	md->size = sizeof(_MUTEX_ENTRY) * nitems;

	md->reginfo.type = REGION_TYPE_MUTEX;
	md->reginfo.id = INVALID_REGION_TYPE;
	md->reginfo.mode = mode;
	md->reginfo.flags = REGION_CREATE_OK | REGION_JOIN_OK;
	if ((ret = __db_r_attach(envp, &md->reginfo, md->size)) != 0)
		goto posixout;
	md->marray = md->reginfo.addr;

	/* Initialize a created region. */
	if (F_ISSET(&md->reginfo, REGION_CREATE))
		for (i = 0; i < nitems; i++) {
			md->marray[i].val = 0;
			if ((ret =
			    __db_mutex_init(envp, &md->marray[i].m, i, 0)) != 0)
				goto posixout;
		}
	R_UNLOCK(envp, &md->reginfo);

	/*
	 * Success.  Set up return.  Set up new info
	 * and command widget for this mutex.
	 */
	envip->i_envmutexid++;
	ip->i_parent = envip;
	_SetInfoData(ip, md);
	Tcl_CreateObjCommand(interp, newname,
	    (Tcl_ObjCmdProc *)mutex_Cmd, (ClientData)md, NULL);
	res = Tcl_NewStringObj(newname, strlen(newname));
	Tcl_SetObjResult(interp, res);

	return (TCL_OK);

posixout:
	if (ret > 0)
		Tcl_PosixError(interp);
	result = _ReturnSetup(interp, ret, "mutex");
	_DeleteInfo(ip);

	if (md != NULL) {
		if (md->reginfo.addr != NULL)
			(void)__db_r_detach(md->env,
			    &md->reginfo, F_ISSET(&md->reginfo, REGION_CREATE));
		__os_free(md, sizeof(*md));
	}
	return (result);
}

/*
 * mutex_Cmd --
 *	Implements the "mutex" widget.
 */
static int
mutex_Cmd(clientData, interp, objc, objv)
	ClientData clientData;          /* Mutex handle */
	Tcl_Interp *interp;             /* Interpreter */
	int objc;                       /* How many arguments? */
	Tcl_Obj *CONST objv[];          /* The argument objects */
{
	static char *mxcmds[] = {
		"close",
		"get",
		"getval",
		"release",
		"setval",
		NULL
	};
	enum mxcmds {
		MXCLOSE,
		MXGET,
		MXGETVAL,
		MXRELE,
		MXSETVAL
	};
	DB_ENV *dbenv;
	DBTCL_INFO *envip, *mpip;
	_MUTEX_DATA *mp;
	Tcl_Obj *res;
	int cmdindex, id, result, newval;

	Tcl_ResetResult(interp);
	mp = (_MUTEX_DATA *)clientData;
	mpip = _PtrToInfo((void *)mp);
	envip = mpip->i_parent;
	dbenv = envip->i_envp;
	result = TCL_OK;

	if (mp == NULL) {
		Tcl_SetResult(interp, "NULL mp pointer", TCL_STATIC);
		return (TCL_ERROR);
	}
	if (mpip == NULL) {
		Tcl_SetResult(interp, "NULL mp info pointer", TCL_STATIC);
		return (TCL_ERROR);
	}

	/*
	 * Get the command name index from the object based on the dbcmds
	 * defined above.
	 */
	if (Tcl_GetIndexFromObj(interp,
	    objv[1], mxcmds, "command", TCL_EXACT, &cmdindex) != TCL_OK)
		return (IS_HELP(objv[1]));

	res = NULL;
	switch ((enum mxcmds)cmdindex) {
	case MXCLOSE:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, NULL);
			return (TCL_ERROR);
		}
		_debug_check();
		(void)__db_r_detach(mp->env, &mp->reginfo, 0);
		res = Tcl_NewIntObj(0);
		(void)Tcl_DeleteCommand(interp, mpip->i_name);
		_DeleteInfo(mpip);
		__os_free(mp, sizeof(*mp));
		break;
	case MXRELE:
		/*
		 * Check for 1 arg.  Error if different.
		 */
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "id");
			return (TCL_ERROR);
		}
		result = Tcl_GetIntFromObj(interp, objv[2], &id);
		if (result != TCL_OK)
			break;
		MUTEX_UNLOCK(dbenv, &mp->marray[id].m);
		res = Tcl_NewIntObj(0);
		break;
	case MXGET:
		/*
		 * Check for 1 arg.  Error if different.
		 */
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "id");
			return (TCL_ERROR);
		}
		result = Tcl_GetIntFromObj(interp, objv[2], &id);
		if (result != TCL_OK)
			break;
		MUTEX_LOCK(dbenv, &mp->marray[id].m, mp->env->lockfhp);
		res = Tcl_NewIntObj(0);
		break;
	case MXGETVAL:
		/*
		 * Check for 1 arg.  Error if different.
		 */
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "id");
			return (TCL_ERROR);
		}
		result = Tcl_GetIntFromObj(interp, objv[2], &id);
		if (result != TCL_OK)
			break;
		res = Tcl_NewIntObj(mp->marray[id].val);
		break;
	case MXSETVAL:
		/*
		 * Check for 2 args.  Error if different.
		 */
		if (objc != 4) {
			Tcl_WrongNumArgs(interp, 2, objv, "id val");
			return (TCL_ERROR);
		}
		result = Tcl_GetIntFromObj(interp, objv[2], &id);
		if (result != TCL_OK)
			break;
		result = Tcl_GetIntFromObj(interp, objv[3], &newval);
		if (result != TCL_OK)
			break;
		mp->marray[id].val = newval;
		res = Tcl_NewIntObj(0);
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
