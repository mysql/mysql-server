/* DO NOT EDIT: automatically built by dist/s_java. */
#include "java_util.h"
int __jv_fill_bt_stat(JNIEnv *jnienv, jclass cl,
    jobject jobj, struct __db_bt_stat *statp) {
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, bt_magic);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, bt_version);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, bt_metaflags);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, bt_nkeys);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, bt_ndata);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, bt_pagesize);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, bt_maxkey);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, bt_minkey);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, bt_re_len);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, bt_re_pad);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, bt_levels);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, bt_int_pg);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, bt_leaf_pg);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, bt_dup_pg);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, bt_over_pg);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, bt_free);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, bt_int_pgfree);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, bt_leaf_pgfree);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, bt_dup_pgfree);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, bt_over_pgfree);
	return (0);
}
int __jv_fill_h_stat(JNIEnv *jnienv, jclass cl,
    jobject jobj, struct __db_h_stat *statp) {
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, hash_magic);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, hash_version);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, hash_metaflags);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, hash_nkeys);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, hash_ndata);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, hash_pagesize);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, hash_ffactor);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, hash_buckets);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, hash_free);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, hash_bfree);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, hash_bigpages);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, hash_big_bfree);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, hash_overflows);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, hash_ovfl_free);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, hash_dup);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, hash_dup_free);
	return (0);
}
int __jv_fill_lock_stat(JNIEnv *jnienv, jclass cl,
    jobject jobj, struct __db_lock_stat *statp) {
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_id);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_cur_maxid);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_maxlocks);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_maxlockers);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_maxobjects);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_nmodes);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_nlocks);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_maxnlocks);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_nlockers);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_maxnlockers);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_nobjects);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_maxnobjects);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_nconflicts);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_nrequests);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_nreleases);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_nnowaits);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_ndeadlocks);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_locktimeout);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_nlocktimeouts);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_txntimeout);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_ntxntimeouts);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_region_wait);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_region_nowait);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_regsize);
	return (0);
}
int __jv_fill_log_stat(JNIEnv *jnienv, jclass cl,
    jobject jobj, struct __db_log_stat *statp) {
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_magic);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_version);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_mode);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_lg_bsize);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_lg_size);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_w_bytes);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_w_mbytes);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_wc_bytes);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_wc_mbytes);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_wcount);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_wcount_fill);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_scount);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_region_wait);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_region_nowait);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_cur_file);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_cur_offset);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_disk_file);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_disk_offset);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_regsize);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_maxcommitperflush);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_mincommitperflush);
	return (0);
}
int __jv_fill_mpool_stat(JNIEnv *jnienv, jclass cl,
    jobject jobj, struct __db_mpool_stat *statp) {
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_gbytes);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_bytes);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_ncache);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_regsize);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_map);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_cache_hit);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_cache_miss);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_page_create);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_page_in);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_page_out);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_ro_evict);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_rw_evict);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_page_trickle);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_pages);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_page_clean);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_page_dirty);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_hash_buckets);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_hash_searches);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_hash_longest);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_hash_examined);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_hash_nowait);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_hash_wait);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_hash_max_wait);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_region_nowait);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_region_wait);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_alloc);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_alloc_buckets);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_alloc_max_buckets);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_alloc_pages);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_alloc_max_pages);
	return (0);
}
int __jv_fill_qam_stat(JNIEnv *jnienv, jclass cl,
    jobject jobj, struct __db_qam_stat *statp) {
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, qs_magic);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, qs_version);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, qs_metaflags);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, qs_nkeys);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, qs_ndata);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, qs_pagesize);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, qs_extentsize);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, qs_pages);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, qs_re_len);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, qs_re_pad);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, qs_pgfree);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, qs_first_recno);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, qs_cur_recno);
	return (0);
}
int __jv_fill_rep_stat(JNIEnv *jnienv, jclass cl,
    jobject jobj, struct __db_rep_stat *statp) {
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_status);
	JAVADB_STAT_LSN(jnienv, cl, jobj, statp, st_next_lsn);
	JAVADB_STAT_LSN(jnienv, cl, jobj, statp, st_waiting_lsn);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_dupmasters);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_env_id);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_env_priority);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_gen);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_log_duplicated);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_log_queued);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_log_queued_max);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_log_queued_total);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_log_records);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_log_requested);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_master);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_master_changes);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_msgs_badgen);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_msgs_processed);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_msgs_recover);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_msgs_send_failures);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_msgs_sent);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_newsites);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_nsites);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_nthrottles);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_outdated);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_txns_applied);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_elections);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_elections_won);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_election_cur_winner);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_election_gen);
	JAVADB_STAT_LSN(jnienv, cl, jobj, statp, st_election_lsn);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_election_nsites);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_election_priority);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_election_status);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_election_tiebreaker);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_election_votes);
	return (0);
}
int __jv_fill_txn_stat(JNIEnv *jnienv, jclass cl,
    jobject jobj, struct __db_txn_stat *statp) {
	JAVADB_STAT_LSN(jnienv, cl, jobj, statp, st_last_ckp);
	JAVADB_STAT_LONG(jnienv, cl, jobj, statp, st_time_ckp);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_last_txnid);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_maxtxns);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_naborts);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_nbegins);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_ncommits);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_nactive);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_nrestores);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_maxnactive);
	JAVADB_STAT_ACTIVE(jnienv, cl, jobj, statp, st_txnarray);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_region_wait);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_region_nowait);
	JAVADB_STAT_INT(jnienv, cl, jobj, statp, st_regsize);
	return (0);
}
