/* Do not edit: automatically built by gen_rec.awk. */

#ifndef	db_AUTO_H
#define	db_AUTO_H

#define	DB_db_addrem	41
typedef struct _db_addrem_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	opcode;
	int32_t	fileid;
	db_pgno_t	pgno;
	u_int32_t	indx;
	size_t	nbytes;
	DBT	hdr;
	DBT	dbt;
	DB_LSN	pagelsn;
} __db_addrem_args;

int __db_addrem_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t, u_int32_t, int32_t, db_pgno_t, u_int32_t, size_t, const DBT *, const DBT *, DB_LSN *));
int __db_addrem_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __db_addrem_read __P((DB_ENV *, void *, __db_addrem_args **));

#define	DB_db_split	42
typedef struct _db_split_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	opcode;
	int32_t	fileid;
	db_pgno_t	pgno;
	DBT	pageimage;
	DB_LSN	pagelsn;
} __db_split_args;

int __db_split_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __db_split_read __P((DB_ENV *, void *, __db_split_args **));

#define	DB_db_big	43
typedef struct _db_big_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	opcode;
	int32_t	fileid;
	db_pgno_t	pgno;
	db_pgno_t	prev_pgno;
	db_pgno_t	next_pgno;
	DBT	dbt;
	DB_LSN	pagelsn;
	DB_LSN	prevlsn;
	DB_LSN	nextlsn;
} __db_big_args;

int __db_big_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t, u_int32_t, int32_t, db_pgno_t, db_pgno_t, db_pgno_t, const DBT *, DB_LSN *, DB_LSN *, DB_LSN *));
int __db_big_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __db_big_read __P((DB_ENV *, void *, __db_big_args **));

#define	DB_db_ovref	44
typedef struct _db_ovref_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	int32_t	fileid;
	db_pgno_t	pgno;
	int32_t	adjust;
	DB_LSN	lsn;
} __db_ovref_args;

int __db_ovref_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t, int32_t, db_pgno_t, int32_t, DB_LSN *));
int __db_ovref_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __db_ovref_read __P((DB_ENV *, void *, __db_ovref_args **));

#define	DB_db_relink	45
typedef struct _db_relink_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	opcode;
	int32_t	fileid;
	db_pgno_t	pgno;
	DB_LSN	lsn;
	db_pgno_t	prev;
	DB_LSN	lsn_prev;
	db_pgno_t	next;
	DB_LSN	lsn_next;
} __db_relink_args;

int __db_relink_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t, u_int32_t, int32_t, db_pgno_t, DB_LSN *, db_pgno_t, DB_LSN *, db_pgno_t, DB_LSN *));
int __db_relink_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __db_relink_read __P((DB_ENV *, void *, __db_relink_args **));

#define	DB_db_addpage	46
typedef struct _db_addpage_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	int32_t	fileid;
	db_pgno_t	pgno;
	DB_LSN	lsn;
	db_pgno_t	nextpgno;
	DB_LSN	nextlsn;
} __db_addpage_args;

int __db_addpage_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __db_addpage_read __P((DB_ENV *, void *, __db_addpage_args **));

#define	DB_db_debug	47
typedef struct _db_debug_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	DBT	op;
	int32_t	fileid;
	DBT	key;
	DBT	data;
	u_int32_t	arg_flags;
} __db_debug_args;

int __db_debug_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t, const DBT *, int32_t, const DBT *, const DBT *, u_int32_t));
int __db_debug_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __db_debug_read __P((DB_ENV *, void *, __db_debug_args **));

#define	DB_db_noop	48
typedef struct _db_noop_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	int32_t	fileid;
	db_pgno_t	pgno;
	DB_LSN	prevlsn;
} __db_noop_args;

int __db_noop_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t, int32_t, db_pgno_t, DB_LSN *));
int __db_noop_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __db_noop_read __P((DB_ENV *, void *, __db_noop_args **));
int __db_init_print __P((DB_ENV *));
int __db_init_recover __P((DB_ENV *));
#endif
