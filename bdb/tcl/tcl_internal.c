/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: tcl_internal.c,v 11.27 2000/05/22 18:36:51 sue Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#include <tcl.h>
#endif

#include "db_int.h"
#include "tcl_db.h"
#include "db_page.h"
#include "db_am.h"
#include "db_ext.h"

/*
 *
 * internal.c --
 *
 *	This file contains internal functions we need to maintain
 *	state for our Tcl interface.
 *
 *	NOTE: This all uses a linear linked list.  If we end up with
 *	too many info structs such that this is a performance hit, it
 *	should be redone using hashes or a list per type.  The assumption
 *	is that the user won't have more than a few dozen info structs
 *	in operation at any given point in time.  Even a complicated
 *	application with a few environments, nested transactions, locking,
 *	and several databases open, using cursors should not have a
 *	negative performance impact, in terms of searching the list to
 *	get/manipulate the info structure.
 */

/*
 * Prototypes for procedures defined later in this file:
 */

#define	GLOB_CHAR(c)	((c) == '*' || (c) == '?')

/*
 * PUBLIC: DBTCL_INFO *_NewInfo __P((Tcl_Interp *,
 * PUBLIC:    void *, char *, enum INFOTYPE));
 *
 * _NewInfo --
 *
 * This function will create a new info structure and fill it in
 * with the name and pointer, id and type.
 */
DBTCL_INFO *
_NewInfo(interp, anyp, name, type)
	Tcl_Interp *interp;
	void *anyp;
	char *name;
	enum INFOTYPE type;
{
	DBTCL_INFO *p;
	int i, ret;

	if ((ret = __os_malloc(NULL, sizeof(DBTCL_INFO), NULL, &p)) != 0) {
		Tcl_SetResult(interp, db_strerror(ret), TCL_STATIC);
		return (NULL);
	}

	if ((ret = __os_strdup(NULL, name, &p->i_name)) != 0) {
		Tcl_SetResult(interp, db_strerror(ret), TCL_STATIC);
		__os_free(p, sizeof(DBTCL_INFO));
		return (NULL);
	}
	p->i_interp = interp;
	p->i_anyp = anyp;
	p->i_data = 0;
	p->i_data2 = 0;
	p->i_type = type;
	p->i_parent = NULL;
	p->i_err = NULL;
	p->i_errpfx = NULL;
	p->i_lockobj.data = NULL;
	for (i = 0; i < MAX_ID; i++)
		p->i_otherid[i] = 0;

	LIST_INSERT_HEAD(&__db_infohead, p, entries);
	return (p);
}

/*
 * PUBLIC: void *_NameToPtr __P((CONST char *));
 */
void	*
_NameToPtr(name)
	CONST char *name;
{
	DBTCL_INFO *p;

	for (p = LIST_FIRST(&__db_infohead); p != NULL;
	    p = LIST_NEXT(p, entries))
		if (strcmp(name, p->i_name) == 0)
			return (p->i_anyp);
	return (NULL);
}

/*
 * PUBLIC: char *_PtrToName __P((CONST void *));
 */
char	*
_PtrToName(ptr)
	CONST void *ptr;
{
	DBTCL_INFO *p;

	for (p = LIST_FIRST(&__db_infohead); p != NULL;
	    p = LIST_NEXT(p, entries))
		if (p->i_anyp == ptr)
			return (p->i_name);
	return (NULL);
}

/*
 * PUBLIC: DBTCL_INFO *_PtrToInfo __P((CONST void *));
 */
DBTCL_INFO *
_PtrToInfo(ptr)
	CONST void *ptr;
{
	DBTCL_INFO *p;

	for (p = LIST_FIRST(&__db_infohead); p != NULL;
	    p = LIST_NEXT(p, entries))
		if (p->i_anyp == ptr)
			return (p);
	return (NULL);
}

/*
 * PUBLIC: DBTCL_INFO *_NameToInfo __P((CONST char *));
 */
DBTCL_INFO *
_NameToInfo(name)
	CONST char *name;
{
	DBTCL_INFO *p;

	for (p = LIST_FIRST(&__db_infohead); p != NULL;
	    p = LIST_NEXT(p, entries))
		if (strcmp(name, p->i_name) == 0)
			return (p);
	return (NULL);
}

/*
 * PUBLIC: void  _SetInfoData __P((DBTCL_INFO *, void *));
 */
void
_SetInfoData(p, data)
	DBTCL_INFO *p;
	void *data;
{
	if (p == NULL)
		return;
	p->i_anyp = data;
	return;
}

/*
 * PUBLIC: void  _DeleteInfo __P((DBTCL_INFO *));
 */
void
_DeleteInfo(p)
	DBTCL_INFO *p;
{
	if (p == NULL)
		return;
	LIST_REMOVE(p, entries);
	if (p->i_lockobj.data != NULL)
		__os_free(p->i_lockobj.data, p->i_lockobj.size);
	if (p->i_err != NULL) {
		fclose(p->i_err);
		p->i_err = NULL;
	}
	if (p->i_errpfx != NULL)
		__os_freestr(p->i_errpfx);
	__os_freestr(p->i_name);
	__os_free(p, sizeof(DBTCL_INFO));

	return;
}

/*
 * PUBLIC: int _SetListElem __P((Tcl_Interp *,
 * PUBLIC:    Tcl_Obj *, void *, int, void *, int));
 */
int
_SetListElem(interp, list, elem1, e1cnt, elem2, e2cnt)
	Tcl_Interp *interp;
	Tcl_Obj *list;
	void *elem1, *elem2;
	int e1cnt, e2cnt;
{
	Tcl_Obj *myobjv[2], *thislist;
	int myobjc;

	myobjc = 2;
	myobjv[0] = Tcl_NewByteArrayObj((u_char *)elem1, e1cnt);
	myobjv[1] = Tcl_NewByteArrayObj((u_char *)elem2, e2cnt);
	thislist = Tcl_NewListObj(myobjc, myobjv);
	if (thislist == NULL)
		return (TCL_ERROR);
	return (Tcl_ListObjAppendElement(interp, list, thislist));

}

/*
 * PUBLIC: int _SetListElemInt __P((Tcl_Interp *, Tcl_Obj *, void *, int));
 */
int
_SetListElemInt(interp, list, elem1, elem2)
	Tcl_Interp *interp;
	Tcl_Obj *list;
	void *elem1;
	int elem2;
{
	Tcl_Obj *myobjv[2], *thislist;
	int myobjc;

	myobjc = 2;
	myobjv[0] = Tcl_NewByteArrayObj((u_char *)elem1, strlen((char *)elem1));
	myobjv[1] = Tcl_NewIntObj(elem2);
	thislist = Tcl_NewListObj(myobjc, myobjv);
	if (thislist == NULL)
		return (TCL_ERROR);
	return (Tcl_ListObjAppendElement(interp, list, thislist));
}

/*
 * PUBLIC: int _SetListRecnoElem __P((Tcl_Interp *, Tcl_Obj *,
 * PUBLIC:     db_recno_t, u_char *, int));
 */
int
_SetListRecnoElem(interp, list, elem1, elem2, e2size)
	Tcl_Interp *interp;
	Tcl_Obj *list;
	db_recno_t elem1;
	u_char *elem2;
	int e2size;
{
	Tcl_Obj *myobjv[2], *thislist;
	int myobjc;

	myobjc = 2;
	myobjv[0] = Tcl_NewIntObj(elem1);
	myobjv[1] = Tcl_NewByteArrayObj(elem2, e2size);
	thislist = Tcl_NewListObj(myobjc, myobjv);
	if (thislist == NULL)
		return (TCL_ERROR);
	return (Tcl_ListObjAppendElement(interp, list, thislist));

}

/*
 * PUBLIC: int _GetGlobPrefix __P((char *, char **));
 */
int
_GetGlobPrefix(pattern, prefix)
	char *pattern;
	char **prefix;
{
	int i, j;
	char *p;

	/*
	 * Duplicate it, we get enough space and most of the work is done.
	 */
	if (__os_strdup(NULL, pattern, prefix) != 0)
		return (1);

	p = *prefix;
	for (i = 0, j = 0; p[i] && !GLOB_CHAR(p[i]); i++, j++)
		/*
		 * Check for an escaped character and adjust
		 */
		if (p[i] == '\\' && p[i+1]) {
			p[j] = p[i+1];
			i++;
		} else
			p[j] = p[i];
	p[j] = 0;
	return (0);
}

/*
 * PUBLIC: int _ReturnSetup __P((Tcl_Interp *, int, char *));
 */
int
_ReturnSetup(interp, ret, errmsg)
	Tcl_Interp *interp;
	int ret;
	char *errmsg;
{
	char *msg;

	if (ret > 0)
		return (_ErrorSetup(interp, ret, errmsg));

	/*
	 * We either have success or a DB error.  If a DB error, set up the
	 * string.  We return an error if not one of the errors we catch.
	 * If anyone wants to reset the result to return anything different,
	 * then the calling function is responsible for doing so via
	 * Tcl_ResetResult or another Tcl_SetObjResult.
	 */
	if (ret == 0) {
		Tcl_SetResult(interp, "0", TCL_STATIC);
		return (TCL_OK);
	}

	msg = db_strerror(ret);
	Tcl_AppendResult(interp, msg, NULL);

	switch (ret) {
	case DB_NOTFOUND:
	case DB_KEYEXIST:
	case DB_KEYEMPTY:
		return (TCL_OK);
	default:
		Tcl_SetErrorCode(interp, "BerkeleyDB", msg, NULL);
		return (TCL_ERROR);
	}
}

/*
 * PUBLIC: int _ErrorSetup __P((Tcl_Interp *, int, char *));
 */
int
_ErrorSetup(interp, ret, errmsg)
	Tcl_Interp *interp;
	int ret;
	char *errmsg;
{
	Tcl_SetErrno(ret);
	Tcl_AppendResult(interp, errmsg, ":", Tcl_PosixError(interp), NULL);
	return (TCL_ERROR);
}

/*
 * PUBLIC: void _ErrorFunc __P((CONST char *, char *));
 */
void
_ErrorFunc(pfx, msg)
	CONST char *pfx;
	char *msg;
{
	DBTCL_INFO *p;
	Tcl_Interp *interp;
	int size;
	char *err;

	p = _NameToInfo(pfx);
	if (p == NULL)
		return;
	interp = p->i_interp;

	size = strlen(pfx) + strlen(msg) + 4;
	/*
	 * If we cannot allocate enough to put together the prefix
	 * and message then give them just the message.
	 */
	if (__os_malloc(NULL, size, NULL, &err) != 0) {
		Tcl_AddErrorInfo(interp, msg);
		Tcl_AppendResult(interp, msg, "\n", NULL);
		return;
	}
	snprintf(err, size, "%s: %s", pfx, msg);
	Tcl_AddErrorInfo(interp, err);
	Tcl_AppendResult(interp, err, "\n", NULL);
	__os_free(err, size);
	return;
}

#define	INVALID_LSNMSG "Invalid LSN with %d parts. Should have 2.\n"

/*
 * PUBLIC: int _GetLsn __P((Tcl_Interp *, Tcl_Obj *, DB_LSN *));
 */
int
_GetLsn(interp, obj, lsn)
	Tcl_Interp *interp;
	Tcl_Obj *obj;
	DB_LSN *lsn;
{
	Tcl_Obj **myobjv;
	int itmp, myobjc, result;
	char msg[MSG_SIZE];

	result = Tcl_ListObjGetElements(interp, obj, &myobjc, &myobjv);
	if (result == TCL_ERROR)
		return (result);
	if (myobjc != 2) {
		result = TCL_ERROR;
		snprintf(msg, MSG_SIZE, INVALID_LSNMSG, myobjc);
		Tcl_SetResult(interp, msg, TCL_VOLATILE);
		return (result);
	}
	result = Tcl_GetIntFromObj(interp, myobjv[0], &itmp);
	if (result == TCL_ERROR)
		return (result);
	lsn->file = itmp;
	result = Tcl_GetIntFromObj(interp, myobjv[1], &itmp);
	lsn->offset = itmp;
	return (result);
}

int __debug_stop, __debug_on, __debug_print, __debug_test;

/*
 * PUBLIC: void _debug_check  __P((void));
 */
void
_debug_check()
{
	if (__debug_on == 0)
		return;

	if (__debug_print != 0) {
		printf("\r%6d:", __debug_on);
		fflush(stdout);
	}
	if (__debug_on++ == __debug_test || __debug_stop)
		__db_loadme();
}
