/* Do not edit: automatically built by gen_rec.awk. */

#ifndef	log_AUTO_H
#define	log_AUTO_H

#define	DB_log_register1	1
typedef struct _log_register1_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	opcode;
	DBT	name;
	DBT	uid;
	int32_t	fileid;
	DBTYPE	ftype;
} __log_register1_args;

int __log_register1_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __log_register1_read __P((DB_ENV *, void *, __log_register1_args **));

#define	DB_log_register	2
typedef struct _log_register_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	opcode;
	DBT	name;
	DBT	uid;
	int32_t	fileid;
	DBTYPE	ftype;
	db_pgno_t	meta_pgno;
} __log_register_args;

int __log_register_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t, u_int32_t, const DBT *, const DBT *, int32_t, DBTYPE, db_pgno_t));
int __log_register_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __log_register_read __P((DB_ENV *, void *, __log_register_args **));
int __log_init_print __P((DB_ENV *));
int __log_init_recover __P((DB_ENV *));
#endif
