/* Do not edit: automatically built by gen_rec.awk. */

#ifndef	__crdel_AUTO_H
#define	__crdel_AUTO_H
#define	DB___crdel_metasub	142
typedef struct ___crdel_metasub_args {
	u_int32_t type;
	DB_TXN *txnid;
	DB_LSN prev_lsn;
	int32_t	fileid;
	db_pgno_t	pgno;
	DBT	page;
	DB_LSN	lsn;
} __crdel_metasub_args;

#endif
