/* component.cdf - dynamically updated configuration */

/*
 * NOTE: you may edit this file to alter the configuration
 * But all non-configuration information, including comments,
 * will be lost upon rebuilding this project.
 */

/* Component information */

Component INCLUDE_BERKELEYDB {
	ENTRY_POINTS	ALL_GLOBAL_SYMBOLS
	MODULES			bt_compare.o \
			bt_conv.o \
			bt_curadj.o \
			bt_cursor.o \
			bt_delete.o \
			bt_method.o \
			bt_open.o \
			bt_put.o \
			bt_rec.o \
			bt_reclaim.o \
			bt_recno.o \
			bt_rsearch.o \
			bt_search.o \
			bt_split.o \
			bt_stat.o \
			bt_upgrade.o \
			bt_verify.o \
			btree_auto.o \
			client.o \
			crdel_auto.o \
			crdel_rec.o \
			db.o \
			db_am.o \
			db_auto.o \
			db_byteorder.o \
			db_cam.o \
			db_conv.o \
			db_dispatch.o \
			db_dup.o \
			db_err.o \
			db_getlong.o \
			db_idspace.o \
			db_iface.o \
			db_join.o \
			db_log2.o \
			db_meta.o \
			db_method.o \
			db_open.o \
			db_overflow.o \
			db_pr.o \
			db_rec.o \
			db_reclaim.o \
			db_remove.o \
			db_rename.o \
			db_ret.o \
			db_salloc.o \
			db_server_clnt.o \
			db_server_xdr.o \
			db_shash.o \
			db_truncate.o \
			db_upg.o \
			db_upg_opd.o \
			db_vrfy.o \
			db_vrfyutil.o \
			dbreg.o \
			dbreg_auto.o \
			dbreg_rec.o \
			dbreg_util.o \
			env_file.o \
			env_method.o \
			env_open.o \
			env_recover.o \
			env_region.o \
			fileops_auto.o \
			fop_basic.o \
			fop_rec.o \
			fop_util.o \
			gen_client.o \
			gen_client_ret.o \
			getopt.o \
			hash.o \
			hash_auto.o \
			hash_conv.o \
			hash_dup.o \
			hash_func.o \
			hash_meta.o \
			hash_method.o \
			hash_open.o \
			hash_page.o \
			hash_rec.o \
			hash_reclaim.o \
			hash_stat.o \
			hash_upgrade.o \
			hash_verify.o \
			hmac.o \
			hsearch.o \
			lock.o \
			lock_deadlock.o \
			lock_method.o \
			lock_region.o \
			lock_stat.o \
			lock_util.o \
			log.o \
			log_archive.o \
			log_compare.o \
			log_get.o \
			log_method.o \
			log_put.o \
			mp_alloc.o \
			mp_bh.o \
			mp_fget.o \
			mp_fopen.o \
			mp_fput.o \
			mp_fset.o \
			mp_method.o \
			mp_region.o \
			mp_register.o \
			mp_stat.o \
			mp_sync.o \
			mp_trickle.o \
			mut_tas.o \
			mutex.o \
			os_alloc.o \
			os_clock.o \
			os_dir.o \
			os_errno.o \
			os_fid.o \
			os_fsync.o \
			os_handle.o \
			os_id.o \
			os_method.o \
			os_oflags.o \
			os_open.o \
			os_region.o \
			os_rename.o \
			os_root.o \
			os_rpath.o \
			os_rw.o \
			os_seek.o \
			os_sleep.o \
			os_spin.o \
			os_stat.o \
			os_tmpdir.o \
			os_unlink.o \
			os_vx_abs.o \
			os_vx_config.o \
			os_vx_map.o \
			qam.o \
			qam_auto.o \
			qam_conv.o \
			qam_files.o \
			qam_method.o \
			qam_open.o \
			qam_rec.o \
			qam_stat.o \
			qam_upgrade.o \
			qam_verify.o \
			rep_method.o \
			rep_record.o \
			rep_region.o \
			rep_util.o \
			sha1.o \
			snprintf.o \
			strcasecmp.o \
			strdup.o \
			txn.o \
			txn_auto.o \
			txn_method.o \
			txn_rec.o \
			txn_recover.o \
			txn_region.o \
			txn_stat.o \
			txn_util.o \
			util_arg.o \
			util_cache.o \
			util_log.o \
			util_sig.o \
			vsnprintf.o \
			xa.o \
			xa_db.o \
			xa_map.o
	NAME		BerkeleyDB
	PREF_DOMAIN	ANY
	_INIT_ORDER	usrComponentsInit
}

/* EntryPoint information */

/* Module information */

Module bt_compare.o {

	NAME		bt_compare.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../btree/bt_compare.c
}

Module bt_conv.o {

	NAME		bt_conv.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../btree/bt_conv.c
}

Module bt_curadj.o {

	NAME		bt_curadj.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../btree/bt_curadj.c
}

Module bt_cursor.o {

	NAME		bt_cursor.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../btree/bt_cursor.c
}

Module bt_delete.o {

	NAME		bt_delete.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../btree/bt_delete.c
}

Module bt_method.o {

	NAME		bt_method.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../btree/bt_method.c
}

Module bt_open.o {

	NAME		bt_open.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../btree/bt_open.c
}

Module bt_put.o {

	NAME		bt_put.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../btree/bt_put.c
}

Module bt_rec.o {

	NAME		bt_rec.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../btree/bt_rec.c
}

Module bt_reclaim.o {

	NAME		bt_reclaim.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../btree/bt_reclaim.c
}

Module bt_recno.o {

	NAME		bt_recno.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../btree/bt_recno.c
}

Module bt_rsearch.o {

	NAME		bt_rsearch.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../btree/bt_rsearch.c
}

Module bt_search.o {

	NAME		bt_search.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../btree/bt_search.c
}

Module bt_split.o {

	NAME		bt_split.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../btree/bt_split.c
}

Module bt_stat.o {

	NAME		bt_stat.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../btree/bt_stat.c
}

Module bt_upgrade.o {

	NAME		bt_upgrade.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../btree/bt_upgrade.c
}

Module bt_verify.o {

	NAME		bt_verify.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../btree/bt_verify.c
}

Module btree_auto.o {

	NAME		btree_auto.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../btree/btree_auto.c
}

Module getopt.o {

	NAME		getopt.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../clib/getopt.c
}

Module snprintf.o {

	NAME		snprintf.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../clib/snprintf.c
}

Module strcasecmp.o {

	NAME		strcasecmp.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../clib/strcasecmp.c
}

Module strdup.o {

	NAME		strdup.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../clib/strdup.c
}

Module vsnprintf.o {

	NAME		vsnprintf.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../clib/vsnprintf.c
}

Module db_byteorder.o {

	NAME		db_byteorder.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../common/db_byteorder.c
}

Module db_err.o {

	NAME		db_err.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../common/db_err.c
}

Module db_getlong.o {

	NAME		db_getlong.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../common/db_getlong.c
}

Module db_idspace.o {

	NAME		db_idspace.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../common/db_idspace.c
}

Module db_log2.o {

	NAME		db_log2.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../common/db_log2.c
}

Module util_arg.o {

	NAME		util_arg.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../common/util_arg.c
}

Module util_cache.o {

	NAME		util_cache.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../common/util_cache.c
}

Module util_log.o {

	NAME		util_log.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../common/util_log.c
}

Module util_sig.o {

	NAME		util_sig.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../common/util_sig.c
}

Module crdel_auto.o {

	NAME		crdel_auto.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../db/crdel_auto.c
}

Module crdel_rec.o {

	NAME		crdel_rec.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../db/crdel_rec.c
}

Module db.o {

	NAME		db.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../db/db.c
}

Module db_am.o {

	NAME		db_am.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../db/db_am.c
}

Module db_auto.o {

	NAME		db_auto.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../db/db_auto.c
}

Module db_cam.o {

	NAME		db_cam.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../db/db_cam.c
}

Module db_conv.o {

	NAME		db_conv.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../db/db_conv.c
}

Module db_dispatch.o {

	NAME		db_dispatch.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../db/db_dispatch.c
}

Module db_dup.o {

	NAME		db_dup.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../db/db_dup.c
}

Module db_iface.o {

	NAME		db_iface.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../db/db_iface.c
}

Module db_join.o {

	NAME		db_join.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../db/db_join.c
}

Module db_meta.o {

	NAME		db_meta.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../db/db_meta.c
}

Module db_method.o {

	NAME		db_method.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../db/db_method.c
}

Module db_open.o {

	NAME		db_open.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../db/db_open.c
}

Module db_overflow.o {

	NAME		db_overflow.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../db/db_overflow.c
}

Module db_pr.o {

	NAME		db_pr.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../db/db_pr.c
}

Module db_rec.o {

	NAME		db_rec.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../db/db_rec.c
}

Module db_reclaim.o {

	NAME		db_reclaim.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../db/db_reclaim.c
}

Module db_remove.o {

	NAME		db_remove.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../db/db_remove.c
}

Module db_rename.o {

	NAME		db_rename.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../db/db_rename.c
}

Module db_ret.o {

	NAME		db_ret.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../db/db_ret.c
}

Module db_truncate.o {

	NAME		db_truncate.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../db/db_truncate.c
}

Module db_upg.o {

	NAME		db_upg.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../db/db_upg.c
}

Module db_upg_opd.o {

	NAME		db_upg_opd.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../db/db_upg_opd.c
}

Module db_vrfy.o {

	NAME		db_vrfy.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../db/db_vrfy.c
}

Module db_vrfyutil.o {

	NAME		db_vrfyutil.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../db/db_vrfyutil.c
}

Module dbreg.o {

	NAME		dbreg.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../dbreg/dbreg.c
}

Module dbreg_auto.o {

	NAME		dbreg_auto.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../dbreg/dbreg_auto.c
}

Module dbreg_rec.o {

	NAME		dbreg_rec.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../dbreg/dbreg_rec.c
}

Module dbreg_util.o {

	NAME		dbreg_util.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../dbreg/dbreg_util.c
}

Module db_salloc.o {

	NAME		db_salloc.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../env/db_salloc.c
}

Module db_shash.o {

	NAME		db_shash.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../env/db_shash.c
}

Module env_file.o {

	NAME		env_file.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../env/env_file.c
}

Module env_method.o {

	NAME		env_method.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../env/env_method.c
}

Module env_open.o {

	NAME		env_open.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../env/env_open.c
}

Module env_recover.o {

	NAME		env_recover.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../env/env_recover.c
}

Module env_region.o {

	NAME		env_region.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../env/env_region.c
}

Module fileops_auto.o {

	NAME		fileops_auto.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../fileops/fileops_auto.c
}

Module fop_basic.o {

	NAME		fop_basic.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../fileops/fop_basic.c
}

Module fop_rec.o {

	NAME		fop_rec.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../fileops/fop_rec.c
}

Module fop_util.o {

	NAME		fop_util.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../fileops/fop_util.c
}

Module hash.o {

	NAME		hash.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../hash/hash.c
}

Module hash_auto.o {

	NAME		hash_auto.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../hash/hash_auto.c
}

Module hash_conv.o {

	NAME		hash_conv.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../hash/hash_conv.c
}

Module hash_dup.o {

	NAME		hash_dup.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../hash/hash_dup.c
}

Module hash_func.o {

	NAME		hash_func.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../hash/hash_func.c
}

Module hash_meta.o {

	NAME		hash_meta.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../hash/hash_meta.c
}

Module hash_method.o {

	NAME		hash_method.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../hash/hash_method.c
}

Module hash_open.o {

	NAME		hash_open.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../hash/hash_open.c
}

Module hash_page.o {

	NAME		hash_page.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../hash/hash_page.c
}

Module hash_rec.o {

	NAME		hash_rec.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../hash/hash_rec.c
}

Module hash_reclaim.o {

	NAME		hash_reclaim.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../hash/hash_reclaim.c
}

Module hash_stat.o {

	NAME		hash_stat.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../hash/hash_stat.c
}

Module hash_upgrade.o {

	NAME		hash_upgrade.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../hash/hash_upgrade.c
}

Module hash_verify.o {

	NAME		hash_verify.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../hash/hash_verify.c
}

Module hmac.o {

	NAME		hmac.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../hmac/hmac.c
}

Module sha1.o {

	NAME		sha1.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../hmac/sha1.c
}

Module hsearch.o {

	NAME		hsearch.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../hsearch/hsearch.c
}

Module lock.o {

	NAME		lock.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../lock/lock.c
}

Module lock_deadlock.o {

	NAME		lock_deadlock.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../lock/lock_deadlock.c
}

Module lock_method.o {

	NAME		lock_method.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../lock/lock_method.c
}

Module lock_region.o {

	NAME		lock_region.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../lock/lock_region.c
}

Module lock_stat.o {

	NAME		lock_stat.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../lock/lock_stat.c
}

Module lock_util.o {

	NAME		lock_util.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../lock/lock_util.c
}

Module log.o {

	NAME		log.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../log/log.c
}

Module log_archive.o {

	NAME		log_archive.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../log/log_archive.c
}

Module log_compare.o {

	NAME		log_compare.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../log/log_compare.c
}

Module log_get.o {

	NAME		log_get.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../log/log_get.c
}

Module log_method.o {

	NAME		log_method.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../log/log_method.c
}

Module log_put.o {

	NAME		log_put.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../log/log_put.c
}

Module mp_alloc.o {

	NAME		mp_alloc.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../mp/mp_alloc.c
}

Module mp_bh.o {

	NAME		mp_bh.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../mp/mp_bh.c
}

Module mp_fget.o {

	NAME		mp_fget.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../mp/mp_fget.c
}

Module mp_fopen.o {

	NAME		mp_fopen.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../mp/mp_fopen.c
}

Module mp_fput.o {

	NAME		mp_fput.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../mp/mp_fput.c
}

Module mp_fset.o {

	NAME		mp_fset.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../mp/mp_fset.c
}

Module mp_method.o {

	NAME		mp_method.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../mp/mp_method.c
}

Module mp_region.o {

	NAME		mp_region.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../mp/mp_region.c
}

Module mp_register.o {

	NAME		mp_register.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../mp/mp_register.c
}

Module mp_stat.o {

	NAME		mp_stat.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../mp/mp_stat.c
}

Module mp_sync.o {

	NAME		mp_sync.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../mp/mp_sync.c
}

Module mp_trickle.o {

	NAME		mp_trickle.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../mp/mp_trickle.c
}

Module mut_tas.o {

	NAME		mut_tas.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../mutex/mut_tas.c
}

Module mutex.o {

	NAME		mutex.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../mutex/mutex.c
}

Module os_alloc.o {

	NAME		os_alloc.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../os/os_alloc.c
}

Module os_clock.o {

	NAME		os_clock.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../os/os_clock.c
}

Module os_dir.o {

	NAME		os_dir.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../os/os_dir.c
}

Module os_errno.o {

	NAME		os_errno.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../os/os_errno.c
}

Module os_fid.o {

	NAME		os_fid.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../os/os_fid.c
}

Module os_fsync.o {

	NAME		os_fsync.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../os/os_fsync.c
}

Module os_handle.o {

	NAME		os_handle.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../os/os_handle.c
}

Module os_id.o {

	NAME		os_id.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../os/os_id.c
}

Module os_method.o {

	NAME		os_method.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../os/os_method.c
}

Module os_oflags.o {

	NAME		os_oflags.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../os/os_oflags.c
}

Module os_open.o {

	NAME		os_open.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../os/os_open.c
}

Module os_region.o {

	NAME		os_region.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../os/os_region.c
}

Module os_rename.o {

	NAME		os_rename.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../os/os_rename.c
}

Module os_root.o {

	NAME		os_root.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../os/os_root.c
}

Module os_rpath.o {

	NAME		os_rpath.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../os/os_rpath.c
}

Module os_rw.o {

	NAME		os_rw.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../os/os_rw.c
}

Module os_seek.o {

	NAME		os_seek.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../os/os_seek.c
}

Module os_sleep.o {

	NAME		os_sleep.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../os/os_sleep.c
}

Module os_spin.o {

	NAME		os_spin.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../os/os_spin.c
}

Module os_stat.o {

	NAME		os_stat.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../os/os_stat.c
}

Module os_tmpdir.o {

	NAME		os_tmpdir.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../os/os_tmpdir.c
}

Module os_unlink.o {

	NAME		os_unlink.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../os/os_unlink.c
}

Module os_vx_abs.o {

	NAME		os_vx_abs.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../os_vxworks/os_vx_abs.c
}

Module os_vx_config.o {

	NAME		os_vx_config.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../os_vxworks/os_vx_config.c
}

Module os_vx_map.o {

	NAME		os_vx_map.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../os_vxworks/os_vx_map.c
}

Module qam.o {

	NAME		qam.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../qam/qam.c
}

Module qam_auto.o {

	NAME		qam_auto.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../qam/qam_auto.c
}

Module qam_conv.o {

	NAME		qam_conv.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../qam/qam_conv.c
}

Module qam_files.o {

	NAME		qam_files.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../qam/qam_files.c
}

Module qam_method.o {

	NAME		qam_method.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../qam/qam_method.c
}

Module qam_open.o {

	NAME		qam_open.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../qam/qam_open.c
}

Module qam_rec.o {

	NAME		qam_rec.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../qam/qam_rec.c
}

Module qam_stat.o {

	NAME		qam_stat.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../qam/qam_stat.c
}

Module qam_upgrade.o {

	NAME		qam_upgrade.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../qam/qam_upgrade.c
}

Module qam_verify.o {

	NAME		qam_verify.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../qam/qam_verify.c
}

Module rep_method.o {

	NAME		rep_method.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../rep/rep_method.c
}

Module rep_record.o {

	NAME		rep_record.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../rep/rep_record.c
}

Module rep_region.o {

	NAME		rep_region.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../rep/rep_region.c
}

Module rep_util.o {

	NAME		rep_util.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../rep/rep_util.c
}

Module client.o {

	NAME		client.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../rpc_client/client.c
}

Module db_server_clnt.o {

	NAME		db_server_clnt.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../rpc_client/db_server_clnt.c
}

Module gen_client.o {

	NAME		gen_client.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../rpc_client/gen_client.c
}

Module gen_client_ret.o {

	NAME		gen_client_ret.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../rpc_client/gen_client_ret.c
}

Module db_server_xdr.o {

	NAME		db_server_xdr.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../rpc_server/c/db_server_xdr.c
}

Module txn.o {

	NAME		txn.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../txn/txn.c
}

Module txn_auto.o {

	NAME		txn_auto.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../txn/txn_auto.c
}

Module txn_method.o {

	NAME		txn_method.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../txn/txn_method.c
}

Module txn_rec.o {

	NAME		txn_rec.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../txn/txn_rec.c
}

Module txn_recover.o {

	NAME		txn_recover.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../txn/txn_recover.c
}

Module txn_region.o {

	NAME		txn_region.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../txn/txn_region.c
}

Module txn_stat.o {

	NAME		txn_stat.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../txn/txn_stat.c
}

Module txn_util.o {

	NAME		txn_util.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../txn/txn_util.c
}

Module xa.o {

	NAME		xa.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../xa/xa.c
}

Module xa_db.o {

	NAME		xa_db.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../xa/xa_db.c
}

Module xa_map.o {

	NAME		xa_map.o
	SRC_PATH_NAME	$(PRJ_DIR)/../../xa/xa_map.c
}

/* Parameter information */

