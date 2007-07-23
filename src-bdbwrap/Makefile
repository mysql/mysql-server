CFLAGS = -g -W -Wall -Wno-unused
CPPFLAGS = -I../include

C_OBJS= mut_pthread.lo    \
	bt_compare.lo bt_conv.lo bt_curadj.lo bt_cursor.lo bt_delete.lo \
	bt_method.lo bt_open.lo bt_put.lo bt_rec.lo bt_reclaim.lo \
	bt_recno.lo bt_rsearch.lo bt_search.lo bt_split.lo bt_stat.lo \
	bt_upgrade.lo bt_verify.lo btree_auto.lo crdel_auto.lo \
	crdel_rec.lo db.lo db_am.lo db_auto.lo db_byteorder.lo db_cam.lo \
	db_conv.lo db_dispatch.lo db_dup.lo db_err.lo db_getlong.lo \
	db_idspace.lo db_iface.lo db_join.lo db_log2.lo db_meta.lo \
	db_method.lo db_open.lo db_overflow.lo db_pr.lo db_rec.lo \
	db_reclaim.lo db_rename.lo db_remove.lo db_ret.lo db_salloc.lo \
	db_shash.lo db_truncate.lo db_upg.lo db_upg_opd.lo db_vrfy.lo \
	db_vrfyutil.lo dbm.lo dbreg.lo dbreg_auto.lo dbreg_rec.lo \
	dbreg_util.lo env_file.lo env_method.lo env_open.lo env_recover.lo \
	env_region.lo fileops_auto.lo fop_basic.lo fop_rec.lo \
	fop_util.lo hash.lo hash_auto.lo hash_conv.lo hash_dup.lo \
	hash_func.lo hash_meta.lo hash_method.lo hash_open.lo \
	hash_page.lo hash_rec.lo hash_reclaim.lo hash_stat.lo \
	hash_upgrade.lo hash_verify.lo hmac.lo hsearch.lo lock.lo \
	lock_deadlock.lo lock_method.lo lock_region.lo lock_stat.lo \
	lock_util.lo log.lo log_archive.lo log_compare.lo log_get.lo \
	log_method.lo log_put.lo mp_alloc.lo mp_bh.lo mp_fget.lo \
	mp_fopen.lo mp_fput.lo mp_fset.lo mp_method.lo mp_region.lo \
	mp_register.lo mp_stat.lo mp_sync.lo mp_trickle.lo mutex.lo \
	os_abs.lo os_alloc.lo os_clock.lo os_config.lo os_dir.lo \
	os_errno.lo os_fid.lo os_fsync.lo os_handle.lo os_id.lo \
	os_map.lo os_method.lo os_oflags.lo os_open.lo os_region.lo \
	os_rename.lo os_root.lo os_rpath.lo os_rw.lo os_seek.lo \
	os_sleep.lo os_spin.lo os_stat.lo os_tmpdir.lo os_unlink.lo \
	qam.lo qam_auto.lo qam_conv.lo qam_files.lo qam_method.lo \
	qam_open.lo qam_rec.lo qam_stat.lo qam_upgrade.lo qam_verify.lo \
	rep_method.lo rep_record.lo rep_region.lo rep_util.lo sha1.lo \
	txn.lo txn_auto.lo txn_method.lo txn_rec.lo txn_recover.lo \
	txn_region.lo txn_stat.lo txn_util.lo xa.lo xa_db.lo xa_map.lo

MAYBENOTICECALL = noticecall.lo
install: libdb.so
	cp libdb.so ../lib/
libdb.so: ydb.lo bdbw.lo $(MAYBENOTICECALL)
	echo cc ydb.lo bdbw.lo BDB-OBJS -shared -fPIC -o libdb.so $(CFLAGS) 
	@cc ydb.lo bdbw.lo $(MAYBENOTICECALL) $(patsubst %,/home/bradley/mysql/build-bdb-with-uniquename/bdb/build_unix/%,$(C_OBJS)) -shared -fPIC -o libdb.so $(CFLAGS) 
ydb.lo: bdbw.h
bdbw.lo: CPPFLAGS=-I/home/bradley/mysql/build-bdb-with-uniquename/bdb/build_unix
%.lo: %.c
	cc $(CPPFLAGS) $< -c -fPIC -o $@ $(CFLAGS)
