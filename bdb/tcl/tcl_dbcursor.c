/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: tcl_dbcursor.c,v 11.26 2001/01/11 18:19:55 bostic Exp $";
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
static int	tcl_DbcDup __P((Tcl_Interp *, int, Tcl_Obj * CONST*, DBC *));
static int	tcl_DbcGet __P((Tcl_Interp *, int, Tcl_Obj * CONST*, DBC *));
static int	tcl_DbcPut __P((Tcl_Interp *, int, Tcl_Obj * CONST*, DBC *));

/*
 * PUBLIC: int dbc_Cmd __P((ClientData, Tcl_Interp *, int, Tcl_Obj * CONST*));
 *
 * dbc_cmd --
 *	Implements the cursor command.
 */
int
dbc_Cmd(clientData, interp, objc, objv)
	ClientData clientData;          /* Cursor handle */
	Tcl_Interp *interp;             /* Interpreter */
	int objc;                       /* How many arguments? */
	Tcl_Obj *CONST objv[];          /* The argument objects */
{
	static char *dbccmds[] = {
		"close",
		"del",
		"dup",
		"get",
		"put",
		NULL
	};
	enum dbccmds {
		DBCCLOSE,
		DBCDELETE,
		DBCDUP,
		DBCGET,
		DBCPUT
	};
	DBC *dbc;
	DBTCL_INFO *dbip;
	int cmdindex, result, ret;

	Tcl_ResetResult(interp);
	dbc = (DBC *)clientData;
	dbip = _PtrToInfo((void *)dbc);
	result = TCL_OK;

	if (objc <= 1) {
		Tcl_WrongNumArgs(interp, 1, objv, "command cmdargs");
		return (TCL_ERROR);
	}
	if (dbc == NULL) {
		Tcl_SetResult(interp, "NULL dbc pointer", TCL_STATIC);
		return (TCL_ERROR);
	}
	if (dbip == NULL) {
		Tcl_SetResult(interp, "NULL dbc info pointer", TCL_STATIC);
		return (TCL_ERROR);
	}

	/*
	 * Get the command name index from the object based on the berkdbcmds
	 * defined above.
	 */
	if (Tcl_GetIndexFromObj(interp, objv[1], dbccmds, "command",
	    TCL_EXACT, &cmdindex) != TCL_OK)
		return (IS_HELP(objv[1]));
	switch ((enum dbccmds)cmdindex) {
	case DBCCLOSE:
		/*
		 * No args for this.  Error if there are some.
		 */
		if (objc > 2) {
			Tcl_WrongNumArgs(interp, 2, objv, NULL);
			return (TCL_ERROR);
		}
		_debug_check();
		ret = dbc->c_close(dbc);
		result = _ReturnSetup(interp, ret, "dbc close");
		if (result == TCL_OK) {
			(void)Tcl_DeleteCommand(interp, dbip->i_name);
			_DeleteInfo(dbip);
		}
		break;
	case DBCDELETE:
		/*
		 * No args for this.  Error if there are some.
		 */
		if (objc > 2) {
			Tcl_WrongNumArgs(interp, 2, objv, NULL);
			return (TCL_ERROR);
		}
		_debug_check();
		ret = dbc->c_del(dbc, 0);
		result = _ReturnSetup(interp, ret, "dbc delete");
		break;
	case DBCDUP:
		result = tcl_DbcDup(interp, objc, objv, dbc);
		break;
	case DBCGET:
		result = tcl_DbcGet(interp, objc, objv, dbc);
		break;
	case DBCPUT:
		result = tcl_DbcPut(interp, objc, objv, dbc);
		break;
	}
	return (result);
}

/*
 * tcl_DbcPut --
 */
static int
tcl_DbcPut(interp, objc, objv, dbc)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DBC *dbc;			/* Cursor pointer */
{
	static char *dbcutopts[] = {
		"-after",	"-before",	"-current",
		"-keyfirst",	"-keylast",	"-nodupdata",
		"-partial",
		NULL
	};
	enum dbcutopts {
		DBCPUT_AFTER,	DBCPUT_BEFORE,	DBCPUT_CURRENT,
		DBCPUT_KEYFIRST,DBCPUT_KEYLAST,	DBCPUT_NODUPDATA,
		DBCPUT_PART
	};
	DB *thisdbp;
	DBT key, data;
	DBTCL_INFO *dbcip, *dbip;
	DBTYPE type;
	Tcl_Obj **elemv, *res;
	db_recno_t recno;
	u_int32_t flag;
	int elemc, i, itmp, optindex, result, ret;

	result = TCL_OK;
	flag = 0;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 2, objv, "?-args? ?key?");
		return (TCL_ERROR);
	}

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	/*
	 * Get the command name index from the object based on the options
	 * defined above.
	 */
	i = 2;
	while (i < (objc - 1)) {
		if (Tcl_GetIndexFromObj(interp, objv[i], dbcutopts, "option",
		    TCL_EXACT, &optindex) != TCL_OK) {
			/*
			 * Reset the result so we don't get
			 * an errant error message if there is another error.
			 */
			if (IS_HELP(objv[i]) == TCL_OK) {
				result = TCL_OK;
				goto out;
			}
			Tcl_ResetResult(interp);
			break;
		}
		i++;
		switch ((enum dbcutopts)optindex) {
		case DBCPUT_AFTER:
			FLAG_CHECK(flag);
			flag = DB_AFTER;
			break;
		case DBCPUT_BEFORE:
			FLAG_CHECK(flag);
			flag = DB_BEFORE;
			break;
		case DBCPUT_CURRENT:
			FLAG_CHECK(flag);
			flag = DB_CURRENT;
			break;
		case DBCPUT_KEYFIRST:
			FLAG_CHECK(flag);
			flag = DB_KEYFIRST;
			break;
		case DBCPUT_KEYLAST:
			FLAG_CHECK(flag);
			flag = DB_KEYLAST;
			break;
		case DBCPUT_NODUPDATA:
			FLAG_CHECK(flag);
			flag = DB_NODUPDATA;
			break;
		case DBCPUT_PART:
			if (i > (objc - 2)) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-partial {offset length}?");
				result = TCL_ERROR;
				break;
			}
			/*
			 * Get sublist as {offset length}
			 */
			result = Tcl_ListObjGetElements(interp, objv[i++],
			    &elemc, &elemv);
			if (elemc != 2) {
				Tcl_SetResult(interp,
				    "List must be {offset length}", TCL_STATIC);
				result = TCL_ERROR;
				break;
			}
			data.flags |= DB_DBT_PARTIAL;
			result = Tcl_GetIntFromObj(interp, elemv[0], &itmp);
			data.doff = itmp;
			if (result != TCL_OK)
				break;
			result = Tcl_GetIntFromObj(interp, elemv[1], &itmp);
			data.dlen = itmp;
			/*
			 * NOTE: We don't check result here because all we'd
			 * do is break anyway, and we are doing that.  If you
			 * add code here, you WILL need to add the check
			 * for result.  (See the check for save.doff, a few
			 * lines above and copy that.)
			 */
		}
		if (result != TCL_OK)
			break;
	}
	if (result != TCL_OK)
		goto out;

	/*
	 * We need to determine if we are a recno database or not.  If we are,
	 * then key.data is a recno, not a string.
	 */
	dbcip = _PtrToInfo(dbc);
	if (dbcip == NULL)
		type = DB_UNKNOWN;
	else {
		dbip = dbcip->i_parent;
		if (dbip == NULL) {
			Tcl_SetResult(interp, "Cursor without parent database",
			    TCL_STATIC);
			result = TCL_ERROR;
			return (result);
		}
		thisdbp = dbip->i_dbp;
		type = thisdbp->get_type(thisdbp);
	}
	/*
	 * When we get here, we better have:
	 * 1 arg if -after, -before or -current
	 * 2 args in all other cases
	 */
	if (flag == DB_AFTER || flag == DB_BEFORE || flag == DB_CURRENT) {
		if (i != (objc - 1)) {
			Tcl_WrongNumArgs(interp, 2, objv,
			    "?-args? data");
			result = TCL_ERROR;
			goto out;
		}
		/*
		 * We want to get the key back, so we need to set
		 * up the location to get it back in.
		 */
		if (type == DB_RECNO || type == DB_QUEUE) {
			recno = 0;
			key.data = &recno;
			key.size = sizeof(db_recno_t);
		}
	} else {
		if (i != (objc - 2)) {
			Tcl_WrongNumArgs(interp, 2, objv,
			    "?-args? key data");
			result = TCL_ERROR;
			goto out;
		}
		if (type == DB_RECNO || type == DB_QUEUE) {
			result = Tcl_GetIntFromObj(interp, objv[objc-2], &itmp);
			recno = itmp;
			if (result == TCL_OK) {
				key.data = &recno;
				key.size = sizeof(db_recno_t);
			} else
				return (result);
		} else {
			key.data = Tcl_GetByteArrayFromObj(objv[objc-2], &itmp);
			key.size = itmp;
		}
	}
	data.data = Tcl_GetByteArrayFromObj(objv[objc-1], &itmp);
	data.size = itmp;
	_debug_check();
	ret = dbc->c_put(dbc, &key, &data, flag);
	result = _ReturnSetup(interp, ret, "dbc put");
	if (ret == 0 && (flag == DB_AFTER || flag == DB_BEFORE)
	    && type == DB_RECNO) {
		res = Tcl_NewIntObj(*(db_recno_t *)key.data);
		Tcl_SetObjResult(interp, res);
	}
out:
	return (result);
}

/*
 * tcl_dbc_get --
 */
static int
tcl_DbcGet(interp, objc, objv, dbc)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DBC *dbc;			/* Cursor pointer */
{
	static char *dbcgetopts[] = {
		"-current",
		"-first",
		"-get_both",
		"-get_recno",
		"-join_item",
		"-last",
		"-next",
		"-nextdup",
		"-nextnodup",
		"-partial",
		"-prev",
		"-prevnodup",
		"-rmw",
		"-set",
		"-set_range",
		"-set_recno",
		NULL
	};
	enum dbcgetopts {
		DBCGET_CURRENT,
		DBCGET_FIRST,
		DBCGET_BOTH,
		DBCGET_RECNO,
		DBCGET_JOIN,
		DBCGET_LAST,
		DBCGET_NEXT,
		DBCGET_NEXTDUP,
		DBCGET_NEXTNODUP,
		DBCGET_PART,
		DBCGET_PREV,
		DBCGET_PREVNODUP,
		DBCGET_RMW,
		DBCGET_SET,
		DBCGET_SETRANGE,
		DBCGET_SETRECNO
	};
	DB *thisdbp;
	DBT key, data;
	DBTCL_INFO *dbcip, *dbip;
	DBTYPE type;
	Tcl_Obj **elemv, *myobj, *retlist;
	db_recno_t recno;
	u_int32_t flag;
	int elemc, i, itmp, optindex, result, ret;

	result = TCL_OK;
	flag = 0;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 2, objv, "?-args? ?key?");
		return (TCL_ERROR);
	}

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	/*
	 * Get the command name index from the object based on the options
	 * defined above.
	 */
	i = 2;
	while (i < objc) {
		if (Tcl_GetIndexFromObj(interp, objv[i], dbcgetopts,
		    "option", TCL_EXACT, &optindex) != TCL_OK) {
			/*
			 * Reset the result so we don't get
			 * an errant error message if there is another error.
			 */
			if (IS_HELP(objv[i]) == TCL_OK) {
				result = TCL_OK;
				goto out;
			}
			Tcl_ResetResult(interp);
			break;
		}
		i++;
		switch ((enum dbcgetopts)optindex) {
		case DBCGET_RMW:
			flag |= DB_RMW;
			break;
		case DBCGET_CURRENT:
			FLAG_CHECK2(flag, DB_RMW);
			flag |= DB_CURRENT;
			break;
		case DBCGET_FIRST:
			FLAG_CHECK2(flag, DB_RMW);
			flag |= DB_FIRST;
			break;
		case DBCGET_LAST:
			FLAG_CHECK2(flag, DB_RMW);
			flag |= DB_LAST;
			break;
		case DBCGET_NEXT:
			FLAG_CHECK2(flag, DB_RMW);
			flag |= DB_NEXT;
			break;
		case DBCGET_PREV:
			FLAG_CHECK2(flag, DB_RMW);
			flag |= DB_PREV;
			break;
		case DBCGET_PREVNODUP:
			FLAG_CHECK2(flag, DB_RMW);
			flag |= DB_PREV_NODUP;
			break;
		case DBCGET_NEXTNODUP:
			FLAG_CHECK2(flag, DB_RMW);
			flag |= DB_NEXT_NODUP;
			break;
		case DBCGET_NEXTDUP:
			FLAG_CHECK2(flag, DB_RMW);
			flag |= DB_NEXT_DUP;
			break;
		case DBCGET_BOTH:
			FLAG_CHECK2(flag, DB_RMW);
			flag |= DB_GET_BOTH;
			break;
		case DBCGET_RECNO:
			FLAG_CHECK2(flag, DB_RMW);
			flag |= DB_GET_RECNO;
			break;
		case DBCGET_JOIN:
			FLAG_CHECK2(flag, DB_RMW);
			flag |= DB_JOIN_ITEM;
			break;
		case DBCGET_SET:
			FLAG_CHECK2(flag, DB_RMW);
			flag |= DB_SET;
			break;
		case DBCGET_SETRANGE:
			FLAG_CHECK2(flag, DB_RMW);
			flag |= DB_SET_RANGE;
			break;
		case DBCGET_SETRECNO:
			FLAG_CHECK2(flag, DB_RMW);
			flag |= DB_SET_RECNO;
			break;
		case DBCGET_PART:
			if (i == objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-partial {offset length}?");
				result = TCL_ERROR;
				break;
			}
			/*
			 * Get sublist as {offset length}
			 */
			result = Tcl_ListObjGetElements(interp, objv[i++],
			    &elemc, &elemv);
			if (elemc != 2) {
				Tcl_SetResult(interp,
				    "List must be {offset length}", TCL_STATIC);
				result = TCL_ERROR;
				break;
			}
			data.flags |= DB_DBT_PARTIAL;
			result = Tcl_GetIntFromObj(interp, elemv[0], &itmp);
			data.doff = itmp;
			if (result != TCL_OK)
				break;
			result = Tcl_GetIntFromObj(interp, elemv[1], &itmp);
			data.dlen = itmp;
			/*
			 * NOTE: We don't check result here because all we'd
			 * do is break anyway, and we are doing that.  If you
			 * add code here, you WILL need to add the check
			 * for result.  (See the check for save.doff, a few
			 * lines above and copy that.)
			 */
			break;
		}
		if (result != TCL_OK)
			break;
	}
	if (result != TCL_OK)
		goto out;

	/*
	 * We need to determine if we are a recno database
	 * or not.  If we are, then key.data is a recno, not
	 * a string.
	 */
	dbcip = _PtrToInfo(dbc);
	if (dbcip == NULL)
		type = DB_UNKNOWN;
	else {
		dbip = dbcip->i_parent;
		if (dbip == NULL) {
			Tcl_SetResult(interp, "Cursor without parent database",
			    TCL_STATIC);
			result = TCL_ERROR;
			goto out;
		}
		thisdbp = dbip->i_dbp;
		type = thisdbp->get_type(thisdbp);
	}
	/*
	 * When we get here, we better have:
	 * 2 args, key and data if GET_BOTH was specified.
	 * 1 arg if -set, -set_range or -set_recno
	 * 0 in all other cases.
	 */
	if ((flag & DB_OPFLAGS_MASK) == DB_GET_BOTH) {
		if (i != (objc - 2)) {
			Tcl_WrongNumArgs(interp, 2, objv,
			    "?-args? -get_both key data");
			result = TCL_ERROR;
			goto out;
		} else {
			if (type == DB_RECNO || type == DB_QUEUE) {
				result = Tcl_GetIntFromObj(
				    interp, objv[objc-2], &itmp);
				recno = itmp;
				if (result == TCL_OK) {
					key.data = &recno;
					key.size = sizeof(db_recno_t);
				} else
					goto out;
			} else {
				key.data = Tcl_GetByteArrayFromObj(
				    objv[objc - 2], &itmp);
				key.size = itmp;
			}
			data.data =
			    Tcl_GetByteArrayFromObj(objv[objc - 1], &itmp);
			data.size = itmp;
		}
	} else if ((flag & DB_OPFLAGS_MASK) == DB_SET ||
		   (flag & DB_OPFLAGS_MASK) == DB_SET_RANGE ||
		   (flag & DB_OPFLAGS_MASK) == DB_SET_RECNO) {
		if (i != (objc - 1)) {
			Tcl_WrongNumArgs(interp, 2, objv, "?-args? key");
			result = TCL_ERROR;
			goto out;
		}
		data.flags |= DB_DBT_MALLOC;
		if ((flag & DB_OPFLAGS_MASK) == DB_SET_RECNO ||
		    type == DB_RECNO || type == DB_QUEUE) {
			result = Tcl_GetIntFromObj(interp,
			    objv[objc - 1], (int *)&recno);
			key.data = &recno;
			key.size = sizeof(db_recno_t);
		} else {
			key.data =
			    Tcl_GetByteArrayFromObj(objv[objc - 1], &itmp);
			key.size = itmp;
		}
	} else {
		if (i != objc) {
			Tcl_WrongNumArgs(interp, 2, objv, "?-args?");
			result = TCL_ERROR;
			goto out;
		}
		key.flags |= DB_DBT_MALLOC;
		data.flags |= DB_DBT_MALLOC;
	}

	_debug_check();
	ret = dbc->c_get(dbc, &key, &data, flag);
	result = _ReturnSetup(interp, ret, "dbc get");
	if (result == TCL_ERROR)
		goto out;

	retlist = Tcl_NewListObj(0, NULL);
	if (ret == DB_NOTFOUND)
		goto out1;
	if ((flag & DB_OPFLAGS_MASK) == DB_GET_RECNO) {
		recno = *((db_recno_t *)data.data);
		myobj = Tcl_NewIntObj((int)recno);
		result = Tcl_ListObjAppendElement(interp, retlist, myobj);
	} else {
		if ((type == DB_RECNO || type == DB_QUEUE) && key.data != NULL)
			result = _SetListRecnoElem(interp, retlist,
			    *(db_recno_t *)key.data, data.data, data.size);
		else
			result = _SetListElem(interp, retlist,
			    key.data, key.size, data.data, data.size);
	}
	if (key.flags & DB_DBT_MALLOC)
		__os_free(key.data, key.size);
	if (data.flags & DB_DBT_MALLOC)
		__os_free(data.data, data.size);
out1:
	if (result == TCL_OK)
		Tcl_SetObjResult(interp, retlist);
out:
	return (result);

}

/*
 * tcl_DbcDup --
 */
static int
tcl_DbcDup(interp, objc, objv, dbc)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DBC *dbc;			/* Cursor pointer */
{
	static char *dbcdupopts[] = {
		"-position",
		NULL
	};
	enum dbcdupopts {
		DBCDUP_POS
	};
	DB *thisdbp;
	DBC *newdbc;
	DBTCL_INFO *dbcip, *newdbcip, *dbip;
	Tcl_Obj *res;
	u_int32_t flag;
	int i, optindex, result, ret;
	char newname[MSG_SIZE];

	result = TCL_OK;
	flag = 0;
	res = NULL;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 2, objv, "?-args?");
		return (TCL_ERROR);
	}

	/*
	 * Get the command name index from the object based on the options
	 * defined above.
	 */
	i = 2;
	while (i < objc) {
		if (Tcl_GetIndexFromObj(interp, objv[i], dbcdupopts,
		    "option", TCL_EXACT, &optindex) != TCL_OK) {
			/*
			 * Reset the result so we don't get
			 * an errant error message if there is another error.
			 */
			if (IS_HELP(objv[i]) == TCL_OK) {
				result = TCL_OK;
				goto out;
			}
			Tcl_ResetResult(interp);
			break;
		}
		i++;
		switch ((enum dbcdupopts)optindex) {
		case DBCDUP_POS:
			flag = DB_POSITION;
			break;
		}
		if (result != TCL_OK)
			break;
	}
	if (result != TCL_OK)
		goto out;

	/*
	 * We need to determine if we are a recno database
	 * or not.  If we are, then key.data is a recno, not
	 * a string.
	 */
	dbcip = _PtrToInfo(dbc);
	if (dbcip == NULL) {
		Tcl_SetResult(interp, "Cursor without info structure",
		    TCL_STATIC);
		result = TCL_ERROR;
		goto out;
	} else {
		dbip = dbcip->i_parent;
		if (dbip == NULL) {
			Tcl_SetResult(interp, "Cursor without parent database",
			    TCL_STATIC);
			result = TCL_ERROR;
			goto out;
		}
		thisdbp = dbip->i_dbp;
	}
	/*
	 * Now duplicate the cursor.  If successful, we need to create
	 * a new cursor command.
	 */

	snprintf(newname, sizeof(newname),
	    "%s.c%d", dbip->i_name, dbip->i_dbdbcid);
	newdbcip = _NewInfo(interp, NULL, newname, I_DBC);
	if (newdbcip != NULL) {
		ret = dbc->c_dup(dbc, &newdbc, flag);
		if (ret == 0) {
			dbip->i_dbdbcid++;
			newdbcip->i_parent = dbip;
			Tcl_CreateObjCommand(interp, newname,
			    (Tcl_ObjCmdProc *)dbc_Cmd,
			    (ClientData)newdbc, NULL);
			res = Tcl_NewStringObj(newname, strlen(newname));
			_SetInfoData(newdbcip, newdbc);
			Tcl_SetObjResult(interp, res);
		} else {
			result = _ReturnSetup(interp, ret, "db dup");
			_DeleteInfo(newdbcip);
		}
	} else {
		Tcl_SetResult(interp, "Could not set up info", TCL_STATIC);
		result = TCL_ERROR;
	}
out:
	return (result);

}
