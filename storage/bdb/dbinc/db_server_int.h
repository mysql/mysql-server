/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 2000-2004
 *	Sleepycat Software.  All rights reserved.
 *
 * $Id: db_server_int.h,v 1.25 2004/01/28 03:36:02 bostic Exp $
 */

#ifndef _DB_SERVER_INT_H_
#define	_DB_SERVER_INT_H_

#define	DB_SERVER_TIMEOUT	300	/* 5 minutes */
#define	DB_SERVER_MAXTIMEOUT	1200	/* 20 minutes */
#define	DB_SERVER_IDLETIMEOUT	86400	/* 1 day */

/*
 * Ignore/mask off the following env->open flags:
 * Most are illegal for a client to specify as they would control
 * server resource usage.  We will just ignore them.
 *	DB_LOCKDOWN
 *	DB_PRIVATE
 *	DB_RECOVER
 *	DB_RECOVER_FATAL
 *	DB_SYSTEM_MEM
 *	DB_USE_ENVIRON, DB_USE_ENVIRON_ROOT	- handled on client
 */
#define	DB_SERVER_FLAGMASK	(					\
DB_LOCKDOWN | DB_PRIVATE | DB_RECOVER | DB_RECOVER_FATAL |		\
DB_SYSTEM_MEM | DB_USE_ENVIRON | DB_USE_ENVIRON_ROOT)

#define	CT_CURSOR	0x001		/* Cursor */
#define	CT_DB		0x002		/* Database */
#define	CT_ENV		0x004		/* Env */
#define	CT_TXN		0x008		/* Txn */

#define	CT_JOIN		0x10000000	/* Join cursor component */
#define	CT_JOINCUR	0x20000000	/* Join cursor */

typedef struct home_entry home_entry;
struct home_entry {
	LIST_ENTRY(home_entry) entries;
	char *home;
	char *dir;
	char *name;
	char *passwd;
};

/*
 * Data needed for sharing handles.
 * To share an env handle, on the open call, they must have matching
 * env flags, and matching set_flags.
 *
 * To share a db handle on the open call, the db, subdb and flags must
 * all be the same.
 */
#define	DB_SERVER_ENVFLAGS	 (					\
DB_INIT_CDB | DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL |		\
DB_INIT_TXN | DB_JOINENV)

#define	DB_SERVER_DBFLAGS	 (DB_DIRTY_READ | DB_NOMMAP | DB_RDONLY)
#define	DB_SERVER_DBNOSHARE	 (DB_EXCL | DB_TRUNCATE)

typedef struct ct_envdata ct_envdata;
typedef struct ct_dbdata ct_dbdata;
struct ct_envdata {
	u_int32_t	envflags;
	u_int32_t	onflags;
	u_int32_t	offflags;
	home_entry	*home;
};

struct ct_dbdata {
	u_int32_t	dbflags;
	u_int32_t	setflags;
	char		*db;
	char		*subdb;
	DBTYPE		type;
};

/*
 * We maintain an activity timestamp for each handle.  However, we
 * set it to point, possibly to the ct_active field of its own handle
 * or it may point to the ct_active field of a parent.  In the case
 * of nested transactions and any cursors within transactions it must
 * point to the ct_active field of the ultimate parent of the transaction
 * no matter how deeply it is nested.
 */
typedef struct ct_entry ct_entry;
struct ct_entry {
	LIST_ENTRY(ct_entry) entries;		/* List of entries */
	union {
#ifdef __cplusplus
		DbEnv *envp;			/* H_ENV */
		DbTxn *txnp;			/* H_TXN */
		Db *dbp;			/* H_DB */
		Dbc *dbc;			/* H_CURSOR */
#else
		DB_ENV *envp;			/* H_ENV */
		DB_TXN *txnp;			/* H_TXN */
		DB *dbp;			/* H_DB */
		DBC *dbc;			/* H_CURSOR */
#endif
		void *anyp;
	} handle_u;
	union {					/* Private data per type */
		ct_envdata	envdp;		/* Env info */
		ct_dbdata	dbdp;		/* Db info */
	} private_u;
	long ct_id;				/* Client ID */
	long *ct_activep;			/* Activity timestamp pointer*/
	long *ct_origp;				/* Original timestamp pointer*/
	long ct_active;				/* Activity timestamp */
	long ct_timeout;			/* Resource timeout */
	long ct_idle;				/* Idle timeout */
	u_int32_t ct_refcount;			/* Ref count for sharing */
	u_int32_t ct_type;			/* This entry's type */
	struct ct_entry *ct_parent;		/* Its parent */
	struct ct_entry *ct_envparent;		/* Its environment */
};

#define	ct_envp handle_u.envp
#define	ct_txnp handle_u.txnp
#define	ct_dbp handle_u.dbp
#define	ct_dbc handle_u.dbc
#define	ct_anyp handle_u.anyp

#define	ct_envdp private_u.envdp
#define	ct_dbdp private_u.dbdp

extern int __dbsrv_verbose;

/*
 * Get ctp and activate it.
 * Assumes local variable 'replyp'.
 * NOTE: May 'return' from macro.
 */
#define	ACTIVATE_CTP(ctp, id, type) {		\
	(ctp) = get_tableent(id);		\
	if ((ctp) == NULL) {			\
		replyp->status = DB_NOSERVER_ID;\
		return;				\
	}					\
	DB_ASSERT((ctp)->ct_type & (type));	\
	__dbsrv_active(ctp);			\
}

#endif	/* !_DB_SERVER_INT_H_ */
