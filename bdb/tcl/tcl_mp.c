/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "db_config.h"

#ifndef lint
static const char revid[] = "$Id: tcl_mp.c,v 11.24 2001/01/09 16:13:59 sue Exp $";
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
static int      mp_Cmd __P((ClientData, Tcl_Interp *, int, Tcl_Obj * CONST*));
static int      pg_Cmd __P((ClientData, Tcl_Interp *, int, Tcl_Obj * CONST*));
static int      tcl_MpGet __P((Tcl_Interp *, int, Tcl_Obj * CONST*,
    DB_MPOOLFILE *, DBTCL_INFO *));
static int      tcl_Pg __P((Tcl_Interp *, int, Tcl_Obj * CONST*,
    void *, DB_MPOOLFILE *, DBTCL_INFO *, int));
static int      tcl_PgInit __P((Tcl_Interp *, int, Tcl_Obj * CONST*,
    void *, DBTCL_INFO *));
static int      tcl_PgIsset __P((Tcl_Interp *, int, Tcl_Obj * CONST*,
    void *, DBTCL_INFO *));

/*
 * _MpInfoDelete --
 *	Removes "sub" mp page info structures that are children
 *	of this mp.
 *
 * PUBLIC: void _MpInfoDelete __P((Tcl_Interp *, DBTCL_INFO *));
 */
void
_MpInfoDelete(interp, mpip)
	Tcl_Interp *interp;             /* Interpreter */
	DBTCL_INFO *mpip;		/* Info for mp */
{
	DBTCL_INFO *nextp, *p;

	for (p = LIST_FIRST(&__db_infohead); p != NULL; p = nextp) {
		/*
		 * Check if this info structure "belongs" to this
		 * mp.  Remove its commands and info structure.
		 */
		nextp = LIST_NEXT(p, entries);
		 if (p->i_parent == mpip && p->i_type == I_PG) {
			(void)Tcl_DeleteCommand(interp, p->i_name);
			_DeleteInfo(p);
		}
	}
}

/*
 * tcl_MpSync --
 *
 * PUBLIC: int tcl_MpSync __P((Tcl_Interp *, int, Tcl_Obj * CONST*, DB_ENV *));
 */
int
tcl_MpSync(interp, objc, objv, envp)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *envp;			/* Environment pointer */
{

	DB_LSN lsn;
	int result, ret;

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

	_debug_check();
	ret = memp_sync(envp, &lsn);
	result = _ReturnSetup(interp, ret, "memp sync");
	return (result);
}

/*
 * tcl_MpTrickle --
 *
 * PUBLIC: int tcl_MpTrickle __P((Tcl_Interp *, int,
 * PUBLIC:    Tcl_Obj * CONST*, DB_ENV *));
 */
int
tcl_MpTrickle(interp, objc, objv, envp)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *envp;			/* Environment pointer */
{

	int pages;
	int percent;
	int result;
	int ret;
	Tcl_Obj *res;

	result = TCL_OK;
	/*
	 * No flags, must be 3 args.
	 */
	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "percent");
		return (TCL_ERROR);
	}

	result = Tcl_GetIntFromObj(interp, objv[2], &percent);
	if (result == TCL_ERROR)
		return (result);

	_debug_check();
	ret = memp_trickle(envp, percent, &pages);
	result = _ReturnSetup(interp, ret, "memp trickle");
	if (result == TCL_ERROR)
		return (result);

	res = Tcl_NewIntObj(pages);
	Tcl_SetObjResult(interp, res);
	return (result);

}

/*
 * tcl_Mp --
 *
 * PUBLIC: int tcl_Mp __P((Tcl_Interp *, int,
 * PUBLIC:    Tcl_Obj * CONST*, DB_ENV *, DBTCL_INFO *));
 */
int
tcl_Mp(interp, objc, objv, envp, envip)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *envp;			/* Environment pointer */
	DBTCL_INFO *envip;		/* Info pointer */
{
	static char *mpopts[] = {
		"-create",
		"-mode",
		"-nommap",
		"-pagesize",
		"-rdonly",
		 NULL
	};
	enum mpopts {
		MPCREATE,
		MPMODE,
		MPNOMMAP,
		MPPAGE,
		MPRDONLY
	};
	DBTCL_INFO *ip;
	DB_MPOOLFILE *mpf;
	Tcl_Obj *res;
	u_int32_t flag;
	int i, pgsize, mode, optindex, result, ret;
	char *file, newname[MSG_SIZE];

	result = TCL_OK;
	i = 2;
	flag = 0;
	mode = 0;
	pgsize = 0;
	memset(newname, 0, MSG_SIZE);
	while (i < objc) {
		if (Tcl_GetIndexFromObj(interp, objv[i],
		    mpopts, "option", TCL_EXACT, &optindex) != TCL_OK) {
			/*
			 * Reset the result so we don't get an errant
			 * error message if there is another error.
			 * This arg is the file name.
			 */
			if (IS_HELP(objv[i]) == TCL_OK)
				return (TCL_OK);
			Tcl_ResetResult(interp);
			break;
		}
		i++;
		switch ((enum mpopts)optindex) {
		case MPCREATE:
			flag |= DB_CREATE;
			break;
		case MPNOMMAP:
			flag |= DB_NOMMAP;
			break;
		case MPPAGE:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-pagesize size?");
				result = TCL_ERROR;
				break;
			}
			/*
			 * Don't need to check result here because
			 * if TCL_ERROR, the error message is already
			 * set up, and we'll bail out below.  If ok,
			 * the mode is set and we go on.
			 */
			result = Tcl_GetIntFromObj(interp, objv[i++], &pgsize);
			break;
		case MPRDONLY:
			flag |= DB_RDONLY;
			break;
		case MPMODE:
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
		}
		if (result != TCL_OK)
			goto error;
	}
	/*
	 * Any left over arg is a file name.  It better be the last arg.
	 */
	file = NULL;
	if (i != objc) {
		if (i != objc - 1) {
			Tcl_WrongNumArgs(interp, 2, objv, "?args? ?file?");
			result = TCL_ERROR;
			goto error;
		}
		file = Tcl_GetStringFromObj(objv[i++], NULL);
	}

	snprintf(newname, sizeof(newname), "%s.mp%d",
	    envip->i_name, envip->i_envmpid);
	ip = _NewInfo(interp, NULL, newname, I_MP);
	if (ip == NULL) {
		Tcl_SetResult(interp, "Could not set up info",
		    TCL_STATIC);
		return (TCL_ERROR);
	}
	/*
	 * XXX finfop is NULL here.  Interface currently doesn't
	 * have all the stuff.  Should expand interface.
	 */
	_debug_check();
	ret = memp_fopen(envp, file, flag, mode, (size_t)pgsize, NULL, &mpf);
	if (ret != 0) {
		result = _ReturnSetup(interp, ret, "mpool");
		_DeleteInfo(ip);
	} else {
		/*
		 * Success.  Set up return.  Set up new info
		 * and command widget for this mpool.
		 */
		envip->i_envmpid++;
		ip->i_parent = envip;
		ip->i_pgsz = pgsize;
		_SetInfoData(ip, mpf);
		Tcl_CreateObjCommand(interp, newname,
		    (Tcl_ObjCmdProc *)mp_Cmd, (ClientData)mpf, NULL);
		res = Tcl_NewStringObj(newname, strlen(newname));
		Tcl_SetObjResult(interp, res);
	}
error:
	return (result);
}

/*
 * tcl_MpStat --
 *
 * PUBLIC: int tcl_MpStat __P((Tcl_Interp *, int, Tcl_Obj * CONST*, DB_ENV *));
 */
int
tcl_MpStat(interp, objc, objv, envp)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_ENV *envp;			/* Environment pointer */
{
	DB_MPOOL_STAT *sp;
	DB_MPOOL_FSTAT **fsp, **savefsp;
	int result;
	int ret;
	Tcl_Obj *res;
	Tcl_Obj *res1;

	result = TCL_OK;
	savefsp = NULL;
	/*
	 * No args for this.  Error if there are some.
	 */
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 2, objv, NULL);
		return (TCL_ERROR);
	}
	_debug_check();
	ret = memp_stat(envp, &sp, &fsp, NULL);
	result = _ReturnSetup(interp, ret, "memp stat");
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
	MAKE_STAT_LIST("Cache size (gbytes)", sp->st_gbytes);
	MAKE_STAT_LIST("Cache size (bytes)", sp->st_bytes);
	MAKE_STAT_LIST("Cache hits", sp->st_cache_hit);
	MAKE_STAT_LIST("Cache misses", sp->st_cache_miss);
	MAKE_STAT_LIST("Number of caches", sp->st_ncache);
	MAKE_STAT_LIST("Pages mapped into address space", sp->st_map);
	MAKE_STAT_LIST("Pages created", sp->st_page_create);
	MAKE_STAT_LIST("Pages read in", sp->st_page_in);
	MAKE_STAT_LIST("Pages written", sp->st_page_out);
	MAKE_STAT_LIST("Clean page evictions", sp->st_ro_evict);
	MAKE_STAT_LIST("Dirty page evictions", sp->st_rw_evict);
	MAKE_STAT_LIST("Hash buckets", sp->st_hash_buckets);
	MAKE_STAT_LIST("Hash lookups", sp->st_hash_searches);
	MAKE_STAT_LIST("Longest hash chain found", sp->st_hash_longest);
	MAKE_STAT_LIST("Hash elements examined", sp->st_hash_examined);
	MAKE_STAT_LIST("Cached clean pages", sp->st_page_clean);
	MAKE_STAT_LIST("Cached dirty pages", sp->st_page_dirty);
	MAKE_STAT_LIST("Dirty pages trickled", sp->st_page_trickle);
	MAKE_STAT_LIST("Number of region lock waits", sp->st_region_wait);
	MAKE_STAT_LIST("Number of region lock nowaits", sp->st_region_nowait);
	/*
	 * Save global stat list as res1.  The MAKE_STAT_LIST
	 * macro assumes 'res' so we'll use that to build up
	 * our per-file sublist.
	 */
	res1 = res;
	savefsp = fsp;
	for (; fsp != NULL && *fsp != NULL; fsp++) {
		res = Tcl_NewObj();
		result = _SetListElem(interp, res, "File Name",
		    strlen("File Name"), (*fsp)->file_name,
		    strlen((*fsp)->file_name));
		if (result != TCL_OK)
			goto error;
		MAKE_STAT_LIST("Page size", (*fsp)->st_pagesize);
		MAKE_STAT_LIST("Cache Hits", (*fsp)->st_cache_hit);
		MAKE_STAT_LIST("Cache Misses", (*fsp)->st_cache_miss);
		MAKE_STAT_LIST("Pages mapped into address space",
		    (*fsp)->st_map);
		MAKE_STAT_LIST("Pages created", (*fsp)->st_page_create);
		MAKE_STAT_LIST("Pages read in", (*fsp)->st_page_in);
		MAKE_STAT_LIST("Pages written", (*fsp)->st_page_out);
		/*
		 * Now that we have a complete "per-file" stat
		 * list, append that to the other list.
		 */
		result = Tcl_ListObjAppendElement(interp, res1, res);
		if (result != TCL_OK)
			goto error;
	}
	Tcl_SetObjResult(interp, res1);
error:
	__os_free(sp, sizeof(*sp));
	if (savefsp != NULL)
		__os_free(savefsp, 0);
	return (result);
}

/*
 * mp_Cmd --
 *	Implements the "mp" widget.
 */
static int
mp_Cmd(clientData, interp, objc, objv)
	ClientData clientData;          /* Mp handle */
	Tcl_Interp *interp;             /* Interpreter */
	int objc;                       /* How many arguments? */
	Tcl_Obj *CONST objv[];          /* The argument objects */
{
	static char *mpcmds[] = {
		"close",	"fsync",	"get",
		NULL
	};
	enum mpcmds {
		MPCLOSE,	MPFSYNC,	MPGET
	};
	DB_MPOOLFILE *mp;
	int cmdindex, length, result, ret;
	DBTCL_INFO *mpip;
	Tcl_Obj *res;
	char *obj_name;

	Tcl_ResetResult(interp);
	mp = (DB_MPOOLFILE *)clientData;
	obj_name = Tcl_GetStringFromObj(objv[0], &length);
	mpip = _NameToInfo(obj_name);
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
	    objv[1], mpcmds, "command", TCL_EXACT, &cmdindex) != TCL_OK)
		return (IS_HELP(objv[1]));

	res = NULL;
	switch ((enum mpcmds)cmdindex) {
	case MPCLOSE:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, NULL);
			return (TCL_ERROR);
		}
		_debug_check();
		ret = memp_fclose(mp);
		result = _ReturnSetup(interp, ret, "mp close");
		_MpInfoDelete(interp, mpip);
		(void)Tcl_DeleteCommand(interp, mpip->i_name);
		_DeleteInfo(mpip);
		break;
	case MPFSYNC:
		if (objc != 2) {
			Tcl_WrongNumArgs(interp, 1, objv, NULL);
			return (TCL_ERROR);
		}
		_debug_check();
		ret = memp_fsync(mp);
		res = Tcl_NewIntObj(ret);
		break;
	case MPGET:
		result = tcl_MpGet(interp, objc, objv, mp, mpip);
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
 * tcl_MpGet --
 */
static int
tcl_MpGet(interp, objc, objv, mp, mpip)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DB_MPOOLFILE *mp;		/* mp pointer */
	DBTCL_INFO *mpip;		/* mp info pointer */
{
	static char *mpget[] = {
		"-create",	"-last",	"-new",
		NULL
	};
	enum mpget {
		MPGET_CREATE,	MPGET_LAST,	MPGET_NEW
	};

	DBTCL_INFO *ip;
	Tcl_Obj *res;
	db_pgno_t pgno;
	u_int32_t flag;
	int i, ipgno, optindex, result, ret;
	char newname[MSG_SIZE];
	void *page;

	result = TCL_OK;
	memset(newname, 0, MSG_SIZE);
	i = 2;
	flag = 0;
	while (i < objc) {
		if (Tcl_GetIndexFromObj(interp, objv[i],
		    mpget, "option", TCL_EXACT, &optindex) != TCL_OK) {
			/*
			 * Reset the result so we don't get an errant
			 * error message if there is another error.
			 * This arg is the page number.
			 */
			if (IS_HELP(objv[i]) == TCL_OK)
				return (TCL_OK);
			Tcl_ResetResult(interp);
			break;
		}
		i++;
		switch ((enum mpget)optindex) {
		case MPGET_CREATE:
			flag |= DB_MPOOL_CREATE;
			break;
		case MPGET_LAST:
			flag |= DB_MPOOL_LAST;
			break;
		case MPGET_NEW:
			flag |= DB_MPOOL_NEW;
			break;
		}
		if (result != TCL_OK)
			goto error;
	}
	/*
	 * Any left over arg is a page number.  It better be the last arg.
	 */
	ipgno = 0;
	if (i != objc) {
		if (i != objc - 1) {
			Tcl_WrongNumArgs(interp, 2, objv, "?args? ?pgno?");
			result = TCL_ERROR;
			goto error;
		}
		result = Tcl_GetIntFromObj(interp, objv[i++], &ipgno);
		if (result != TCL_OK)
			goto error;
	}

	snprintf(newname, sizeof(newname), "%s.pg%d",
	    mpip->i_name, mpip->i_mppgid);
	ip = _NewInfo(interp, NULL, newname, I_PG);
	if (ip == NULL) {
		Tcl_SetResult(interp, "Could not set up info",
		    TCL_STATIC);
		return (TCL_ERROR);
	}
	_debug_check();
	pgno = ipgno;
	ret = memp_fget(mp, &pgno, flag, &page);
	result = _ReturnSetup(interp, ret, "mpool get");
	if (result == TCL_ERROR)
		_DeleteInfo(ip);
	else {
		/*
		 * Success.  Set up return.  Set up new info
		 * and command widget for this mpool.
		 */
		mpip->i_mppgid++;
		ip->i_parent = mpip;
		ip->i_pgno = pgno;
		ip->i_pgsz = mpip->i_pgsz;
		_SetInfoData(ip, page);
		Tcl_CreateObjCommand(interp, newname,
		    (Tcl_ObjCmdProc *)pg_Cmd, (ClientData)page, NULL);
		res = Tcl_NewStringObj(newname, strlen(newname));
		Tcl_SetObjResult(interp, res);
	}
error:
	return (result);
}

/*
 * pg_Cmd --
 *	Implements the "pg" widget.
 */
static int
pg_Cmd(clientData, interp, objc, objv)
	ClientData clientData;          /* Page handle */
	Tcl_Interp *interp;             /* Interpreter */
	int objc;                       /* How many arguments? */
	Tcl_Obj *CONST objv[];          /* The argument objects */
{
	static char *pgcmds[] = {
		"init",
		"is_setto",
		"pgnum",
		"pgsize",
		"put",
		"set",
		NULL
	};
	enum pgcmds {
		PGINIT,
		PGISSET,
		PGNUM,
		PGSIZE,
		PGPUT,
		PGSET
	};
	DB_MPOOLFILE *mp;
	int cmdindex, length, result;
	char *obj_name;
	void *page;
	DBTCL_INFO *pgip;
	Tcl_Obj *res;

	Tcl_ResetResult(interp);
	page = (void *)clientData;
	obj_name = Tcl_GetStringFromObj(objv[0], &length);
	pgip = _NameToInfo(obj_name);
	mp = NAME_TO_MP(pgip->i_parent->i_name);
	result = TCL_OK;

	if (page == NULL) {
		Tcl_SetResult(interp, "NULL page pointer", TCL_STATIC);
		return (TCL_ERROR);
	}
	if (mp == NULL) {
		Tcl_SetResult(interp, "NULL mp pointer", TCL_STATIC);
		return (TCL_ERROR);
	}
	if (pgip == NULL) {
		Tcl_SetResult(interp, "NULL page info pointer", TCL_STATIC);
		return (TCL_ERROR);
	}

	/*
	 * Get the command name index from the object based on the dbcmds
	 * defined above.
	 */
	if (Tcl_GetIndexFromObj(interp,
	    objv[1], pgcmds, "command", TCL_EXACT, &cmdindex) != TCL_OK)
		return (IS_HELP(objv[1]));

	res = NULL;
	switch ((enum pgcmds)cmdindex) {
	case PGNUM:
		res = Tcl_NewIntObj(pgip->i_pgno);
		break;
	case PGSIZE:
		res = Tcl_NewLongObj(pgip->i_pgsz);
		break;
	case PGSET:
	case PGPUT:
		result = tcl_Pg(interp, objc, objv, page, mp, pgip,
		    cmdindex == PGSET ? 0 : 1);
		break;
	case PGINIT:
		result = tcl_PgInit(interp, objc, objv, page, pgip);
		break;
	case PGISSET:
		result = tcl_PgIsset(interp, objc, objv, page, pgip);
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
tcl_Pg(interp, objc, objv, page, mp, pgip, putop)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	void *page;			/* Page pointer */
	DB_MPOOLFILE *mp;		/* Mpool pointer */
	DBTCL_INFO *pgip;		/* Info pointer */
	int putop;			/* Operation */
{
	static char *pgopt[] = {
		"-clean",	"-dirty",	"-discard",
		NULL
	};
	enum pgopt {
		PGCLEAN,	PGDIRTY,	PGDISCARD
	};
	u_int32_t flag;
	int i, optindex, result, ret;

	result = TCL_OK;
	i = 2;
	flag = 0;
	while (i < objc) {
		if (Tcl_GetIndexFromObj(interp, objv[i],
		    pgopt, "option", TCL_EXACT, &optindex) != TCL_OK)
			return (IS_HELP(objv[i]));
		i++;
		switch ((enum pgopt)optindex) {
		case PGCLEAN:
			flag |= DB_MPOOL_CLEAN;
			break;
		case PGDIRTY:
			flag |= DB_MPOOL_DIRTY;
			break;
		case PGDISCARD:
			flag |= DB_MPOOL_DISCARD;
			break;
		}
	}

	_debug_check();
	if (putop)
		ret = memp_fput(mp, page, flag);
	else
		ret = memp_fset(mp, page, flag);

	result = _ReturnSetup(interp, ret, "page");

	if (putop) {
		(void)Tcl_DeleteCommand(interp, pgip->i_name);
		_DeleteInfo(pgip);
	}
	return (result);
}

static int
tcl_PgInit(interp, objc, objv, page, pgip)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	void *page;			/* Page pointer */
	DBTCL_INFO *pgip;		/* Info pointer */
{
	Tcl_Obj *res;
	size_t pgsz;
	long *p, *endp, newval;
	int length, result;
	u_char *s;

	result = TCL_OK;
	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "val");
		return (TCL_ERROR);
	}

	pgsz = pgip->i_pgsz;
	result = Tcl_GetLongFromObj(interp, objv[2], &newval);
	if (result != TCL_OK) {
		s = Tcl_GetByteArrayFromObj(objv[2], &length);
		if (s == NULL)
			return (TCL_ERROR);
		memcpy(page, s, ((size_t)length < pgsz) ? length : pgsz);
		result = TCL_OK;
	} else {
		p = (long *)page;
		for (endp = p + (pgsz / sizeof(long)); p < endp; p++)
			*p = newval;
	}
	res = Tcl_NewIntObj(0);
	Tcl_SetObjResult(interp, res);
	return (result);
}

static int
tcl_PgIsset(interp, objc, objv, page, pgip)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	void *page;			/* Page pointer */
	DBTCL_INFO *pgip;		/* Info pointer */
{
	Tcl_Obj *res;
	size_t pgsz;
	long *p, *endp, newval;
	int length, result;
	u_char *s;

	result = TCL_OK;
	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "val");
		return (TCL_ERROR);
	}

	pgsz = pgip->i_pgsz;
	result = Tcl_GetLongFromObj(interp, objv[2], &newval);
	if (result != TCL_OK) {
		if ((s = Tcl_GetByteArrayFromObj(objv[2], &length)) == NULL)
			return (TCL_ERROR);
		result = TCL_OK;

		if (memcmp(page,
		    s, ((size_t)length < pgsz) ? length : pgsz ) != 0) {
			res = Tcl_NewIntObj(0);
			Tcl_SetObjResult(interp, res);
			return (result);
		}
	} else {
		p = (long *)page;
		/*
		 * If any value is not the same, return 0 (is not set to
		 * this value).  Otherwise, if we finish the loop, we return 1
		 * (is set to this value).
		 */
		for (endp = p + (pgsz/sizeof(long)); p < endp; p++)
			if (*p != newval) {
				res = Tcl_NewIntObj(0);
				Tcl_SetObjResult(interp, res);
				return (result);
			}
	}

	res = Tcl_NewIntObj(1);
	Tcl_SetObjResult(interp, res);
	return (result);
}
