/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: tcl_lock.c,v 11.21 2001/01/11 18:19:55 bostic Exp $";
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
 * Prototypes for procedures defined later in this file:
 */
static int      lock_Cmd __P((ClientData, Tcl_Interp *, int, Tcl_Obj * CONST*));
static int	_LockMode __P((Tcl_Interp *, Tcl_Obj *, db_lockmode_t *));
static int	_GetThisLock __P((Tcl_Interp *, DB_ENV *, u_int32_t,
				     u_int32_t, DBT *, db_lockmode_t, char *));
static void	_LockPutInfo __P((Tcl_Interp *, db_lockop_t, DB_LOCK *,
				     u_int32_t, DBT *));

static char *lkmode[] = {
	"ng",		"read",		"write",
	"iwrite",	"iread",	"iwr",
	 NULL
};
enum lkmode {
	LK_NG,		LK_READ,	LK_WRITE,
	LK_IWRITE,	LK_IREAD,	LK_IWR
};

/*
 * tcl_LockDetect --
 *
 * PUBLIC: int tcl_LockDetect __P((Tcl_Interp *, int,
 * PUBLIC:    Tcl_Obj * CONST*, DB_ENV *));
 */
int
tcl_LockDetect(interp, objc, objv, envp)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *envp;			/* Environment pointer */
{
	static char *ldopts[] = {
		"-lock_conflict",
		"default",
		"oldest",
		"random",
		"youngest",
		 NULL
	};
	enum ldopts {
		LD_CONFLICT,
		LD_DEFAULT,
		LD_OLDEST,
		LD_RANDOM,
		LD_YOUNGEST
	};
	u_int32_t flag, policy;
	int i, optindex, result, ret;

	result = TCL_OK;
	flag = policy = 0;
	i = 2;
	while (i < objc) {
		if (Tcl_GetIndexFromObj(interp, objv[i],
		    ldopts, "option", TCL_EXACT, &optindex) != TCL_OK)
			return (IS_HELP(objv[i]));
		i++;
		switch ((enum ldopts)optindex) {
		case LD_DEFAULT:
			FLAG_CHECK(policy);
			policy = DB_LOCK_DEFAULT;
			break;
		case LD_OLDEST:
			FLAG_CHECK(policy);
			policy = DB_LOCK_OLDEST;
			break;
		case LD_YOUNGEST:
			FLAG_CHECK(policy);
			policy = DB_LOCK_YOUNGEST;
			break;
		case LD_RANDOM:
			FLAG_CHECK(policy);
			policy = DB_LOCK_RANDOM;
			break;
		case LD_CONFLICT:
			flag |= DB_LOCK_CONFLICT;
			break;
		}
	}

	_debug_check();
	ret = lock_detect(envp, flag, policy, NULL);
	result = _ReturnSetup(interp, ret, "lock detect");
	return (result);
}

/*
 * tcl_LockGet --
 *
 * PUBLIC: int tcl_LockGet __P((Tcl_Interp *, int,
 * PUBLIC:    Tcl_Obj * CONST*, DB_ENV *));
 */
int
tcl_LockGet(interp, objc, objv, envp)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *envp;			/* Environment pointer */
{
	static char *lgopts[] = {
		"-nowait",
		 NULL
	};
	enum lgopts {
		LGNOWAIT
	};
	DBT obj;
	Tcl_Obj *res;
	db_lockmode_t mode;
	u_int32_t flag, lockid;
	int itmp, optindex, result;
	char newname[MSG_SIZE];

	result = TCL_OK;
	memset(newname, 0, MSG_SIZE);
	if (objc != 5 && objc != 6) {
		Tcl_WrongNumArgs(interp, 2, objv, "?-nowait? mode id obj");
		return (TCL_ERROR);
	}
	/*
	 * Work back from required args.
	 * Last arg is obj.
	 * Second last is lock id.
	 * Third last is lock mode.
	 */
	memset(&obj, 0, sizeof(obj));

	if ((result =
	    Tcl_GetIntFromObj(interp, objv[objc-2], &itmp)) != TCL_OK)
		return (result);
	lockid = itmp;

	/*
	 * XXX
	 * Tcl 8.1 Tcl_GetByteArrayFromObj/Tcl_GetIntFromObj bug.
	 *
	 * The line below was originally before the Tcl_GetIntFromObj.
	 *
	 * There is a bug in Tcl 8.1 and byte arrays in that if it happens
	 * to use an object as both a byte array and something else like
	 * an int, and you've done a Tcl_GetByteArrayFromObj, then you
	 * do a Tcl_GetIntFromObj, your memory is deleted.
	 *
	 * Workaround is to make sure all Tcl_GetByteArrayFromObj calls
	 * are done last.
	 */
	obj.data = Tcl_GetByteArrayFromObj(objv[objc-1], &itmp);
	obj.size = itmp;
	if ((result = _LockMode(interp, objv[(objc - 3)], &mode)) != TCL_OK)
		return (result);

	/*
	 * Any left over arg is the flag.
	 */
	flag = 0;
	if (objc == 6) {
		if (Tcl_GetIndexFromObj(interp, objv[(objc - 4)],
		    lgopts, "option", TCL_EXACT, &optindex) != TCL_OK)
			return (IS_HELP(objv[(objc - 4)]));
		switch ((enum lgopts)optindex) {
		case LGNOWAIT:
			flag |= DB_LOCK_NOWAIT;
			break;
		}
	}

	result = _GetThisLock(interp, envp, lockid, flag, &obj, mode, newname);
	if (result == TCL_OK) {
		res = Tcl_NewStringObj(newname, strlen(newname));
		Tcl_SetObjResult(interp, res);
	}
	return (result);
}

/*
 * tcl_LockStat --
 *
 * PUBLIC: int tcl_LockStat __P((Tcl_Interp *, int,
 * PUBLIC:    Tcl_Obj * CONST*, DB_ENV *));
 */
int
tcl_LockStat(interp, objc, objv, envp)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *envp;			/* Environment pointer */
{
	DB_LOCK_STAT *sp;
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
	ret = lock_stat(envp, &sp, NULL);
	result = _ReturnSetup(interp, ret, "lock stat");
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
	MAKE_STAT_LIST("Region size", sp->st_regsize);
	MAKE_STAT_LIST("Max locks", sp->st_maxlocks);
	MAKE_STAT_LIST("Max lockers", sp->st_maxlockers);
	MAKE_STAT_LIST("Max objects", sp->st_maxobjects);
	MAKE_STAT_LIST("Lock modes", sp->st_nmodes);
	MAKE_STAT_LIST("Current number of locks", sp->st_nlocks);
	MAKE_STAT_LIST("Maximum number of locks so far", sp->st_maxnlocks);
	MAKE_STAT_LIST("Current number of lockers", sp->st_nlockers);
	MAKE_STAT_LIST("Maximum number of lockers so far", sp->st_maxnlockers);
	MAKE_STAT_LIST("Current number of objects", sp->st_nobjects);
	MAKE_STAT_LIST("Maximum number of objects so far", sp->st_maxnobjects);
	MAKE_STAT_LIST("Number of conflicts", sp->st_nconflicts);
	MAKE_STAT_LIST("Lock requests", sp->st_nrequests);
	MAKE_STAT_LIST("Lock releases", sp->st_nreleases);
	MAKE_STAT_LIST("Deadlocks detected", sp->st_ndeadlocks);
	MAKE_STAT_LIST("Number of region lock waits", sp->st_region_wait);
	MAKE_STAT_LIST("Number of region lock nowaits", sp->st_region_nowait);
	Tcl_SetObjResult(interp, res);
error:
	__os_free(sp, sizeof(*sp));
	return (result);
}

/*
 * lock_Cmd --
 *	Implements the "lock" widget.
 */
static int
lock_Cmd(clientData, interp, objc, objv)
	ClientData clientData;          /* Lock handle */
	Tcl_Interp *interp;             /* Interpreter */
	int objc;                       /* How many arguments? */
	Tcl_Obj *CONST objv[];          /* The argument objects */
{
	static char *lkcmds[] = {
		"put",
		NULL
	};
	enum lkcmds {
		LKPUT
	};
	DB_ENV *env;
	DB_LOCK *lock;
	DBTCL_INFO *lkip;
	int cmdindex, result, ret;

	Tcl_ResetResult(interp);
	lock = (DB_LOCK *)clientData;
	lkip = _PtrToInfo((void *)lock);
	result = TCL_OK;

	if (lock == NULL) {
		Tcl_SetResult(interp, "NULL lock", TCL_STATIC);
		return (TCL_ERROR);
	}
	if (lkip == NULL) {
		Tcl_SetResult(interp, "NULL lock info pointer", TCL_STATIC);
		return (TCL_ERROR);
	}

	env = NAME_TO_ENV(lkip->i_parent->i_name);
	/*
	 * No args for this.  Error if there are some.
	 */
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 2, objv, NULL);
		return (TCL_ERROR);
	}
	/*
	 * Get the command name index from the object based on the dbcmds
	 * defined above.
	 */
	if (Tcl_GetIndexFromObj(interp,
	    objv[1], lkcmds, "command", TCL_EXACT, &cmdindex) != TCL_OK)
		return (IS_HELP(objv[1]));

	switch ((enum lkcmds)cmdindex) {
	case LKPUT:
		_debug_check();
		ret = lock_put(env, lock);
		result = _ReturnSetup(interp, ret, "lock put");
		(void)Tcl_DeleteCommand(interp, lkip->i_name);
		_DeleteInfo(lkip);
		__os_free(lock, sizeof(DB_LOCK));
		break;
	}
	return (result);
}

/*
 * tcl_LockVec --
 *
 * PUBLIC: int tcl_LockVec __P((Tcl_Interp *, int, Tcl_Obj * CONST*, DB_ENV *));
 */
int
tcl_LockVec(interp, objc, objv, envp)
	Tcl_Interp *interp;             /* Interpreter */
	int objc;                       /* How many arguments? */
	Tcl_Obj *CONST objv[];          /* The argument objects */
	DB_ENV *envp;			/* environment pointer */
{
	static char *lvopts[] = {
		"-nowait",
		 NULL
	};
	enum lvopts {
		LVNOWAIT
	};
	static char *lkops[] = {
		"get",	"put",	"put_all",	"put_obj",
		 NULL
	};
	enum lkops {
		LKGET,	LKPUT,	LKPUTALL,	LKPUTOBJ
	};
	DB_LOCK *lock;
	DB_LOCKREQ list;
	DBT obj;
	Tcl_Obj **myobjv, *res, *thisop;
	db_lockmode_t mode;
	u_int32_t flag, lockid;
	int i, itmp, myobjc, optindex, result, ret;
	char *lockname, msg[MSG_SIZE], newname[MSG_SIZE];

	result = TCL_OK;
	memset(newname, 0, MSG_SIZE);
	flag = 0;
	mode = 0;
	/*
	 * If -nowait is given, it MUST be first arg.
	 */
	if (Tcl_GetIndexFromObj(interp, objv[2],
	    lvopts, "option", TCL_EXACT, &optindex) == TCL_OK) {
		switch ((enum lvopts)optindex) {
		case LVNOWAIT:
			flag |= DB_LOCK_NOWAIT;
			break;
		}
		i = 3;
	} else {
		if (IS_HELP(objv[2]) == TCL_OK)
			return (TCL_OK);
		Tcl_ResetResult(interp);
		i = 2;
	}

	/*
	 * Our next arg MUST be the locker ID.
	 */
	result = Tcl_GetIntFromObj(interp, objv[i++], &itmp);
	if (result != TCL_OK)
		return (result);
	lockid = itmp;

	/*
	 * All other remaining args are operation tuples.
	 * Go through sequentially to decode, execute and build
	 * up list of return values.
	 */
	res = Tcl_NewListObj(0, NULL);
	while (i < objc) {
		/*
		 * Get the list of the tuple.
		 */
		lock = NULL;
		result = Tcl_ListObjGetElements(interp, objv[i],
		    &myobjc, &myobjv);
		if (result == TCL_OK)
			i++;
		else
			break;
		/*
		 * First we will set up the list of requests.
		 * We will make a "second pass" after we get back
		 * the results from the lock_vec call to create
		 * the return list.
		 */
		if (Tcl_GetIndexFromObj(interp, myobjv[0],
		    lkops, "option", TCL_EXACT, &optindex) != TCL_OK) {
			result = IS_HELP(myobjv[0]);
			goto error;
		}
		switch ((enum lkops)optindex) {
		case LKGET:
			if (myobjc != 3) {
				Tcl_WrongNumArgs(interp, 1, myobjv,
				    "{get obj mode}");
				result = TCL_ERROR;
				goto error;
			}
			result = _LockMode(interp, myobjv[2], &list.mode);
			if (result != TCL_OK)
				goto error;
			/*
			 * XXX
			 * Tcl 8.1 Tcl_GetByteArrayFromObj/Tcl_GetIntFromObj
			 * bug.
			 *
			 * There is a bug in Tcl 8.1 and byte arrays in that if
			 * it happens to use an object as both a byte array and
			 * something else like an int, and you've done a
			 * Tcl_GetByteArrayFromObj, then you do a
			 * Tcl_GetIntFromObj, your memory is deleted.
			 *
			 * Workaround is to make sure all
			 * Tcl_GetByteArrayFromObj calls are done last.
			 */
			obj.data = Tcl_GetByteArrayFromObj(myobjv[1], &itmp);
			obj.size = itmp;
			ret = _GetThisLock(interp, envp, lockid, flag,
			    &obj, list.mode, newname);
			if (ret != 0) {
				result = _ReturnSetup(interp, ret, "lock vec");
				thisop = Tcl_NewIntObj(ret);
				(void)Tcl_ListObjAppendElement(interp, res,
				    thisop);
				goto error;
			}
			thisop = Tcl_NewStringObj(newname, strlen(newname));
			(void)Tcl_ListObjAppendElement(interp, res, thisop);
			continue;
		case LKPUT:
			if (myobjc != 2) {
				Tcl_WrongNumArgs(interp, 1, myobjv,
				    "{put lock}");
				result = TCL_ERROR;
				goto error;
			}
			list.op = DB_LOCK_PUT;
			lockname = Tcl_GetStringFromObj(myobjv[1], NULL);
			lock = NAME_TO_LOCK(lockname);
			if (lock == NULL) {
				snprintf(msg, MSG_SIZE, "Invalid lock: %s\n",
				    lockname);
				Tcl_SetResult(interp, msg, TCL_VOLATILE);
				result = TCL_ERROR;
				goto error;
			}
			list.lock = *lock;
			break;
		case LKPUTALL:
			if (myobjc != 1) {
				Tcl_WrongNumArgs(interp, 1, myobjv,
				    "{put_all}");
				result = TCL_ERROR;
				goto error;
			}
			list.op = DB_LOCK_PUT_ALL;
			break;
		case LKPUTOBJ:
			if (myobjc != 2) {
				Tcl_WrongNumArgs(interp, 1, myobjv,
				    "{put_obj obj}");
				result = TCL_ERROR;
				goto error;
			}
			list.op = DB_LOCK_PUT_OBJ;
			obj.data = Tcl_GetByteArrayFromObj(myobjv[1], &itmp);
			obj.size = itmp;
			list.obj = &obj;
			break;
		}
		/*
		 * We get here, we have set up our request, now call
		 * lock_vec.
		 */
		_debug_check();
		ret = lock_vec(envp, lockid, flag, &list, 1, NULL);
		/*
		 * Now deal with whether or not the operation succeeded.
		 * Get's were done above, all these are only puts.
		 */
		thisop = Tcl_NewIntObj(ret);
		result = Tcl_ListObjAppendElement(interp, res, thisop);
		if (ret != 0 && result == TCL_OK)
			result = _ReturnSetup(interp, ret, "lock put");
		/*
		 * We did a put of some kind.  Since we did that,
		 * we have to delete the commands associated with
		 * any of the locks we just put.
		 */
		_LockPutInfo(interp, list.op, lock, lockid, &obj);
	}

	if (result == TCL_OK && res)
		Tcl_SetObjResult(interp, res);
error:
	return (result);
}

static int
_LockMode(interp, obj, mode)
	Tcl_Interp *interp;
	Tcl_Obj *obj;
	db_lockmode_t *mode;
{
	int optindex;

	if (Tcl_GetIndexFromObj(interp, obj, lkmode, "option",
	    TCL_EXACT, &optindex) != TCL_OK)
		return (IS_HELP(obj));
	switch ((enum lkmode)optindex) {
	case LK_NG:
		*mode = DB_LOCK_NG;
		break;
	case LK_READ:
		*mode = DB_LOCK_READ;
		break;
	case LK_WRITE:
		*mode = DB_LOCK_WRITE;
		break;
	case LK_IREAD:
		*mode = DB_LOCK_IREAD;
		break;
	case LK_IWRITE:
		*mode = DB_LOCK_IWRITE;
		break;
	case LK_IWR:
		*mode = DB_LOCK_IWR;
		break;
	}
	return (TCL_OK);
}

static void
_LockPutInfo(interp, op, lock, lockid, objp)
	Tcl_Interp *interp;
	db_lockop_t op;
	DB_LOCK *lock;
	u_int32_t lockid;
	DBT *objp;
{
	DBTCL_INFO *p, *nextp;
	int found;

	for (p = LIST_FIRST(&__db_infohead); p != NULL; p = nextp) {
		found = 0;
		nextp = LIST_NEXT(p, entries);
		if ((op == DB_LOCK_PUT && (p->i_lock == lock)) ||
		    (op == DB_LOCK_PUT_ALL && p->i_locker == lockid) ||
		    (op == DB_LOCK_PUT_OBJ && p->i_lockobj.data &&
			memcmp(p->i_lockobj.data, objp->data, objp->size) == 0))
			found = 1;
		if (found) {
			(void)Tcl_DeleteCommand(interp, p->i_name);
			__os_free(p->i_lock, sizeof(DB_LOCK));
			_DeleteInfo(p);
		}
	}
}

static int
_GetThisLock(interp, envp, lockid, flag, objp, mode, newname)
	Tcl_Interp *interp;		/* Interpreter */
	DB_ENV *envp;			/* Env handle */
	u_int32_t lockid;		/* Locker ID */
	u_int32_t flag;			/* Lock flag */
	DBT *objp;			/* Object to lock */
	db_lockmode_t mode;		/* Lock mode */
	char *newname;			/* New command name */
{
	DB_LOCK *lock;
	DBTCL_INFO *envip, *ip;
	int result, ret;

	result = TCL_OK;
	envip = _PtrToInfo((void *)envp);
	if (envip == NULL) {
		Tcl_SetResult(interp, "Could not find env info\n", TCL_STATIC);
		return (TCL_ERROR);
	}
	snprintf(newname, MSG_SIZE, "%s.lock%d",
	    envip->i_name, envip->i_envlockid);
	ip = _NewInfo(interp, NULL, newname, I_LOCK);
	if (ip == NULL) {
		Tcl_SetResult(interp, "Could not set up info",
		    TCL_STATIC);
		return (TCL_ERROR);
	}
	ret = __os_malloc(envp, sizeof(DB_LOCK), NULL, &lock);
	if (ret != 0) {
		Tcl_SetResult(interp, db_strerror(ret), TCL_STATIC);
		return (TCL_ERROR);
	}
	_debug_check();
	ret = lock_get(envp, lockid, flag, objp, mode, lock);
	result = _ReturnSetup(interp, ret, "lock get");
	if (result == TCL_ERROR) {
		__os_free(lock, sizeof(DB_LOCK));
		_DeleteInfo(ip);
		return (result);
	}
	/*
	 * Success.  Set up return.  Set up new info
	 * and command widget for this lock.
	 */
	ret = __os_malloc(envp, objp->size, NULL, &ip->i_lockobj.data);
	if (ret != 0) {
		Tcl_SetResult(interp, "Could not duplicate obj",
		    TCL_STATIC);
		(void)lock_put(envp, lock);
		__os_free(lock, sizeof(DB_LOCK));
		_DeleteInfo(ip);
		result = TCL_ERROR;
		goto error;
	}
	memcpy(ip->i_lockobj.data, objp->data, objp->size);
	ip->i_lockobj.size = objp->size;
	envip->i_envlockid++;
	ip->i_parent = envip;
	ip->i_locker = lockid;
	_SetInfoData(ip, lock);
	Tcl_CreateObjCommand(interp, newname,
	    (Tcl_ObjCmdProc *)lock_Cmd, (ClientData)lock, NULL);
error:
	return (result);
}
