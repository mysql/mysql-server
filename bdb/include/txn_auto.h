/* Do not edit: automatically built by gen_rec.awk. */

#ifndef	txn_AUTO_H
#define	txn_AUTO_H

#define	DB_txn_old_regop	6
typedef struct _txn_old_regop_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	opcode;
} __txn_old_regop_args;

int __txn_old_regop_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __txn_old_regop_read __P((DB_ENV *, void *, __txn_old_regop_args **));

#define	DB_txn_regop	10
typedef struct _txn_regop_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	opcode;
	int32_t	timestamp;
} __txn_regop_args;

int __txn_regop_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t, u_int32_t, int32_t));
int __txn_regop_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __txn_regop_read __P((DB_ENV *, void *, __txn_regop_args **));

#define	DB_txn_old_ckp	7
typedef struct _txn_old_ckp_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	DB_LSN	ckp_lsn;
	DB_LSN	last_ckp;
} __txn_old_ckp_args;

int __txn_old_ckp_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __txn_old_ckp_read __P((DB_ENV *, void *, __txn_old_ckp_args **));

#define	DB_txn_ckp	11
typedef struct _txn_ckp_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	DB_LSN	ckp_lsn;
	DB_LSN	last_ckp;
	int32_t	timestamp;
} __txn_ckp_args;

int __txn_ckp_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t, DB_LSN *, DB_LSN *, int32_t));
int __txn_ckp_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __txn_ckp_read __P((DB_ENV *, void *, __txn_ckp_args **));

#define	DB_txn_xa_regop_old	8
typedef struct _txn_xa_regop_old_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	opcode;
	DBT	xid;
	int32_t	formatID;
	u_int32_t	gtrid;
	u_int32_t	bqual;
} __txn_xa_regop_old_args;

int __txn_xa_regop_old_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __txn_xa_regop_old_read __P((DB_ENV *, void *, __txn_xa_regop_old_args **));

#define	DB_txn_xa_regop	13
typedef struct _txn_xa_regop_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	opcode;
	DBT	xid;
	int32_t	formatID;
	u_int32_t	gtrid;
	u_int32_t	bqual;
	DB_LSN	begin_lsn;
} __txn_xa_regop_args;

int __txn_xa_regop_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t, u_int32_t, const DBT *, int32_t, u_int32_t, u_int32_t, DB_LSN *));
int __txn_xa_regop_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __txn_xa_regop_read __P((DB_ENV *, void *, __txn_xa_regop_args **));

#define	DB_txn_child_old	9
typedef struct _txn_child_old_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	opcode;
	u_int32_t	parent;
} __txn_child_old_args;

int __txn_child_old_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __txn_child_old_read __P((DB_ENV *, void *, __txn_child_old_args **));

#define	DB_txn_child	12
typedef struct _txn_child_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	u_int32_t	child;
	DB_LSN	c_lsn;
} __txn_child_args;

int __txn_child_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t, u_int32_t, DB_LSN *));
int __txn_child_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __txn_child_read __P((DB_ENV *, void *, __txn_child_args **));
int __txn_init_print __P((DB_ENV *));
int __txn_init_recover __P((DB_ENV *));
#endif
