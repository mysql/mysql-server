/* Do not edit: automatically built by gen_rec.awk. */

#ifndef	bam_AUTO_H
#define	bam_AUTO_H

#define	DB_bam_pg_alloc	51
typedef struct _bam_pg_alloc_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	int32_t	fileid;
	DB_LSN	meta_lsn;
	DB_LSN	page_lsn;
	db_pgno_t	pgno;
	u_int32_t	ptype;
	db_pgno_t	next;
} __bam_pg_alloc_args;

int __bam_pg_alloc_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t, int32_t, DB_LSN *, DB_LSN *, db_pgno_t, u_int32_t, db_pgno_t));
int __bam_pg_alloc_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __bam_pg_alloc_read __P((DB_ENV *, void *, __bam_pg_alloc_args **));

#define	DB_bam_pg_alloc1	60
typedef struct _bam_pg_alloc1_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	int32_t	fileid;
	DB_LSN	meta_lsn;
	DB_LSN	alloc_lsn;
	DB_LSN	page_lsn;
	db_pgno_t	pgno;
	u_int32_t	ptype;
	db_pgno_t	next;
} __bam_pg_alloc1_args;

int __bam_pg_alloc1_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __bam_pg_alloc1_read __P((DB_ENV *, void *, __bam_pg_alloc1_args **));

#define	DB_bam_pg_free	52
typedef struct _bam_pg_free_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	int32_t	fileid;
	db_pgno_t	pgno;
	DB_LSN	meta_lsn;
	DBT	header;
	db_pgno_t	next;
} __bam_pg_free_args;

int __bam_pg_free_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t, int32_t, db_pgno_t, DB_LSN *, const DBT *, db_pgno_t));
int __bam_pg_free_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __bam_pg_free_read __P((DB_ENV *, void *, __bam_pg_free_args **));

#define	DB_bam_pg_free1	61
typedef struct _bam_pg_free1_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	int32_t	fileid;
	db_pgno_t	pgno;
	DB_LSN	meta_lsn;
	DB_LSN	alloc_lsn;
	DBT	header;
	db_pgno_t	next;
} __bam_pg_free1_args;

int __bam_pg_free1_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __bam_pg_free1_read __P((DB_ENV *, void *, __bam_pg_free1_args **));

#define	DB_bam_split1	53
typedef struct _bam_split1_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	int32_t	fileid;
	db_pgno_t	left;
	DB_LSN	llsn;
	db_pgno_t	right;
	DB_LSN	rlsn;
	u_int32_t	indx;
	db_pgno_t	npgno;
	DB_LSN	nlsn;
	DBT	pg;
} __bam_split1_args;

int __bam_split1_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __bam_split1_read __P((DB_ENV *, void *, __bam_split1_args **));

#define	DB_bam_split	62
typedef struct _bam_split_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	int32_t	fileid;
	db_pgno_t	left;
	DB_LSN	llsn;
	db_pgno_t	right;
	DB_LSN	rlsn;
	u_int32_t	indx;
	db_pgno_t	npgno;
	DB_LSN	nlsn;
	db_pgno_t	root_pgno;
	DBT	pg;
	u_int32_t	opflags;
} __bam_split_args;

int __bam_split_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t, int32_t, db_pgno_t, DB_LSN *, db_pgno_t, DB_LSN *, u_int32_t, db_pgno_t, DB_LSN *, db_pgno_t, const DBT *, u_int32_t));
int __bam_split_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __bam_split_read __P((DB_ENV *, void *, __bam_split_args **));

#define	DB_bam_rsplit1	54
typedef struct _bam_rsplit1_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	int32_t	fileid;
	db_pgno_t	pgno;
	DBT	pgdbt;
	db_pgno_t	nrec;
	DBT	rootent;
	DB_LSN	rootlsn;
} __bam_rsplit1_args;

int __bam_rsplit1_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __bam_rsplit1_read __P((DB_ENV *, void *, __bam_rsplit1_args **));

#define	DB_bam_rsplit	63
typedef struct _bam_rsplit_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	int32_t	fileid;
	db_pgno_t	pgno;
	DBT	pgdbt;
	db_pgno_t	root_pgno;
	db_pgno_t	nrec;
	DBT	rootent;
	DB_LSN	rootlsn;
} __bam_rsplit_args;

int __bam_rsplit_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t, int32_t, db_pgno_t, const DBT *, db_pgno_t, db_pgno_t, const DBT *, DB_LSN *));
int __bam_rsplit_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __bam_rsplit_read __P((DB_ENV *, void *, __bam_rsplit_args **));

#define	DB_bam_adj	55
typedef struct _bam_adj_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	int32_t	fileid;
	db_pgno_t	pgno;
	DB_LSN	lsn;
	u_int32_t	indx;
	u_int32_t	indx_copy;
	u_int32_t	is_insert;
} __bam_adj_args;

int __bam_adj_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t, int32_t, db_pgno_t, DB_LSN *, u_int32_t, u_int32_t, u_int32_t));
int __bam_adj_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __bam_adj_read __P((DB_ENV *, void *, __bam_adj_args **));

#define	DB_bam_cadjust	56
typedef struct _bam_cadjust_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	int32_t	fileid;
	db_pgno_t	pgno;
	DB_LSN	lsn;
	u_int32_t	indx;
	int32_t	adjust;
	u_int32_t	opflags;
} __bam_cadjust_args;

int __bam_cadjust_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t, int32_t, db_pgno_t, DB_LSN *, u_int32_t, int32_t, u_int32_t));
int __bam_cadjust_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __bam_cadjust_read __P((DB_ENV *, void *, __bam_cadjust_args **));

#define	DB_bam_cdel	57
typedef struct _bam_cdel_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	int32_t	fileid;
	db_pgno_t	pgno;
	DB_LSN	lsn;
	u_int32_t	indx;
} __bam_cdel_args;

int __bam_cdel_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t, int32_t, db_pgno_t, DB_LSN *, u_int32_t));
int __bam_cdel_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __bam_cdel_read __P((DB_ENV *, void *, __bam_cdel_args **));

#define	DB_bam_repl	58
typedef struct _bam_repl_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	int32_t	fileid;
	db_pgno_t	pgno;
	DB_LSN	lsn;
	u_int32_t	indx;
	u_int32_t	isdeleted;
	DBT	orig;
	DBT	repl;
	u_int32_t	prefix;
	u_int32_t	suffix;
} __bam_repl_args;

int __bam_repl_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t, int32_t, db_pgno_t, DB_LSN *, u_int32_t, u_int32_t, const DBT *, const DBT *, u_int32_t, u_int32_t));
int __bam_repl_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __bam_repl_read __P((DB_ENV *, void *, __bam_repl_args **));

#define	DB_bam_root	59
typedef struct _bam_root_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	int32_t	fileid;
	db_pgno_t	meta_pgno;
	db_pgno_t	root_pgno;
	DB_LSN	meta_lsn;
} __bam_root_args;

int __bam_root_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t, int32_t, db_pgno_t, db_pgno_t, DB_LSN *));
int __bam_root_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __bam_root_read __P((DB_ENV *, void *, __bam_root_args **));

#define	DB_bam_curadj	64
typedef struct _bam_curadj_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	int32_t	fileid;
	db_ca_mode	mode;
	db_pgno_t	from_pgno;
	db_pgno_t	to_pgno;
	db_pgno_t	left_pgno;
	u_int32_t	first_indx;
	u_int32_t	from_indx;
	u_int32_t	to_indx;
} __bam_curadj_args;

int __bam_curadj_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t, int32_t, db_ca_mode, db_pgno_t, db_pgno_t, db_pgno_t, u_int32_t, u_int32_t, u_int32_t));
int __bam_curadj_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __bam_curadj_read __P((DB_ENV *, void *, __bam_curadj_args **));

#define	DB_bam_rcuradj	65
typedef struct _bam_rcuradj_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	int32_t	fileid;
	ca_recno_arg	mode;
	db_pgno_t	root;
	db_recno_t	recno;
	u_int32_t	order;
} __bam_rcuradj_args;

int __bam_rcuradj_log __P((DB_ENV *, DB_TXN *, DB_LSN *, u_int32_t, int32_t, ca_recno_arg, db_pgno_t, db_recno_t, u_int32_t));
int __bam_rcuradj_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __bam_rcuradj_read __P((DB_ENV *, void *, __bam_rcuradj_args **));
int __bam_init_print __P((DB_ENV *));
int __bam_init_recover __P((DB_ENV *));
#endif
