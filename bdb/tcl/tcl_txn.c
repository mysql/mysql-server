/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: tcl_txn.c,v 11.24 2000/12/31 19:26:23 bostic Exp $";
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
static int      tcl_TxnCommit __P((Tcl_Interp *, int, Tcl_Obj * CONST*,
    DB_TXN *, DBTCL_INFO *));

/*
 * _TxnInfoDelete --
 *	Removes nested txn info structures that are children
 *	of this txn.
 *	RECURSIVE:  Transactions can be arbitrarily nested, so we
 *	must recurse down until we get them all.
 *
 * PUBLIC: void _TxnInfoDelete __P((Tcl_Interp *, DBTCL_INFO *));
 */
void
_TxnInfoDelete(interp, txnip)
	Tcl_Interp *interp;             /* Interpreter */
	DBTCL_INFO *txnip;		/* Info for txn */
{
	DBTCL_INFO *nextp, *p;

	for (p = LIST_FIRST(&__db_infohead); p != NULL; p = nextp) {
		/*
		 * Check if this info structure "belongs" to this
		 * txn.  Remove its commands and info structure.
		 */
		nextp = LIST_NEXT(p, entries);
		 if (p->i_parent == txnip && p->i_type == I_TXN) {
			_TxnInfoDelete(interp, p);
			(void)Tcl_DeleteCommand(interp, p->i_name);
			_DeleteInfo(p);
		}
	}
}

/*
 * tcl_TxnCheckpoint --
 *
 * PUBLIC: int tcl_TxnCheckpoint __P((Tcl_Interp *, int,
 * PUBLIC:    Tcl_Obj * CONST*, DB_ENV *));
 */
int
tcl_TxnCheckpoint(interp, objc, objv, envp)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *envp;			/* Environment pointer */
{
	static char *txnckpopts[] = {
		"-kbyte",	"-min",
		NULL
	};
	enum txnckpopts {
		TXNCKP_KB,	TXNCKP_MIN
	};
	int i, kb, min, optindex, result, ret;

	result = TCL_OK;
	kb = min = 0;

	/*
	 * Get the flag index from the object based on the options
	 * defined above.
	 */
	i = 2;
	while (i < objc) {
		if (Tcl_GetIndexFromObj(interp, objv[i],
		    txnckpopts, "option", TCL_EXACT, &optindex) != TCL_OK) {
			return (IS_HELP(objv[i]));
		}
		i++;
		switch ((enum txnckpopts)optindex) {
		case TXNCKP_KB:
			if (i == objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-kbyte kb?");
				result = TCL_ERROR;
				break;
			}
			result = Tcl_GetIntFromObj(interp, objv[i++], &kb);
			break;
		case TXNCKP_MIN:
			if (i == objc) {
				Tcl_WrongNumArgs(interp, 2, objv, "?-min min?");
				result = TCL_ERROR;
				break;
			}
			result = Tcl_GetIntFromObj(interp, objv[i++], &min);
			break;
		}
	}
	_debug_check();
	ret = txn_checkpoint(envp, (u_int32_t)kb, (u_int32_t)min, 0);
	result = _ReturnSetup(interp, ret, "txn checkpoint");
	return (result);
}

/*
 * tcl_Txn --
 *
 * PUBLIC: int tcl_Txn __P((Tcl_Interp *, int,
 * PUBLIC:    Tcl_Obj * CONST*, DB_ENV *, DBTCL_INFO *));
 */
int
tcl_Txn(interp, objc, objv, envp, envip)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *envp;			/* Environment pointer */
	DBTCL_INFO *envip;		/* Info pointer */
{
	static char *txnopts[] = {
		"-nosync",
		"-nowait",
		"-parent",
		"-sync",
		NULL
	};
	enum txnopts {
		TXN_NOSYNC,
		TXN_NOWAIT,
		TXN_PARENT,
		TXN_SYNC
	};
	DBTCL_INFO *ip;
	DB_TXN *parent;
	DB_TXN *txn;
	Tcl_Obj *res;
	u_int32_t flag;
	int i, optindex, result, ret;
	char *arg, msg[MSG_SIZE], newname[MSG_SIZE];

	result = TCL_OK;
	memset(newname, 0, MSG_SIZE);

	parent = NULL;
	flag = 0;
	i = 2;
	while (i < objc) {
		if (Tcl_GetIndexFromObj(interp, objv[i],
		    txnopts, "option", TCL_EXACT, &optindex) != TCL_OK) {
			return (IS_HELP(objv[i]));
		}
		i++;
		switch ((enum txnopts)optindex) {
		case TXN_PARENT:
			if (i == objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-parent txn?");
				result = TCL_ERROR;
				break;
			}
			arg = Tcl_GetStringFromObj(objv[i++], NULL);
			parent = NAME_TO_TXN(arg);
			if (parent == NULL) {
				snprintf(msg, MSG_SIZE,
				    "Invalid parent txn: %s\n",
				    arg);
				Tcl_SetResult(interp, msg, TCL_VOLATILE);
				return (TCL_ERROR);
			}
			break;
		case TXN_NOWAIT:
			FLAG_CHECK(flag);
			flag |= DB_TXN_NOWAIT;
			break;
		case TXN_SYNC:
			FLAG_CHECK(flag);
			flag |= DB_TXN_SYNC;
			break;
		case TXN_NOSYNC:
			FLAG_CHECK(flag);
			flag |= DB_TXN_NOSYNC;
			break;
		}
	}
	snprintf(newname, sizeof(newname), "%s.txn%d",
	    envip->i_name, envip->i_envtxnid);
	ip = _NewInfo(interp, NULL, newname, I_TXN);
	if (ip == NULL) {
		Tcl_SetResult(interp, "Could not set up info",
		    TCL_STATIC);
		return (TCL_ERROR);
	}
	_debug_check();
	ret = txn_begin(envp, parent, &txn, flag);
	result = _ReturnSetup(interp, ret, "txn");
	if (result == TCL_ERROR)
		_DeleteInfo(ip);
	else {
		/*
		 * Success.  Set up return.  Set up new info
		 * and command widget for this txn.
		 */
		envip->i_envtxnid++;
		if (parent)
			ip->i_parent = _PtrToInfo(parent);
		else
			ip->i_parent = envip;
		_SetInfoData(ip, txn);
		Tcl_CreateObjCommand(interp, newname,
		    (Tcl_ObjCmdProc *)txn_Cmd, (ClientData)txn, NULL);
		res = Tcl_NewStringObj(newname, strlen(newname));
		Tcl_SetObjResult(interp, res);
	}
	return (result);
}

/*
 * tcl_TxnStat --
 *
 * PUBLIC: int tcl_TxnStat __P((Tcl_Interp *, int,
 * PUBLIC:    Tcl_Obj * CONST*, DB_ENV *));
 */
int
tcl_TxnStat(interp, objc, objv, envp)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *envp;			/* Environment pointer */
{
#define	MAKE_STAT_LSN(s, lsn)						\
do {									\
	myobjc = 2;							\
	myobjv[0] = Tcl_NewIntObj((lsn)->file);				\
	myobjv[1] = Tcl_NewIntObj((lsn)->offset);			\
	lsnlist = Tcl_NewListObj(myobjc, myobjv);			\
	myobjc = 2;							\
	myobjv[0] = Tcl_NewStringObj((s), strlen(s));			\
	myobjv[1] = lsnlist;						\
	thislist = Tcl_NewListObj(myobjc, myobjv);			\
	result = Tcl_ListObjAppendElement(interp, res, thislist);	\
	if (result != TCL_OK)						\
		goto error;						\
} while (0);

	DBTCL_INFO *ip;
	DB_TXN_ACTIVE *p;
	DB_TXN_STAT *sp;
	Tcl_Obj *myobjv[2], *res, *thislist, *lsnlist;
	u_int32_t i;
	int myobjc, result, ret;

	result = TCL_OK;
	/*
	 * No args for this.  Error if there are some.
	 */
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 2, objv, NULL);
		return (TCL_ERROR);
	}
	_debug_check();
	ret = txn_stat(envp, &sp, NULL);
	result = _ReturnSetup(interp, ret, "txn stat");
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
	MAKE_STAT_LSN("LSN of last checkpoint", &sp->st_last_ckp);
	MAKE_STAT_LSN("LSN of pending checkpoint", &sp->st_pending_ckp);
	MAKE_STAT_LIST("Time of last checkpoint", sp->st_time_ckp);
	MAKE_STAT_LIST("Last txn ID allocated", sp->st_last_txnid);
	MAKE_STAT_LIST("Max Txns", sp->st_maxtxns);
	MAKE_STAT_LIST("Number aborted txns", sp->st_naborts);
	MAKE_STAT_LIST("Number active txns", sp->st_nactive);
	MAKE_STAT_LIST("Number txns begun", sp->st_nbegins);
	MAKE_STAT_LIST("Number committed txns", sp->st_ncommits);
	MAKE_STAT_LIST("Number of region lock waits", sp->st_region_wait);
	MAKE_STAT_LIST("Number of region lock nowaits", sp->st_region_nowait);
	for (i = 0, p = sp->st_txnarray; i < sp->st_nactive; i++, p++)
		for (ip = LIST_FIRST(&__db_infohead); ip != NULL;
		    ip = LIST_NEXT(ip, entries)) {
			if (ip->i_type != I_TXN)
				continue;
			if (ip->i_type == I_TXN &&
			    (txn_id(ip->i_txnp) == p->txnid)) {
				MAKE_STAT_LSN(ip->i_name, &p->lsn);
				if (p->parentid != 0)
					MAKE_STAT_STRLIST("Parent",
					    ip->i_parent->i_name);
				else
					MAKE_STAT_LIST("Parent", 0);
				break;
			}
		}
	Tcl_SetObjResult(interp, res);
error:
	__os_free(sp, sizeof(*sp));
	return (result);
}

/*
 * txn_Cmd --
 *	Implements the "txn" widget.
 *
 * PUBLIC: int txn_Cmd __P((ClientData, Tcl_Interp *, int, Tcl_Obj * CONST*));
 */
int
txn_Cmd(clientData, interp, objc, objv)
	ClientData clientData;          /* Txn handle */
	Tcl_Interp *interp;             /* Interpreter */
	int objc;                       /* How many arguments? */
	Tcl_Obj *CONST objv[];          /* The argument objects */
{
	static char *txncmds[] = {
		"abort",
		"commit",
		"id",
		"prepare",
		NULL
	};
	enum txncmds {
		TXNABORT,
		TXNCOMMIT,
		TXNID,
		TXNPREPARE
	};
	DBTCL_INFO *txnip;
	DB_TXN *txnp;
	Tcl_Obj *res;
	int cmdindex, result, ret;

	Tcl_ResetResult(interp);
	txnp = (DB_TXN *)clientData;
	txnip = _PtrToInfo((void *)txnp);
	result = TCL_OK;
	if (txnp == NULL) {
		Tcl_SetResult(interp, "NULL txn pointer", TCL_STATIC);
		return (TCL_ERROR);
	}
	if (txnip == NULL) {
		Tcl_SetResult(interp, "NULL txn info pointer", TCL_STATIC);
		return (TCL_ERROR);
	}

	/*
	 * Get the command name index from the object based on the dbcmds
	 * defined above.
	 */
	if (Tcl_GetIndexFromObj(interp,
	    objv[1], txncmds, "command", TCL_EXACT, &cmdindex) != TCL_OK)
		return (IS_HELP(objv[1]));

	res = NULL;
	switch ((enum txncmds)cmdindex) {
	case TXNID:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, NULL);
			return (TCL_ERROR);
		}
		_debug_check();
		ret = txn_id(txnp);
		res = Tcl_NewIntObj(ret);
		break;
	case TXNPREPARE:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, NULL);
			return (TCL_ERROR);
		}
		_debug_check();
		ret = txn_prepare(txnp);
		result = _ReturnSetup(interp, ret, "txn prepare");
		break;
	case TXNCOMMIT:
		result = tcl_TxnCommit(interp, objc, objv, txnp, txnip);
		_TxnInfoDelete(interp, txnip);
		(void)Tcl_DeleteCommand(interp, txnip->i_name);
		_DeleteInfo(txnip);
		break;
	case TXNABORT:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, NULL);
			return (TCL_ERROR);
		}
		_debug_check();
		ret = txn_abort(txnp);
		result = _ReturnSetup(interp, ret, "txn abort");
		_TxnInfoDelete(interp, txnip);
		(void)Tcl_DeleteCommand(interp, txnip->i_name);
		_DeleteInfo(txnip);
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

static int
tcl_TxnCommit(interp, objc, objv, txnp, txnip)
	Tcl_Interp *interp;             /* Interpreter */
	int objc;                       /* How many arguments? */
	Tcl_Obj *CONST objv[];          /* The argument objects */
	DB_TXN *txnp;			/* Transaction pointer */
	DBTCL_INFO *txnip;		/* Info pointer */
{
	static char *commitopt[] = {
		"-nosync",
		"-sync",
		NULL
	};
	enum commitopt {
		COMSYNC,
		COMNOSYNC
	};
	u_int32_t flag;
	int optindex, result, ret;

	COMPQUIET(txnip, NULL);

	result = TCL_OK;
	flag = 0;
	if (objc != 2 && objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, NULL);
		return (TCL_ERROR);
	}
	if (objc == 3) {
		if (Tcl_GetIndexFromObj(interp, objv[2], commitopt,
		    "option", TCL_EXACT, &optindex) != TCL_OK)
			return (IS_HELP(objv[2]));
		switch ((enum commitopt)optindex) {
		case COMSYNC:
			FLAG_CHECK(flag);
			flag = DB_TXN_SYNC;
			break;
		case COMNOSYNC:
			FLAG_CHECK(flag);
			flag = DB_TXN_NOSYNC;
			break;
		}
	}

	_debug_check();
	ret = txn_commit(txnp, flag);
	result = _ReturnSetup(interp, ret, "txn commit");
	return (result);
}
