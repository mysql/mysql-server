/* DO NOT EDIT: automatically built by dist/s_include. */
#ifndef	_db_ext_h_
#define	_db_ext_h_
#if defined(__cplusplus)
extern "C" {
#endif
int __crdel_fileopen_recover
  __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __crdel_metasub_recover
  __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __crdel_metapage_recover
  __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __crdel_delete_recover
  __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __crdel_rename_recover
  __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __db_open __P((DB *,
    const char *, const char *, DBTYPE, u_int32_t, int));
int __db_dbopen __P((DB *, const char *, u_int32_t, int, db_pgno_t));
int __db_master_open __P((DB *,
    const char *, u_int32_t, int, DB **));
int __db_dbenv_setup __P((DB *, const char *, u_int32_t));
int __db_close __P((DB *, u_int32_t));
int __db_remove __P((DB *, const char *, const char *, u_int32_t));
int __db_rename __P((DB *,
    const char *, const char *, const char *, u_int32_t));
int __db_metabegin __P((DB *, DB_LOCK *));
int __db_metaend __P((DB *,
      DB_LOCK *, int, int (*)(DB *, void *), void *));
int __db_log_page __P((DB *,
    const char *, DB_LSN *, db_pgno_t, PAGE *));
int __db_backup_name __P((DB_ENV *,
    const char *, char **, DB_LSN *));
DB *__dblist_get __P((DB_ENV *, u_int32_t));
int __db_testcopy __P((DB *, const char *));
int __db_cursor __P((DB *, DB_TXN *, DBC **, u_int32_t));
int __db_icursor
    __P((DB *, DB_TXN *, DBTYPE, db_pgno_t, int, DBC **));
int __db_cprint __P((DB *));
int __db_fd __P((DB *, int *));
int __db_get __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
int __db_put __P((DB *, DB_TXN *, DBT *, DBT *, u_int32_t));
int __db_sync __P((DB *, u_int32_t));
int __db_c_close __P((DBC *));
int __db_c_destroy __P((DBC *));
int __db_c_count __P((DBC *, db_recno_t *, u_int32_t));
int __db_c_del __P((DBC *, u_int32_t));
int __db_c_dup __P((DBC *, DBC **, u_int32_t));
int __db_c_newopd __P((DBC *, db_pgno_t, DBC **));
int __db_c_get __P((DBC *, DBT *, DBT *, u_int32_t));
int __db_c_put __P((DBC *, DBT *, DBT *, u_int32_t));
int __db_duperr __P((DB *, u_int32_t));
int __db_pgin __P((DB_ENV *, db_pgno_t, void *, DBT *));
int __db_pgout __P((DB_ENV *, db_pgno_t, void *, DBT *));
void __db_metaswap __P((PAGE *));
int __db_byteswap __P((DB_ENV *, db_pgno_t, PAGE *, size_t, int));
int __db_dispatch __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __db_add_recovery __P((DB_ENV *,
   int (*)(DB_ENV *, DBT *, DB_LSN *, db_recops, void *), u_int32_t));
int __deprecated_recover
    __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __db_txnlist_init __P((DB_ENV *, void *));
int __db_txnlist_add __P((DB_ENV *, void *, u_int32_t, int32_t));
int __db_txnlist_remove __P((void *, u_int32_t));
int __db_txnlist_close __P((void *, int32_t, u_int32_t));
int __db_txnlist_delete __P((DB_ENV *,
    void *, char *, u_int32_t, int));
void __db_txnlist_end __P((DB_ENV *, void *));
int __db_txnlist_find __P((void *, u_int32_t));
void __db_txnlist_gen __P((void *, int));
int __db_txnlist_lsnadd __P((DB_ENV *, void *, DB_LSN *, u_int32_t));
int __db_txnlist_lsnhead __P((void *, DB_LSN **));
int __db_txnlist_lsninit __P((DB_ENV *, DB_TXNHEAD *, DB_LSN *));
int __db_add_limbo __P((DB_ENV *,
     void *, int32_t, db_pgno_t, int32_t));
int __db_do_the_limbo __P((DB_ENV *, DB_TXNHEAD *));
int __db_txnlist_pgnoadd __P((DB_ENV *, DB_TXNHEAD *,
     int32_t, u_int8_t [DB_FILE_ID_LEN], char *, db_pgno_t));
void __db_txnlist_print __P((void *));
 int __db_ditem __P((DBC *, PAGE *, u_int32_t, u_int32_t));
int __db_pitem
    __P((DBC *, PAGE *, u_int32_t, u_int32_t, DBT *, DBT *));
int __db_relink __P((DBC *, u_int32_t, PAGE *, PAGE **, int));
int __db_cursorchk __P((const DB *, u_int32_t, int));
int __db_ccountchk __P((const DB *, u_int32_t, int));
int __db_cdelchk __P((const DB *, u_int32_t, int, int));
int __db_cgetchk __P((const DB *, DBT *, DBT *, u_int32_t, int));
int __db_cputchk __P((const DB *,
   const DBT *, DBT *, u_int32_t, int, int));
int __db_closechk __P((const DB *, u_int32_t));
int __db_delchk __P((const DB *, DBT *, u_int32_t, int));
int __db_getchk __P((const DB *, const DBT *, DBT *, u_int32_t));
int __db_joinchk __P((const DB *, DBC * const *, u_int32_t));
int __db_joingetchk __P((const DB *, DBT *, u_int32_t));
int __db_putchk
   __P((const DB *, DBT *, const DBT *, u_int32_t, int, int));
int __db_removechk __P((const DB *, u_int32_t));
int __db_statchk __P((const DB *, u_int32_t));
int __db_syncchk __P((const DB *, u_int32_t));
int __db_join __P((DB *, DBC **, DBC **, u_int32_t));
int __db_new __P((DBC *, u_int32_t, PAGE **));
int __db_free __P((DBC *, PAGE *));
int __db_lprint __P((DBC *));
int __db_lget __P((DBC *,
    int, db_pgno_t, db_lockmode_t, int, DB_LOCK *));
int __dbh_am_chk __P((DB *, u_int32_t));
#ifdef HAVE_RPC
int __dbcl_init __P((DB *, DB_ENV *, u_int32_t));
#endif
int __db_goff __P((DB *, DBT *,
    u_int32_t, db_pgno_t, void **, u_int32_t *));
int __db_poff __P((DBC *, const DBT *, db_pgno_t *));
int __db_ovref __P((DBC *, db_pgno_t, int32_t));
int __db_doff __P((DBC *, db_pgno_t));
int __db_moff __P((DB *, const DBT *, db_pgno_t, u_int32_t,
    int (*)(DB *, const DBT *, const DBT *), int *));
int __db_vrfy_overflow __P((DB *, VRFY_DBINFO *, PAGE *, db_pgno_t,
    u_int32_t));
int __db_vrfy_ovfl_structure
    __P((DB *, VRFY_DBINFO *, db_pgno_t, u_int32_t, u_int32_t));
int __db_safe_goff __P((DB *, VRFY_DBINFO *, db_pgno_t,
    DBT *, void **, u_int32_t));
void __db_loadme __P((void));
int __db_dump __P((DB *, char *, char *));
int __db_prnpage __P((DB *, db_pgno_t));
int __db_prpage __P((DB *, PAGE *, u_int32_t));
void __db_pr __P((u_int8_t *, u_int32_t));
int __db_prdbt __P((DBT *, int, const char *, void *,
    int (*)(void *, const void *), int, VRFY_DBINFO *));
void __db_prflags __P((u_int32_t, const FN *, FILE *));
const char *__db_pagetype_to_string __P((u_int32_t));
int	__db_prheader __P((DB *, char *, int, int, void *,
    int (*)(void *, const void *), VRFY_DBINFO *, db_pgno_t));
int __db_prfooter __P((void *, int (*)(void *, const void *)));
int __db_addrem_recover
   __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __db_big_recover
    __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __db_ovref_recover __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __db_relink_recover
  __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __db_debug_recover __P((DB_ENV *,
    DBT *, DB_LSN *, db_recops, void *));
int __db_noop_recover __P((DB_ENV *,
     DBT *, DB_LSN *, db_recops, void *));
int __db_traverse_dup __P((DB *,
   db_pgno_t, int (*)(DB *, PAGE *, void *, int *), void *));
int __db_traverse_big __P((DB *,
    db_pgno_t, int (*)(DB *, PAGE *, void *, int *), void *));
int __db_reclaim_callback __P((DB *, PAGE *, void *, int *));
int __db_ret __P((DB *,
   PAGE *, u_int32_t, DBT *, void **, u_int32_t *));
int __db_retcopy __P((DB *, DBT *,
   void *, u_int32_t, void **, u_int32_t *));
int __db_upgrade __P((DB *, const char *, u_int32_t));
int __db_lastpgno __P((DB *, char *, DB_FH *, db_pgno_t *));
int __db_31_offdup __P((DB *, char *, DB_FH *, int, db_pgno_t *));
int __db_verify
    __P((DB *, const char *, const char *, FILE *, u_int32_t));
int  __db_verify_callback __P((void *, const void *));
int __db_verify_internal __P((DB *, const char *,
    const char *, void *, int (*)(void *, const void *), u_int32_t));
int __db_vrfy_datapage
    __P((DB *, VRFY_DBINFO *, PAGE *, db_pgno_t, u_int32_t));
int __db_vrfy_meta
    __P((DB *, VRFY_DBINFO *, DBMETA *, db_pgno_t, u_int32_t));
int __db_vrfy_struct_feedback __P((DB *, VRFY_DBINFO *));
int __db_salvage __P((DB *, VRFY_DBINFO *, db_pgno_t, PAGE *,
    void *, int (*)(void *, const void *), u_int32_t));
int __db_vrfy_inpitem __P((DB *, PAGE *,
    db_pgno_t, u_int32_t, int, u_int32_t, u_int32_t *, u_int32_t *));
int __db_vrfy_duptype
    __P((DB *, VRFY_DBINFO *, db_pgno_t, u_int32_t));
int __db_salvage_duptree __P((DB *, VRFY_DBINFO *, db_pgno_t,
    DBT *, void *, int (*)(void *, const void *), u_int32_t));
int __db_salvage_subdbpg
    __P((DB *, VRFY_DBINFO *, PAGE *, void *,
    int (*)(void *, const void *), u_int32_t));
int __db_vrfy_dbinfo_create
    __P((DB_ENV *, u_int32_t, VRFY_DBINFO **));
int __db_vrfy_dbinfo_destroy __P((VRFY_DBINFO *));
int __db_vrfy_getpageinfo
    __P((VRFY_DBINFO *, db_pgno_t, VRFY_PAGEINFO **));
int __db_vrfy_putpageinfo __P((VRFY_DBINFO *, VRFY_PAGEINFO *));
int __db_vrfy_pgset __P((DB_ENV *, u_int32_t, DB **));
int __db_vrfy_pgset_get __P((DB *, db_pgno_t, int *));
int __db_vrfy_pgset_inc __P((DB *, db_pgno_t));
int __db_vrfy_pgset_dec __P((DB *, db_pgno_t));
int __db_vrfy_pgset_next __P((DBC *, db_pgno_t *));
int __db_vrfy_childcursor __P((VRFY_DBINFO *, DBC **));
int __db_vrfy_childput
    __P((VRFY_DBINFO *, db_pgno_t, VRFY_CHILDINFO *));
int __db_vrfy_ccset __P((DBC *, db_pgno_t, VRFY_CHILDINFO **));
int __db_vrfy_ccnext __P((DBC *, VRFY_CHILDINFO **));
int __db_vrfy_ccclose __P((DBC *));
int __db_vrfy_pageinfo_create __P((VRFY_PAGEINFO **));
int  __db_salvage_init __P((VRFY_DBINFO *));
void  __db_salvage_destroy __P((VRFY_DBINFO *));
int __db_salvage_getnext
    __P((VRFY_DBINFO *, db_pgno_t *, u_int32_t *));
int __db_salvage_isdone __P((VRFY_DBINFO *, db_pgno_t));
int __db_salvage_markdone __P((VRFY_DBINFO *, db_pgno_t));
int __db_salvage_markneeded
    __P((VRFY_DBINFO *, db_pgno_t, u_int32_t));
#if defined(__cplusplus)
}
#endif
#endif /* _db_ext_h_ */
