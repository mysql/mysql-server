/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: tcl_util.c,v 11.43 2004/06/10 17:20:57 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <tcl.h>
#endif

#include "db_int.h"
#include "dbinc/tcl_db.h"

/*
 * Prototypes for procedures defined later in this file:
 */
static int mutex_Cmd __P((ClientData, Tcl_Interp *, int, Tcl_Obj * CONST*));

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
	static const char *rcmds[] = {
		"rand",	"random_int",	"srand",
		NULL
	};
	enum rcmds {
		RRAND, RRAND_INT, RSRAND
	};
	Tcl_Obj *res;
	int cmdindex, hi, lo, result, ret;

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
		if ((result =
		    Tcl_GetIntFromObj(interp, objv[2], &lo)) != TCL_OK)
			return (result);
		if ((result =
		    Tcl_GetIntFromObj(interp, objv[3], &hi)) != TCL_OK)
			return (result);
		if (lo < 0 || hi < 0) {
			Tcl_SetResult(interp,
			    "Range value less than 0", TCL_STATIC);
			return (TCL_ERROR);
		}

		_debug_check();
		ret = lo + rand() % ((hi - lo) + 1);
		res = Tcl_NewIntObj(ret);
		break;
	case RSRAND:
		/*
		 * Must be 1 arg.  Error if different.
		 */
		if (objc != 3) {
			Tcl_WrongNumArgs(interp, 2, objv, "seed");
			return (TCL_ERROR);
		}
		if ((result =
		    Tcl_GetIntFromObj(interp, objv[2], &lo)) == TCL_OK) {
			srand((u_int)lo);
			res = Tcl_NewIntObj(0);
		}
		break;
	}

	/*
	 * Only set result if we have a res.  Otherwise, lower functions have
	 * already done so.
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
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *envp;			/* Environment pointer */
	DBTCL_INFO *envip;		/* Info pointer */
{
	DBTCL_INFO *ip;
	Tcl_Obj *res;
	_MUTEX_DATA *md;
	int i, nitems, mode, result, ret;
	char newname[MSG_SIZE];

	md = NULL;
	result = TCL_OK;
	ret = 0;

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

	memset(newname, 0, MSG_SIZE);
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
	md->size = sizeof(_MUTEX_ENTRY) * (u_int)nitems;

	md->reginfo.dbenv = envp;
	md->reginfo.type = REGION_TYPE_MUTEX;
	md->reginfo.id = INVALID_REGION_ID;
	md->reginfo.flags = REGION_CREATE_OK | REGION_JOIN_OK;
	if ((ret = __db_r_attach(envp, &md->reginfo, md->size)) != 0)
		goto posixout;
	md->marray = md->reginfo.addr;

	/* Initialize a created region. */
	if (F_ISSET(&md->reginfo, REGION_CREATE))
		for (i = 0; i < nitems; i++) {
			md->marray[i].val = 0;
			if ((ret = __db_mutex_init_int(envp,
			    &md->marray[i].m, i, 0)) != 0)
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
	(void)Tcl_CreateObjCommand(interp, newname,
	    (Tcl_ObjCmdProc *)mutex_Cmd, (ClientData)md, NULL);
	res = NewStringObj(newname, strlen(newname));
	Tcl_SetObjResult(interp, res);

	return (TCL_OK);

posixout:
	if (ret > 0)
		(void)Tcl_PosixError(interp);
	result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret), "mutex");
	_DeleteInfo(ip);

	if (md != NULL) {
		if (md->reginfo.addr != NULL)
			(void)__db_r_detach(md->env, &md->reginfo, 0);
		__os_free(md->env, md);
	}
	return (result);
}

/*
 * mutex_Cmd --
 *	Implements the "mutex" widget.
 */
static int
mutex_Cmd(clientData, interp, objc, objv)
	ClientData clientData;		/* Mutex handle */
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
{
	static const char *mxcmds[] = {
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
		__os_free(mp->env, mp);
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
		MUTEX_LOCK(dbenv, &mp->marray[id].m);
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
		res = Tcl_NewLongObj((long)mp->marray[id].val);
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
