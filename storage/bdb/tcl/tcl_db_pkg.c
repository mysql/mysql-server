/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1999-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: tcl_db_pkg.c,v 11.190 2004/10/27 16:48:32 bostic Exp $
 */

#include "db_config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#include <tcl.h>
#endif

#ifdef CONFIG_TEST
#define	DB_DBM_HSEARCH 1
#endif

#include "db_int.h"
#include "dbinc/db_page.h"
#include "dbinc/hash.h"
#include "dbinc/tcl_db.h"

/* XXX we must declare global data in just one place */
DBTCL_GLOBAL __dbtcl_global;

/*
 * Prototypes for procedures defined later in this file:
 */
static int	berkdb_Cmd __P((ClientData, Tcl_Interp *, int,
    Tcl_Obj * CONST*));
static int	bdb_EnvOpen __P((Tcl_Interp *, int, Tcl_Obj * CONST*,
    DBTCL_INFO *, DB_ENV **));
static int	bdb_DbOpen __P((Tcl_Interp *, int, Tcl_Obj * CONST*,
    DBTCL_INFO *, DB **));
static int	bdb_DbRemove __P((Tcl_Interp *, int, Tcl_Obj * CONST*));
static int	bdb_DbRename __P((Tcl_Interp *, int, Tcl_Obj * CONST*));
static int	bdb_Version __P((Tcl_Interp *, int, Tcl_Obj * CONST*));
static int	bdb_SeqOpen __P((Tcl_Interp *, int, Tcl_Obj * CONST*,
    DBTCL_INFO *, DB_SEQUENCE **));

#ifdef CONFIG_TEST
static int	bdb_DbUpgrade __P((Tcl_Interp *, int, Tcl_Obj * CONST*));
static int	bdb_DbVerify __P((Tcl_Interp *, int, Tcl_Obj * CONST*));
static int	bdb_Handles __P((Tcl_Interp *, int, Tcl_Obj * CONST*));
static int	bdb_MsgType __P((Tcl_Interp *, int, Tcl_Obj * CONST*));

static int	tcl_bt_compare __P((DB *, const DBT *, const DBT *));
static int	tcl_compare_callback __P((DB *, const DBT *, const DBT *,
    Tcl_Obj *, char *));
static void	tcl_db_free __P((void *));
static void *	tcl_db_malloc __P((size_t));
static void *	tcl_db_realloc __P((void *, size_t));
static int	tcl_dup_compare __P((DB *, const DBT *, const DBT *));
static u_int32_t tcl_h_hash __P((DB *, const void *, u_int32_t));
static int	tcl_rep_send __P((DB_ENV *,
    const DBT *, const DBT *, const DB_LSN *, int, u_int32_t));
#endif

/*
 * Db_tcl_Init --
 *
 * This is a package initialization procedure, which is called by Tcl when
 * this package is to be added to an interpreter.  The name is based on the
 * name of the shared library, currently libdb_tcl-X.Y.so, which Tcl uses
 * to determine the name of this function.
 */
int
Db_tcl_Init(interp)
	Tcl_Interp *interp;		/* Interpreter in which the package is
					 * to be made available. */
{
	int code;
	char pkg[12];

	snprintf(pkg, sizeof(pkg), "%d.%d", DB_VERSION_MAJOR, DB_VERSION_MINOR);
	code = Tcl_PkgProvide(interp, "Db_tcl", pkg);
	if (code != TCL_OK)
		return (code);

	(void)Tcl_CreateObjCommand(interp,
	    "berkdb", (Tcl_ObjCmdProc *)berkdb_Cmd, (ClientData)0, NULL);
	/*
	 * Create shared global debugging variables
	 */
	(void)Tcl_LinkVar(
	    interp, "__debug_on", (char *)&__debug_on, TCL_LINK_INT);
	(void)Tcl_LinkVar(
	    interp, "__debug_print", (char *)&__debug_print, TCL_LINK_INT);
	(void)Tcl_LinkVar(
	    interp, "__debug_stop", (char *)&__debug_stop, TCL_LINK_INT);
	(void)Tcl_LinkVar(
	    interp, "__debug_test", (char *)&__debug_test,
	    TCL_LINK_INT);
	LIST_INIT(&__db_infohead);
	return (TCL_OK);
}

/*
 * berkdb_cmd --
 *	Implements the "berkdb" command.
 *	This command supports three sub commands:
 *	berkdb version - Returns a list {major minor patch}
 *	berkdb env - Creates a new DB_ENV and returns a binding
 *	  to a new command of the form dbenvX, where X is an
 *	  integer starting at 0 (dbenv0, dbenv1, ...)
 *	berkdb open - Creates a new DB (optionally within
 *	  the given environment.  Returns a binding to a new
 *	  command of the form dbX, where X is an integer
 *	  starting at 0 (db0, db1, ...)
 */
static int
berkdb_Cmd(notused, interp, objc, objv)
	ClientData notused;		/* Not used. */
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
{
	static const char *berkdbcmds[] = {
#ifdef CONFIG_TEST
		"dbverify",
		"handles",
		"msgtype",
		"upgrade",
#endif
		"dbremove",
		"dbrename",
		"env",
		"envremove",
		"open",
#ifdef HAVE_SEQUENCE
		"sequence",
#endif
		"version",
#ifdef CONFIG_TEST
		/* All below are compatibility functions */
		"hcreate",	"hsearch",	"hdestroy",
		"dbminit",	"fetch",	"store",
		"delete",	"firstkey",	"nextkey",
		"ndbm_open",	"dbmclose",
#endif
		/* All below are convenience functions */
		"rand",		"random_int",	"srand",
		"debug_check",
		NULL
	};
	/*
	 * All commands enums below ending in X are compatibility
	 */
	enum berkdbcmds {
#ifdef CONFIG_TEST
		BDB_DBVERIFY,
		BDB_HANDLES,
		BDB_MSGTYPE,
		BDB_UPGRADE,
#endif
		BDB_DBREMOVE,
		BDB_DBRENAME,
		BDB_ENV,
		BDB_ENVREMOVE,
		BDB_OPEN,
#ifdef HAVE_SEQUENCE
		BDB_SEQUENCE,
#endif
		BDB_VERSION,
#ifdef CONFIG_TEST
		BDB_HCREATEX,	BDB_HSEARCHX,	BDB_HDESTROYX,
		BDB_DBMINITX,	BDB_FETCHX,	BDB_STOREX,
		BDB_DELETEX,	BDB_FIRSTKEYX,	BDB_NEXTKEYX,
		BDB_NDBMOPENX,	BDB_DBMCLOSEX,
#endif
		BDB_RANDX,	BDB_RAND_INTX,	BDB_SRANDX,
		BDB_DBGCKX
	};
	static int env_id = 0;
	static int db_id = 0;
#ifdef HAVE_SEQUENCE
	static int seq_id = 0;
#endif

	DB *dbp;
#ifdef HAVE_SEQUENCE
	DB_SEQUENCE *seq;
#endif
#ifdef CONFIG_TEST
	DBM *ndbmp;
	static int ndbm_id = 0;
#endif
	DBTCL_INFO *ip;
	DB_ENV *envp;
	Tcl_Obj *res;
	int cmdindex, result;
	char newname[MSG_SIZE];

	COMPQUIET(notused, NULL);

	Tcl_ResetResult(interp);
	memset(newname, 0, MSG_SIZE);
	result = TCL_OK;
	if (objc <= 1) {
		Tcl_WrongNumArgs(interp, 1, objv, "command cmdargs");
		return (TCL_ERROR);
	}

	/*
	 * Get the command name index from the object based on the berkdbcmds
	 * defined above.
	 */
	if (Tcl_GetIndexFromObj(interp,
	    objv[1], berkdbcmds, "command", TCL_EXACT, &cmdindex) != TCL_OK)
		return (IS_HELP(objv[1]));
	res = NULL;
	switch ((enum berkdbcmds)cmdindex) {
#ifdef CONFIG_TEST
	case BDB_DBVERIFY:
		result = bdb_DbVerify(interp, objc, objv);
		break;
	case BDB_HANDLES:
		result = bdb_Handles(interp, objc, objv);
		break;
	case BDB_MSGTYPE:
		result = bdb_MsgType(interp, objc, objv);
		break;
	case BDB_UPGRADE:
		result = bdb_DbUpgrade(interp, objc, objv);
		break;
#endif
	case BDB_VERSION:
		_debug_check();
		result = bdb_Version(interp, objc, objv);
		break;
	case BDB_ENV:
		snprintf(newname, sizeof(newname), "env%d", env_id);
		ip = _NewInfo(interp, NULL, newname, I_ENV);
		if (ip != NULL) {
			result = bdb_EnvOpen(interp, objc, objv, ip, &envp);
			if (result == TCL_OK && envp != NULL) {
				env_id++;
				(void)Tcl_CreateObjCommand(interp, newname,
				    (Tcl_ObjCmdProc *)env_Cmd,
				    (ClientData)envp, NULL);
				/* Use ip->i_name - newname is overwritten */
				res = NewStringObj(newname, strlen(newname));
				_SetInfoData(ip, envp);
			} else
				_DeleteInfo(ip);
		} else {
			Tcl_SetResult(interp, "Could not set up info",
			    TCL_STATIC);
			result = TCL_ERROR;
		}
		break;
	case BDB_DBREMOVE:
		result = bdb_DbRemove(interp, objc, objv);
		break;
	case BDB_DBRENAME:
		result = bdb_DbRename(interp, objc, objv);
		break;
	case BDB_ENVREMOVE:
		result = tcl_EnvRemove(interp, objc, objv, NULL, NULL);
		break;
	case BDB_OPEN:
		snprintf(newname, sizeof(newname), "db%d", db_id);
		ip = _NewInfo(interp, NULL, newname, I_DB);
		if (ip != NULL) {
			result = bdb_DbOpen(interp, objc, objv, ip, &dbp);
			if (result == TCL_OK && dbp != NULL) {
				db_id++;
				(void)Tcl_CreateObjCommand(interp, newname,
				    (Tcl_ObjCmdProc *)db_Cmd,
				    (ClientData)dbp, NULL);
				/* Use ip->i_name - newname is overwritten */
				res = NewStringObj(newname, strlen(newname));
				_SetInfoData(ip, dbp);
			} else
				_DeleteInfo(ip);
		} else {
			Tcl_SetResult(interp, "Could not set up info",
			    TCL_STATIC);
			result = TCL_ERROR;
		}
		break;
#ifdef HAVE_SEQUENCE
	case BDB_SEQUENCE:
		snprintf(newname, sizeof(newname), "seq%d", seq_id);
		ip = _NewInfo(interp, NULL, newname, I_SEQ);
		if (ip != NULL) {
			result = bdb_SeqOpen(interp, objc, objv, ip, &seq);
			if (result == TCL_OK && seq != NULL) {
				seq_id++;
				(void)Tcl_CreateObjCommand(interp, newname,
				    (Tcl_ObjCmdProc *)seq_Cmd,
				    (ClientData)seq, NULL);
				/* Use ip->i_name - newname is overwritten */
				res = NewStringObj(newname, strlen(newname));
				_SetInfoData(ip, seq);
			} else
				_DeleteInfo(ip);
		} else {
			Tcl_SetResult(interp, "Could not set up info",
			    TCL_STATIC);
			result = TCL_ERROR;
		}
		break;
#endif
#ifdef CONFIG_TEST
	case BDB_HCREATEX:
	case BDB_HSEARCHX:
	case BDB_HDESTROYX:
		result = bdb_HCommand(interp, objc, objv);
		break;
	case BDB_DBMINITX:
	case BDB_DBMCLOSEX:
	case BDB_FETCHX:
	case BDB_STOREX:
	case BDB_DELETEX:
	case BDB_FIRSTKEYX:
	case BDB_NEXTKEYX:
		result = bdb_DbmCommand(interp, objc, objv, DBTCL_DBM, NULL);
		break;
	case BDB_NDBMOPENX:
		snprintf(newname, sizeof(newname), "ndbm%d", ndbm_id);
		ip = _NewInfo(interp, NULL, newname, I_NDBM);
		if (ip != NULL) {
			result = bdb_NdbmOpen(interp, objc, objv, &ndbmp);
			if (result == TCL_OK) {
				ndbm_id++;
				(void)Tcl_CreateObjCommand(interp, newname,
				    (Tcl_ObjCmdProc *)ndbm_Cmd,
				    (ClientData)ndbmp, NULL);
				/* Use ip->i_name - newname is overwritten */
				res = NewStringObj(newname, strlen(newname));
				_SetInfoData(ip, ndbmp);
			} else
				_DeleteInfo(ip);
		} else {
			Tcl_SetResult(interp, "Could not set up info",
			    TCL_STATIC);
			result = TCL_ERROR;
		}
		break;
#endif
	case BDB_RANDX:
	case BDB_RAND_INTX:
	case BDB_SRANDX:
		result = bdb_RandCommand(interp, objc, objv);
		break;
	case BDB_DBGCKX:
		_debug_check();
		res = Tcl_NewIntObj(0);
		break;
	}
	/*
	 * For each different arg call different function to create
	 * new commands (or if version, get/return it).
	 */
	if (result == TCL_OK && res != NULL)
		Tcl_SetObjResult(interp, res);
	return (result);
}

/*
 * bdb_EnvOpen -
 *	Implements the environment open command.
 *	There are many, many options to the open command.
 *	Here is the general flow:
 *
 *	1.  Call db_env_create to create the env handle.
 *	2.  Parse args tracking options.
 *	3.  Make any pre-open setup calls necessary.
 *	4.  Call DB_ENV->open to open the env.
 *	5.  Return env widget handle to user.
 */
static int
bdb_EnvOpen(interp, objc, objv, ip, env)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DBTCL_INFO *ip;			/* Our internal info */
	DB_ENV **env;			/* Environment pointer */
{
	static const char *envopen[] = {
#ifdef CONFIG_TEST
		"-alloc",
		"-auto_commit",
		"-cdb",
		"-cdb_alldb",
		"-client_timeout",
		"-lock",
		"-lock_conflict",
		"-lock_detect",
		"-lock_max",
		"-lock_max_locks",
		"-lock_max_lockers",
		"-lock_max_objects",
		"-lock_timeout",
		"-log",
		"-log_buffer",
		"-log_inmemory",
		"-log_max",
		"-log_regionmax",
		"-log_remove",
		"-mpool_max_openfd",
		"-mpool_max_write",
		"-mpool_mmap_size",
		"-mpool_nommap",
		"-overwrite",
		"-region_init",
		"-rep_client",
		"-rep_master",
		"-rep_transport",
		"-server",
		"-server_timeout",
		"-set_intermediate_dir",
		"-thread",
		"-time_notgranted",
		"-txn_timeout",
		"-txn_timestamp",
		"-verbose",
		"-wrnosync",
#endif
		"-cachesize",
		"-create",
		"-data_dir",
		"-encryptaes",
		"-encryptany",
		"-errfile",
		"-errpfx",
		"-home",
		"-log_dir",
		"-mode",
		"-private",
		"-recover",
		"-recover_fatal",
		"-shm_key",
		"-system_mem",
		"-tmp_dir",
		"-txn",
		"-txn_max",
		"-use_environ",
		"-use_environ_root",
		NULL
	};
	/*
	 * !!!
	 * These have to be in the same order as the above,
	 * which is close to but not quite alphabetical.
	 */
	enum envopen {
#ifdef CONFIG_TEST
		ENV_ALLOC,
		ENV_AUTO_COMMIT,
		ENV_CDB,
		ENV_CDB_ALLDB,
		ENV_CLIENT_TO,
		ENV_LOCK,
		ENV_CONFLICT,
		ENV_DETECT,
		ENV_LOCK_MAX,
		ENV_LOCK_MAX_LOCKS,
		ENV_LOCK_MAX_LOCKERS,
		ENV_LOCK_MAX_OBJECTS,
		ENV_LOCK_TIMEOUT,
		ENV_LOG,
		ENV_LOG_BUFFER,
		ENV_LOG_INMEMORY,
		ENV_LOG_MAX,
		ENV_LOG_REGIONMAX,
		ENV_LOG_REMOVE,
		ENV_MPOOL_MAX_OPENFD,
		ENV_MPOOL_MAX_WRITE,
		ENV_MPOOL_MMAP_SIZE,
		ENV_MPOOL_NOMMAP,
		ENV_OVERWRITE,
		ENV_REGION_INIT,
		ENV_REP_CLIENT,
		ENV_REP_MASTER,
		ENV_REP_TRANSPORT,
		ENV_SERVER,
		ENV_SERVER_TO,
		ENV_SET_INTERMEDIATE_DIR,
		ENV_THREAD,
		ENV_TIME_NOTGRANTED,
		ENV_TXN_TIMEOUT,
		ENV_TXN_TIME,
		ENV_VERBOSE,
		ENV_WRNOSYNC,
#endif
		ENV_CACHESIZE,
		ENV_CREATE,
		ENV_DATA_DIR,
		ENV_ENCRYPT_AES,
		ENV_ENCRYPT_ANY,
		ENV_ERRFILE,
		ENV_ERRPFX,
		ENV_HOME,
		ENV_LOG_DIR,
		ENV_MODE,
		ENV_PRIVATE,
		ENV_RECOVER,
		ENV_RECOVER_FATAL,
		ENV_SHM_KEY,
		ENV_SYSTEM_MEM,
		ENV_TMP_DIR,
		ENV_TXN,
		ENV_TXN_MAX,
		ENV_USE_ENVIRON,
		ENV_USE_ENVIRON_ROOT
	};
	Tcl_Obj **myobjv;
	u_int32_t cr_flags, gbytes, bytes, logbufset, logmaxset;
	u_int32_t open_flags, rep_flags, set_flags, uintarg;
	int i, mode, myobjc, ncaches, optindex, result, ret;
	long client_to, server_to, shm;
	char *arg, *home, *passwd, *server;
#ifdef CONFIG_TEST
	Tcl_Obj **myobjv1;
	time_t timestamp;
	long v;
	u_int32_t detect;
	u_int8_t *conflicts;
	int intarg, intarg2, j, nmodes, temp;
#endif

	result = TCL_OK;
	mode = 0;
	rep_flags = set_flags = cr_flags = 0;
	home = NULL;

	/*
	 * XXX
	 * If/when our Tcl interface becomes thread-safe, we should enable
	 * DB_THREAD here in all cases.  For now, we turn it on later in this
	 * function, and only when we're in testing and we specify the
	 * -thread flag, so that we can exercise MUTEX_THREAD_LOCK cases.
	 *
	 * In order to become truly thread-safe, we need to look at making sure
	 * DBTCL_INFO structs are safe to share across threads (they're not
	 * mutex-protected) before we declare the Tcl interface thread-safe.
	 * Meanwhile, there's no strong reason to enable DB_THREAD when not
	 * testing.
	 */
	open_flags = DB_JOINENV;

	logmaxset = logbufset = 0;

	if (objc <= 2) {
		Tcl_WrongNumArgs(interp, 2, objv, "?args?");
		return (TCL_ERROR);
	}

	/*
	 * Server code must go before the call to db_env_create.
	 */
	server = NULL;
	server_to = client_to = 0;
	i = 2;
	while (i < objc) {
		if (Tcl_GetIndexFromObj(interp, objv[i++], envopen, "option",
		    TCL_EXACT, &optindex) != TCL_OK) {
			Tcl_ResetResult(interp);
			continue;
		}
#ifdef CONFIG_TEST
		switch ((enum envopen)optindex) {
		case ENV_SERVER:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-server hostname");
				result = TCL_ERROR;
				break;
			}
			FLD_SET(cr_flags, DB_RPCCLIENT);
			server = Tcl_GetStringFromObj(objv[i++], NULL);
			break;
		case ENV_SERVER_TO:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-server_to secs");
				result = TCL_ERROR;
				break;
			}
			FLD_SET(cr_flags, DB_RPCCLIENT);
			result = Tcl_GetLongFromObj(interp, objv[i++],
			    &server_to);
			break;
		case ENV_CLIENT_TO:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-client_to secs");
				result = TCL_ERROR;
				break;
			}
			FLD_SET(cr_flags, DB_RPCCLIENT);
			result = Tcl_GetLongFromObj(interp, objv[i++],
			    &client_to);
			break;
		default:
			break;
		}
#endif
	}
	if (result != TCL_OK)
		return (TCL_ERROR);
	ret = db_env_create(env, cr_flags);
	if (ret)
		return (_ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "db_env_create"));
	/*
	 * From here on we must 'goto error' in order to clean up the
	 * env from db_env_create.
	 */
	if (server != NULL) {
		(*env)->set_errpfx((*env), ip->i_name);
		(*env)->set_errcall((*env), _ErrorFunc);
		if ((ret = (*env)->set_rpc_server((*env), NULL, server,
		    client_to, server_to, 0)) != 0) {
			result = TCL_ERROR;
			goto error;
		}
	} else {
		/*
		 * Create the environment handle before parsing the args
		 * since we'll be modifying the environment as we parse.
		 */
		(*env)->set_errpfx((*env), ip->i_name);
		(*env)->set_errcall((*env), _ErrorFunc);
	}

	/* Hang our info pointer on the env handle, so we can do callbacks. */
	(*env)->app_private = ip;

	/*
	 * Get the command name index from the object based on the bdbcmds
	 * defined above.
	 */
	i = 2;
	while (i < objc) {
		Tcl_ResetResult(interp);
		if (Tcl_GetIndexFromObj(interp, objv[i], envopen, "option",
		    TCL_EXACT, &optindex) != TCL_OK) {
			result = IS_HELP(objv[i]);
			goto error;
		}
		i++;
		switch ((enum envopen)optindex) {
#ifdef CONFIG_TEST
		case ENV_SERVER:
		case ENV_SERVER_TO:
		case ENV_CLIENT_TO:
			/*
			 * Already handled these, skip them and their arg.
			 */
			i++;
			break;
		case ENV_ALLOC:
			/*
			 * Use a Tcl-local alloc and free function so that
			 * we're sure to test whether we use umalloc/ufree in
			 * the right places.
			 */
			(void)(*env)->set_alloc(*env,
			    tcl_db_malloc, tcl_db_realloc, tcl_db_free);
			break;
		case ENV_AUTO_COMMIT:
			FLD_SET(set_flags, DB_AUTO_COMMIT);
			break;
		case ENV_CDB:
			FLD_SET(open_flags, DB_INIT_CDB | DB_INIT_MPOOL);
			FLD_CLR(open_flags, DB_JOINENV);
			break;
		case ENV_CDB_ALLDB:
			FLD_SET(set_flags, DB_CDB_ALLDB);
			break;
		case ENV_LOCK:
			FLD_SET(open_flags, DB_INIT_LOCK | DB_INIT_MPOOL);
			FLD_CLR(open_flags, DB_JOINENV);
			break;
		case ENV_CONFLICT:
			/*
			 * Get conflict list.  List is:
			 * {nmodes {matrix}}
			 *
			 * Where matrix must be nmodes*nmodes big.
			 * Set up conflicts array to pass.
			 */
			result = Tcl_ListObjGetElements(interp, objv[i],
			    &myobjc, &myobjv);
			if (result == TCL_OK)
				i++;
			else
				break;
			if (myobjc != 2) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-lock_conflict {nmodes {matrix}}?");
				result = TCL_ERROR;
				break;
			}
			result = Tcl_GetIntFromObj(interp, myobjv[0], &nmodes);
			if (result != TCL_OK)
				break;
			result = Tcl_ListObjGetElements(interp, myobjv[1],
			    &myobjc, &myobjv1);
			if (myobjc != (nmodes * nmodes)) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-lock_conflict {nmodes {matrix}}?");
				result = TCL_ERROR;
				break;
			}

			ret = __os_malloc(*env, sizeof(u_int8_t) *
			    (size_t)nmodes * (size_t)nmodes, &conflicts);
			if (ret != 0) {
				result = TCL_ERROR;
				break;
			}
			for (j = 0; j < myobjc; j++) {
				result = Tcl_GetIntFromObj(interp, myobjv1[j],
				    &temp);
				conflicts[j] = temp;
				if (result != TCL_OK) {
					__os_free(NULL, conflicts);
					break;
				}
			}
			_debug_check();
			ret = (*env)->set_lk_conflicts(*env,
			    (u_int8_t *)conflicts, nmodes);
			__os_free(NULL, conflicts);
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "set_lk_conflicts");
			break;
		case ENV_DETECT:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-lock_detect policy?");
				result = TCL_ERROR;
				break;
			}
			arg = Tcl_GetStringFromObj(objv[i++], NULL);
			if (strcmp(arg, "default") == 0)
				detect = DB_LOCK_DEFAULT;
			else if (strcmp(arg, "expire") == 0)
				detect = DB_LOCK_EXPIRE;
			else if (strcmp(arg, "maxlocks") == 0)
				detect = DB_LOCK_MAXLOCKS;
			else if (strcmp(arg, "maxwrites") == 0)
				detect = DB_LOCK_MAXWRITE;
			else if (strcmp(arg, "minlocks") == 0)
				detect = DB_LOCK_MINLOCKS;
			else if (strcmp(arg, "minwrites") == 0)
				detect = DB_LOCK_MINWRITE;
			else if (strcmp(arg, "oldest") == 0)
				detect = DB_LOCK_OLDEST;
			else if (strcmp(arg, "youngest") == 0)
				detect = DB_LOCK_YOUNGEST;
			else if (strcmp(arg, "random") == 0)
				detect = DB_LOCK_RANDOM;
			else {
				Tcl_AddErrorInfo(interp,
				    "lock_detect: illegal policy");
				result = TCL_ERROR;
				break;
			}
			_debug_check();
			ret = (*env)->set_lk_detect(*env, detect);
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "lock_detect");
			break;
		case ENV_LOCK_MAX:
		case ENV_LOCK_MAX_LOCKS:
		case ENV_LOCK_MAX_LOCKERS:
		case ENV_LOCK_MAX_OBJECTS:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-lock_max max?");
				result = TCL_ERROR;
				break;
			}
			result = _GetUInt32(interp, objv[i++], &uintarg);
			if (result == TCL_OK) {
				_debug_check();
				switch ((enum envopen)optindex) {
				case ENV_LOCK_MAX:
					ret = (*env)->set_lk_max(*env,
					    uintarg);
					break;
				case ENV_LOCK_MAX_LOCKS:
					ret = (*env)->set_lk_max_locks(*env,
					    uintarg);
					break;
				case ENV_LOCK_MAX_LOCKERS:
					ret = (*env)->set_lk_max_lockers(*env,
					    uintarg);
					break;
				case ENV_LOCK_MAX_OBJECTS:
					ret = (*env)->set_lk_max_objects(*env,
					    uintarg);
					break;
				default:
					break;
				}
				result = _ReturnSetup(interp, ret,
				    DB_RETOK_STD(ret), "lock_max");
			}
			break;
		case ENV_TXN_TIME:
		case ENV_TXN_TIMEOUT:
		case ENV_LOCK_TIMEOUT:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-txn_timestamp time?");
				result = TCL_ERROR;
				break;
			}

			if ((result = Tcl_GetLongFromObj(
			   interp, objv[i++], &v)) != TCL_OK)
				break;
			timestamp = (time_t)v;

			_debug_check();
			if ((enum envopen)optindex == ENV_TXN_TIME)
				ret =
				    (*env)->set_tx_timestamp(*env, &timestamp);
			else
				ret = (*env)->set_timeout(*env,
				    (db_timeout_t)timestamp,
				    (enum envopen)optindex == ENV_TXN_TIMEOUT ?
				    DB_SET_TXN_TIMEOUT : DB_SET_LOCK_TIMEOUT);
			result = _ReturnSetup(interp, ret,
			    DB_RETOK_STD(ret), "txn_timestamp");
			break;
		case ENV_LOG:
			FLD_SET(open_flags, DB_INIT_LOG | DB_INIT_MPOOL);
			FLD_CLR(open_flags, DB_JOINENV);
			break;
		case ENV_LOG_BUFFER:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-log_buffer size?");
				result = TCL_ERROR;
				break;
			}
			result = _GetUInt32(interp, objv[i++], &uintarg);
			if (result == TCL_OK) {
				_debug_check();
				ret = (*env)->set_lg_bsize(*env, uintarg);
				result = _ReturnSetup(interp, ret,
				    DB_RETOK_STD(ret), "log_bsize");
				logbufset = 1;
				if (logmaxset) {
					_debug_check();
					ret = (*env)->set_lg_max(*env,
					    logmaxset);
					result = _ReturnSetup(interp, ret,
					    DB_RETOK_STD(ret), "log_max");
					logmaxset = 0;
					logbufset = 0;
				}
			}
			break;
		case ENV_LOG_INMEMORY:
			FLD_SET(set_flags, DB_LOG_INMEMORY);
			break;
		case ENV_LOG_MAX:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-log_max max?");
				result = TCL_ERROR;
				break;
			}
			result = _GetUInt32(interp, objv[i++], &uintarg);
			if (result == TCL_OK && logbufset) {
				_debug_check();
				ret = (*env)->set_lg_max(*env, uintarg);
				result = _ReturnSetup(interp, ret,
				    DB_RETOK_STD(ret), "log_max");
				logbufset = 0;
			} else
				logmaxset = uintarg;
			break;
		case ENV_LOG_REGIONMAX:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-log_regionmax size?");
				result = TCL_ERROR;
				break;
			}
			result = _GetUInt32(interp, objv[i++], &uintarg);
			if (result == TCL_OK) {
				_debug_check();
				ret = (*env)->set_lg_regionmax(*env, uintarg);
				result =
				    _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
					"log_regionmax");
			}
			break;
		case ENV_LOG_REMOVE:
			FLD_SET(set_flags, DB_LOG_AUTOREMOVE);
			break;
		case ENV_MPOOL_MAX_OPENFD:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-mpool_max_openfd fd_count?");
				result = TCL_ERROR;
				break;
			}
			result = Tcl_GetIntFromObj(interp, objv[i++], &intarg);
			if (result == TCL_OK) {
				_debug_check();
				ret = (*env)->set_mp_max_openfd(*env, intarg);
				result = _ReturnSetup(interp, ret,
				    DB_RETOK_STD(ret), "mpool_max_openfd");
			}
			break;
		case ENV_MPOOL_MAX_WRITE:
			result = Tcl_ListObjGetElements(interp, objv[i],
			    &myobjc, &myobjv);
			if (result == TCL_OK)
				i++;
			else
				break;
			if (myobjc != 2) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-mpool_max_write {nwrite nsleep}?");
				result = TCL_ERROR;
				break;
			}
			result = Tcl_GetIntFromObj(interp, myobjv[0], &intarg);
			if (result != TCL_OK)
				break;
			result = Tcl_GetIntFromObj(interp, myobjv[1], &intarg2);
			if (result != TCL_OK)
				break;
			_debug_check();
			ret = (*env)->set_mp_max_write(*env, intarg, intarg2);
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "set_mp_max_write");
			break;
		case ENV_MPOOL_MMAP_SIZE:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-mpool_mmap_size size?");
				result = TCL_ERROR;
				break;
			}
			result = Tcl_GetIntFromObj(interp, objv[i++], &intarg);
			if (result == TCL_OK) {
				_debug_check();
				ret = (*env)->set_mp_mmapsize(*env,
				    (size_t)intarg);
				result = _ReturnSetup(interp, ret,
				    DB_RETOK_STD(ret), "mpool_mmap_size");
			}
			break;
		case ENV_MPOOL_NOMMAP:
			FLD_SET(set_flags, DB_NOMMAP);
			break;
		case ENV_OVERWRITE:
			FLD_SET(set_flags, DB_OVERWRITE);
			break;
		case ENV_REGION_INIT:
			_debug_check();
			ret = (*env)->set_flags(*env, DB_REGION_INIT, 1);
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "region_init");
			break;
		case ENV_SET_INTERMEDIATE_DIR:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp,
				    2, objv, "?-set_intermediate_dir mode?");
				result = TCL_ERROR;
				break;
			}
			result = Tcl_GetIntFromObj(interp, objv[i++], &intarg);
			if (result == TCL_OK) {
				_debug_check();
				ret = (*env)->
				    set_intermediate_dir(*env, intarg, 0);
				result = _ReturnSetup(interp, ret,
				    DB_RETOK_STD(ret), "set_intermediate_dir");
			}
			break;
		case ENV_REP_CLIENT:
			rep_flags = DB_REP_CLIENT;
			FLD_SET(open_flags, DB_INIT_REP);
			break;
		case ENV_REP_MASTER:
			rep_flags = DB_REP_MASTER;
			FLD_SET(open_flags, DB_INIT_REP);
			break;
		case ENV_REP_TRANSPORT:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "-rep_transport {envid sendproc}");
				result = TCL_ERROR;
				break;
			}

			/*
			 * Store the objects containing the machine ID
			 * and the procedure name.  We don't need to crack
			 * the send procedure out now, but we do convert the
			 * machine ID to an int, since set_rep_transport needs
			 * it.  Even so, it'll be easier later to deal with
			 * the Tcl_Obj *, so we save that, not the int.
			 *
			 * Note that we Tcl_IncrRefCount both objects
			 * independently;  Tcl is free to discard the list
			 * that they're bundled into.
			 */
			result = Tcl_ListObjGetElements(interp, objv[i++],
			    &myobjc, &myobjv);
			if (myobjc != 2) {
				Tcl_SetResult(interp,
				    "List must be {envid sendproc}",
				    TCL_STATIC);
				result = TCL_ERROR;
				break;
			}

			FLD_SET(open_flags, DB_INIT_REP);
			/*
			 * Check that the machine ID is an int.  Note that
			 * we do want to use GetIntFromObj;  the machine
			 * ID is explicitly an int, not a u_int32_t.
			 */
			ip->i_rep_eid = myobjv[0];
			Tcl_IncrRefCount(ip->i_rep_eid);
			result = Tcl_GetIntFromObj(interp,
			    ip->i_rep_eid, &intarg);
			if (result != TCL_OK)
				break;

			ip->i_rep_send = myobjv[1];
			Tcl_IncrRefCount(ip->i_rep_send);
			_debug_check();
			ret = (*env)->set_rep_transport(*env,
			    intarg, tcl_rep_send);
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "set_rep_transport");
			break;
		case ENV_THREAD:
			/* Enable DB_THREAD when specified in testing. */
			FLD_SET(open_flags, DB_THREAD);
			break;
		case ENV_TIME_NOTGRANTED:
			FLD_SET(set_flags, DB_TIME_NOTGRANTED);
			break;
		case ENV_VERBOSE:
			result = Tcl_ListObjGetElements(interp, objv[i],
			    &myobjc, &myobjv);
			if (result == TCL_OK)
				i++;
			else
				break;
			if (myobjc != 2) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-verbose {which on|off}?");
				result = TCL_ERROR;
				break;
			}
			result = tcl_EnvVerbose(interp, *env,
			    myobjv[0], myobjv[1]);
			break;
		case ENV_WRNOSYNC:
			FLD_SET(set_flags, DB_TXN_WRITE_NOSYNC);
			break;
#endif
		case ENV_TXN:
			FLD_SET(open_flags, DB_INIT_LOCK |
			    DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_TXN);
			FLD_CLR(open_flags, DB_JOINENV);
			/* Make sure we have an arg to check against! */
			if (i < objc) {
				arg = Tcl_GetStringFromObj(objv[i], NULL);
				if (strcmp(arg, "nosync") == 0) {
					FLD_SET(set_flags, DB_TXN_NOSYNC);
					i++;
				}
			}
			break;
		case ENV_CREATE:
			FLD_SET(open_flags, DB_CREATE | DB_INIT_MPOOL);
			FLD_CLR(open_flags, DB_JOINENV);
			break;
		case ENV_ENCRYPT_AES:
			/* Make sure we have an arg to check against! */
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-encryptaes passwd?");
				result = TCL_ERROR;
				break;
			}
			passwd = Tcl_GetStringFromObj(objv[i++], NULL);
			_debug_check();
			ret = (*env)->set_encrypt(*env, passwd, DB_ENCRYPT_AES);
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "set_encrypt");
			break;
		case ENV_ENCRYPT_ANY:
			/* Make sure we have an arg to check against! */
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-encryptany passwd?");
				result = TCL_ERROR;
				break;
			}
			passwd = Tcl_GetStringFromObj(objv[i++], NULL);
			_debug_check();
			ret = (*env)->set_encrypt(*env, passwd, 0);
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "set_encrypt");
			break;
		case ENV_HOME:
			/* Make sure we have an arg to check against! */
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-home dir?");
				result = TCL_ERROR;
				break;
			}
			home = Tcl_GetStringFromObj(objv[i++], NULL);
			break;
		case ENV_MODE:
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
		case ENV_PRIVATE:
			FLD_SET(open_flags, DB_PRIVATE | DB_INIT_MPOOL);
			FLD_CLR(open_flags, DB_JOINENV);
			break;
		case ENV_RECOVER:
			FLD_SET(open_flags, DB_RECOVER);
			break;
		case ENV_RECOVER_FATAL:
			FLD_SET(open_flags, DB_RECOVER_FATAL);
			break;
		case ENV_SYSTEM_MEM:
			FLD_SET(open_flags, DB_SYSTEM_MEM);
			break;
		case ENV_USE_ENVIRON_ROOT:
			FLD_SET(open_flags, DB_USE_ENVIRON_ROOT);
			break;
		case ENV_USE_ENVIRON:
			FLD_SET(open_flags, DB_USE_ENVIRON);
			break;
		case ENV_CACHESIZE:
			result = Tcl_ListObjGetElements(interp, objv[i],
			    &myobjc, &myobjv);
			if (result == TCL_OK)
				i++;
			else
				break;
			if (myobjc != 3) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-cachesize {gbytes bytes ncaches}?");
				result = TCL_ERROR;
				break;
			}
			result = _GetUInt32(interp, myobjv[0], &gbytes);
			if (result != TCL_OK)
				break;
			result = _GetUInt32(interp, myobjv[1], &bytes);
			if (result != TCL_OK)
				break;
			result = Tcl_GetIntFromObj(interp, myobjv[2], &ncaches);
			if (result != TCL_OK)
				break;
			_debug_check();
			ret = (*env)->set_cachesize(*env, gbytes, bytes,
			    ncaches);
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "set_cachesize");
			break;
		case ENV_SHM_KEY:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-shm_key key?");
				result = TCL_ERROR;
				break;
			}
			result = Tcl_GetLongFromObj(interp, objv[i++], &shm);
			if (result == TCL_OK) {
				_debug_check();
				ret = (*env)->set_shm_key(*env, shm);
				result = _ReturnSetup(interp, ret,
				    DB_RETOK_STD(ret), "shm_key");
			}
			break;
		case ENV_TXN_MAX:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-txn_max max?");
				result = TCL_ERROR;
				break;
			}
			result = _GetUInt32(interp, objv[i++], &uintarg);
			if (result == TCL_OK) {
				_debug_check();
				ret = (*env)->set_tx_max(*env, uintarg);
				result = _ReturnSetup(interp, ret,
				    DB_RETOK_STD(ret), "txn_max");
			}
			break;
		case ENV_ERRFILE:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "-errfile file");
				result = TCL_ERROR;
				break;
			}
			arg = Tcl_GetStringFromObj(objv[i++], NULL);
			tcl_EnvSetErrfile(interp, *env, ip, arg);
			break;
		case ENV_ERRPFX:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "-errpfx prefix");
				result = TCL_ERROR;
				break;
			}
			arg = Tcl_GetStringFromObj(objv[i++], NULL);
			_debug_check();
			result = tcl_EnvSetErrpfx(interp, *env, ip, arg);
			break;
		case ENV_DATA_DIR:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "-data_dir dir");
				result = TCL_ERROR;
				break;
			}
			arg = Tcl_GetStringFromObj(objv[i++], NULL);
			_debug_check();
			ret = (*env)->set_data_dir(*env, arg);
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "set_data_dir");
			break;
		case ENV_LOG_DIR:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "-log_dir dir");
				result = TCL_ERROR;
				break;
			}
			arg = Tcl_GetStringFromObj(objv[i++], NULL);
			_debug_check();
			ret = (*env)->set_lg_dir(*env, arg);
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "set_lg_dir");
			break;
		case ENV_TMP_DIR:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "-tmp_dir dir");
				result = TCL_ERROR;
				break;
			}
			arg = Tcl_GetStringFromObj(objv[i++], NULL);
			_debug_check();
			ret = (*env)->set_tmp_dir(*env, arg);
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "set_tmp_dir");
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
	 * We have to check this here.  We want to set the log buffer
	 * size first, if it is specified.  So if the user did so,
	 * then we took care of it above.  But, if we get out here and
	 * logmaxset is non-zero, then they set the log_max without
	 * resetting the log buffer size, so we now have to do the
	 * call to set_lg_max, since we didn't do it above.
	 */
	if (logmaxset) {
		_debug_check();
		ret = (*env)->set_lg_max(*env, (u_int32_t)logmaxset);
		result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "log_max");
	}

	if (result != TCL_OK)
		goto error;

	if (set_flags) {
		ret = (*env)->set_flags(*env, set_flags, 1);
		result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "set_flags");
		if (result == TCL_ERROR)
			goto error;
		/*
		 * If we are successful, clear the result so that the
		 * return from set_flags isn't part of the result.
		 */
		Tcl_ResetResult(interp);
	}
	/*
	 * When we get here, we have already parsed all of our args
	 * and made all our calls to set up the environment.  Everything
	 * is okay so far, no errors, if we get here.
	 *
	 * Now open the environment.
	 */
	_debug_check();
	ret = (*env)->open(*env, home, open_flags, mode);
	result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret), "env open");

	if (rep_flags != 0 && result == TCL_OK) {
		_debug_check();
		ret = (*env)->rep_start(*env, NULL, rep_flags);
		result = _ReturnSetup(interp,
		    ret, DB_RETOK_STD(ret), "rep_start");
	}

error:	if (result == TCL_ERROR) {
		if (ip->i_err && ip->i_err != stdout && ip->i_err != stderr) {
			(void)fclose(ip->i_err);
			ip->i_err = NULL;
		}
		(void)(*env)->close(*env, 0);
		*env = NULL;
	}
	return (result);
}

/*
 * bdb_DbOpen --
 *	Implements the "db_create/db_open" command.
 *	There are many, many options to the open command.
 *	Here is the general flow:
 *
 *	0.  Preparse args to determine if we have -env.
 *	1.  Call db_create to create the db handle.
 *	2.  Parse args tracking options.
 *	3.  Make any pre-open setup calls necessary.
 *	4.  Call DB->open to open the database.
 *	5.  Return db widget handle to user.
 */
static int
bdb_DbOpen(interp, objc, objv, ip, dbp)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DBTCL_INFO *ip;			/* Our internal info */
	DB **dbp;			/* DB handle */
{
	static const char *bdbenvopen[] = {
		"-env",	NULL
	};
	enum bdbenvopen {
		TCL_DB_ENV0
	};
	static const char *bdbopen[] = {
#ifdef CONFIG_TEST
		"-btcompare",
		"-dirty",
		"-dupcompare",
		"-hashproc",
		"-lorder",
		"-minkey",
		"-nommap",
		"-notdurable",
		"-revsplitoff",
		"-test",
		"-thread",
#endif
		"-auto_commit",
		"-btree",
		"-cachesize",
		"-chksum",
		"-create",
		"-delim",
		"-dup",
		"-dupsort",
		"-encrypt",
		"-encryptaes",
		"-encryptany",
		"-env",
		"-errfile",
		"-errpfx",
		"-excl",
		"-extent",
		"-ffactor",
		"-hash",
		"-inorder",
		"-len",
		"-maxsize",
		"-mode",
		"-nelem",
		"-pad",
		"-pagesize",
		"-queue",
		"-rdonly",
		"-recno",
		"-recnum",
		"-renumber",
		"-snapshot",
		"-source",
		"-truncate",
		"-txn",
		"-unknown",
		"--",
		NULL
	};
	enum bdbopen {
#ifdef CONFIG_TEST
		TCL_DB_BTCOMPARE,
		TCL_DB_DIRTY,
		TCL_DB_DUPCOMPARE,
		TCL_DB_HASHPROC,
		TCL_DB_LORDER,
		TCL_DB_MINKEY,
		TCL_DB_NOMMAP,
		TCL_DB_NOTDURABLE,
		TCL_DB_REVSPLIT,
		TCL_DB_TEST,
		TCL_DB_THREAD,
#endif
		TCL_DB_AUTO_COMMIT,
		TCL_DB_BTREE,
		TCL_DB_CACHESIZE,
		TCL_DB_CHKSUM,
		TCL_DB_CREATE,
		TCL_DB_DELIM,
		TCL_DB_DUP,
		TCL_DB_DUPSORT,
		TCL_DB_ENCRYPT,
		TCL_DB_ENCRYPT_AES,
		TCL_DB_ENCRYPT_ANY,
		TCL_DB_ENV,
		TCL_DB_ERRFILE,
		TCL_DB_ERRPFX,
		TCL_DB_EXCL,
		TCL_DB_EXTENT,
		TCL_DB_FFACTOR,
		TCL_DB_HASH,
		TCL_DB_INORDER,
		TCL_DB_LEN,
		TCL_DB_MAXSIZE,
		TCL_DB_MODE,
		TCL_DB_NELEM,
		TCL_DB_PAD,
		TCL_DB_PAGESIZE,
		TCL_DB_QUEUE,
		TCL_DB_RDONLY,
		TCL_DB_RECNO,
		TCL_DB_RECNUM,
		TCL_DB_RENUMBER,
		TCL_DB_SNAPSHOT,
		TCL_DB_SOURCE,
		TCL_DB_TRUNCATE,
		TCL_DB_TXN,
		TCL_DB_UNKNOWN,
		TCL_DB_ENDARG
	};

	DBTCL_INFO *envip, *errip;
	DB_TXN *txn;
	DBTYPE type;
	DB_ENV *envp;
	Tcl_Obj **myobjv;
	u_int32_t gbytes, bytes, open_flags, set_flags, uintarg;
	int endarg, i, intarg, mode, myobjc, ncaches;
	int optindex, result, ret, set_err, set_pfx, subdblen;
	u_char *subdbtmp;
	char *arg, *db, *passwd, *subdb, msg[MSG_SIZE];

	type = DB_UNKNOWN;
	endarg = mode = set_err = set_flags = set_pfx = 0;
	result = TCL_OK;
	subdbtmp = NULL;
	db = subdb = NULL;

	/*
	 * XXX
	 * If/when our Tcl interface becomes thread-safe, we should enable
	 * DB_THREAD here in all cases.  For now, we turn it on later in this
	 * function, and only when we're in testing and we specify the
	 * -thread flag, so that we can exercise MUTEX_THREAD_LOCK cases.
	 *
	 * In order to become truly thread-safe, we need to look at making sure
	 * DBTCL_INFO structs are safe to share across threads (they're not
	 * mutex-protected) before we declare the Tcl interface thread-safe.
	 * Meanwhile, there's no strong reason to enable DB_THREAD when not
	 * testing.
	 */
	open_flags = 0;

	envp = NULL;
	txn = NULL;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 2, objv, "?args?");
		return (TCL_ERROR);
	}

	/*
	 * We must first parse for the environment flag, since that
	 * is needed for db_create.  Then create the db handle.
	 */
	i = 2;
	while (i < objc) {
		if (Tcl_GetIndexFromObj(interp, objv[i++], bdbenvopen,
		    "option", TCL_EXACT, &optindex) != TCL_OK) {
			/*
			 * Reset the result so we don't get
			 * an errant error message if there is another error.
			 */
			Tcl_ResetResult(interp);
			continue;
		}
		switch ((enum bdbenvopen)optindex) {
		case TCL_DB_ENV0:
			arg = Tcl_GetStringFromObj(objv[i], NULL);
			envp = NAME_TO_ENV(arg);
			if (envp == NULL) {
				Tcl_SetResult(interp,
				    "db open: illegal environment", TCL_STATIC);
				return (TCL_ERROR);
			}
		}
		break;
	}

	/*
	 * Create the db handle before parsing the args
	 * since we'll be modifying the database options as we parse.
	 */
	ret = db_create(dbp, envp, 0);
	if (ret)
		return (_ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "db_create"));

	/* Hang our info pointer on the DB handle, so we can do callbacks. */
	(*dbp)->api_internal = ip;

	/*
	 * XXX Remove restriction when err stuff is not tied to env.
	 *
	 * The DB->set_err* functions actually overwrite in the
	 * environment.  So, if we are explicitly using an env,
	 * don't overwrite what we have already set up.  If we are
	 * not using one, then we set up since we get a private
	 * default env.
	 */
	/* XXX  - remove this conditional if/when err is not tied to env */
	if (envp == NULL) {
		(*dbp)->set_errpfx((*dbp), ip->i_name);
		(*dbp)->set_errcall((*dbp), _ErrorFunc);
	}
	envip = _PtrToInfo(envp); /* XXX */
	/*
	 * If we are using an env, we keep track of err info in the env's ip.
	 * Otherwise use the DB's ip.
	 */
	if (envip)
		errip = envip;
	else
		errip = ip;
	/*
	 * Get the option name index from the object based on the args
	 * defined above.
	 */
	i = 2;
	while (i < objc) {
		Tcl_ResetResult(interp);
		if (Tcl_GetIndexFromObj(interp, objv[i], bdbopen, "option",
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
		switch ((enum bdbopen)optindex) {
#ifdef CONFIG_TEST
		case TCL_DB_BTCOMPARE:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "-btcompare compareproc");
				result = TCL_ERROR;
				break;
			}

			/*
			 * Store the object containing the procedure name.
			 * We don't need to crack it out now--we'll want
			 * to bundle it up to pass into Tcl_EvalObjv anyway.
			 * Tcl's object refcounting will--I hope--take care
			 * of the memory management here.
			 */
			ip->i_btcompare = objv[i++];
			Tcl_IncrRefCount(ip->i_btcompare);
			_debug_check();
			ret = (*dbp)->set_bt_compare(*dbp, tcl_bt_compare);
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "set_bt_compare");
			break;
		case TCL_DB_DIRTY:
			open_flags |= DB_DIRTY_READ;
			break;
		case TCL_DB_DUPCOMPARE:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "-dupcompare compareproc");
				result = TCL_ERROR;
				break;
			}

			/*
			 * Store the object containing the procedure name.
			 * See TCL_DB_BTCOMPARE.
			 */
			ip->i_dupcompare = objv[i++];
			Tcl_IncrRefCount(ip->i_dupcompare);
			_debug_check();
			ret = (*dbp)->set_dup_compare(*dbp, tcl_dup_compare);
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "set_dup_compare");
			break;
		case TCL_DB_HASHPROC:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "-hashproc hashproc");
				result = TCL_ERROR;
				break;
			}

			/*
			 * Store the object containing the procedure name.
			 * See TCL_DB_BTCOMPARE.
			 */
			ip->i_hashproc = objv[i++];
			Tcl_IncrRefCount(ip->i_hashproc);
			_debug_check();
			ret = (*dbp)->set_h_hash(*dbp, tcl_h_hash);
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "set_h_hash");
			break;
		case TCL_DB_LORDER:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "-lorder 1234|4321");
				result = TCL_ERROR;
				break;
			}
			result = Tcl_GetIntFromObj(interp, objv[i++], &intarg);
			if (result == TCL_OK) {
				_debug_check();
				ret = (*dbp)->set_lorder(*dbp, intarg);
				result = _ReturnSetup(interp, ret,
				    DB_RETOK_STD(ret), "set_lorder");
			}
			break;
		case TCL_DB_MINKEY:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "-minkey minkey");
				result = TCL_ERROR;
				break;
			}
			result = _GetUInt32(interp, objv[i++], &uintarg);
			if (result == TCL_OK) {
				_debug_check();
				ret = (*dbp)->set_bt_minkey(*dbp, uintarg);
				result = _ReturnSetup(interp, ret,
				    DB_RETOK_STD(ret), "set_bt_minkey");
			}
			break;
		case TCL_DB_NOMMAP:
			open_flags |= DB_NOMMAP;
			break;
		case TCL_DB_NOTDURABLE:
			set_flags |= DB_TXN_NOT_DURABLE;
			break;
		case TCL_DB_REVSPLIT:
			set_flags |= DB_REVSPLITOFF;
			break;
		case TCL_DB_TEST:
			ret = (*dbp)->set_h_hash(*dbp, __ham_test);
			result = _ReturnSetup(interp, ret,
			    DB_RETOK_STD(ret), "set_h_hash");
			break;
		case TCL_DB_THREAD:
			/* Enable DB_THREAD when specified in testing. */
			open_flags |= DB_THREAD;
			break;
#endif
		case TCL_DB_AUTO_COMMIT:
			open_flags |= DB_AUTO_COMMIT;
			break;
		case TCL_DB_ENV:
			/*
			 * Already parsed this, skip it and the env pointer.
			 */
			i++;
			continue;
		case TCL_DB_TXN:
			if (i > (objc - 1)) {
				Tcl_WrongNumArgs(interp, 2, objv, "?-txn id?");
				result = TCL_ERROR;
				break;
			}
			arg = Tcl_GetStringFromObj(objv[i++], NULL);
			txn = NAME_TO_TXN(arg);
			if (txn == NULL) {
				snprintf(msg, MSG_SIZE,
				    "Open: Invalid txn: %s\n", arg);
				Tcl_SetResult(interp, msg, TCL_VOLATILE);
				result = TCL_ERROR;
			}
			break;
		case TCL_DB_BTREE:
			if (type != DB_UNKNOWN) {
				Tcl_SetResult(interp,
				    "Too many DB types specified", TCL_STATIC);
				result = TCL_ERROR;
				goto error;
			}
			type = DB_BTREE;
			break;
		case TCL_DB_HASH:
			if (type != DB_UNKNOWN) {
				Tcl_SetResult(interp,
				    "Too many DB types specified", TCL_STATIC);
				result = TCL_ERROR;
				goto error;
			}
			type = DB_HASH;
			break;
		case TCL_DB_RECNO:
			if (type != DB_UNKNOWN) {
				Tcl_SetResult(interp,
				    "Too many DB types specified", TCL_STATIC);
				result = TCL_ERROR;
				goto error;
			}
			type = DB_RECNO;
			break;
		case TCL_DB_QUEUE:
			if (type != DB_UNKNOWN) {
				Tcl_SetResult(interp,
				    "Too many DB types specified", TCL_STATIC);
				result = TCL_ERROR;
				goto error;
			}
			type = DB_QUEUE;
			break;
		case TCL_DB_UNKNOWN:
			if (type != DB_UNKNOWN) {
				Tcl_SetResult(interp,
				    "Too many DB types specified", TCL_STATIC);
				result = TCL_ERROR;
				goto error;
			}
			break;
		case TCL_DB_CREATE:
			open_flags |= DB_CREATE;
			break;
		case TCL_DB_EXCL:
			open_flags |= DB_EXCL;
			break;
		case TCL_DB_RDONLY:
			open_flags |= DB_RDONLY;
			break;
		case TCL_DB_TRUNCATE:
			open_flags |= DB_TRUNCATE;
			break;
		case TCL_DB_MODE:
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
		case TCL_DB_DUP:
			set_flags |= DB_DUP;
			break;
		case TCL_DB_DUPSORT:
			set_flags |= DB_DUPSORT;
			break;
		case TCL_DB_INORDER:
			set_flags |= DB_INORDER;
			break;
		case TCL_DB_RECNUM:
			set_flags |= DB_RECNUM;
			break;
		case TCL_DB_RENUMBER:
			set_flags |= DB_RENUMBER;
			break;
		case TCL_DB_SNAPSHOT:
			set_flags |= DB_SNAPSHOT;
			break;
		case TCL_DB_CHKSUM:
			set_flags |= DB_CHKSUM;
			break;
		case TCL_DB_ENCRYPT:
			set_flags |= DB_ENCRYPT;
			break;
		case TCL_DB_ENCRYPT_AES:
			/* Make sure we have an arg to check against! */
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-encryptaes passwd?");
				result = TCL_ERROR;
				break;
			}
			passwd = Tcl_GetStringFromObj(objv[i++], NULL);
			_debug_check();
			ret = (*dbp)->set_encrypt(*dbp, passwd, DB_ENCRYPT_AES);
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "set_encrypt");
			break;
		case TCL_DB_ENCRYPT_ANY:
			/* Make sure we have an arg to check against! */
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-encryptany passwd?");
				result = TCL_ERROR;
				break;
			}
			passwd = Tcl_GetStringFromObj(objv[i++], NULL);
			_debug_check();
			ret = (*dbp)->set_encrypt(*dbp, passwd, 0);
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "set_encrypt");
			break;
		case TCL_DB_FFACTOR:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "-ffactor density");
				result = TCL_ERROR;
				break;
			}
			result = _GetUInt32(interp, objv[i++], &uintarg);
			if (result == TCL_OK) {
				_debug_check();
				ret = (*dbp)->set_h_ffactor(*dbp, uintarg);
				result = _ReturnSetup(interp, ret,
				    DB_RETOK_STD(ret), "set_h_ffactor");
			}
			break;
		case TCL_DB_NELEM:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "-nelem nelem");
				result = TCL_ERROR;
				break;
			}
			result = _GetUInt32(interp, objv[i++], &uintarg);
			if (result == TCL_OK) {
				_debug_check();
				ret = (*dbp)->set_h_nelem(*dbp, uintarg);
				result = _ReturnSetup(interp, ret,
				    DB_RETOK_STD(ret), "set_h_nelem");
			}
			break;
		case TCL_DB_DELIM:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "-delim delim");
				result = TCL_ERROR;
				break;
			}
			result = Tcl_GetIntFromObj(interp, objv[i++], &intarg);
			if (result == TCL_OK) {
				_debug_check();
				ret = (*dbp)->set_re_delim(*dbp, intarg);
				result = _ReturnSetup(interp, ret,
				    DB_RETOK_STD(ret), "set_re_delim");
			}
			break;
		case TCL_DB_LEN:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "-len length");
				result = TCL_ERROR;
				break;
			}
			result = _GetUInt32(interp, objv[i++], &uintarg);
			if (result == TCL_OK) {
				_debug_check();
				ret = (*dbp)->set_re_len(*dbp, uintarg);
				result = _ReturnSetup(interp, ret,
				    DB_RETOK_STD(ret), "set_re_len");
			}
			break;
		case TCL_DB_MAXSIZE:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "-len length");
				result = TCL_ERROR;
				break;
			}
			result = _GetUInt32(interp, objv[i++], &uintarg);
			if (result == TCL_OK) {
				_debug_check();
				ret = (*dbp)->mpf->set_maxsize(
				    (*dbp)->mpf, 0, uintarg);
				result = _ReturnSetup(interp, ret,
				    DB_RETOK_STD(ret), "set_re_len");
			}
			break;
		case TCL_DB_PAD:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "-pad pad");
				result = TCL_ERROR;
				break;
			}
			result = Tcl_GetIntFromObj(interp, objv[i++], &intarg);
			if (result == TCL_OK) {
				_debug_check();
				ret = (*dbp)->set_re_pad(*dbp, intarg);
				result = _ReturnSetup(interp, ret,
				    DB_RETOK_STD(ret), "set_re_pad");
			}
			break;
		case TCL_DB_SOURCE:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "-source file");
				result = TCL_ERROR;
				break;
			}
			arg = Tcl_GetStringFromObj(objv[i++], NULL);
			_debug_check();
			ret = (*dbp)->set_re_source(*dbp, arg);
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "set_re_source");
			break;
		case TCL_DB_EXTENT:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "-extent size");
				result = TCL_ERROR;
				break;
			}
			result = _GetUInt32(interp, objv[i++], &uintarg);
			if (result == TCL_OK) {
				_debug_check();
				ret = (*dbp)->set_q_extentsize(*dbp, uintarg);
				result = _ReturnSetup(interp, ret,
				    DB_RETOK_STD(ret), "set_q_extentsize");
			}
			break;
		case TCL_DB_CACHESIZE:
			result = Tcl_ListObjGetElements(interp, objv[i++],
			    &myobjc, &myobjv);
			if (result != TCL_OK)
				break;
			if (myobjc != 3) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-cachesize {gbytes bytes ncaches}?");
				result = TCL_ERROR;
				break;
			}
			result = _GetUInt32(interp, myobjv[0], &gbytes);
			if (result != TCL_OK)
				break;
			result = _GetUInt32(interp, myobjv[1], &bytes);
			if (result != TCL_OK)
				break;
			result = Tcl_GetIntFromObj(interp, myobjv[2], &ncaches);
			if (result != TCL_OK)
				break;
			_debug_check();
			ret = (*dbp)->set_cachesize(*dbp, gbytes, bytes,
			    ncaches);
			result = _ReturnSetup(interp, ret,
			    DB_RETOK_STD(ret), "set_cachesize");
			break;
		case TCL_DB_PAGESIZE:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-pagesize size?");
				result = TCL_ERROR;
				break;
			}
			result = Tcl_GetIntFromObj(interp, objv[i++], &intarg);
			if (result == TCL_OK) {
				_debug_check();
				ret = (*dbp)->set_pagesize(*dbp,
				    (size_t)intarg);
				result = _ReturnSetup(interp, ret,
				    DB_RETOK_STD(ret), "set pagesize");
			}
			break;
		case TCL_DB_ERRFILE:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "-errfile file");
				result = TCL_ERROR;
				break;
			}
			arg = Tcl_GetStringFromObj(objv[i++], NULL);
			/*
			 * If the user already set one, close it.
			 */
			if (errip->i_err != NULL &&
			    errip->i_err != stdout && errip->i_err != stderr)
				(void)fclose(errip->i_err);
			if (strcmp(arg, "/dev/stdout") == 0)
				errip->i_err = stdout;
			else if (strcmp(arg, "/dev/stderr") == 0)
				errip->i_err = stderr;
			else
				errip->i_err = fopen(arg, "a");
			if (errip->i_err != NULL) {
				_debug_check();
				(*dbp)->set_errfile(*dbp, errip->i_err);
				set_err = 1;
			}
			break;
		case TCL_DB_ERRPFX:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "-errpfx prefix");
				result = TCL_ERROR;
				break;
			}
			arg = Tcl_GetStringFromObj(objv[i++], NULL);
			/*
			 * If the user already set one, free it.
			 */
			if (errip->i_errpfx != NULL)
				__os_free(NULL, errip->i_errpfx);
			if ((ret = __os_strdup((*dbp)->dbenv,
			    arg, &errip->i_errpfx)) != 0) {
				result = _ReturnSetup(interp, ret,
				    DB_RETOK_STD(ret), "__os_strdup");
				break;
			}
			if (errip->i_errpfx != NULL) {
				_debug_check();
				(*dbp)->set_errpfx(*dbp, errip->i_errpfx);
				set_pfx = 1;
			}
			break;
		case TCL_DB_ENDARG:
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
	 * Any args we have left, (better be 0, 1 or 2 left) are
	 * file names.  If we have 0, then an in-memory db.  If
	 * there is 1, a db name, if 2 a db and subdb name.
	 */
	if (i != objc) {
		/*
		 * Dbs must be NULL terminated file names, but subdbs can
		 * be anything.  Use Strings for the db name and byte
		 * arrays for the subdb.
		 */
		db = Tcl_GetStringFromObj(objv[i++], NULL);
		if (i != objc) {
			subdbtmp =
			    Tcl_GetByteArrayFromObj(objv[i++], &subdblen);
			if ((ret = __os_malloc(envp,
			   (size_t)subdblen + 1, &subdb)) != 0) {
				Tcl_SetResult(interp, db_strerror(ret),
				    TCL_STATIC);
				return (0);
			}
			memcpy(subdb, subdbtmp, (size_t)subdblen);
			subdb[subdblen] = '\0';
		}
	}
	if (set_flags) {
		ret = (*dbp)->set_flags(*dbp, set_flags);
		result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "set_flags");
		if (result == TCL_ERROR)
			goto error;
		/*
		 * If we are successful, clear the result so that the
		 * return from set_flags isn't part of the result.
		 */
		Tcl_ResetResult(interp);
	}

	/*
	 * When we get here, we have already parsed all of our args and made
	 * all our calls to set up the database.  Everything is okay so far,
	 * no errors, if we get here.
	 */
	_debug_check();

	/* Open the database. */
	ret = (*dbp)->open(*dbp, txn, db, subdb, type, open_flags, mode);
	result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret), "db open");

error:
	if (subdb)
		__os_free(envp, subdb);
	if (result == TCL_ERROR) {
		(void)(*dbp)->close(*dbp, 0);
		/*
		 * If we opened and set up the error file in the environment
		 * on this open, but we failed for some other reason, clean
		 * up and close the file.
		 *
		 * XXX when err stuff isn't tied to env, change to use ip,
		 * instead of envip.  Also, set_err is irrelevant when that
		 * happens.  It will just read:
		 * if (ip->i_err)
		 *	fclose(ip->i_err);
		 */
		if (set_err && errip && errip->i_err != NULL &&
		    errip->i_err != stdout && errip->i_err != stderr) {
			(void)fclose(errip->i_err);
			errip->i_err = NULL;
		}
		if (set_pfx && errip && errip->i_errpfx != NULL) {
			__os_free(envp, errip->i_errpfx);
			errip->i_errpfx = NULL;
		}
		*dbp = NULL;
	}
	return (result);
}

#ifdef HAVE_SEQUENCE
/*
 * bdb_SeqOpen --
 *	Implements the "Seq_create/Seq_open" command.
 */
static int
bdb_SeqOpen(interp, objc, objv, ip, seqp)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
	DBTCL_INFO *ip;			/* Our internal info */
	DB_SEQUENCE **seqp;		/* DB_SEQUENCE handle */
{
	static const char *seqopen[] = {
		"-auto_commit",
		"-cachesize",
		"-create",
		"-inc",
		"-init",
		"-dec",
		"-max",
		"-min",
		"-txn",
		"-wrap",
		"--",
		NULL
	} ;
	enum seqopen {
		TCL_SEQ_AUTO_COMMIT,
		TCL_SEQ_CACHESIZE,
		TCL_SEQ_CREATE,
		TCL_SEQ_INC,
		TCL_SEQ_INIT,
		TCL_SEQ_DEC,
		TCL_SEQ_MAX,
		TCL_SEQ_MIN,
		TCL_SEQ_TXN,
		TCL_SEQ_WRAP,
		TCL_SEQ_ENDARG
	};
	DB *dbp;
	DBT key;
	DBTYPE type;
	DB_TXN *txn;
	db_recno_t recno;
	db_seq_t min, max, value;
	u_int32_t flags, oflags;
	int cache, endarg, i, optindex, result, ret, setrange, setvalue, v;
	char *arg, *db, msg[MSG_SIZE];

	COMPQUIET(ip, NULL);
	COMPQUIET(value, 0);

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 2, objv, "?args?");
		return (TCL_ERROR);
	}

	txn = NULL;
	endarg = 0;
	flags = oflags = 0;
	setrange = setvalue = 0;
	min = INT64_MIN;
	max = INT64_MAX;
	cache = 0;

	for (i = 2; i < objc;) {
		Tcl_ResetResult(interp);
		if (Tcl_GetIndexFromObj(interp, objv[i], seqopen, "option",
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
		result = TCL_OK;
		switch ((enum seqopen)optindex) {
		case TCL_SEQ_AUTO_COMMIT:
			oflags |= DB_AUTO_COMMIT;
			break;
		case TCL_SEQ_CREATE:
			oflags |= DB_CREATE;
			break;
		case TCL_SEQ_INC:
			LF_SET(DB_SEQ_INC);
			break;
		case TCL_SEQ_CACHESIZE:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-cachesize value?");
				result = TCL_ERROR;
				break;
			}
			result = Tcl_GetIntFromObj(interp, objv[i++], &cache);
			break;
		case TCL_SEQ_INIT:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-init value?");
				result = TCL_ERROR;
				break;
			}
			result =
			     Tcl_GetWideIntFromObj(interp, objv[i++], &value);
			setvalue = 1;
			break;
		case TCL_SEQ_DEC:
			LF_SET(DB_SEQ_DEC);
			break;
		case TCL_SEQ_MAX:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-max value?");
				result = TCL_ERROR;
				break;
			}
			if ((result =
			     Tcl_GetWideIntFromObj(interp,
			     objv[i++], &max)) != TCL_OK)
				goto error;
			setrange = 1;
			break;
		case TCL_SEQ_MIN:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "?-min value?");
				result = TCL_ERROR;
				break;
			}
			if ((result =
			     Tcl_GetWideIntFromObj(interp,
			     objv[i++], &min)) != TCL_OK)
				goto error;
			setrange = 1;
			break;
		case TCL_SEQ_TXN:
			if (i > (objc - 1)) {
				Tcl_WrongNumArgs(interp, 2, objv, "?-txn id?");
				result = TCL_ERROR;
				break;
			}
			arg = Tcl_GetStringFromObj(objv[i++], NULL);
			txn = NAME_TO_TXN(arg);
			if (txn == NULL) {
				snprintf(msg, MSG_SIZE,
				    "Sequence: Invalid txn: %s\n", arg);
				Tcl_SetResult(interp, msg, TCL_VOLATILE);
				result = TCL_ERROR;
			}
			break;
		case TCL_SEQ_WRAP:
			LF_SET(DB_SEQ_WRAP);
			break;
		case TCL_SEQ_ENDARG:
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

	if (objc - i != 2) {
		Tcl_WrongNumArgs(interp, 2, objv, "?args?");
		return (TCL_ERROR);
	}
	/*
	 * The db must be a string but the sequence key may
	 * be anything.
	 */
	db = Tcl_GetStringFromObj(objv[i++], NULL);
	if ((dbp = NAME_TO_DB(db)) == NULL) {
		Tcl_SetResult(interp, "No such dbp", TCL_STATIC);
		return (TCL_ERROR);
	}
	(void)dbp->get_type(dbp, &type);

	memset(&key, 0, sizeof(key));
	if (type == DB_QUEUE || type == DB_RECNO) {
		result = _GetUInt32(interp, objv[i++], &recno);
		if (result != TCL_OK)
			return (result);
		key.data = &recno;
		key.size = sizeof(recno);
	} else {
		key.data = Tcl_GetByteArrayFromObj(objv[i++], &v);
		key.size = (u_int32_t)v;
	}
	ret = db_sequence_create(seqp, dbp, 0);
	if ((result = _ReturnSetup(interp,
	    ret, DB_RETOK_STD(ret), "sequence create")) != TCL_OK) {
		*seqp = NULL;
		return (result);
	}

	ret = (*seqp)->set_flags(*seqp, flags);
	if ((result = _ReturnSetup(interp,
	    ret, DB_RETOK_STD(ret), "sequence set_flags")) != TCL_OK)
		goto error;
	if (setrange) {
		ret = (*seqp)->set_range(*seqp, min, max);
		if ((result = _ReturnSetup(interp,
		    ret, DB_RETOK_STD(ret), "sequence set_range")) != TCL_OK)
			goto error;
	}
	if (cache) {
		ret = (*seqp)->set_cachesize(*seqp, cache);
		if ((result = _ReturnSetup(interp,
		    ret, DB_RETOK_STD(ret), "sequence cachesize")) != TCL_OK)
			goto error;
	}
	if (setvalue) {
		ret = (*seqp)->initial_value(*seqp, value);
		if ((result = _ReturnSetup(interp,
		    ret, DB_RETOK_STD(ret), "sequence init")) != TCL_OK)
			goto error;
	}
	ret = (*seqp)->open(*seqp, txn, &key, oflags);
	if ((result = _ReturnSetup(interp,
	    ret, DB_RETOK_STD(ret), "sequence open")) != TCL_OK)
		goto error;

	if (0) {
error:		(void)(*seqp)->close(*seqp, 0);
		*seqp = NULL;
	}
	return (result);
}
#endif

/*
 * bdb_DbRemove --
 *	Implements the DB_ENV->remove and DB->remove command.
 */
static int
bdb_DbRemove(interp, objc, objv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
{
	static const char *bdbrem[] = {
		"-auto_commit",
		"-encrypt",
		"-encryptaes",
		"-encryptany",
		"-env",
		"-txn",
		"--",
		NULL
	};
	enum bdbrem {
		TCL_DBREM_AUTOCOMMIT,
		TCL_DBREM_ENCRYPT,
		TCL_DBREM_ENCRYPT_AES,
		TCL_DBREM_ENCRYPT_ANY,
		TCL_DBREM_ENV,
		TCL_DBREM_TXN,
		TCL_DBREM_ENDARG
	};
	DB *dbp;
	DB_ENV *envp;
	DB_TXN *txn;
	int endarg, i, optindex, result, ret, subdblen;
	u_int32_t enc_flag, iflags, set_flags;
	u_char *subdbtmp;
	char *arg, *db, msg[MSG_SIZE], *passwd, *subdb;

	db = subdb = NULL;
	dbp = NULL;
	endarg = 0;
	envp = NULL;
	iflags = enc_flag = set_flags = 0;
	passwd = NULL;
	result = TCL_OK;
	subdbtmp = NULL;
	txn = NULL;

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
		if (Tcl_GetIndexFromObj(interp, objv[i], bdbrem,
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
		switch ((enum bdbrem)optindex) {
		case TCL_DBREM_AUTOCOMMIT:
			iflags |= DB_AUTO_COMMIT;
			_debug_check();
			break;
		case TCL_DBREM_ENCRYPT:
			set_flags |= DB_ENCRYPT;
			_debug_check();
			break;
		case TCL_DBREM_ENCRYPT_AES:
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
		case TCL_DBREM_ENCRYPT_ANY:
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
		case TCL_DBREM_ENV:
			arg = Tcl_GetStringFromObj(objv[i++], NULL);
			envp = NAME_TO_ENV(arg);
			if (envp == NULL) {
				Tcl_SetResult(interp,
				    "db remove: illegal environment",
				    TCL_STATIC);
				return (TCL_ERROR);
			}
			break;
		case TCL_DBREM_ENDARG:
			endarg = 1;
			break;
		case TCL_DBREM_TXN:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv, "?-txn id?");
				result = TCL_ERROR;
				break;
			}
			arg = Tcl_GetStringFromObj(objv[i++], NULL);
			txn = NAME_TO_TXN(arg);
			if (txn == NULL) {
				snprintf(msg, MSG_SIZE,
				    "Put: Invalid txn: %s\n", arg);
				Tcl_SetResult(interp, msg, TCL_VOLATILE);
				result = TCL_ERROR;
			}
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
			if ((ret = __os_malloc(envp, (size_t)subdblen + 1,
			    &subdb)) != 0) { Tcl_SetResult(interp,
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
	if (envp == NULL) {
		ret = db_create(&dbp, envp, 0);
		if (ret) {
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "db_create");
			goto error;
		}

		if (passwd != NULL) {
			ret = dbp->set_encrypt(dbp, passwd, enc_flag);
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "set_encrypt");
		}
		if (set_flags != 0) {
			ret = dbp->set_flags(dbp, set_flags);
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "set_flags");
		}
	}

	/*
	 * The dbremove method is a destructor, NULL out the dbp.
	 */
	_debug_check();
	if (dbp == NULL)
		ret = envp->dbremove(envp, txn, db, subdb, iflags);
	else
		ret = dbp->remove(dbp, db, subdb, 0);

	result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret), "db remove");
	dbp = NULL;
error:
	if (subdb)
		__os_free(envp, subdb);
	if (result == TCL_ERROR && dbp != NULL)
		(void)dbp->close(dbp, 0);
	return (result);
}

/*
 * bdb_DbRename --
 *	Implements the DB_ENV->dbrename and DB->rename commands.
 */
static int
bdb_DbRename(interp, objc, objv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
{
	static const char *bdbmv[] = {
		"-auto_commit",
		"-encrypt",
		"-encryptaes",
		"-encryptany",
		"-env",
		"-txn",
		"--",
		NULL
	};
	enum bdbmv {
		TCL_DBMV_AUTOCOMMIT,
		TCL_DBMV_ENCRYPT,
		TCL_DBMV_ENCRYPT_AES,
		TCL_DBMV_ENCRYPT_ANY,
		TCL_DBMV_ENV,
		TCL_DBMV_TXN,
		TCL_DBMV_ENDARG
	};
	DB *dbp;
	DB_ENV *envp;
	DB_TXN *txn;
	u_int32_t enc_flag, iflags, set_flags;
	int endarg, i, newlen, optindex, result, ret, subdblen;
	u_char *subdbtmp;
	char *arg, *db, msg[MSG_SIZE], *newname, *passwd, *subdb;

	db = newname = subdb = NULL;
	dbp = NULL;
	endarg = 0;
	envp = NULL;
	iflags = enc_flag = set_flags = 0;
	passwd = NULL;
	result = TCL_OK;
	subdbtmp = NULL;
	txn = NULL;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp,
			3, objv, "?args? filename ?database? ?newname?");
		return (TCL_ERROR);
	}

	/*
	 * We must first parse for the environment flag, since that
	 * is needed for db_create.  Then create the db handle.
	 */
	i = 2;
	while (i < objc) {
		if (Tcl_GetIndexFromObj(interp, objv[i], bdbmv,
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
		switch ((enum bdbmv)optindex) {
		case TCL_DBMV_AUTOCOMMIT:
			iflags |= DB_AUTO_COMMIT;
			_debug_check();
			break;
		case TCL_DBMV_ENCRYPT:
			set_flags |= DB_ENCRYPT;
			_debug_check();
			break;
		case TCL_DBMV_ENCRYPT_AES:
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
		case TCL_DBMV_ENCRYPT_ANY:
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
		case TCL_DBMV_ENV:
			arg = Tcl_GetStringFromObj(objv[i++], NULL);
			envp = NAME_TO_ENV(arg);
			if (envp == NULL) {
				Tcl_SetResult(interp,
				    "db rename: illegal environment",
				    TCL_STATIC);
				return (TCL_ERROR);
			}
			break;
		case TCL_DBMV_ENDARG:
			endarg = 1;
			break;
		case TCL_DBMV_TXN:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv, "?-txn id?");
				result = TCL_ERROR;
				break;
			}
			arg = Tcl_GetStringFromObj(objv[i++], NULL);
			txn = NAME_TO_TXN(arg);
			if (txn == NULL) {
				snprintf(msg, MSG_SIZE,
				    "Put: Invalid txn: %s\n", arg);
				Tcl_SetResult(interp, msg, TCL_VOLATILE);
				result = TCL_ERROR;
			}
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
	 * file names. If there is 2, a file name, if 3 a file and db name.
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
			if ((ret = __os_malloc(envp, (size_t)subdblen + 1,
			    &subdb)) != 0) {
				Tcl_SetResult(interp,
				    db_strerror(ret), TCL_STATIC);
				return (0);
			}
			memcpy(subdb, subdbtmp, (size_t)subdblen);
			subdb[subdblen] = '\0';
		}
		subdbtmp =
		    Tcl_GetByteArrayFromObj(objv[i++], &newlen);
		if ((ret = __os_malloc(envp, (size_t)newlen + 1,
		    &newname)) != 0) {
			Tcl_SetResult(interp,
			    db_strerror(ret), TCL_STATIC);
			return (0);
		}
		memcpy(newname, subdbtmp, (size_t)newlen);
		newname[newlen] = '\0';
	} else {
		Tcl_WrongNumArgs(
		    interp, 3, objv, "?args? filename ?database? ?newname?");
		result = TCL_ERROR;
		goto error;
	}
	if (envp == NULL) {
		ret = db_create(&dbp, envp, 0);
		if (ret) {
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "db_create");
			goto error;
		}
		if (passwd != NULL) {
			ret = dbp->set_encrypt(dbp, passwd, enc_flag);
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "set_encrypt");
		}
		if (set_flags != 0) {
			ret = dbp->set_flags(dbp, set_flags);
			result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
			    "set_flags");
		}
	}

	/*
	 * The dbrename method is a destructor, NULL out the dbp.
	 */
	_debug_check();
	if (dbp == NULL)
		ret = envp->dbrename(envp, txn, db, subdb, newname, iflags);
	else
		ret = dbp->rename(dbp, db, subdb, newname, 0);
	result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret), "db rename");
	dbp = NULL;
error:
	if (subdb)
		__os_free(envp, subdb);
	if (newname)
		__os_free(envp, newname);
	if (result == TCL_ERROR && dbp != NULL)
		(void)dbp->close(dbp, 0);
	return (result);
}

#ifdef CONFIG_TEST
/*
 * bdb_DbVerify --
 *	Implements the DB->verify command.
 */
static int
bdb_DbVerify(interp, objc, objv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
{
	static const char *bdbverify[] = {
		"-encrypt",
		"-encryptaes",
		"-encryptany",
		"-env",
		"-errfile",
		"-errpfx",
		"-unref",
		"--",
		NULL
	};
	enum bdbvrfy {
		TCL_DBVRFY_ENCRYPT,
		TCL_DBVRFY_ENCRYPT_AES,
		TCL_DBVRFY_ENCRYPT_ANY,
		TCL_DBVRFY_ENV,
		TCL_DBVRFY_ERRFILE,
		TCL_DBVRFY_ERRPFX,
		TCL_DBVRFY_UNREF,
		TCL_DBVRFY_ENDARG
	};
	DB_ENV *envp;
	DB *dbp;
	FILE *errf;
	u_int32_t enc_flag, flags, set_flags;
	int endarg, i, optindex, result, ret;
	char *arg, *db, *errpfx, *passwd;

	envp = NULL;
	dbp = NULL;
	passwd = NULL;
	result = TCL_OK;
	db = errpfx = NULL;
	errf = NULL;
	flags = endarg = 0;
	enc_flag = set_flags = 0;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 2, objv, "?args? filename");
		return (TCL_ERROR);
	}

	/*
	 * We must first parse for the environment flag, since that
	 * is needed for db_create.  Then create the db handle.
	 */
	i = 2;
	while (i < objc) {
		if (Tcl_GetIndexFromObj(interp, objv[i], bdbverify,
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
		switch ((enum bdbvrfy)optindex) {
		case TCL_DBVRFY_ENCRYPT:
			set_flags |= DB_ENCRYPT;
			_debug_check();
			break;
		case TCL_DBVRFY_ENCRYPT_AES:
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
		case TCL_DBVRFY_ENCRYPT_ANY:
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
		case TCL_DBVRFY_ENV:
			arg = Tcl_GetStringFromObj(objv[i++], NULL);
			envp = NAME_TO_ENV(arg);
			if (envp == NULL) {
				Tcl_SetResult(interp,
				    "db verify: illegal environment",
				    TCL_STATIC);
				result = TCL_ERROR;
				break;
			}
			break;
		case TCL_DBVRFY_ERRFILE:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "-errfile file");
				result = TCL_ERROR;
				break;
			}
			arg = Tcl_GetStringFromObj(objv[i++], NULL);
			/*
			 * If the user already set one, close it.
			 */
			if (errf != NULL && errf != stdout && errf != stderr)
				(void)fclose(errf);
			if (strcmp(arg, "/dev/stdout") == 0)
				errf = stdout;
			else if (strcmp(arg, "/dev/stderr") == 0)
				errf = stderr;
			else
				errf = fopen(arg, "a");
			break;
		case TCL_DBVRFY_ERRPFX:
			if (i >= objc) {
				Tcl_WrongNumArgs(interp, 2, objv,
				    "-errpfx prefix");
				result = TCL_ERROR;
				break;
			}
			arg = Tcl_GetStringFromObj(objv[i++], NULL);
			/*
			 * If the user already set one, free it.
			 */
			if (errpfx != NULL)
				__os_free(envp, errpfx);
			if ((ret = __os_strdup(NULL, arg, &errpfx)) != 0) {
				result = _ReturnSetup(interp, ret,
				    DB_RETOK_STD(ret), "__os_strdup");
				break;
			}
			break;
		case TCL_DBVRFY_UNREF:
			flags |= DB_UNREF;
			break;
		case TCL_DBVRFY_ENDARG:
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
	 * The remaining arg is the db filename.
	 */
	if (i == (objc - 1))
		db = Tcl_GetStringFromObj(objv[i++], NULL);
	else {
		Tcl_WrongNumArgs(interp, 2, objv, "?args? filename");
		result = TCL_ERROR;
		goto error;
	}
	ret = db_create(&dbp, envp, 0);
	if (ret) {
		result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "db_create");
		goto error;
	}

	if (passwd != NULL) {
		ret = dbp->set_encrypt(dbp, passwd, enc_flag);
		result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "set_encrypt");
	}

	if (set_flags != 0) {
		ret = dbp->set_flags(dbp, set_flags);
		result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "set_flags");
	}
	if (errf != NULL)
		dbp->set_errfile(dbp, errf);
	if (errpfx != NULL)
		dbp->set_errpfx(dbp, errpfx);

	/*
	 * The verify method is a destructor, NULL out the dbp.
	 */
	ret = dbp->verify(dbp, db, NULL, NULL, flags);
	result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret), "db verify");
	dbp = NULL;
error:
	if (errf != NULL && errf != stdout && errf != stderr)
		(void)fclose(errf);
	if (errpfx != NULL)
		__os_free(envp, errpfx);
	if (dbp)
		(void)dbp->close(dbp, 0);
	return (result);
}
#endif

/*
 * bdb_Version --
 *	Implements the version command.
 */
static int
bdb_Version(interp, objc, objv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
{
	static const char *bdbver[] = {
		"-string", NULL
	};
	enum bdbver {
		TCL_VERSTRING
	};
	int i, optindex, maj, min, patch, result, string, verobjc;
	char *arg, *v;
	Tcl_Obj *res, *verobjv[3];

	result = TCL_OK;
	string = 0;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 2, objv, "?args?");
		return (TCL_ERROR);
	}

	/*
	 * We must first parse for the environment flag, since that
	 * is needed for db_create.  Then create the db handle.
	 */
	i = 2;
	while (i < objc) {
		if (Tcl_GetIndexFromObj(interp, objv[i], bdbver,
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
		switch ((enum bdbver)optindex) {
		case TCL_VERSTRING:
			string = 1;
			break;
		}
		/*
		 * If, at any time, parsing the args we get an error,
		 * bail out and return.
		 */
		if (result != TCL_OK)
			goto error;
	}
	if (result != TCL_OK)
		goto error;

	v = db_version(&maj, &min, &patch);
	if (string)
		res = NewStringObj(v, strlen(v));
	else {
		verobjc = 3;
		verobjv[0] = Tcl_NewIntObj(maj);
		verobjv[1] = Tcl_NewIntObj(min);
		verobjv[2] = Tcl_NewIntObj(patch);
		res = Tcl_NewListObj(verobjc, verobjv);
	}
	Tcl_SetObjResult(interp, res);
error:
	return (result);
}

#ifdef CONFIG_TEST
/*
 * bdb_Handles --
 *	Implements the handles command.
 */
static int
bdb_Handles(interp, objc, objv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
{
	DBTCL_INFO *p;
	Tcl_Obj *res, *handle;

	/*
	 * No args.  Error if we have some
	 */
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 2, objv, "");
		return (TCL_ERROR);
	}
	res = Tcl_NewListObj(0, NULL);

	for (p = LIST_FIRST(&__db_infohead); p != NULL;
	    p = LIST_NEXT(p, entries)) {
		handle = NewStringObj(p->i_name, strlen(p->i_name));
		if (Tcl_ListObjAppendElement(interp, res, handle) != TCL_OK)
			return (TCL_ERROR);
	}
	Tcl_SetObjResult(interp, res);
	return (TCL_OK);
}

/*
 * bdb_MsgType -
 *	Implements the msgtype command.
 *	Given a replication message return its message type name.
 */
static int
bdb_MsgType(interp, objc, objv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
{
	REP_CONTROL *rp;
	Tcl_Obj *msgname;
	u_int32_t len, msgtype;
	int freerp, ret;

	/*
	 * If the messages in rep.h change, this must change too!
	 * Add "no_type" for 0 so that we directly index.
	 */
	static const char *msgnames[] = {
		"no_type", "alive", "alive_req", "all_req",
		"dupmaster", "file", "file_fail", "file_req", "log",
		"log_more", "log_req", "master_req", "newclient",
		"newfile", "newmaster", "newsite", "page",
		"page_fail", "page_req", "update", "update_req",
		"verify", "verify_fail", "verify_req",
		"vote1", "vote2", NULL
	};

	/*
	 * 1 arg, the message.  Error if different.
	 */
	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 3, objv, "msgtype msg");
		return (TCL_ERROR);
	}

	ret = _CopyObjBytes(interp, objv[2], (void **)&rp, &len, &freerp);
	if (ret != TCL_OK) {
		Tcl_SetResult(interp,
		    "msgtype: bad control message", TCL_STATIC);
		return (TCL_ERROR);
	}
	msgtype = rp->rectype;
	msgname = NewStringObj(msgnames[msgtype], strlen(msgnames[msgtype]));
	Tcl_SetObjResult(interp, msgname);
	if (rp != NULL && freerp)
		__os_free(NULL, rp);
	return (TCL_OK);
}

/*
 * bdb_DbUpgrade --
 *	Implements the DB->upgrade command.
 */
static int
bdb_DbUpgrade(interp, objc, objv)
	Tcl_Interp *interp;		/* Interpreter */
	int objc;			/* How many arguments? */
	Tcl_Obj *CONST objv[];		/* The argument objects */
{
	static const char *bdbupg[] = {
		"-dupsort", "-env", "--", NULL
	};
	enum bdbupg {
		TCL_DBUPG_DUPSORT,
		TCL_DBUPG_ENV,
		TCL_DBUPG_ENDARG
	};
	DB_ENV *envp;
	DB *dbp;
	u_int32_t flags;
	int endarg, i, optindex, result, ret;
	char *arg, *db;

	envp = NULL;
	dbp = NULL;
	result = TCL_OK;
	db = NULL;
	flags = endarg = 0;

	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 2, objv, "?args? filename");
		return (TCL_ERROR);
	}

	i = 2;
	while (i < objc) {
		if (Tcl_GetIndexFromObj(interp, objv[i], bdbupg,
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
		switch ((enum bdbupg)optindex) {
		case TCL_DBUPG_DUPSORT:
			flags |= DB_DUPSORT;
			break;
		case TCL_DBUPG_ENV:
			arg = Tcl_GetStringFromObj(objv[i++], NULL);
			envp = NAME_TO_ENV(arg);
			if (envp == NULL) {
				Tcl_SetResult(interp,
				    "db upgrade: illegal environment",
				    TCL_STATIC);
				return (TCL_ERROR);
			}
			break;
		case TCL_DBUPG_ENDARG:
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
	 * The remaining arg is the db filename.
	 */
	if (i == (objc - 1))
		db = Tcl_GetStringFromObj(objv[i++], NULL);
	else {
		Tcl_WrongNumArgs(interp, 2, objv, "?args? filename");
		result = TCL_ERROR;
		goto error;
	}
	ret = db_create(&dbp, envp, 0);
	if (ret) {
		result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret),
		    "db_create");
		goto error;
	}

	ret = dbp->upgrade(dbp, db, flags);
	result = _ReturnSetup(interp, ret, DB_RETOK_STD(ret), "db upgrade");
error:
	if (dbp)
		(void)dbp->close(dbp, 0);
	return (result);
}

/*
 * tcl_bt_compare and tcl_dup_compare --
 *	These two are basically identical internally, so may as well
 * share code.  The only differences are the name used in error
 * reporting and the Tcl_Obj representing their respective procs.
 */
static int
tcl_bt_compare(dbp, dbta, dbtb)
	DB *dbp;
	const DBT *dbta, *dbtb;
{
	return (tcl_compare_callback(dbp, dbta, dbtb,
	    ((DBTCL_INFO *)dbp->api_internal)->i_btcompare, "bt_compare"));
}

static int
tcl_dup_compare(dbp, dbta, dbtb)
	DB *dbp;
	const DBT *dbta, *dbtb;
{
	return (tcl_compare_callback(dbp, dbta, dbtb,
	    ((DBTCL_INFO *)dbp->api_internal)->i_dupcompare, "dup_compare"));
}

/*
 * tcl_compare_callback --
 *	Tcl callback for set_bt_compare and set_dup_compare. What this
 * function does is stuff the data fields of the two DBTs into Tcl ByteArray
 * objects, then call the procedure stored in ip->i_btcompare on the two
 * objects.  Then we return that procedure's result as the comparison.
 */
static int
tcl_compare_callback(dbp, dbta, dbtb, procobj, errname)
	DB *dbp;
	const DBT *dbta, *dbtb;
	Tcl_Obj *procobj;
	char *errname;
{
	DBTCL_INFO *ip;
	Tcl_Interp *interp;
	Tcl_Obj *a, *b, *resobj, *objv[3];
	int result, cmp;

	ip = (DBTCL_INFO *)dbp->api_internal;
	interp = ip->i_interp;
	objv[0] = procobj;

	/*
	 * Create two ByteArray objects, with the two data we've been passed.
	 * This will involve a copy, which is unpleasantly slow, but there's
	 * little we can do to avoid this (I think).
	 */
	a = Tcl_NewByteArrayObj(dbta->data, (int)dbta->size);
	Tcl_IncrRefCount(a);
	b = Tcl_NewByteArrayObj(dbtb->data, (int)dbtb->size);
	Tcl_IncrRefCount(b);

	objv[1] = a;
	objv[2] = b;

	result = Tcl_EvalObjv(interp, 3, objv, 0);
	if (result != TCL_OK) {
		/*
		 * XXX
		 * If this or the next Tcl call fails, we're doomed.
		 * There's no way to return an error from comparison functions,
		 * no way to determine what the correct sort order is, and
		 * so no way to avoid corrupting the database if we proceed.
		 * We could play some games stashing return values on the
		 * DB handle, but it's not worth the trouble--no one with
		 * any sense is going to be using this other than for testing,
		 * and failure typically means that the bt_compare proc
		 * had a syntax error in it or something similarly dumb.
		 *
		 * So, drop core.  If we're not running with diagnostic
		 * mode, panic--and always return a negative number. :-)
		 */
panic:		__db_err(dbp->dbenv, "Tcl %s callback failed", errname);
		DB_ASSERT(0);
		return (__db_panic(dbp->dbenv, DB_RUNRECOVERY));
	}

	resobj = Tcl_GetObjResult(interp);
	result = Tcl_GetIntFromObj(interp, resobj, &cmp);
	if (result != TCL_OK)
		goto panic;

	Tcl_DecrRefCount(a);
	Tcl_DecrRefCount(b);
	return (cmp);
}

/*
 * tcl_h_hash --
 *	Tcl callback for the hashing function.  See tcl_compare_callback--
 * this works much the same way, only we're given a buffer and a length
 * instead of two DBTs.
 */
static u_int32_t
tcl_h_hash(dbp, buf, len)
	DB *dbp;
	const void *buf;
	u_int32_t len;
{
	DBTCL_INFO *ip;
	Tcl_Interp *interp;
	Tcl_Obj *objv[2];
	int result, hval;

	ip = (DBTCL_INFO *)dbp->api_internal;
	interp = ip->i_interp;
	objv[0] = ip->i_hashproc;

	/*
	 * Create a ByteArray for the buffer.
	 */
	objv[1] = Tcl_NewByteArrayObj((void *)buf, (int)len);
	Tcl_IncrRefCount(objv[1]);
	result = Tcl_EvalObjv(interp, 2, objv, 0);
	if (result != TCL_OK)
		goto panic;

	result = Tcl_GetIntFromObj(interp, Tcl_GetObjResult(interp), &hval);
	if (result != TCL_OK)
		goto panic;

	Tcl_DecrRefCount(objv[1]);
	return ((u_int32_t)hval);

panic:	/*
	 * We drop core on error, in diagnostic mode.  See the comment in
	 * tcl_compare_callback.
	 */
	__db_err(dbp->dbenv, "Tcl h_hash callback failed");
	(void)__db_panic(dbp->dbenv, DB_RUNRECOVERY);

	DB_ASSERT(0);

	/* NOTREACHED */
	return (0);
}

/*
 * tcl_rep_send --
 *	Replication send callback.
 */
static int
tcl_rep_send(dbenv, control, rec, lsnp, eid, flags)
	DB_ENV *dbenv;
	const DBT *control, *rec;
	const DB_LSN *lsnp;
	int eid;
	u_int32_t flags;
{
#define	TCLDB_SENDITEMS	7
	DBTCL_INFO *ip;
	Tcl_Interp *interp;
	Tcl_Obj *control_o, *eid_o, *flags_o, *lsn_o, *origobj, *rec_o;
	Tcl_Obj *myobjv[2], *resobj, *objv[TCLDB_SENDITEMS];
	int myobjc, result, ret;

	ip = (DBTCL_INFO *)dbenv->app_private;
	interp = ip->i_interp;
	objv[0] = ip->i_rep_send;

	control_o = Tcl_NewByteArrayObj(control->data, (int)control->size);
	Tcl_IncrRefCount(control_o);

	rec_o = Tcl_NewByteArrayObj(rec->data, (int)rec->size);
	Tcl_IncrRefCount(rec_o);

	eid_o = Tcl_NewIntObj(eid);
	Tcl_IncrRefCount(eid_o);

	if (LF_ISSET(DB_REP_PERMANENT))
		flags_o = NewStringObj("perm", strlen("perm"));
	else if (LF_ISSET(DB_REP_NOBUFFER))
		flags_o = NewStringObj("nobuffer", strlen("nobuffer"));
	else
		flags_o = NewStringObj("none", strlen("none"));
	Tcl_IncrRefCount(flags_o);

	myobjc = 2;
	myobjv[0] = Tcl_NewLongObj((long)lsnp->file);
	myobjv[1] = Tcl_NewLongObj((long)lsnp->offset);
	lsn_o = Tcl_NewListObj(myobjc, myobjv);

	objv[1] = control_o;
	objv[2] = rec_o;
	objv[3] = ip->i_rep_eid;	/* From ID */
	objv[4] = eid_o;		/* To ID */
	objv[5] = flags_o;		/* Flags */
	objv[6] = lsn_o;		/* LSN */

	/*
	 * We really want to return the original result to the
	 * user.  So, save the result obj here, and then after
	 * we've taken care of the Tcl_EvalObjv, set the result
	 * back to this original result.
	 */
	origobj = Tcl_GetObjResult(interp);
	Tcl_IncrRefCount(origobj);
	result = Tcl_EvalObjv(interp, TCLDB_SENDITEMS, objv, 0);
	if (result != TCL_OK) {
		/*
		 * XXX
		 * This probably isn't the right error behavior, but
		 * this error should only happen if the Tcl callback is
		 * somehow invalid, which is a fatal scripting bug.
		 */
err:		__db_err(dbenv, "Tcl rep_send failure");
		return (EINVAL);
	}

	resobj = Tcl_GetObjResult(interp);
	result = Tcl_GetIntFromObj(interp, resobj, &ret);
	if (result != TCL_OK)
		goto err;

	Tcl_SetObjResult(interp, origobj);
	Tcl_DecrRefCount(origobj);
	Tcl_DecrRefCount(control_o);
	Tcl_DecrRefCount(rec_o);
	Tcl_DecrRefCount(eid_o);
	Tcl_DecrRefCount(flags_o);

	return (ret);
}
#endif

#ifdef CONFIG_TEST
/*
 * tcl_db_malloc, tcl_db_realloc, tcl_db_free --
 *	Tcl-local malloc, realloc, and free functions to use for user data
 * to exercise umalloc/urealloc/ufree.  Allocate the memory as a Tcl object
 * so we're sure to exacerbate and catch any shared-library issues.
 */
static void *
tcl_db_malloc(size)
	size_t size;
{
	Tcl_Obj *obj;
	void *buf;

	obj = Tcl_NewObj();
	if (obj == NULL)
		return (NULL);
	Tcl_IncrRefCount(obj);

	Tcl_SetObjLength(obj, (int)(size + sizeof(Tcl_Obj *)));
	buf = Tcl_GetString(obj);
	memcpy(buf, &obj, sizeof(&obj));

	buf = (Tcl_Obj **)buf + 1;
	return (buf);
}

static void *
tcl_db_realloc(ptr, size)
	void *ptr;
	size_t size;
{
	Tcl_Obj *obj;

	if (ptr == NULL)
		return (tcl_db_malloc(size));

	obj = *(Tcl_Obj **)((Tcl_Obj **)ptr - 1);
	Tcl_SetObjLength(obj, (int)(size + sizeof(Tcl_Obj *)));

	ptr = Tcl_GetString(obj);
	memcpy(ptr, &obj, sizeof(&obj));

	ptr = (Tcl_Obj **)ptr + 1;
	return (ptr);
}

static void
tcl_db_free(ptr)
	void *ptr;
{
	Tcl_Obj *obj;

	obj = *(Tcl_Obj **)((Tcl_Obj **)ptr - 1);
	Tcl_DecrRefCount(obj);
}
#endif
