/* Do not edit: automatically built by gen_rec.awk. */

#ifndef	__bam_AUTO_H
#define	__bam_AUTO_H
#define	DB___bam_split	62
typedef struct ___bam_split_args {
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

#define	DB___bam_rsplit	63
typedef struct ___bam_rsplit_args {
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

#define	DB___bam_adj	55
typedef struct ___bam_adj_args {
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

#define	DB___bam_cadjust	56
typedef struct ___bam_cadjust_args {
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

#define	DB___bam_cdel	57
typedef struct ___bam_cdel_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	int32_t	fileid;
	db_pgno_t	pgno;
	DB_LSN	lsn;
	u_int32_t	indx;
} __bam_cdel_args;

#define	DB___bam_repl	58
typedef struct ___bam_repl_args {
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

#define	DB___bam_root	59
typedef struct ___bam_root_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	int32_t	fileid;
	db_pgno_t	meta_pgno;
	db_pgno_t	root_pgno;
	DB_LSN	meta_lsn;
} __bam_root_args;

#define	DB___bam_curadj	64
typedef struct ___bam_curadj_args {
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

#define	DB___bam_rcuradj	65
typedef struct ___bam_rcuradj_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	int32_t	fileid;
	ca_recno_arg	mode;
	db_pgno_t	root;
	db_recno_t	recno;
	u_int32_t	order;
} __bam_rcuradj_args;

#endif
