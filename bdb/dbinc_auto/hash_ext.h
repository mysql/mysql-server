/* DO NOT EDIT: automatically built by dist/s_include. */
#ifndef	_hash_ext_h_
#define	_hash_ext_h_

#if defined(__cplusplus)
extern "C" {
#endif

int __ham_quick_delete __P((DBC *));
int __ham_c_init __P((DBC *));
int __ham_c_count __P((DBC *, db_recno_t *));
int __ham_c_dup __P((DBC *, DBC *));
u_int32_t __ham_call_hash __P((DBC *, u_int8_t *, int32_t));
int __ham_init_dbt __P((DB_ENV *, DBT *, u_int32_t, void **, u_int32_t *));
int __ham_c_update __P((DBC *, u_int32_t, int, int));
int __ham_get_clist __P((DB *, db_pgno_t, u_int32_t, DBC ***));
int __ham_insdel_log __P((DB *, DB_TXN *, DB_LSN *, u_int32_t, u_int32_t, db_pgno_t, u_int32_t, DB_LSN *, const DBT *, const DBT *));
int __ham_insdel_getpgnos __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_insdel_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_insdel_read __P((DB_ENV *, void *, __ham_insdel_args **));
int __ham_newpage_log __P((DB *, DB_TXN *, DB_LSN *, u_int32_t, u_int32_t, db_pgno_t, DB_LSN *, db_pgno_t, DB_LSN *, db_pgno_t, DB_LSN *));
int __ham_newpage_getpgnos __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_newpage_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_newpage_read __P((DB_ENV *, void *, __ham_newpage_args **));
int __ham_splitdata_log __P((DB *, DB_TXN *, DB_LSN *, u_int32_t, u_int32_t, db_pgno_t, const DBT *, DB_LSN *));
int __ham_splitdata_getpgnos __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_splitdata_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_splitdata_read __P((DB_ENV *, void *, __ham_splitdata_args **));
int __ham_replace_log __P((DB *, DB_TXN *, DB_LSN *, u_int32_t, db_pgno_t, u_int32_t, DB_LSN *, int32_t, const DBT *, const DBT *, u_int32_t));
int __ham_replace_getpgnos __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_replace_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_replace_read __P((DB_ENV *, void *, __ham_replace_args **));
int __ham_copypage_log __P((DB *, DB_TXN *, DB_LSN *, u_int32_t, db_pgno_t, DB_LSN *, db_pgno_t, DB_LSN *, db_pgno_t, DB_LSN *, const DBT *));
int __ham_copypage_getpgnos __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_copypage_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_copypage_read __P((DB_ENV *, void *, __ham_copypage_args **));
int __ham_metagroup_log __P((DB *, DB_TXN *, DB_LSN *, u_int32_t, u_int32_t, db_pgno_t, DB_LSN *, db_pgno_t, DB_LSN *, db_pgno_t, DB_LSN *, u_int32_t));
int __ham_metagroup_getpgnos __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_metagroup_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_metagroup_read __P((DB_ENV *, void *, __ham_metagroup_args **));
int __ham_groupalloc_log __P((DB *, DB_TXN *, DB_LSN *, u_int32_t, DB_LSN *, db_pgno_t, u_int32_t, db_pgno_t));
int __ham_groupalloc_getpgnos __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_groupalloc_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_groupalloc_read __P((DB_ENV *, void *, __ham_groupalloc_args **));
int __ham_curadj_log __P((DB *, DB_TXN *, DB_LSN *, u_int32_t, db_pgno_t, u_int32_t, u_int32_t, u_int32_t, int, int, u_int32_t));
int __ham_curadj_getpgnos __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_curadj_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_curadj_read __P((DB_ENV *, void *, __ham_curadj_args **));
int __ham_chgpg_log __P((DB *, DB_TXN *, DB_LSN *, u_int32_t, db_ham_mode, db_pgno_t, db_pgno_t, u_int32_t, u_int32_t));
int __ham_chgpg_getpgnos __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_chgpg_print __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_chgpg_read __P((DB_ENV *, void *, __ham_chgpg_args **));
int __ham_init_print __P((DB_ENV *, int (***)(DB_ENV *, DBT *, DB_LSN *, db_recops, void *), size_t *));
int __ham_init_getpgnos __P((DB_ENV *, int (***)(DB_ENV *, DBT *, DB_LSN *, db_recops, void *), size_t *));
int __ham_init_recover __P((DB_ENV *, int (***)(DB_ENV *, DBT *, DB_LSN *, db_recops, void *), size_t *));
int __ham_pgin __P((DB_ENV *, DB *, db_pgno_t, void *, DBT *));
int __ham_pgout __P((DB_ENV *, DB *, db_pgno_t, void *, DBT *));
int __ham_mswap __P((void *));
int __ham_add_dup __P((DBC *, DBT *, u_int32_t, db_pgno_t *));
int __ham_dup_convert __P((DBC *));
int __ham_make_dup __P((DB_ENV *, const DBT *, DBT *d, void **, u_int32_t *));
void __ham_dsearch __P((DBC *, DBT *, u_int32_t *, int *, u_int32_t));
void __ham_cprint __P((DBC *));
u_int32_t __ham_func2 __P((DB *, const void *, u_int32_t));
u_int32_t __ham_func3 __P((DB *, const void *, u_int32_t));
u_int32_t __ham_func4 __P((DB *, const void *, u_int32_t));
u_int32_t __ham_func5 __P((DB *, const void *, u_int32_t));
u_int32_t __ham_test __P((DB *, const void *, u_int32_t));
int __ham_get_meta __P((DBC *));
int __ham_release_meta __P((DBC *));
int __ham_dirty_meta __P((DBC *));
int __ham_db_create __P((DB *));
int __ham_db_close __P((DB *));
int __ham_open __P((DB *, DB_TXN *, const char * name, db_pgno_t, u_int32_t));
int __ham_metachk __P((DB *, const char *, HMETA *));
int __ham_new_file __P((DB *, DB_TXN *, DB_FH *, const char *));
int __ham_new_subdb __P((DB *, DB *, DB_TXN *));
int __ham_item __P((DBC *, db_lockmode_t, db_pgno_t *));
int __ham_item_reset __P((DBC *));
void __ham_item_init __P((DBC *));
int __ham_item_last __P((DBC *, db_lockmode_t, db_pgno_t *));
int __ham_item_first __P((DBC *, db_lockmode_t, db_pgno_t *));
int __ham_item_prev __P((DBC *, db_lockmode_t, db_pgno_t *));
int __ham_item_next __P((DBC *, db_lockmode_t, db_pgno_t *));
void __ham_putitem __P((DB *, PAGE *p, const DBT *, int));
void __ham_reputpair  __P((DB *, PAGE *, u_int32_t, const DBT *, const DBT *));
int __ham_del_pair __P((DBC *, int));
int __ham_replpair __P((DBC *, DBT *, u_int32_t));
void __ham_onpage_replace __P((DB *, PAGE *, u_int32_t, int32_t, int32_t,  DBT *));
int __ham_split_page __P((DBC *, u_int32_t, u_int32_t));
int __ham_add_el __P((DBC *, const DBT *, const DBT *, int));
void __ham_copy_item __P((DB *, PAGE *, u_int32_t, PAGE *));
int __ham_add_ovflpage __P((DBC *, PAGE *, int, PAGE **));
int __ham_get_cpage __P((DBC *, db_lockmode_t));
int __ham_next_cpage __P((DBC *, db_pgno_t, int));
int __ham_lock_bucket __P((DBC *, db_lockmode_t));
void __ham_dpair __P((DB *, PAGE *, u_int32_t));
int __ham_insdel_recover __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_newpage_recover __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_replace_recover __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_splitdata_recover __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_copypage_recover __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_metagroup_recover __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_groupalloc_recover __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_curadj_recover __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_chgpg_recover __P((DB_ENV *, DBT *, DB_LSN *, db_recops, void *));
int __ham_reclaim __P((DB *, DB_TXN *txn));
int __ham_truncate __P((DB *, DB_TXN *txn, u_int32_t *));
int __ham_stat __P((DB *, void *, u_int32_t));
int __ham_traverse __P((DBC *, db_lockmode_t, int (*)(DB *, PAGE *, void *, int *), void *, int));
int __ham_30_hashmeta __P((DB *, char *, u_int8_t *));
int __ham_30_sizefix __P((DB *, DB_FH *, char *, u_int8_t *));
int __ham_31_hashmeta __P((DB *, char *, u_int32_t, DB_FH *, PAGE *, int *));
int __ham_31_hash __P((DB *, char *, u_int32_t, DB_FH *, PAGE *, int *));
int __ham_vrfy_meta __P((DB *, VRFY_DBINFO *, HMETA *, db_pgno_t, u_int32_t));
int __ham_vrfy __P((DB *, VRFY_DBINFO *, PAGE *, db_pgno_t, u_int32_t));
int __ham_vrfy_structure __P((DB *, VRFY_DBINFO *, db_pgno_t, u_int32_t));
int __ham_vrfy_hashing __P((DB *, u_int32_t, HMETA *, u_int32_t, db_pgno_t, u_int32_t, u_int32_t (*) __P((DB *, const void *, u_int32_t))));
int __ham_salvage __P((DB *, VRFY_DBINFO *, db_pgno_t, PAGE *, void *, int (*)(void *, const void *), u_int32_t));
int __ham_meta2pgset __P((DB *, VRFY_DBINFO *, HMETA *, u_int32_t, DB *));

#if defined(__cplusplus)
}
#endif
#endif /* !_hash_ext_h_ */
