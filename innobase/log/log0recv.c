/******************************************************
Recovery

(c) 1997 Innobase Oy

Created 9/20/1997 Heikki Tuuri
*******************************************************/

#include "log0recv.h"

#ifdef UNIV_NONINL
#include "log0recv.ic"
#endif

#include "mem0mem.h"
#include "buf0buf.h"
#include "buf0flu.h"
#include "buf0rea.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "mtr0mtr.h"
#include "mtr0log.h"
#include "page0page.h"
#include "page0cur.h"
#include "btr0btr.h"
#include "btr0cur.h"
#include "ibuf0ibuf.h"
#include "trx0undo.h"
#include "trx0rec.h"
#include "trx0roll.h"
#include "btr0cur.h"
#include "btr0cur.h"
#include "btr0cur.h"
#include "dict0boot.h"
#include "fil0fil.h"

#ifdef UNIV_HOTBACKUP
/* This is set to FALSE if the backup was originally taken with the
ibbackup --include regexp option: then we do not want to create tables in
directories which were not included */
ibool	recv_replay_file_ops	= TRUE;
#endif /* UNIV_HOTBACKUP */

/* Log records are stored in the hash table in chunks at most of this size;
this must be less than UNIV_PAGE_SIZE as it is stored in the buffer pool */
#define RECV_DATA_BLOCK_SIZE	(MEM_MAX_ALLOC_IN_BUF - sizeof(recv_data_t))

/* Read-ahead area in applying log records to file pages */
#define RECV_READ_AHEAD_AREA	32

recv_sys_t*	recv_sys = NULL;
ibool		recv_recovery_on = FALSE;
ibool		recv_recovery_from_backup_on = FALSE;

ibool		recv_needed_recovery = FALSE;

ibool		recv_lsn_checks_on = FALSE;

/* If the following is TRUE, the buffer pool file pages must be invalidated
after recovery and no ibuf operations are allowed; this becomes TRUE if
the log record hash table becomes too full, and log records must be merged
to file pages already before the recovery is finished: in this case no
ibuf operations are allowed, as they could modify the pages read in the
buffer pool before the pages have been recovered to the up-to-date state */

/* Recovery is running and no operations on the log files are allowed
yet: the variable name is misleading */

ibool	recv_no_ibuf_operations = FALSE;

/* The following counter is used to decide when to print info on
log scan */
ulint	recv_scan_print_counter	= 0;

ibool	recv_is_from_backup	= FALSE;
#ifdef UNIV_HOTBACKUP
ibool	recv_is_making_a_backup = FALSE;
#else
# define recv_is_making_a_backup FALSE
#endif /* UNIV_HOTBACKUP */

ulint	recv_previous_parsed_rec_type	= 999999;
ulint	recv_previous_parsed_rec_offset	= 0;
ulint	recv_previous_parsed_rec_is_multi = 0;

ulint	recv_max_parsed_page_no		= 0;

/* This many frames must be left free in the buffer pool when we scan
the log and store the scanned log records in the buffer pool: we will
use these free frames to read in pages when we start applying the
log records to the database. */

ulint  recv_n_pool_free_frames         = 256;

/* The maximum lsn we see for a page during the recovery process. If this
is bigger than the lsn we are able to scan up to, that is an indication that
the recovery failed and the database may be corrupt. */

dulint	recv_max_page_lsn;

/************************************************************
Creates the recovery system. */

void
recv_sys_create(void)
/*=================*/
{
	if (recv_sys != NULL) {

		return;
	}

	recv_sys = mem_alloc(sizeof(recv_sys_t));

	mutex_create(&(recv_sys->mutex));
	mutex_set_level(&(recv_sys->mutex), SYNC_RECV);

	recv_sys->heap = NULL;
	recv_sys->addr_hash = NULL;
}

/************************************************************
Inits the recovery system for a recovery operation. */

void
recv_sys_init(
/*==========*/
	ibool	recover_from_backup,	/* in: TRUE if this is called
					to recover from a hot backup */
	ulint	available_memory)	/* in: available memory in bytes */
{
	if (recv_sys->heap != NULL) {

		return;
	}

	mutex_enter(&(recv_sys->mutex));

	if (!recover_from_backup) {
		recv_sys->heap = mem_heap_create_in_buffer(256);
	} else {
		recv_sys->heap = mem_heap_create(256);
		recv_is_from_backup = TRUE;
	}

	recv_sys->buf = ut_malloc(RECV_PARSING_BUF_SIZE);
	recv_sys->len = 0;
	recv_sys->recovered_offset = 0;

	recv_sys->addr_hash = hash_create(available_memory / 64);
	recv_sys->n_addrs = 0;
	
	recv_sys->apply_log_recs = FALSE;
	recv_sys->apply_batch_on = FALSE;

	recv_sys->last_block_buf_start = mem_alloc(2 * OS_FILE_LOG_BLOCK_SIZE);

	recv_sys->last_block = ut_align(recv_sys->last_block_buf_start,
						OS_FILE_LOG_BLOCK_SIZE);
	recv_sys->found_corrupt_log = FALSE;

	recv_max_page_lsn = ut_dulint_zero;

	mutex_exit(&(recv_sys->mutex));
}

/************************************************************
Empties the hash table when it has been fully processed. */
static
void
recv_sys_empty_hash(void)
/*=====================*/
{
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(recv_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */
	if (recv_sys->n_addrs != 0) {
		fprintf(stderr,
"InnoDB: Error: %lu pages with log records were left unprocessed!\n"
"InnoDB: Maximum page number with log records on it %lu\n",
			(ulong) recv_sys->n_addrs, 
			(ulong) recv_max_parsed_page_no);
		ut_error;
	}
	
	hash_table_free(recv_sys->addr_hash);
	mem_heap_empty(recv_sys->heap);

	recv_sys->addr_hash = hash_create(buf_pool_get_curr_size() / 256);
}

/************************************************************
Frees the recovery system. */
static
void
recv_sys_free(void)
/*===============*/
{
	mutex_enter(&(recv_sys->mutex));
	
	hash_table_free(recv_sys->addr_hash);
	mem_heap_free(recv_sys->heap);
	ut_free(recv_sys->buf);
	mem_free(recv_sys->last_block_buf_start);

	recv_sys->addr_hash = NULL;
	recv_sys->heap = NULL;

	mutex_exit(&(recv_sys->mutex));
}

/************************************************************
Truncates possible corrupted or extra records from a log group. */
static
void
recv_truncate_group(
/*================*/
	log_group_t*	group,		/* in: log group */
	dulint		recovered_lsn,	/* in: recovery succeeded up to this
					lsn */
	dulint		limit_lsn,	/* in: this was the limit for
					recovery */
	dulint		checkpoint_lsn,	/* in: recovery was started from this
					checkpoint */
	dulint		archived_lsn)	/* in: the log has been archived up to
					this lsn */
{
	dulint	start_lsn;
	dulint	end_lsn;
	dulint	finish_lsn1;
	dulint	finish_lsn2;
	dulint	finish_lsn;
	ulint	len;
	ulint	i;

	if (ut_dulint_cmp(archived_lsn, ut_dulint_max) == 0) {
		/* Checkpoint was taken in the NOARCHIVELOG mode */
		archived_lsn = checkpoint_lsn;
	}

	finish_lsn1 = ut_dulint_add(ut_dulint_align_down(archived_lsn,
						OS_FILE_LOG_BLOCK_SIZE),
					log_group_get_capacity(group));
					
	finish_lsn2 = ut_dulint_add(ut_dulint_align_up(recovered_lsn,
						OS_FILE_LOG_BLOCK_SIZE),
					recv_sys->last_log_buf_size);

	if (ut_dulint_cmp(limit_lsn, ut_dulint_max) != 0) {
		/* We do not know how far we should erase log records: erase
		as much as possible */

		finish_lsn = finish_lsn1;
	} else {
		/* It is enough to erase the length of the log buffer */
		finish_lsn = ut_dulint_get_min(finish_lsn1, finish_lsn2);
	}
				
	ut_a(RECV_SCAN_SIZE <= log_sys->buf_size);	

	/* Write the log buffer full of zeros */
	for (i = 0; i < RECV_SCAN_SIZE; i++) {

		*(log_sys->buf + i) = '\0';
	}

	start_lsn = ut_dulint_align_down(recovered_lsn,
						OS_FILE_LOG_BLOCK_SIZE);
	
	if (ut_dulint_cmp(start_lsn, recovered_lsn) != 0) {
		/* Copy the last incomplete log block to the log buffer and
		edit its data length: */

		ut_memcpy(log_sys->buf, recv_sys->last_block,
						OS_FILE_LOG_BLOCK_SIZE);
		log_block_set_data_len(log_sys->buf,
				ut_dulint_minus(recovered_lsn, start_lsn));
	}
				
	if (ut_dulint_cmp(start_lsn, finish_lsn) >= 0) {

		return;
	}

    	for (;;) {
		end_lsn = ut_dulint_add(start_lsn, RECV_SCAN_SIZE);
    	
		if (ut_dulint_cmp(end_lsn, finish_lsn) > 0) {

			end_lsn = finish_lsn;
		}

		len = ut_dulint_minus(end_lsn, start_lsn);
		
		log_group_write_buf(group, log_sys->buf, len, start_lsn, 0);
		if (ut_dulint_cmp(end_lsn, finish_lsn) >= 0) {

			return;
		}

		/* Write the log buffer full of zeros */
		for (i = 0; i < RECV_SCAN_SIZE; i++) {

			*(log_sys->buf + i) = '\0';
		}

		start_lsn = end_lsn;
	}
}

/************************************************************
Copies the log segment between group->recovered_lsn and recovered_lsn from the
most up-to-date log group to group, so that it contains the latest log data. */
static
void
recv_copy_group(
/*============*/
	log_group_t*	up_to_date_group,	/* in: the most up-to-date log
						group */
	log_group_t*	group,			/* in: copy to this log
						group */
	dulint		recovered_lsn)		/* in: recovery succeeded up
						to this lsn */
{
	dulint	start_lsn;
	dulint	end_lsn;
	ulint	len;

	if (ut_dulint_cmp(group->scanned_lsn, recovered_lsn) >= 0) {

		return;
	}
					
	ut_a(RECV_SCAN_SIZE <= log_sys->buf_size);

	start_lsn = ut_dulint_align_down(group->scanned_lsn,
						OS_FILE_LOG_BLOCK_SIZE);
    	for (;;) {
		end_lsn = ut_dulint_add(start_lsn, RECV_SCAN_SIZE);
    	
		if (ut_dulint_cmp(end_lsn, recovered_lsn) > 0) {
			end_lsn = ut_dulint_align_up(recovered_lsn,
						OS_FILE_LOG_BLOCK_SIZE);
		}

		log_group_read_log_seg(LOG_RECOVER, log_sys->buf,
					up_to_date_group, start_lsn, end_lsn);

		len = ut_dulint_minus(end_lsn, start_lsn);
		
		log_group_write_buf(group, log_sys->buf, len, start_lsn, 0);
		
		if (ut_dulint_cmp(end_lsn, recovered_lsn) >= 0) {

			return;
		}

		start_lsn = end_lsn;
	}
}

/************************************************************
Copies a log segment from the most up-to-date log group to the other log
groups, so that they all contain the latest log data. Also writes the info
about the latest checkpoint to the groups, and inits the fields in the group
memory structs to up-to-date values. */
static
void
recv_synchronize_groups(
/*====================*/
	log_group_t*	up_to_date_group)	/* in: the most up-to-date
						log group */
{
	log_group_t*	group;
	dulint		start_lsn;
	dulint		end_lsn;
	dulint		recovered_lsn;
	dulint		limit_lsn;

	recovered_lsn = recv_sys->recovered_lsn;
	limit_lsn = recv_sys->limit_lsn;

	/* Read the last recovered log block to the recovery system buffer:
	the block is always incomplete */

	start_lsn = ut_dulint_align_down(recovered_lsn,
						OS_FILE_LOG_BLOCK_SIZE);
	end_lsn = ut_dulint_align_up(recovered_lsn, OS_FILE_LOG_BLOCK_SIZE);

	ut_a(ut_dulint_cmp(start_lsn, end_lsn) != 0);

	log_group_read_log_seg(LOG_RECOVER, recv_sys->last_block,
					up_to_date_group, start_lsn, end_lsn);

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	while (group) {
		if (group != up_to_date_group) {

			/* Copy log data if needed */

			recv_copy_group(group, up_to_date_group,
								recovered_lsn);
		}

		/* Update the fields in the group struct to correspond to
		recovered_lsn */

		log_group_set_fields(group, recovered_lsn);

		group = UT_LIST_GET_NEXT(log_groups, group);
	}

	/* Copy the checkpoint info to the groups; remember that we have
	incremented checkpoint_no by one, and the info will not be written
	over the max checkpoint info, thus making the preservation of max
	checkpoint info on disk certain */

	log_groups_write_checkpoint_info();

	mutex_exit(&(log_sys->mutex));

	/* Wait for the checkpoint write to complete */
	rw_lock_s_lock(&(log_sys->checkpoint_lock));
	rw_lock_s_unlock(&(log_sys->checkpoint_lock));

	mutex_enter(&(log_sys->mutex));
}

/***************************************************************************
Checks the consistency of the checkpoint info */
static
ibool
recv_check_cp_is_consistent(
/*========================*/
			/* out: TRUE if ok */
	byte*	buf)	/* in: buffer containing checkpoint info */
{
	ulint	fold;

	fold = ut_fold_binary(buf, LOG_CHECKPOINT_CHECKSUM_1);

	if ((fold & 0xFFFFFFFFUL) != mach_read_from_4(buf
				+ LOG_CHECKPOINT_CHECKSUM_1)) {		
		return(FALSE);
	}

	fold = ut_fold_binary(buf + LOG_CHECKPOINT_LSN,
			LOG_CHECKPOINT_CHECKSUM_2 - LOG_CHECKPOINT_LSN);

	if ((fold & 0xFFFFFFFFUL) != mach_read_from_4(buf
					+ LOG_CHECKPOINT_CHECKSUM_2)) {
		return(FALSE);
	}

	return(TRUE);
}

/************************************************************
Looks for the maximum consistent checkpoint from the log groups. */
static
ulint
recv_find_max_checkpoint(
/*=====================*/
					/* out: error code or DB_SUCCESS */
	log_group_t**	max_group,	/* out: max group */
	ulint*		max_field)	/* out: LOG_CHECKPOINT_1 or
					LOG_CHECKPOINT_2 */
{
	log_group_t*	group;
	dulint		max_no;
	dulint		checkpoint_no;
	ulint		field;
	byte*		buf;
	
	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	max_no = ut_dulint_zero;
	*max_group = NULL;
	*max_field = 0;
	
	buf = log_sys->checkpoint_buf;
	
	while (group) {
		group->state = LOG_GROUP_CORRUPTED;
	
		for (field = LOG_CHECKPOINT_1; field <= LOG_CHECKPOINT_2;
				field += LOG_CHECKPOINT_2 - LOG_CHECKPOINT_1) {
	
			log_group_read_checkpoint_info(group, field);

			if (!recv_check_cp_is_consistent(buf)) {
#ifdef UNIV_DEBUG
				if (log_debug_writes) {
					fprintf(stderr, 
	    "InnoDB: Checkpoint in group %lu at %lu invalid, %lu\n",
						(ulong) group->id,
						(ulong) field,
                                 (ulong) mach_read_from_4(buf
					      + LOG_CHECKPOINT_CHECKSUM_1));

				}
#endif /* UNIV_DEBUG */
				goto not_consistent;
			}

			group->state = LOG_GROUP_OK;

			group->lsn = mach_read_from_8(buf
						+ LOG_CHECKPOINT_LSN);
			group->lsn_offset = mach_read_from_4(buf
						+ LOG_CHECKPOINT_OFFSET);
			checkpoint_no =
				mach_read_from_8(buf + LOG_CHECKPOINT_NO);

#ifdef UNIV_DEBUG
			if (log_debug_writes) {
				fprintf(stderr, 
			"InnoDB: Checkpoint number %lu found in group %lu\n",
				(ulong) ut_dulint_get_low(checkpoint_no),
				(ulong) group->id);
			}
#endif /* UNIV_DEBUG */

			if (ut_dulint_cmp(checkpoint_no, max_no) >= 0) {
				*max_group = group;
				*max_field = field;
				max_no = checkpoint_no;
			}

		not_consistent:
			;
		}

		group = UT_LIST_GET_NEXT(log_groups, group);
	}

	if (*max_group == NULL) {

		fprintf(stderr,
"InnoDB: No valid checkpoint found.\n"
"InnoDB: If this error appears when you are creating an InnoDB database,\n"
"InnoDB: the problem may be that during an earlier attempt you managed\n"
"InnoDB: to create the InnoDB data files, but log file creation failed.\n"
"InnoDB: If that is the case, please refer to\n"
"InnoDB: http://dev.mysql.com/doc/refman/5.0/en/error-creating-innodb.html\n");
		return(DB_ERROR);
	}

	return(DB_SUCCESS);
}

/***********************************************************************
Reads the checkpoint info needed in hot backup. */

ibool
recv_read_cp_info_for_backup(
/*=========================*/
			/* out: TRUE if success */
	byte*	hdr,	/* in: buffer containing the log group header */
	dulint*	lsn,	/* out: checkpoint lsn */
	ulint*	offset,	/* out: checkpoint offset in the log group */
	ulint*	fsp_limit,/* out: fsp limit of space 0, 1000000000 if the
			database is running with < version 3.23.50 of InnoDB */
	dulint*	cp_no,	/* out: checkpoint number */
	dulint*	first_header_lsn)
			/* out: lsn of of the start of the first log file */
{
	ulint	max_cp		= 0;
	dulint	max_cp_no	= ut_dulint_zero;
	byte*	cp_buf;

	cp_buf = hdr + LOG_CHECKPOINT_1;

	if (recv_check_cp_is_consistent(cp_buf)) {
		max_cp_no = mach_read_from_8(cp_buf + LOG_CHECKPOINT_NO);
		max_cp = LOG_CHECKPOINT_1;
	}

	cp_buf = hdr + LOG_CHECKPOINT_2;

	if (recv_check_cp_is_consistent(cp_buf)) {
		if (ut_dulint_cmp(mach_read_from_8(cp_buf + LOG_CHECKPOINT_NO),
					max_cp_no) > 0) {
			max_cp = LOG_CHECKPOINT_2;
		}
	}

	if (max_cp == 0) {
		return(FALSE);
	}

	cp_buf = hdr + max_cp;
	
	*lsn = mach_read_from_8(cp_buf + LOG_CHECKPOINT_LSN);
	*offset = mach_read_from_4(cp_buf + LOG_CHECKPOINT_OFFSET);

	/* If the user is running a pre-3.23.50 version of InnoDB, its
	checkpoint data does not contain the fsp limit info */
	if (mach_read_from_4(cp_buf + LOG_CHECKPOINT_FSP_MAGIC_N)
	    == LOG_CHECKPOINT_FSP_MAGIC_N_VAL) {
	
		*fsp_limit = mach_read_from_4(
				cp_buf + LOG_CHECKPOINT_FSP_FREE_LIMIT);

		if (*fsp_limit == 0) {
			*fsp_limit = 1000000000;
		}	
	} else {
		*fsp_limit = 1000000000;
	}

/*	fprintf(stderr, "fsp limit %lu MB\n", *fsp_limit); */

	*cp_no = mach_read_from_8(cp_buf + LOG_CHECKPOINT_NO);

	*first_header_lsn = mach_read_from_8(hdr + LOG_FILE_START_LSN);

	return(TRUE);
}

/**********************************************************
Checks the 4-byte checksum to the trailer checksum field of a log block.
We also accept a log block in the old format < InnoDB-3.23.52 where the
checksum field contains the log block number. */
static
ibool
log_block_checksum_is_ok_or_old_format(
/*===================================*/
			/* out: TRUE if ok, or if the log block may be in the
			format of InnoDB version < 3.23.52 */
	byte*	block)	/* in: pointer to a log block */
{
#ifdef UNIV_LOG_DEBUG
	return(TRUE);
#endif /* UNIV_LOG_DEBUG */
	if (log_block_calc_checksum(block) == log_block_get_checksum(block)) {

		return(TRUE);
	}

	if (log_block_get_hdr_no(block) == log_block_get_checksum(block)) {

		/* We assume the log block is in the format of
		InnoDB version < 3.23.52 and the block is ok */
/*
		fprintf(stderr,
"InnoDB: Scanned old format < InnoDB-3.23.52 log block number %lu\n",
			log_block_get_hdr_no(block));
*/
		return(TRUE);
	}

	return(FALSE);
}

/***********************************************************************
Scans the log segment and n_bytes_scanned is set to the length of valid
log scanned. */

void
recv_scan_log_seg_for_backup(
/*=========================*/
	byte*		buf,		/* in: buffer containing log data */
	ulint		buf_len,	/* in: data length in that buffer */
	dulint*		scanned_lsn,	/* in/out: lsn of buffer start,
					we return scanned lsn */
	ulint*		scanned_checkpoint_no,
					/* in/out: 4 lowest bytes of the
					highest scanned checkpoint number so
					far */
	ulint*		n_bytes_scanned)/* out: how much we were able to
					scan, smaller than buf_len if log
					data ended here */
{
	ulint	data_len;
	byte*	log_block;
	ulint	no;

	*n_bytes_scanned = 0;
	
	for (log_block = buf; log_block < buf + buf_len;
				log_block += OS_FILE_LOG_BLOCK_SIZE) {
	
		no = log_block_get_hdr_no(log_block);

/*		fprintf(stderr, "Log block header no %lu\n", no); */

		if (no != log_block_convert_lsn_to_no(*scanned_lsn)
		    || !log_block_checksum_is_ok_or_old_format(log_block)) {
/*
			fprintf(stderr,
"Log block n:o %lu, scanned lsn n:o %lu\n",
			no, log_block_convert_lsn_to_no(*scanned_lsn));
*/
			/* Garbage or an incompletely written log block */

			log_block += OS_FILE_LOG_BLOCK_SIZE;
/*
			fprintf(stderr,
"Next log block n:o %lu\n",
			log_block_get_hdr_no(log_block));
*/			
			break;
		}

		if (*scanned_checkpoint_no > 0
		    && log_block_get_checkpoint_no(log_block)
						< *scanned_checkpoint_no
		    && *scanned_checkpoint_no
			- log_block_get_checkpoint_no(log_block)
							> 0x80000000UL) {

			/* Garbage from a log buffer flush which was made
			before the most recent database recovery */
/*
			fprintf(stderr,
				"Scanned cp n:o %lu, block cp n:o %lu\n",
				*scanned_checkpoint_no,
				log_block_get_checkpoint_no(log_block));
*/
			break;
		}

		data_len = log_block_get_data_len(log_block);

		*scanned_checkpoint_no
				= log_block_get_checkpoint_no(log_block);
		*scanned_lsn = ut_dulint_add(*scanned_lsn, data_len);

		*n_bytes_scanned += data_len;
		
		if (data_len < OS_FILE_LOG_BLOCK_SIZE) {
			/* Log data ends here */

			/* fprintf(stderr, "Log block data len %lu\n",
				data_len); */

			break;
		}
	}
}

/***********************************************************************
Tries to parse a single log record body and also applies it to a page if
specified. File ops are parsed, but not applied in this function. */
static
byte*
recv_parse_or_apply_log_rec_body(
/*=============================*/
			/* out: log record end, NULL if not a complete
			record */
	byte	type,	/* in: type */
	byte*	ptr,	/* in: pointer to a buffer */
	byte*	end_ptr,/* in: pointer to the buffer end */
	page_t*	page,	/* in: buffer page or NULL; if not NULL, then the log
			record is applied to the page, and the log record
			should be complete then */
	mtr_t*	mtr)	/* in: mtr or NULL; should be non-NULL if and only if
			page is non-NULL */
{
	dict_index_t*	index = NULL;

	switch (type) {
	case MLOG_1BYTE: case MLOG_2BYTES: case MLOG_4BYTES: case MLOG_8BYTES:
		ptr = mlog_parse_nbytes(type, ptr, end_ptr, page);
		break;
	case MLOG_REC_INSERT: case MLOG_COMP_REC_INSERT:
		if (NULL != (ptr = mlog_parse_index(ptr, end_ptr,
				type == MLOG_COMP_REC_INSERT, &index))) {
			ut_a(!page
			  || (ibool)!!page_is_comp(page)==index->table->comp);
			ptr = page_cur_parse_insert_rec(FALSE, ptr, end_ptr,
							index, page, mtr);
		}
		break;
	case MLOG_REC_CLUST_DELETE_MARK: case MLOG_COMP_REC_CLUST_DELETE_MARK:
		if (NULL != (ptr = mlog_parse_index(ptr, end_ptr,
			type == MLOG_COMP_REC_CLUST_DELETE_MARK, &index))) {
			ut_a(!page
			  || (ibool)!!page_is_comp(page)==index->table->comp);
			ptr = btr_cur_parse_del_mark_set_clust_rec(ptr,
						end_ptr, index, page);
		}
		break;
	case MLOG_COMP_REC_SEC_DELETE_MARK:
		/* This log record type is obsolete, but we process it for
		backward compatibility with MySQL 5.0.3 and 5.0.4. */
		ut_a(!page || page_is_comp(page));
		ptr = mlog_parse_index(ptr, end_ptr, TRUE, &index);
		if (!ptr) {
			break;
		}
		/* Fall through */
	case MLOG_REC_SEC_DELETE_MARK:
		ptr = btr_cur_parse_del_mark_set_sec_rec(ptr, end_ptr, page);
		break;
	case MLOG_REC_UPDATE_IN_PLACE: case MLOG_COMP_REC_UPDATE_IN_PLACE:
		if (NULL != (ptr = mlog_parse_index(ptr, end_ptr,
			type == MLOG_COMP_REC_UPDATE_IN_PLACE, &index))) {
			ut_a(!page
			  || (ibool)!!page_is_comp(page)==index->table->comp);
			ptr = btr_cur_parse_update_in_place(ptr, end_ptr,
							page, index);
		}
		break;
	case MLOG_LIST_END_DELETE: case MLOG_COMP_LIST_END_DELETE:
	case MLOG_LIST_START_DELETE: case MLOG_COMP_LIST_START_DELETE:
		if (NULL != (ptr = mlog_parse_index(ptr, end_ptr,
			type == MLOG_COMP_LIST_END_DELETE
			|| type == MLOG_COMP_LIST_START_DELETE, &index))) {
			ut_a(!page
			  || (ibool)!!page_is_comp(page)==index->table->comp);
			ptr = page_parse_delete_rec_list(type, ptr, end_ptr,
							index, page, mtr);
		}
		break;
	case MLOG_LIST_END_COPY_CREATED: case MLOG_COMP_LIST_END_COPY_CREATED:
		if (NULL != (ptr = mlog_parse_index(ptr, end_ptr,
			type == MLOG_COMP_LIST_END_COPY_CREATED, &index))) {
			ut_a(!page
			  || (ibool)!!page_is_comp(page)==index->table->comp);
			ptr = page_parse_copy_rec_list_to_created_page(ptr,
						end_ptr, index, page, mtr);
		}
		break;
	case MLOG_PAGE_REORGANIZE: case MLOG_COMP_PAGE_REORGANIZE:
		if (NULL != (ptr = mlog_parse_index(ptr, end_ptr,
				type == MLOG_COMP_PAGE_REORGANIZE, &index))) {
			ut_a(!page
			  || (ibool)!!page_is_comp(page)==index->table->comp);
			ptr = btr_parse_page_reorganize(ptr, end_ptr, index,
								page, mtr);
		}
		break;
	case MLOG_PAGE_CREATE: case MLOG_COMP_PAGE_CREATE:
		ptr = page_parse_create(ptr, end_ptr,
				type == MLOG_COMP_PAGE_CREATE, page, mtr);
		break;
	case MLOG_UNDO_INSERT:
		ptr = trx_undo_parse_add_undo_rec(ptr, end_ptr, page);
		break;
	case MLOG_UNDO_ERASE_END:
		ptr = trx_undo_parse_erase_page_end(ptr, end_ptr, page, mtr);
		break;
	case MLOG_UNDO_INIT:
		ptr = trx_undo_parse_page_init(ptr, end_ptr, page, mtr);
		break;
	case MLOG_UNDO_HDR_DISCARD:
		ptr = trx_undo_parse_discard_latest(ptr, end_ptr, page, mtr);
		break;
	case MLOG_UNDO_HDR_CREATE:
	case MLOG_UNDO_HDR_REUSE:
		ptr = trx_undo_parse_page_header(type, ptr, end_ptr,
								page, mtr);
		break;
	case MLOG_REC_MIN_MARK: case MLOG_COMP_REC_MIN_MARK:
		ptr = btr_parse_set_min_rec_mark(ptr, end_ptr,
				type == MLOG_COMP_REC_MIN_MARK, page, mtr);
		break;
	case MLOG_REC_DELETE: case MLOG_COMP_REC_DELETE:
		if (NULL != (ptr = mlog_parse_index(ptr, end_ptr,
				type == MLOG_COMP_REC_DELETE, &index))) {
			ut_a(!page
			  || (ibool)!!page_is_comp(page)==index->table->comp);
			ptr = page_cur_parse_delete_rec(ptr, end_ptr,
							index, page, mtr);
		}
		break;
	case MLOG_IBUF_BITMAP_INIT:
		ptr = ibuf_parse_bitmap_init(ptr, end_ptr, page, mtr);
		break;
	case MLOG_INIT_FILE_PAGE:
		ptr = fsp_parse_init_file_page(ptr, end_ptr, page);
		break;
	case MLOG_WRITE_STRING:
		ptr = mlog_parse_string(ptr, end_ptr, page);
		break;
	case MLOG_FILE_CREATE:
	case MLOG_FILE_RENAME:
	case MLOG_FILE_DELETE:
		ptr = fil_op_log_parse_or_replay(ptr, end_ptr, type, FALSE,
							ULINT_UNDEFINED);
		break;
	default:
		ptr = NULL;
		recv_sys->found_corrupt_log = TRUE;
	}

	ut_ad(!page || ptr);
	if (index) {
		dict_table_t*	table = index->table;

		dict_mem_index_free(index);
		dict_mem_table_free(table);
	}
	
	return(ptr);
}

/*************************************************************************
Calculates the fold value of a page file address: used in inserting or
searching for a log record in the hash table. */
UNIV_INLINE
ulint
recv_fold(
/*======*/
			/* out: folded value */
	ulint	space,	/* in: space */
	ulint	page_no)/* in: page number */
{
	return(ut_fold_ulint_pair(space, page_no));
}

/*************************************************************************
Calculates the hash value of a page file address: used in inserting or
searching for a log record in the hash table. */
UNIV_INLINE
ulint
recv_hash(
/*======*/
			/* out: folded value */
	ulint	space,	/* in: space */
	ulint	page_no)/* in: page number */
{
	return(hash_calc_hash(recv_fold(space, page_no), recv_sys->addr_hash));
}

/*************************************************************************
Gets the hashed file address struct for a page. */
static
recv_addr_t*
recv_get_fil_addr_struct(
/*=====================*/
			/* out: file address struct, NULL if not found from
			the hash table */
	ulint	space,	/* in: space id */
	ulint	page_no)/* in: page number */
{
	recv_addr_t*	recv_addr;

	recv_addr = HASH_GET_FIRST(recv_sys->addr_hash,
						recv_hash(space, page_no));
	while (recv_addr) {
		if ((recv_addr->space == space)
				&& (recv_addr->page_no == page_no)) {

			break;
		}

		recv_addr = HASH_GET_NEXT(addr_hash, recv_addr);
	}

	return(recv_addr);
}

/***********************************************************************
Adds a new log record to the hash table of log records. */
static
void
recv_add_to_hash_table(
/*===================*/
	byte	type,		/* in: log record type */
	ulint	space,		/* in: space id */
	ulint	page_no,	/* in: page number */
	byte*	body,		/* in: log record body */
	byte*	rec_end,	/* in: log record end */
	dulint	start_lsn,	/* in: start lsn of the mtr */
	dulint	end_lsn)	/* in: end lsn of the mtr */
{
	recv_t*		recv;
	ulint		len;
	recv_data_t*	recv_data;
	recv_data_t**	prev_field;
	recv_addr_t*	recv_addr;
	
	if (fil_tablespace_deleted_or_being_deleted_in_mem(space, -1)) {
		/* The tablespace does not exist any more: do not store the
		log record */

		return;
	}

	len = rec_end - body;

	recv = mem_heap_alloc(recv_sys->heap, sizeof(recv_t));
	recv->type = type;
	recv->len = rec_end - body;
	recv->start_lsn = start_lsn;
	recv->end_lsn = end_lsn;

	recv_addr = recv_get_fil_addr_struct(space, page_no);
	
	if (recv_addr == NULL) {
		recv_addr = mem_heap_alloc(recv_sys->heap,
							sizeof(recv_addr_t));
		recv_addr->space = space;
		recv_addr->page_no = page_no;
		recv_addr->state = RECV_NOT_PROCESSED;

		UT_LIST_INIT(recv_addr->rec_list);

		HASH_INSERT(recv_addr_t, addr_hash, recv_sys->addr_hash,
					recv_fold(space, page_no), recv_addr);
		recv_sys->n_addrs++;

		/* fprintf(stderr, "Inserting log rec for space %lu, page %lu\n",
					  space, page_no); */
	}

	UT_LIST_ADD_LAST(rec_list, recv_addr->rec_list, recv);

	prev_field = &(recv->data);

	/* Store the log record body in chunks of less than UNIV_PAGE_SIZE:
	recv_sys->heap grows into the buffer pool, and bigger chunks could not
	be allocated */
	
	while (rec_end > body) {

		len = rec_end - body;
	
		if (len > RECV_DATA_BLOCK_SIZE) {
			len = RECV_DATA_BLOCK_SIZE;
		}
	
		recv_data = mem_heap_alloc(recv_sys->heap,
						sizeof(recv_data_t) + len);
		*prev_field = recv_data;

		ut_memcpy(((byte*)recv_data) + sizeof(recv_data_t), body, len);

		prev_field = &(recv_data->next);

		body += len;
	}

	*prev_field = NULL;
}

/*************************************************************************
Copies the log record body from recv to buf. */
static
void
recv_data_copy_to_buf(
/*==================*/
	byte*	buf,	/* in: buffer of length at least recv->len */
	recv_t*	recv)	/* in: log record */
{
	recv_data_t*	recv_data;
	ulint		part_len;
	ulint		len;

	len = recv->len;
	recv_data = recv->data;

	while (len > 0) {
		if (len > RECV_DATA_BLOCK_SIZE) {
			part_len = RECV_DATA_BLOCK_SIZE;
		} else {
			part_len = len;
		}

		ut_memcpy(buf, ((byte*)recv_data) + sizeof(recv_data_t),
								part_len);
		buf += part_len;
		len -= part_len;

		recv_data = recv_data->next;
	}
}

/****************************************************************************
Applies the hashed log records to the page, if the page lsn is less than the
lsn of a log record. This can be called when a buffer page has just been
read in, or also for a page already in the buffer pool. */

void
recv_recover_page(
/*==============*/
	ibool	recover_backup,	/* in: TRUE if we are recovering a backup
				page: then we do not acquire any latches
				since the page was read in outside the
				buffer pool */
	ibool	just_read_in,	/* in: TRUE if the i/o-handler calls this for
				a freshly read page */
	page_t*	page,		/* in: buffer page */
	ulint	space,		/* in: space id */
	ulint	page_no)	/* in: page number */
{
	buf_block_t*	block		= NULL;
	recv_addr_t*	recv_addr;
	recv_t*		recv;
	byte*		buf;
	dulint		start_lsn;
	dulint		end_lsn;
	dulint		page_lsn;
	dulint		page_newest_lsn;
	ibool		modification_to_page;
	ibool		success;
	mtr_t		mtr;

	mutex_enter(&(recv_sys->mutex));

	if (recv_sys->apply_log_recs == FALSE) {

		/* Log records should not be applied now */
	
		mutex_exit(&(recv_sys->mutex));

		return;
	}
	
	recv_addr = recv_get_fil_addr_struct(space, page_no);

	if ((recv_addr == NULL)
	    || (recv_addr->state == RECV_BEING_PROCESSED)
	    || (recv_addr->state == RECV_PROCESSED)) {

		mutex_exit(&(recv_sys->mutex));

		return;
	}

	/* fprintf(stderr, "Recovering space %lu, page %lu\n", space, page_no); */

	recv_addr->state = RECV_BEING_PROCESSED;
	
	mutex_exit(&(recv_sys->mutex));

	mtr_start(&mtr);
	mtr_set_log_mode(&mtr, MTR_LOG_NONE);

	if (!recover_backup) {	
		block = buf_block_align(page);

		if (just_read_in) {
		  /* Move the ownership of the x-latch on the page to this OS
		  thread, so that we can acquire a second x-latch on it. This
		  is needed for the operations to the page to pass the debug
		  checks. */

			rw_lock_x_lock_move_ownership(&(block->lock));
		}

		success = buf_page_get_known_nowait(RW_X_LATCH, page,
					BUF_KEEP_OLD,
					__FILE__, __LINE__,
					&mtr);
		ut_a(success);

#ifdef UNIV_SYNC_DEBUG
		buf_page_dbg_add_level(page, SYNC_NO_ORDER_CHECK);
#endif /* UNIV_SYNC_DEBUG */
	}

	/* Read the newest modification lsn from the page */
	page_lsn = mach_read_from_8(page + FIL_PAGE_LSN);

	if (!recover_backup) {
		/* It may be that the page has been modified in the buffer
		pool: read the newest modification lsn there */
		
		page_newest_lsn = buf_frame_get_newest_modification(page);

		if (!ut_dulint_is_zero(page_newest_lsn)) {
		
			page_lsn = page_newest_lsn;
		}
	} else {
		/* In recovery from a backup we do not really use the buffer
		pool */

		page_newest_lsn = ut_dulint_zero;
	}

	modification_to_page = FALSE;
	start_lsn = end_lsn = ut_dulint_zero;

	recv = UT_LIST_GET_FIRST(recv_addr->rec_list);
	
	while (recv) {
		end_lsn = recv->end_lsn;
	
		if (recv->len > RECV_DATA_BLOCK_SIZE) {
			/* We have to copy the record body to a separate
			buffer */

			buf = mem_alloc(recv->len);

			recv_data_copy_to_buf(buf, recv);
		} else {
			buf = ((byte*)(recv->data)) + sizeof(recv_data_t);
		}

		if (recv->type == MLOG_INIT_FILE_PAGE) {
			page_lsn = page_newest_lsn;

			mach_write_to_8(page + UNIV_PAGE_SIZE
				- FIL_PAGE_END_LSN_OLD_CHKSUM, ut_dulint_zero);
			mach_write_to_8(page + FIL_PAGE_LSN, ut_dulint_zero);
		}
		
		if (ut_dulint_cmp(recv->start_lsn, page_lsn) >= 0) {

			if (!modification_to_page) {
		
				modification_to_page = TRUE;
				start_lsn = recv->start_lsn;
			}

#ifdef UNIV_DEBUG
			if (log_debug_writes) {
				fprintf(stderr, 
     "InnoDB: Applying log rec type %lu len %lu to space %lu page no %lu\n",
					(ulong) recv->type, (ulong) recv->len,
					(ulong) recv_addr->space,
					(ulong) recv_addr->page_no);
			}
#endif /* UNIV_DEBUG */

			recv_parse_or_apply_log_rec_body(recv->type, buf,
						buf + recv->len, page, &mtr);
			mach_write_to_8(page + UNIV_PAGE_SIZE
					- FIL_PAGE_END_LSN_OLD_CHKSUM,
					ut_dulint_add(recv->start_lsn,
							recv->len));
			mach_write_to_8(page + FIL_PAGE_LSN,
					ut_dulint_add(recv->start_lsn,
							recv->len));
		}
						
		if (recv->len > RECV_DATA_BLOCK_SIZE) {
			mem_free(buf);
		}

		recv = UT_LIST_GET_NEXT(rec_list, recv);
	}

	mutex_enter(&(recv_sys->mutex));
	
	if (ut_dulint_cmp(recv_max_page_lsn, page_lsn) < 0) {
		recv_max_page_lsn = page_lsn;
	}

	recv_addr->state = RECV_PROCESSED;

	ut_a(recv_sys->n_addrs);
	recv_sys->n_addrs--;

	mutex_exit(&(recv_sys->mutex));
	
	if (!recover_backup && modification_to_page) {
		ut_a(block);

		buf_flush_recv_note_modification(block, start_lsn, end_lsn);
	}
	
	/* Make sure that committing mtr does not change the modification
	lsn values of page */
	
	mtr.modifications = FALSE;
	
	mtr_commit(&mtr);	
}

/***********************************************************************
Reads in pages which have hashed log records, from an area around a given
page number. */
static
ulint
recv_read_in_area(
/*==============*/
			/* out: number of pages found */
	ulint	space,	/* in: space */
	ulint	page_no)/* in: page number */
{
	recv_addr_t* recv_addr;
	ulint	page_nos[RECV_READ_AHEAD_AREA];
	ulint	low_limit;
	ulint	n;

	low_limit = page_no - (page_no % RECV_READ_AHEAD_AREA);

	n = 0;

	for (page_no = low_limit; page_no < low_limit + RECV_READ_AHEAD_AREA;
								page_no++) {
		recv_addr = recv_get_fil_addr_struct(space, page_no);

		if (recv_addr && !buf_page_peek(space, page_no)) {

			mutex_enter(&(recv_sys->mutex));

			if (recv_addr->state == RECV_NOT_PROCESSED) {
				recv_addr->state = RECV_BEING_READ;
	
				page_nos[n] = page_no;

				n++;
			}
			
			mutex_exit(&(recv_sys->mutex));
		}
	}

	buf_read_recv_pages(FALSE, space, page_nos, n);
	/*
	fprintf(stderr, "Recv pages at %lu n %lu\n", page_nos[0], n);
	*/
	return(n);
}
			
/***********************************************************************
Empties the hash table of stored log records, applying them to appropriate
pages. */

void
recv_apply_hashed_log_recs(
/*=======================*/
	ibool	allow_ibuf)	/* in: if TRUE, also ibuf operations are
				allowed during the application; if FALSE,
				no ibuf operations are allowed, and after
				the application all file pages are flushed to
				disk and invalidated in buffer pool: this
				alternative means that no new log records
				can be generated during the application;
				the caller must in this case own the log
				mutex */
{
	recv_addr_t* recv_addr;
	page_t*	page;
	ulint	i;
	ulint	space;
	ulint	page_no;
	ulint	n_pages;
	ibool	has_printed	= FALSE;
	mtr_t	mtr;
loop:
	mutex_enter(&(recv_sys->mutex));

	if (recv_sys->apply_batch_on) {

		mutex_exit(&(recv_sys->mutex));

		os_thread_sleep(500000);

		goto loop;
	}

#ifdef UNIV_SYNC_DEBUG
	ut_ad(!allow_ibuf == mutex_own(&log_sys->mutex));
#endif /* UNIV_SYNC_DEBUG */
	if (!allow_ibuf) {
		recv_no_ibuf_operations = TRUE;
	}

	recv_sys->apply_log_recs = TRUE;
	recv_sys->apply_batch_on = TRUE;

	for (i = 0; i < hash_get_n_cells(recv_sys->addr_hash); i++) {
		
		recv_addr = HASH_GET_FIRST(recv_sys->addr_hash, i);

		while (recv_addr) {
			space = recv_addr->space;
			page_no = recv_addr->page_no;

			if (recv_addr->state == RECV_NOT_PROCESSED) {
				if (!has_printed) {
	    				ut_print_timestamp(stderr);
					fputs( 
"  InnoDB: Starting an apply batch of log records to the database...\n"
"InnoDB: Progress in percents: ",stderr);
					has_printed = TRUE;
				}
				
				mutex_exit(&(recv_sys->mutex));

				if (buf_page_peek(space, page_no)) {

					mtr_start(&mtr);

					page = buf_page_get(space, page_no,
							RW_X_LATCH, &mtr);

#ifdef UNIV_SYNC_DEBUG
					buf_page_dbg_add_level(page,
							SYNC_NO_ORDER_CHECK);
#endif /* UNIV_SYNC_DEBUG */
					recv_recover_page(FALSE, FALSE, page,
							space, page_no);
					mtr_commit(&mtr);
				} else {
					recv_read_in_area(space, page_no);
				}

				mutex_enter(&(recv_sys->mutex));
			}

			recv_addr = HASH_GET_NEXT(addr_hash, recv_addr);
		}

		if (has_printed
		    && (i * 100) / hash_get_n_cells(recv_sys->addr_hash)
		    != ((i + 1) * 100)
		             / hash_get_n_cells(recv_sys->addr_hash)) {

		        fprintf(stderr, "%lu ",
				(ulong) ((i * 100) / hash_get_n_cells(recv_sys->addr_hash)));
		}
	}

	/* Wait until all the pages have been processed */

	while (recv_sys->n_addrs != 0) {

		mutex_exit(&(recv_sys->mutex));

		os_thread_sleep(500000);

		mutex_enter(&(recv_sys->mutex));
	}	

	if (has_printed) {

	        fprintf(stderr, "\n");
	}

	if (!allow_ibuf) {
		/* Flush all the file pages to disk and invalidate them in
		the buffer pool */

		mutex_exit(&(recv_sys->mutex));
		mutex_exit(&(log_sys->mutex));

		n_pages = buf_flush_batch(BUF_FLUSH_LIST, ULINT_MAX,
								ut_dulint_max);
		ut_a(n_pages != ULINT_UNDEFINED);
		
		buf_flush_wait_batch_end(BUF_FLUSH_LIST);

		buf_pool_invalidate();

		mutex_enter(&(log_sys->mutex));
		mutex_enter(&(recv_sys->mutex));

		recv_no_ibuf_operations = FALSE;
	}

	recv_sys->apply_log_recs = FALSE;
	recv_sys->apply_batch_on = FALSE;
			
	recv_sys_empty_hash();

	if (has_printed) {
		fprintf(stderr, "InnoDB: Apply batch completed\n");
	}

	mutex_exit(&(recv_sys->mutex));
}

/* This page is allocated from the buffer pool and used in the function
below */
static page_t* recv_backup_application_page	= NULL;

/***********************************************************************
Applies log records in the hash table to a backup. */

void
recv_apply_log_recs_for_backup(void)
/*================================*/
{
	recv_addr_t*	recv_addr;
	ulint		n_hash_cells;
	byte*		page;
	ulint		actual_size;
	ibool		success;
	ulint		error;
	ulint		i;

	recv_sys->apply_log_recs = TRUE;
	recv_sys->apply_batch_on = TRUE;

	if (recv_backup_application_page == NULL) {
		recv_backup_application_page = buf_frame_alloc();
	}

	page = recv_backup_application_page;

	fputs(
"InnoDB: Starting an apply batch of log records to the database...\n"
"InnoDB: Progress in percents: ", stderr);
	
	n_hash_cells = hash_get_n_cells(recv_sys->addr_hash);

	for (i = 0; i < n_hash_cells; i++) {
	        /* The address hash table is externally chained */
		recv_addr = hash_get_nth_cell(recv_sys->addr_hash, i)->node;

		while (recv_addr != NULL) {

			if (!fil_tablespace_exists_in_mem(recv_addr->space)) {
/*
				fprintf(stderr,
"InnoDB: Warning: cannot apply log record to tablespace %lu page %lu,\n"
"InnoDB: because tablespace with that id does not exist.\n",
				      recv_addr->space, recv_addr->page_no);
*/
				recv_addr->state = RECV_PROCESSED;

				ut_a(recv_sys->n_addrs);
				recv_sys->n_addrs--;

				goto skip_this_recv_addr;
			}

			/* We simulate a page read made by the buffer pool, to
			make sure the recovery apparatus works ok, for
			example, the buf_frame_align() function. We must init
			the block corresponding to buf_pool->frame_zero
			(== page). */

			buf_page_init_for_backup_restore(recv_addr->space,
						recv_addr->page_no,
						buf_block_align(page));

			/* Extend the tablespace's last file if the page_no
			does not fall inside its bounds; we assume the last
			file is auto-extending, and ibbackup copied the file
			when it still was smaller */

			success = fil_extend_space_to_desired_size(
						&actual_size,
						recv_addr->space,
						recv_addr->page_no + 1);
			if (!success) {
			  fprintf(stderr,
"InnoDB: Fatal error: cannot extend tablespace %lu to hold %lu pages\n",
				     recv_addr->space, recv_addr->page_no);
				     
				exit(1);
			}

			/* Read the page from the tablespace file using the
			fil0fil.c routines */

			error = fil_io(OS_FILE_READ, TRUE, recv_addr->space,
					recv_addr->page_no, 0, UNIV_PAGE_SIZE,
					page, NULL);
			if (error != DB_SUCCESS) {
			  fprintf(stderr,
"InnoDB: Fatal error: cannot read from tablespace %lu page number %lu\n",
				     (ulong) recv_addr->space, (ulong) recv_addr->page_no);
				     
				exit(1);
			}

			/* Apply the log records to this page */
			recv_recover_page(TRUE, FALSE, page, recv_addr->space,
						       recv_addr->page_no);

			/* Write the page back to the tablespace file using the
			fil0fil.c routines */

			buf_flush_init_for_writing(page,
				mach_read_from_8(page + FIL_PAGE_LSN),
				recv_addr->space, recv_addr->page_no);

			error = fil_io(OS_FILE_WRITE, TRUE, recv_addr->space,
					recv_addr->page_no, 0, UNIV_PAGE_SIZE,
					page, NULL);
skip_this_recv_addr:
			recv_addr = HASH_GET_NEXT(addr_hash, recv_addr);
		}

		if ((100 * i) / n_hash_cells
				!= (100 * (i + 1)) / n_hash_cells) {
			fprintf(stderr, "%lu ",
                                (ulong) ((100 * i) / n_hash_cells));
			fflush(stderr);
		}
	}

	recv_sys_empty_hash();
}

#ifdef notdefined
/***********************************************************************
In the debug version, updates the replica of a file page, based on a log
record. */
static
void
recv_update_replicate(
/*==================*/
	byte	type,	/* in: log record type */
	ulint	space,	/* in: space id */
	ulint	page_no,/* in: page number */
	byte*	body,	/* in: log record body */
	byte*	end_ptr)/* in: log record end */
{
	page_t*	replica;
	mtr_t	mtr;
	byte*	ptr;

	mtr_start(&mtr);

	mtr_set_log_mode(&mtr, MTR_LOG_NONE);

	replica = buf_page_get(space + RECV_REPLICA_SPACE_ADD, page_no,
							RW_X_LATCH, &mtr);
#ifdef UNIV_SYNC_DEBUG
	buf_page_dbg_add_level(replica, SYNC_NO_ORDER_CHECK);
#endif /* UNIV_SYNC_DEBUG */
							
	ptr = recv_parse_or_apply_log_rec_body(type, body, end_ptr, replica,
									&mtr);
	ut_a(ptr == end_ptr);

	/* Notify the buffer manager that the page has been updated */

	buf_flush_recv_note_modification(buf_block_align(replica),
					log_sys->old_lsn, log_sys->old_lsn);

	/* Make sure that committing mtr does not call log routines, as
	we currently own the log mutex */
	
	mtr.modifications = FALSE;

	mtr_commit(&mtr);
}

/***********************************************************************
Checks that two strings are identical. */
static
void
recv_check_identical(
/*=================*/
	byte*	str1,	/* in: first string */
	byte*	str2,	/* in: second string */
	ulint	len)	/* in: length of strings */
{
	ulint	i;

	for (i = 0; i < len; i++) {

		if (str1[i] != str2[i]) {
			fprintf(stderr,
				"Strings do not match at offset %lu\n", i);
			ut_print_buf(str1 + i, 16);
			fprintf(stderr, "\n");
			ut_print_buf(str2 + i, 16);

			ut_error;
		}
	}
}	
			
/***********************************************************************
In the debug version, checks that the replica of a file page is identical
to the original page. */
static
void
recv_compare_replicate(
/*===================*/
	ulint	space,	/* in: space id */
	ulint	page_no)/* in: page number */
{
	page_t*	replica;
	page_t*	page;
	mtr_t	mtr;

	mtr_start(&mtr);

	mutex_enter(&(buf_pool->mutex));

	page = buf_page_hash_get(space, page_no)->frame;

	mutex_exit(&(buf_pool->mutex));

	replica = buf_page_get(space + RECV_REPLICA_SPACE_ADD, page_no,
							RW_X_LATCH, &mtr);
#ifdef UNIV_SYNC_DEBUG
	buf_page_dbg_add_level(replica, SYNC_NO_ORDER_CHECK);
#endif /* UNIV_SYNC_DEBUG */

	recv_check_identical(page + FIL_PAGE_DATA,
			replica + FIL_PAGE_DATA,
			PAGE_HEADER + PAGE_MAX_TRX_ID - FIL_PAGE_DATA);

	recv_check_identical(page + PAGE_HEADER + PAGE_MAX_TRX_ID + 8,
			replica + PAGE_HEADER + PAGE_MAX_TRX_ID + 8,
			UNIV_PAGE_SIZE - FIL_PAGE_DATA_END
			- PAGE_HEADER - PAGE_MAX_TRX_ID - 8);
	mtr_commit(&mtr);
}

/***********************************************************************
Checks that a replica of a space is identical to the original space. */

void
recv_compare_spaces(
/*================*/
	ulint	space1,	/* in: space id */
	ulint	space2,	/* in: space id */
	ulint	n_pages)/* in: number of pages */
{
	page_t*	replica;
	page_t*	page;
	mtr_t	mtr;
	page_t*	frame;
	ulint	page_no;

	replica = buf_frame_alloc();
	page = buf_frame_alloc();

	for (page_no = 0; page_no < n_pages; page_no++) {
	
		mtr_start(&mtr);

		frame = buf_page_get_gen(space1, page_no, RW_S_LATCH, NULL,
						BUF_GET_IF_IN_POOL,
						__FILE__, __LINE__,
						&mtr);
		if (frame) {
#ifdef UNIV_SYNC_DEBUG
			buf_page_dbg_add_level(frame, SYNC_NO_ORDER_CHECK);
#endif /* UNIV_SYNC_DEBUG */
			ut_memcpy(page, frame, UNIV_PAGE_SIZE);
		} else {
			/* Read it from file */
			fil_io(OS_FILE_READ, TRUE, space1, page_no, 0,
						UNIV_PAGE_SIZE, page, NULL);
		}

		frame = buf_page_get_gen(space2, page_no, RW_S_LATCH, NULL,
						BUF_GET_IF_IN_POOL,
						__FILE__, __LINE__,
						&mtr);
		if (frame) {
#ifdef UNIV_SYNC_DEBUG
			buf_page_dbg_add_level(frame, SYNC_NO_ORDER_CHECK);
#endif /* UNIV_SYNC_DEBUG */
			ut_memcpy(replica, frame, UNIV_PAGE_SIZE);
		} else {
			/* Read it from file */
			fil_io(OS_FILE_READ, TRUE, space2, page_no, 0,
				UNIV_PAGE_SIZE, replica, NULL);
		}
		
		recv_check_identical(page + FIL_PAGE_DATA,
			replica + FIL_PAGE_DATA,
			PAGE_HEADER + PAGE_MAX_TRX_ID - FIL_PAGE_DATA);

		recv_check_identical(page + PAGE_HEADER + PAGE_MAX_TRX_ID + 8,
			replica + PAGE_HEADER + PAGE_MAX_TRX_ID + 8,
			UNIV_PAGE_SIZE - FIL_PAGE_DATA_END
			- PAGE_HEADER - PAGE_MAX_TRX_ID - 8);

		mtr_commit(&mtr);
	}

	buf_frame_free(replica);
	buf_frame_free(page);
}

/***********************************************************************
Checks that a replica of a space is identical to the original space. Disables
ibuf operations and flushes and invalidates the buffer pool pages after the
test. This function can be used to check the recovery before dict or trx
systems are initialized. */

void
recv_compare_spaces_low(
/*====================*/
	ulint	space1,	/* in: space id */
	ulint	space2,	/* in: space id */
	ulint	n_pages)/* in: number of pages */
{
	mutex_enter(&(log_sys->mutex));

	recv_apply_hashed_log_recs(FALSE);
	
	mutex_exit(&(log_sys->mutex));

	recv_compare_spaces(space1, space2, n_pages);
}
#endif /* UNIV_LOG_REPLICATE */

/***********************************************************************
Tries to parse a single log record and returns its length. */
static
ulint
recv_parse_log_rec(
/*===============*/
			/* out: length of the record, or 0 if the record was
			not complete */
	byte*	ptr,	/* in: pointer to a buffer */
	byte*	end_ptr,/* in: pointer to the buffer end */
	byte*	type,	/* out: type */
	ulint*	space,	/* out: space id */
	ulint*	page_no,/* out: page number */
	byte**	body)	/* out: log record body start */
{
	byte*	new_ptr;

	*body = NULL;

	if (ptr == end_ptr) {

		return(0);
	}

	if (*ptr == MLOG_MULTI_REC_END) {
	
		*type = *ptr;

		return(1);
	}

	if (*ptr == MLOG_DUMMY_RECORD) {
		*type = *ptr;

		*space = ULINT_UNDEFINED - 1; /* For debugging */

		return(1);
	}

	new_ptr = mlog_parse_initial_log_record(ptr, end_ptr, type, space,
								page_no);
	*body = new_ptr;

	if (UNIV_UNLIKELY(!new_ptr)) {

	        return(0);
	}

	/* Check that page_no is sensible */

	if (UNIV_UNLIKELY(*page_no > 0x8FFFFFFFUL)) {

		recv_sys->found_corrupt_log = TRUE;

		return(0);
	}

	new_ptr = recv_parse_or_apply_log_rec_body(*type, new_ptr, end_ptr,
								NULL, NULL);
	if (UNIV_UNLIKELY(new_ptr == NULL)) {

		return(0);
	}

	if (*page_no > recv_max_parsed_page_no) {
		recv_max_parsed_page_no = *page_no;
	}
	
	return(new_ptr - ptr);
}

/***********************************************************
Calculates the new value for lsn when more data is added to the log. */
static
dulint
recv_calc_lsn_on_data_add(
/*======================*/
	dulint	lsn,	/* in: old lsn */
	ulint	len)	/* in: this many bytes of data is added, log block
			headers not included */
{
	ulint	frag_len;
	ulint	lsn_len;
	
	frag_len = (ut_dulint_get_low(lsn) % OS_FILE_LOG_BLOCK_SIZE)
		   					- LOG_BLOCK_HDR_SIZE;
	ut_ad(frag_len < OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_HDR_SIZE
		      					- LOG_BLOCK_TRL_SIZE);
	lsn_len = len + ((len + frag_len)
		    	 / (OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_HDR_SIZE
		      					- LOG_BLOCK_TRL_SIZE))
		     	 * (LOG_BLOCK_HDR_SIZE + LOG_BLOCK_TRL_SIZE);

	return(ut_dulint_add(lsn, lsn_len));
}

/***********************************************************
Checks that the parser recognizes incomplete initial segments of a log
record as incomplete. */

void
recv_check_incomplete_log_recs(
/*===========================*/
	byte*	ptr,	/* in: pointer to a complete log record */
	ulint	len)	/* in: length of the log record */
{
	ulint	i;
	byte	type;
	ulint	space;
	ulint	page_no;
	byte*	body;
	
	for (i = 0; i < len; i++) {
		ut_a(0 == recv_parse_log_rec(ptr, ptr + i, &type, &space,
							&page_no, &body));
	}
}		

/***********************************************************
Prints diagnostic info of corrupt log. */
static
void
recv_report_corrupt_log(
/*====================*/
	byte*	ptr,	/* in: pointer to corrupt log record */
	byte	type,	/* in: type of the record */
	ulint	space,	/* in: space id, this may also be garbage */
	ulint	page_no)/* in: page number, this may also be garbage */
{
	fprintf(stderr,
"InnoDB: ############### CORRUPT LOG RECORD FOUND\n"
"InnoDB: Log record type %lu, space id %lu, page number %lu\n"
"InnoDB: Log parsing proceeded successfully up to %lu %lu\n"
"InnoDB: Previous log record type %lu, is multi %lu\n"
"InnoDB: Recv offset %lu, prev %lu\n",
	(ulong) type, (ulong) space, (ulong) page_no,
	(ulong) ut_dulint_get_high(recv_sys->recovered_lsn),
        (ulong) ut_dulint_get_low(recv_sys->recovered_lsn),
	(ulong) recv_previous_parsed_rec_type,
	(ulong) recv_previous_parsed_rec_is_multi,
	(ulong) (ptr - recv_sys->buf),
	(ulong) recv_previous_parsed_rec_offset);

	if ((ulint)(ptr - recv_sys->buf + 100)
					> recv_previous_parsed_rec_offset
	    && (ulint)(ptr - recv_sys->buf + 100
					- recv_previous_parsed_rec_offset)
	       < 200000) {
		fputs(
"InnoDB: Hex dump of corrupt log starting 100 bytes before the start\n"
"InnoDB: of the previous log rec,\n"
"InnoDB: and ending 100 bytes after the start of the corrupt rec:\n",
			stderr);
 
		ut_print_buf(stderr,
		     recv_sys->buf + recv_previous_parsed_rec_offset - 100,
		     ptr - recv_sys->buf + 200 -
					recv_previous_parsed_rec_offset);
		putc('\n', stderr);
	}

	fputs(
	"InnoDB: WARNING: the log file may have been corrupt and it\n"
	"InnoDB: is possible that the log scan did not proceed\n"
	"InnoDB: far enough in recovery! Please run CHECK TABLE\n"
	"InnoDB: on your InnoDB tables to check that they are ok!\n"
	"InnoDB: If mysqld crashes after this recovery, look at\n"
	"InnoDB: http://dev.mysql.com/doc/refman/5.0/en/forcing-recovery.html\n"
	"InnoDB: about forcing recovery.\n", stderr);

	fflush(stderr);
}

/***********************************************************
Parses log records from a buffer and stores them to a hash table to wait
merging to file pages. */
static
ibool
recv_parse_log_recs(
/*================*/
				/* out: currently always returns FALSE */
	ibool	store_to_hash)	/* in: TRUE if the records should be stored
				to the hash table; this is set to FALSE if just
				debug checking is needed */
{
	byte*	ptr;
	byte*	end_ptr;
	ulint	single_rec;
	ulint	len;
	ulint	total_len;
	dulint	new_recovered_lsn;
	dulint	old_lsn;
	byte	type;
	ulint	space;
	ulint	page_no;
	byte*	body;
	ulint	n_recs;
	
#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */
	ut_ad(!ut_dulint_is_zero(recv_sys->parse_start_lsn));
loop:
	ptr = recv_sys->buf + recv_sys->recovered_offset;

	end_ptr = recv_sys->buf + recv_sys->len;

	if (ptr == end_ptr) {

		return(FALSE);
	}

	single_rec = (ulint)*ptr & MLOG_SINGLE_REC_FLAG;

	if (single_rec || *ptr == MLOG_DUMMY_RECORD) {
		/* The mtr only modified a single page, or this is a file op */

		old_lsn = recv_sys->recovered_lsn;

		/* Try to parse a log record, fetching its type, space id,
		page no, and a pointer to the body of the log record */

		len = recv_parse_log_rec(ptr, end_ptr, &type, &space,
							&page_no, &body);

		if (len == 0 || recv_sys->found_corrupt_log) {
			if (recv_sys->found_corrupt_log) {

				recv_report_corrupt_log(ptr,
						type, space, page_no);
			}
		
			return(FALSE);
		}

		new_recovered_lsn = recv_calc_lsn_on_data_add(old_lsn, len);

		if (ut_dulint_cmp(new_recovered_lsn, recv_sys->scanned_lsn)
								> 0) {
			/* The log record filled a log block, and we require
			that also the next log block should have been scanned
			in */

			return(FALSE);
		}
		
		recv_previous_parsed_rec_type = (ulint)type;
		recv_previous_parsed_rec_offset = recv_sys->recovered_offset;
		recv_previous_parsed_rec_is_multi = 0;

		recv_sys->recovered_offset += len;
		recv_sys->recovered_lsn = new_recovered_lsn;

#ifdef UNIV_DEBUG
		if (log_debug_writes) {
			fprintf(stderr, 
"InnoDB: Parsed a single log rec type %lu len %lu space %lu page no %lu\n",
				(ulong) type, (ulong) len, (ulong) space,
				(ulong) page_no);
		}
#endif /* UNIV_DEBUG */

		if (type == MLOG_DUMMY_RECORD) {
			/* Do nothing */
		
		} else if (store_to_hash && (type == MLOG_FILE_CREATE
					     || type == MLOG_FILE_RENAME
					     || type == MLOG_FILE_DELETE)) {
#ifdef UNIV_HOTBACKUP
			if (recv_replay_file_ops) {

				/* In ibbackup --apply-log, replay an .ibd file
				operation, if possible; note that
				fil_path_to_mysql_datadir is set in ibbackup to
				point to the datadir we should use there */
			
				if (NULL == fil_op_log_parse_or_replay(body,
						end_ptr, type, TRUE, space)) {
					fprintf(stderr,
"InnoDB: Error: file op log record of type %lu space %lu not complete in\n"
"InnoDB: the replay phase. Path %s\n", (ulint)type, space, (char*)(body + 2));

					ut_a(0);
				}
			}
#endif
			/* In normal mysqld crash recovery we do not try to
			replay file operations */
		} else if (store_to_hash) {
			recv_add_to_hash_table(type, space, page_no, body,
						ptr + len, old_lsn,
						recv_sys->recovered_lsn);
		} else {
			/* In debug checking, update a replicate page
			according to the log record, and check that it
			becomes identical with the original page */
#ifdef UNIV_LOG_DEBUG
			recv_check_incomplete_log_recs(ptr, len);
#endif/* UNIV_LOG_DEBUG */
#ifdef UNIV_LOG_REPLICATE
			recv_update_replicate(type, space, page_no, body,
								ptr + len);
			recv_compare_replicate(space, page_no);
#endif /* UNIV_LOG_REPLICATE */

		}
	} else {
		/* Check that all the records associated with the single mtr
		are included within the buffer */

		total_len = 0;
		n_recs = 0;
		
		for (;;) {
			len = recv_parse_log_rec(ptr, end_ptr, &type, &space,
							&page_no, &body);
			if (len == 0 || recv_sys->found_corrupt_log) {

			    	if (recv_sys->found_corrupt_log) {

					recv_report_corrupt_log(ptr,
						type, space, page_no);
			    	}

			    	return(FALSE);
			}

			recv_previous_parsed_rec_type = (ulint)type;
			recv_previous_parsed_rec_offset
				= recv_sys->recovered_offset + total_len;
			recv_previous_parsed_rec_is_multi = 1;

			if ((!store_to_hash) && (type != MLOG_MULTI_REC_END)) {
				/* In debug checking, update a replicate page
				according to the log record */
#ifdef UNIV_LOG_DEBUG
				recv_check_incomplete_log_recs(ptr, len);
#endif /* UNIV_LOG_DEBUG */
#ifdef UNIV_LOG_REPLICATE
				recv_update_replicate(type, space, page_no,
							body, ptr + len);
#endif /* UNIV_LOG_REPLICATE */
			}

#ifdef UNIV_DEBUG
			if (log_debug_writes) {
				fprintf(stderr, 
"InnoDB: Parsed a multi log rec type %lu len %lu space %lu page no %lu\n",
				(ulong) type, (ulong) len, (ulong) space,
				(ulong) page_no);
			}
#endif /* UNIV_DEBUG */

			total_len += len;
			n_recs++;

			ptr += len;

			if (type == MLOG_MULTI_REC_END) {

				/* Found the end mark for the records */

				break;
			}
		}

		new_recovered_lsn = recv_calc_lsn_on_data_add(
					recv_sys->recovered_lsn, total_len);

		if (ut_dulint_cmp(new_recovered_lsn, recv_sys->scanned_lsn)
								> 0) {
			/* The log record filled a log block, and we require
			that also the next log block should have been scanned
			in */

			return(FALSE);
		}

		/* Add all the records to the hash table */

		ptr = recv_sys->buf + recv_sys->recovered_offset;

		for (;;) {
			old_lsn = recv_sys->recovered_lsn;
			len = recv_parse_log_rec(ptr, end_ptr, &type, &space,
							&page_no, &body);
			if (recv_sys->found_corrupt_log) {

				recv_report_corrupt_log(ptr,
							type, space, page_no);
			}

			ut_a(len != 0);
			ut_a(0 == ((ulint)*ptr & MLOG_SINGLE_REC_FLAG));

			recv_sys->recovered_offset += len;
			recv_sys->recovered_lsn = recv_calc_lsn_on_data_add(
								old_lsn, len);
			if (type == MLOG_MULTI_REC_END) {

				/* Found the end mark for the records */

				break;
			}

			if (store_to_hash) {
				recv_add_to_hash_table(type, space, page_no,
						body, ptr + len, old_lsn,
						new_recovered_lsn);
#ifdef UNIV_LOG_REPLICATE
			} else {
				/* In debug checking, check that the replicate
				page has become identical with the original
				page */
				recv_compare_replicate(space, page_no);
#endif /* UNIV_LOG_REPLICATE */
			}
			
			ptr += len;
		}
	}
   
	goto loop;
}

/***********************************************************
Adds data from a new log block to the parsing buffer of recv_sys if
recv_sys->parse_start_lsn is non-zero. */
static
ibool
recv_sys_add_to_parsing_buf(
/*========================*/
				/* out: TRUE if more data added */
	byte*	log_block,	/* in: log block */
	dulint	scanned_lsn)	/* in: lsn of how far we were able to find
				data in this log block */
{
	ulint	more_len;
	ulint	data_len;
	ulint	start_offset;
	ulint	end_offset;

	ut_ad(ut_dulint_cmp(scanned_lsn, recv_sys->scanned_lsn) >= 0);

	if (ut_dulint_is_zero(recv_sys->parse_start_lsn)) {
		/* Cannot start parsing yet because no start point for
		it found */

		return(FALSE);
	}

	data_len = log_block_get_data_len(log_block);

	if (ut_dulint_cmp(recv_sys->parse_start_lsn, scanned_lsn) >= 0) {

		return(FALSE);

	} else if (ut_dulint_cmp(recv_sys->scanned_lsn, scanned_lsn) >= 0) {

		return(FALSE);
								
	} else if (ut_dulint_cmp(recv_sys->parse_start_lsn,
						recv_sys->scanned_lsn) > 0) {
		more_len = ut_dulint_minus(scanned_lsn,
						recv_sys->parse_start_lsn);
	} else {
		more_len = ut_dulint_minus(scanned_lsn, recv_sys->scanned_lsn);
	}

	if (more_len == 0) {

		return(FALSE);
	}
	
	ut_ad(data_len >= more_len);

	start_offset = data_len - more_len;

	if (start_offset < LOG_BLOCK_HDR_SIZE) {
		start_offset = LOG_BLOCK_HDR_SIZE;
	}

	end_offset = data_len;

	if (end_offset > OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE) {
		end_offset = OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE;
	}

	ut_ad(start_offset <= end_offset);

	if (start_offset < end_offset) {
		ut_memcpy(recv_sys->buf + recv_sys->len,
			log_block + start_offset, end_offset - start_offset);

		recv_sys->len += end_offset - start_offset;

		ut_a(recv_sys->len <= RECV_PARSING_BUF_SIZE);
	}

	return(TRUE);
}

/***********************************************************
Moves the parsing buffer data left to the buffer start. */
static
void
recv_sys_justify_left_parsing_buf(void)
/*===================================*/
{	
	ut_memmove(recv_sys->buf, recv_sys->buf + recv_sys->recovered_offset,
				recv_sys->len - recv_sys->recovered_offset);

	recv_sys->len -= recv_sys->recovered_offset;

	recv_sys->recovered_offset = 0;
}

/***********************************************************
Scans log from a buffer and stores new log data to the parsing buffer. Parses
and hashes the log records if new data found. */

ibool
recv_scan_log_recs(
/*===============*/
				/* out: TRUE if limit_lsn has been reached, or
				not able to scan any more in this log group */
	ibool	apply_automatically,/* in: TRUE if we want this function to
				apply log records automatically when the
				hash table becomes full; in the hot backup tool
				the tool does the applying, not this
				function */
	ulint	available_memory,/* in: we let the hash table of recs to grow
				to this size, at the maximum */
	ibool	store_to_hash,	/* in: TRUE if the records should be stored
				to the hash table; this is set to FALSE if just
				debug checking is needed */
	byte*	buf,		/* in: buffer containing a log segment or
				garbage */
	ulint	len,		/* in: buffer length */
	dulint	start_lsn,	/* in: buffer start lsn */
	dulint*	contiguous_lsn,	/* in/out: it is known that all log groups
				contain contiguous log data up to this lsn */
	dulint*	group_scanned_lsn)/* out: scanning succeeded up to this lsn */
{
	byte*	log_block;
	ulint	no;
	dulint	scanned_lsn;
	ibool	finished;
	ulint	data_len;
	ibool	more_data;

	ut_ad(ut_dulint_get_low(start_lsn) % OS_FILE_LOG_BLOCK_SIZE == 0);
	ut_ad(len % OS_FILE_LOG_BLOCK_SIZE == 0);
	ut_ad(len > 0);
	ut_a(apply_automatically <= TRUE);
	ut_a(store_to_hash <= TRUE);
	
	finished = FALSE;
	
	log_block = buf;
	scanned_lsn = start_lsn;
	more_data = FALSE;
	
	while (log_block < buf + len && !finished) {

		no = log_block_get_hdr_no(log_block);
/*
		fprintf(stderr, "Log block header no %lu\n", no);

		fprintf(stderr, "Scanned lsn no %lu\n",
				log_block_convert_lsn_to_no(scanned_lsn));
*/
		if (no != log_block_convert_lsn_to_no(scanned_lsn)
		    || !log_block_checksum_is_ok_or_old_format(log_block)) {

			if (no == log_block_convert_lsn_to_no(scanned_lsn)
			    && !log_block_checksum_is_ok_or_old_format(
								log_block)) {
				fprintf(stderr,
"InnoDB: Log block no %lu at lsn %lu %lu has\n"
"InnoDB: ok header, but checksum field contains %lu, should be %lu\n",
				(ulong) no,
				(ulong) ut_dulint_get_high(scanned_lsn),
				(ulong) ut_dulint_get_low(scanned_lsn),
				(ulong) log_block_get_checksum(log_block),
				(ulong) log_block_calc_checksum(log_block));
			}

			/* Garbage or an incompletely written log block */

			finished = TRUE;

			break;
		}

		if (log_block_get_flush_bit(log_block)) {
			/* This block was a start of a log flush operation:
			we know that the previous flush operation must have
			been completed for all log groups before this block
			can have been flushed to any of the groups. Therefore,
			we know that log data is contiguous up to scanned_lsn
			in all non-corrupt log groups. */

			if (ut_dulint_cmp(scanned_lsn, *contiguous_lsn) > 0) {
				*contiguous_lsn = scanned_lsn;
			}
		}

		data_len = log_block_get_data_len(log_block);

		if ((store_to_hash || (data_len == OS_FILE_LOG_BLOCK_SIZE))
		    && (ut_dulint_cmp(ut_dulint_add(scanned_lsn, data_len),
						recv_sys->scanned_lsn) > 0)
		    && (recv_sys->scanned_checkpoint_no > 0)
		    && (log_block_get_checkpoint_no(log_block)
		       < recv_sys->scanned_checkpoint_no)
		    && (recv_sys->scanned_checkpoint_no
			- log_block_get_checkpoint_no(log_block)
			> 0x80000000UL)) {

			/* Garbage from a log buffer flush which was made
			before the most recent database recovery */

			finished = TRUE;
#ifdef UNIV_LOG_DEBUG
			/* This is not really an error, but currently
			we stop here in the debug version: */

			ut_error;
#endif
			break;
		}		    
		
		if (ut_dulint_is_zero(recv_sys->parse_start_lsn)
			&& (log_block_get_first_rec_group(log_block) > 0)) {

			/* We found a point from which to start the parsing
			of log records */

			recv_sys->parse_start_lsn =
				ut_dulint_add(scanned_lsn,
				   log_block_get_first_rec_group(log_block));
			recv_sys->scanned_lsn = recv_sys->parse_start_lsn;
			recv_sys->recovered_lsn = recv_sys->parse_start_lsn;
		}

		scanned_lsn = ut_dulint_add(scanned_lsn, data_len);

		if (ut_dulint_cmp(scanned_lsn, recv_sys->scanned_lsn) > 0) {

			/* We were able to find more log data: add it to the
			parsing buffer if parse_start_lsn is already
			non-zero */

			if (recv_sys->len + 4 * OS_FILE_LOG_BLOCK_SIZE
						>= RECV_PARSING_BUF_SIZE) {
				fprintf(stderr,
"InnoDB: Error: log parsing buffer overflow. Recovery may have failed!\n");

				recv_sys->found_corrupt_log = TRUE;

			} else if (!recv_sys->found_corrupt_log) {
				more_data = recv_sys_add_to_parsing_buf(
						log_block, scanned_lsn);
			}

			recv_sys->scanned_lsn = scanned_lsn;
			recv_sys->scanned_checkpoint_no =
					log_block_get_checkpoint_no(log_block);
		}
						
		if (data_len < OS_FILE_LOG_BLOCK_SIZE) {
			/* Log data for this group ends here */

			finished = TRUE;
		} else {
			log_block += OS_FILE_LOG_BLOCK_SIZE;
		}
	}

	*group_scanned_lsn = scanned_lsn;

	if (recv_needed_recovery
	    || (recv_is_from_backup && !recv_is_making_a_backup)) {
		recv_scan_print_counter++;

		if (finished || (recv_scan_print_counter % 80 == 0)) {

			fprintf(stderr, 
"InnoDB: Doing recovery: scanned up to log sequence number %lu %lu\n",
				(ulong) ut_dulint_get_high(*group_scanned_lsn),
				(ulong) ut_dulint_get_low(*group_scanned_lsn));
		}
	}

	if (more_data && !recv_sys->found_corrupt_log) {
		/* Try to parse more log records */

		recv_parse_log_recs(store_to_hash);

		if (store_to_hash && mem_heap_get_size(recv_sys->heap)
						> available_memory
		    && apply_automatically) {
						
			/* Hash table of log records has grown too big:
			empty it; FALSE means no ibuf operations
			allowed, as we cannot add new records to the
			log yet: they would be produced by ibuf
			operations */
		
			recv_apply_hashed_log_recs(FALSE);
		} 

		if (recv_sys->recovered_offset > RECV_PARSING_BUF_SIZE / 4) {
			/* Move parsing buffer data to the buffer start */

			recv_sys_justify_left_parsing_buf();
		}	
	}

	return(finished);
}	

/***********************************************************
Scans log from a buffer and stores new log data to the parsing buffer. Parses
and hashes the log records if new data found. */
static
void
recv_group_scan_log_recs(
/*=====================*/
	log_group_t* group,	/* in: log group */	
	dulint*	contiguous_lsn,	/* in/out: it is known that all log groups
				contain contiguous log data up to this lsn */
	dulint*	group_scanned_lsn)/* out: scanning succeeded up to this lsn */
{
	ibool	finished;
	dulint	start_lsn;
	dulint	end_lsn;
	
	finished = FALSE;

	start_lsn = *contiguous_lsn;
		
	while (!finished) {			
		end_lsn = ut_dulint_add(start_lsn, RECV_SCAN_SIZE);

		log_group_read_log_seg(LOG_RECOVER, log_sys->buf,
						group, start_lsn, end_lsn);

		finished = recv_scan_log_recs(TRUE,
                                (buf_pool->n_frames
                                - recv_n_pool_free_frames) * UNIV_PAGE_SIZE,
				TRUE, log_sys->buf,
				RECV_SCAN_SIZE, start_lsn,
				contiguous_lsn, group_scanned_lsn);
		start_lsn = end_lsn;
	}

#ifdef UNIV_DEBUG
	if (log_debug_writes) {
		fprintf(stderr,
	"InnoDB: Scanned group %lu up to log sequence number %lu %lu\n",
				(ulong) group->id,
				(ulong) ut_dulint_get_high(*group_scanned_lsn),
				(ulong) ut_dulint_get_low(*group_scanned_lsn));
	}
#endif /* UNIV_DEBUG */
}

/************************************************************
Recovers from a checkpoint. When this function returns, the database is able
to start processing of new user transactions, but the function
recv_recovery_from_checkpoint_finish should be called later to complete
the recovery and free the resources used in it. */

ulint
recv_recovery_from_checkpoint_start(
/*================================*/
				/* out: error code or DB_SUCCESS */
	ulint	type,		/* in: LOG_CHECKPOINT or LOG_ARCHIVE */
	dulint	limit_lsn,	/* in: recover up to this lsn if possible */
	dulint	min_flushed_lsn,/* in: min flushed lsn from data files */
	dulint	max_flushed_lsn)/* in: max flushed lsn from data files */
{
	log_group_t*	group;
	log_group_t*	max_cp_group;
	log_group_t*	up_to_date_group;
	ulint		max_cp_field;
	dulint		checkpoint_lsn;
	dulint		checkpoint_no;
	dulint		old_scanned_lsn;
	dulint		group_scanned_lsn;
	dulint		contiguous_lsn;
	dulint		archived_lsn;
	ulint		capacity;
	byte*		buf;
	byte		log_hdr_buf[LOG_FILE_HDR_SIZE];
	ulint		err;

	ut_ad((type != LOG_CHECKPOINT)
			|| (ut_dulint_cmp(limit_lsn, ut_dulint_max) == 0));
	
	if (type == LOG_CHECKPOINT) {
		recv_sys_create();
		recv_sys_init(FALSE, buf_pool_get_curr_size());
	}

	if (srv_force_recovery >= SRV_FORCE_NO_LOG_REDO) {
		fprintf(stderr,
		"InnoDB: The user has set SRV_FORCE_NO_LOG_REDO on\n");
		fprintf(stderr,
		"InnoDB: Skipping log redo\n");
		
		return(DB_SUCCESS);
	}

	recv_recovery_on = TRUE;

	recv_sys->limit_lsn = limit_lsn;

	mutex_enter(&(log_sys->mutex));

	/* Look for the latest checkpoint from any of the log groups */
	
	err = recv_find_max_checkpoint(&max_cp_group, &max_cp_field);

	if (err != DB_SUCCESS) {

		mutex_exit(&(log_sys->mutex));

		return(err);
	}
		
	log_group_read_checkpoint_info(max_cp_group, max_cp_field);

	buf = log_sys->checkpoint_buf;

	checkpoint_lsn = mach_read_from_8(buf + LOG_CHECKPOINT_LSN);
	checkpoint_no = mach_read_from_8(buf + LOG_CHECKPOINT_NO);
	archived_lsn = mach_read_from_8(buf + LOG_CHECKPOINT_ARCHIVED_LSN);

	/* Read the first log file header to print a note if this is
	a recovery from a restored InnoDB Hot Backup */
	
	fil_io(OS_FILE_READ | OS_FILE_LOG, TRUE, max_cp_group->space_id,
				0, 0, LOG_FILE_HDR_SIZE,
				log_hdr_buf, max_cp_group);

	if (0 == ut_memcmp(log_hdr_buf + LOG_FILE_WAS_CREATED_BY_HOT_BACKUP,
			(byte*)"ibbackup", (sizeof "ibbackup") - 1)) {
		/* This log file was created by ibbackup --restore: print
		a note to the user about it */

		fprintf(stderr,
	"InnoDB: The log file was created by ibbackup --apply-log at\n"
	"InnoDB: %s\n", log_hdr_buf + LOG_FILE_WAS_CREATED_BY_HOT_BACKUP);
		fprintf(stderr,
"InnoDB: NOTE: the following crash recovery is part of a normal restore.\n");
		
		/* Wipe over the label now */

		memset(log_hdr_buf + LOG_FILE_WAS_CREATED_BY_HOT_BACKUP,
					' ', 4);
		/* Write to the log file to wipe over the label */
		fil_io(OS_FILE_WRITE | OS_FILE_LOG, TRUE,
				max_cp_group->space_id,
				0, 0, OS_FILE_LOG_BLOCK_SIZE,
				log_hdr_buf, max_cp_group);
	}

#ifdef UNIV_LOG_ARCHIVE				
	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	while (group) {
		log_checkpoint_get_nth_group_info(buf, group->id,
						&(group->archived_file_no),
						&(group->archived_offset));

		group = UT_LIST_GET_NEXT(log_groups, group);
	}
#endif /* UNIV_LOG_ARCHIVE */

	if (type == LOG_CHECKPOINT) {
		/* Start reading the log groups from the checkpoint lsn up. The
		variable contiguous_lsn contains an lsn up to which the log is
		known to be contiguously written to all log groups. */

		recv_sys->parse_start_lsn = checkpoint_lsn;
		recv_sys->scanned_lsn = checkpoint_lsn;
		recv_sys->scanned_checkpoint_no = 0;
		recv_sys->recovered_lsn = checkpoint_lsn;

		srv_start_lsn = checkpoint_lsn;

		/* NOTE: we always do a 'recovery' at startup, but only if
		there is something wrong we will print a message to the
		user about recovery: */
		
		if (ut_dulint_cmp(checkpoint_lsn, max_flushed_lsn) != 0
	    	   || ut_dulint_cmp(checkpoint_lsn, min_flushed_lsn) != 0) {

			if (ut_dulint_cmp(checkpoint_lsn, max_flushed_lsn)
								< 0) {
				fprintf(stderr,
"InnoDB: ##########################################################\n"
"InnoDB:                          WARNING!\n"
"InnoDB: The log sequence number in ibdata files is higher\n"
"InnoDB: than the log sequence number in the ib_logfiles! Are you sure\n"
"InnoDB: you are using the right ib_logfiles to start up the database?\n"
"InnoDB: Log sequence number in ib_logfiles is %lu %lu, log\n"
"InnoDB: sequence numbers stamped to ibdata file headers are between\n"
"InnoDB: %lu %lu and %lu %lu.\n"
"InnoDB: ##########################################################\n",
				(ulong) ut_dulint_get_high(checkpoint_lsn),
				(ulong) ut_dulint_get_low(checkpoint_lsn),
				(ulong) ut_dulint_get_high(min_flushed_lsn),
				(ulong) ut_dulint_get_low(min_flushed_lsn),
				(ulong) ut_dulint_get_high(max_flushed_lsn),
				(ulong) ut_dulint_get_low(max_flushed_lsn));
			}

	    	   	recv_needed_recovery = TRUE;
	    	   
			ut_print_timestamp(stderr);

	    		fprintf(stderr,
"  InnoDB: Database was not shut down normally!\n"
"InnoDB: Starting crash recovery.\n");

			fprintf(stderr,
"InnoDB: Reading tablespace information from the .ibd files...\n");

			fil_load_single_table_tablespaces();

			/* If we are using the doublewrite method, we will
			check if there are half-written pages in data files,
			and restore them from the doublewrite buffer if
			possible */
		
			if (srv_force_recovery < SRV_FORCE_NO_LOG_REDO) {
		
				fprintf(stderr,
"InnoDB: Restoring possible half-written data pages from the doublewrite\n"
"InnoDB: buffer...\n");
				trx_sys_doublewrite_init_or_restore_pages(
									TRUE);
			}

			ut_print_timestamp(stderr);

			fprintf(stderr, 
"  InnoDB: Starting log scan based on checkpoint at\n"
"InnoDB: log sequence number %lu %lu.\n",
		 			(ulong) ut_dulint_get_high(checkpoint_lsn),
					(ulong) ut_dulint_get_low(checkpoint_lsn));
		} else {
			/* Init the doublewrite buffer memory structure */
			trx_sys_doublewrite_init_or_restore_pages(FALSE);
		}
	}

	contiguous_lsn = ut_dulint_align_down(recv_sys->scanned_lsn,
						OS_FILE_LOG_BLOCK_SIZE);
	if (type == LOG_ARCHIVE) {
 		/* Try to recover the remaining part from logs: first from
		the logs of the archived group */

		group = recv_sys->archive_group;
		capacity = log_group_get_capacity(group);

		if ((ut_dulint_cmp(recv_sys->scanned_lsn,
				ut_dulint_add(checkpoint_lsn, capacity)) > 0)
		   || (ut_dulint_cmp(checkpoint_lsn,
			ut_dulint_add(recv_sys->scanned_lsn, capacity)) > 0)) {

			mutex_exit(&(log_sys->mutex));

			/* The group does not contain enough log: probably
			an archived log file was missing or corrupt */

			return(DB_ERROR);
		}
		
		recv_group_scan_log_recs(group, &contiguous_lsn,
							&group_scanned_lsn);
		if (ut_dulint_cmp(recv_sys->scanned_lsn, checkpoint_lsn) < 0) {

			mutex_exit(&(log_sys->mutex));

			/* The group did not contain enough log: an archived
			log file was missing or invalid, or the log group
			was corrupt */

			return(DB_ERROR);
		}

		group->scanned_lsn = group_scanned_lsn;
		up_to_date_group = group;
	} else {
		up_to_date_group = max_cp_group;
	}

	ut_ad(RECV_SCAN_SIZE <= log_sys->buf_size);

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	if ((type == LOG_ARCHIVE) && (group == recv_sys->archive_group)) {
		group = UT_LIST_GET_NEXT(log_groups, group);
	}		

	while (group) {		
		old_scanned_lsn = recv_sys->scanned_lsn;

		recv_group_scan_log_recs(group, &contiguous_lsn,
							&group_scanned_lsn);
		group->scanned_lsn = group_scanned_lsn;
		
		if (ut_dulint_cmp(old_scanned_lsn, group_scanned_lsn) < 0) {
			/* We found a more up-to-date group */

			up_to_date_group = group;
		}
		
		if ((type == LOG_ARCHIVE)
				&& (group == recv_sys->archive_group)) {
			group = UT_LIST_GET_NEXT(log_groups, group);
		}		

		group = UT_LIST_GET_NEXT(log_groups, group);
	}

	/* We currently have only one log group */
	if (ut_dulint_cmp(group_scanned_lsn, checkpoint_lsn) < 0) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
"  InnoDB: ERROR: We were only able to scan the log up to\n"
"InnoDB: %lu %lu, but a checkpoint was at %lu %lu.\n"
"InnoDB: It is possible that the database is now corrupt!\n",
			 (ulong) ut_dulint_get_high(group_scanned_lsn),
			 (ulong) ut_dulint_get_low(group_scanned_lsn),
			 (ulong) ut_dulint_get_high(checkpoint_lsn),
			 (ulong) ut_dulint_get_low(checkpoint_lsn));
	}

	if (ut_dulint_cmp(group_scanned_lsn, recv_max_page_lsn) < 0) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
"  InnoDB: ERROR: We were only able to scan the log up to %lu %lu\n"
"InnoDB: but a database page a had an lsn %lu %lu. It is possible that the\n"
"InnoDB: database is now corrupt!\n",
			 (ulong) ut_dulint_get_high(group_scanned_lsn),
			 (ulong) ut_dulint_get_low(group_scanned_lsn),
			 (ulong) ut_dulint_get_high(recv_max_page_lsn),
			 (ulong) ut_dulint_get_low(recv_max_page_lsn));
	}

	if (ut_dulint_cmp(recv_sys->recovered_lsn, checkpoint_lsn) < 0) {

		mutex_exit(&(log_sys->mutex));

		if (ut_dulint_cmp(recv_sys->recovered_lsn, limit_lsn) >= 0) {

			return(DB_SUCCESS);
		}

		ut_error;

		return(DB_ERROR);
	}
	
	/* Synchronize the uncorrupted log groups to the most up-to-date log
	group; we also copy checkpoint info to groups */

	log_sys->next_checkpoint_lsn = checkpoint_lsn;
	log_sys->next_checkpoint_no = ut_dulint_add(checkpoint_no, 1);

#ifdef UNIV_LOG_ARCHIVE
	log_sys->archived_lsn = archived_lsn;
#endif /* UNIV_LOG_ARCHIVE */
	
	recv_synchronize_groups(up_to_date_group);

	if (!recv_needed_recovery) {
		if (ut_dulint_cmp(checkpoint_lsn, recv_sys->recovered_lsn)
								!= 0) {
			fprintf(stderr,
"InnoDB: Warning: we did not need to do crash recovery, but log scan\n"
"InnoDB: progressed past the checkpoint lsn %lu %lu up to lsn %lu %lu\n",
			 (ulong) ut_dulint_get_high(checkpoint_lsn),
			 (ulong) ut_dulint_get_low(checkpoint_lsn),
			 (ulong) ut_dulint_get_high(recv_sys->recovered_lsn),
			 (ulong) ut_dulint_get_low(recv_sys->recovered_lsn));
		}
	} else {
		srv_start_lsn = recv_sys->recovered_lsn;
	}
	
	log_sys->lsn = recv_sys->recovered_lsn;

	ut_memcpy(log_sys->buf, recv_sys->last_block, OS_FILE_LOG_BLOCK_SIZE);

	log_sys->buf_free = ut_dulint_get_low(log_sys->lsn)
						% OS_FILE_LOG_BLOCK_SIZE;
	log_sys->buf_next_to_write = log_sys->buf_free;
	log_sys->written_to_some_lsn = log_sys->lsn;
	log_sys->written_to_all_lsn = log_sys->lsn;

	log_sys->last_checkpoint_lsn = checkpoint_lsn;
	
	log_sys->next_checkpoint_no = ut_dulint_add(checkpoint_no, 1);
								
#ifdef UNIV_LOG_ARCHIVE
	if (ut_dulint_cmp(archived_lsn, ut_dulint_max) == 0) {

		log_sys->archiving_state = LOG_ARCH_OFF;
	}
#endif /* UNIV_LOG_ARCHIVE */

	mutex_enter(&(recv_sys->mutex));
	
	recv_sys->apply_log_recs = TRUE;

 	mutex_exit(&(recv_sys->mutex));
	
	mutex_exit(&(log_sys->mutex));

	recv_lsn_checks_on = TRUE;

	/* The database is now ready to start almost normal processing of user
	transactions: transaction rollbacks and the application of the log
	records in the hash table can be run in background. */

	return(DB_SUCCESS);
}

/************************************************************
Completes recovery from a checkpoint. */

void
recv_recovery_from_checkpoint_finish(void)
/*======================================*/
{
	int 		i;
	os_thread_id_t	recovery_thread_id;

	/* Apply the hashed log records to the respective file pages */
	
	if (srv_force_recovery < SRV_FORCE_NO_LOG_REDO) {

		recv_apply_hashed_log_recs(TRUE);
	}

#ifdef UNIV_DEBUG
	if (log_debug_writes) {
		fprintf(stderr,
		"InnoDB: Log records applied to the database\n");
	}
#endif /* UNIV_DEBUG */

	if (recv_needed_recovery) {
		trx_sys_print_mysql_master_log_pos();
		trx_sys_print_mysql_binlog_offset();
	}

	if (recv_sys->found_corrupt_log) {

		fprintf(stderr,
	"InnoDB: WARNING: the log file may have been corrupt and it\n"
	"InnoDB: is possible that the log scan or parsing did not proceed\n"
	"InnoDB: far enough in recovery. Please run CHECK TABLE\n"
	"InnoDB: on your InnoDB tables to check that they are ok!\n"
	"InnoDB: It may be safest to recover your InnoDB database from\n"
	"InnoDB: a backup!\n");
	}

	/* Free the resources of the recovery system */

	recv_recovery_on = FALSE;

#ifndef UNIV_LOG_DEBUG
	recv_sys_free();
#endif
	if (srv_force_recovery < SRV_FORCE_NO_TRX_UNDO) {
		/* Rollback the uncommitted transactions which have no user
		session */

		os_thread_create(trx_rollback_or_clean_all_without_sess,
				(void *)&i, &recovery_thread_id);
	}
}

/**********************************************************
Resets the logs. The contents of log files will be lost! */

void
recv_reset_logs(
/*============*/
	dulint	lsn,		/* in: reset to this lsn rounded up to
				be divisible by OS_FILE_LOG_BLOCK_SIZE,
				after which we add LOG_BLOCK_HDR_SIZE */
#ifdef UNIV_LOG_ARCHIVE
	ulint	arch_log_no,	/* in: next archived log file number */
#endif /* UNIV_LOG_ARCHIVE */
	ibool	new_logs_created)/* in: TRUE if resetting logs is done
				at the log creation; FALSE if it is done
				after archive recovery */
{
	log_group_t*	group;

#ifdef UNIV_SYNC_DEBUG
	ut_ad(mutex_own(&(log_sys->mutex)));
#endif /* UNIV_SYNC_DEBUG */
	log_sys->lsn = ut_dulint_align_up(lsn, OS_FILE_LOG_BLOCK_SIZE);

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	while (group) {
		group->lsn = log_sys->lsn;
		group->lsn_offset = LOG_FILE_HDR_SIZE;
#ifdef UNIV_LOG_ARCHIVE
		group->archived_file_no = arch_log_no;		
		group->archived_offset = 0;
#endif /* UNIV_LOG_ARCHIVE */

		if (!new_logs_created) {
			recv_truncate_group(group, group->lsn, group->lsn,
						group->lsn, group->lsn);
		}
	
		group = UT_LIST_GET_NEXT(log_groups, group);
	}

	log_sys->buf_next_to_write = 0;
	log_sys->written_to_some_lsn = log_sys->lsn;
	log_sys->written_to_all_lsn = log_sys->lsn;

	log_sys->next_checkpoint_no = ut_dulint_zero;
	log_sys->last_checkpoint_lsn = ut_dulint_zero;

#ifdef UNIV_LOG_ARCHIVE
	log_sys->archived_lsn = log_sys->lsn;
#endif /* UNIV_LOG_ARCHIVE */
	
	log_block_init(log_sys->buf, log_sys->lsn);
	log_block_set_first_rec_group(log_sys->buf, LOG_BLOCK_HDR_SIZE);

	log_sys->buf_free = LOG_BLOCK_HDR_SIZE;
	log_sys->lsn = ut_dulint_add(log_sys->lsn, LOG_BLOCK_HDR_SIZE);
	
	mutex_exit(&(log_sys->mutex));

	/* Reset the checkpoint fields in logs */
	
	log_make_checkpoint_at(ut_dulint_max, TRUE);
	log_make_checkpoint_at(ut_dulint_max, TRUE);
	
	mutex_enter(&(log_sys->mutex));
}

#ifdef UNIV_HOTBACKUP
/**********************************************************
Creates new log files after a backup has been restored. */

void
recv_reset_log_files_for_backup(
/*============================*/
	const char*	log_dir,	/* in: log file directory path */
	ulint		n_log_files,	/* in: number of log files */
	ulint		log_file_size,	/* in: log file size */
	dulint		lsn)		/* in: new start lsn, must be
					divisible by OS_FILE_LOG_BLOCK_SIZE */
{
	os_file_t	log_file;
	ibool		success;
	byte*		buf;
	ulint		i;
	ulint		log_dir_len;
	char*		name;
	static const
	char		logfilename[] = "ib_logfile";

	log_dir_len = strlen(log_dir);
	/* reserve space for log_dir, "ib_logfile" and a number */
	name = memcpy(mem_alloc(log_dir_len + ((sizeof logfilename) + 11)),
		log_dir, log_dir_len);
	memcpy(name + log_dir_len, logfilename, sizeof logfilename);

	buf = ut_malloc(LOG_FILE_HDR_SIZE + OS_FILE_LOG_BLOCK_SIZE);
        memset(buf, '\0', LOG_FILE_HDR_SIZE + OS_FILE_LOG_BLOCK_SIZE);

	for (i = 0; i < n_log_files; i++) {

		sprintf(name + log_dir_len + sizeof logfilename, "%lu", (ulong) i);

		log_file = os_file_create_simple(name, OS_FILE_CREATE,
						OS_FILE_READ_WRITE, &success);
		if (!success) {
			fprintf(stderr,
"InnoDB: Cannot create %s. Check that the file does not exist yet.\n", name);

			exit(1);
		}

		fprintf(stderr,
			"Setting log file size to %lu %lu\n",
			(ulong) ut_get_high32(log_file_size),
			(ulong) log_file_size & 0xFFFFFFFFUL);

		success = os_file_set_size(name, log_file,
					log_file_size & 0xFFFFFFFFUL,
					ut_get_high32(log_file_size));

		if (!success) {
			fprintf(stderr,
"InnoDB: Cannot set %s size to %lu %lu\n", name, (ulong) ut_get_high32(log_file_size),
						(ulong) (log_file_size & 0xFFFFFFFFUL));
			exit(1);
		}

		os_file_flush(log_file);
		os_file_close(log_file);
	}

	/* We pretend there is a checkpoint at lsn + LOG_BLOCK_HDR_SIZE */
	
	log_reset_first_header_and_checkpoint(buf, lsn);
	
	log_block_init_in_old_format(buf + LOG_FILE_HDR_SIZE, lsn);
	log_block_set_first_rec_group(buf + LOG_FILE_HDR_SIZE,
							LOG_BLOCK_HDR_SIZE);
	strcpy(name + log_dir_len + sizeof logfilename, "0");

	log_file = os_file_create_simple(name, OS_FILE_OPEN,
						OS_FILE_READ_WRITE, &success);
	if (!success) {
		fprintf(stderr, "InnoDB: Cannot open %s.\n", name);

		exit(1);
	}

	os_file_write(name, log_file, buf, 0, 0,
				LOG_FILE_HDR_SIZE + OS_FILE_LOG_BLOCK_SIZE);
	os_file_flush(log_file);
	os_file_close(log_file);

	mem_free(name);
	ut_free(buf);
}
#endif /* UNIV_HOTBACKUP */

#ifdef UNIV_LOG_ARCHIVE
/**********************************************************
Reads from the archive of a log group and performs recovery. */
static
ibool
log_group_recover_from_archive_file(
/*================================*/
					/* out: TRUE if no more complete
					consistent archive files */
	log_group_t*	group)		/* in: log group */
{
	os_file_t file_handle;
	dulint	start_lsn;
	dulint	file_end_lsn;
	dulint	dummy_lsn;
	dulint	scanned_lsn;
	ulint	len;
	ibool	ret;
	byte*	buf;
	ulint	read_offset;
	ulint	file_size;
	ulint	file_size_high;
	int	input_char;
	char	name[10000];

	ut_a(0);

try_open_again:	
	buf = log_sys->buf;

	/* Add the file to the archive file space; open the file */
	
	log_archived_file_name_gen(name, group->id, group->archived_file_no);

	file_handle = os_file_create(name, OS_FILE_OPEN,
					OS_FILE_LOG, OS_FILE_AIO, &ret);

	if (ret == FALSE) {
ask_again:
		fprintf(stderr, 
	"InnoDB: Do you want to copy additional archived log files\n"
	"InnoDB: to the directory\n");
		fprintf(stderr, 
	"InnoDB: or were these all the files needed in recovery?\n");
		fprintf(stderr, 
	"InnoDB: (Y == copy more files; N == this is all)?");

		input_char = getchar();

		if (input_char == (int) 'N') {

			return(TRUE);
		} else if (input_char == (int) 'Y') {

			goto try_open_again;
		} else {
			goto ask_again;
		}
	}

	ret = os_file_get_size(file_handle, &file_size, &file_size_high);
	ut_a(ret);

	ut_a(file_size_high == 0);
	
	fprintf(stderr, "InnoDB: Opened archived log file %s\n", name);
			
	ret = os_file_close(file_handle);
	
	if (file_size < LOG_FILE_HDR_SIZE) {
		fprintf(stderr,
			"InnoDB: Archive file header incomplete %s\n", name);
	    
		return(TRUE);
	}

	ut_a(ret);
	
	/* Add the archive file as a node to the space */
		
	fil_node_create(name, 1 + file_size / UNIV_PAGE_SIZE,
					    group->archive_space_id, FALSE);
	ut_a(RECV_SCAN_SIZE >= LOG_FILE_HDR_SIZE);

	/* Read the archive file header */
	fil_io(OS_FILE_READ | OS_FILE_LOG, TRUE, group->archive_space_id, 0, 0,
						LOG_FILE_HDR_SIZE, buf, NULL);

	/* Check if the archive file header is consistent */

	if (mach_read_from_4(buf + LOG_GROUP_ID) != group->id
	    || mach_read_from_4(buf + LOG_FILE_NO)
						!= group->archived_file_no) {
		fprintf(stderr,
	"InnoDB: Archive file header inconsistent %s\n", name);
	    
		return(TRUE);
	}

	if (!mach_read_from_4(buf + LOG_FILE_ARCH_COMPLETED)) {
		fprintf(stderr,
	"InnoDB: Archive file not completely written %s\n", name);

		return(TRUE);
	}
	
	start_lsn = mach_read_from_8(buf + LOG_FILE_START_LSN);
	file_end_lsn = mach_read_from_8(buf + LOG_FILE_END_LSN);

	if (ut_dulint_is_zero(recv_sys->scanned_lsn)) {

		if (ut_dulint_cmp(recv_sys->parse_start_lsn, start_lsn) < 0) {
			fprintf(stderr, 
	"InnoDB: Archive log file %s starts from too big a lsn\n",
								name);	    
			return(TRUE);
		}
	
		recv_sys->scanned_lsn = start_lsn;
	}
	
	if (ut_dulint_cmp(recv_sys->scanned_lsn, start_lsn) != 0) {

		fprintf(stderr,
	"InnoDB: Archive log file %s starts from a wrong lsn\n",
								name);
		return(TRUE);
	}

	read_offset = LOG_FILE_HDR_SIZE;
	
	for (;;) {
		len = RECV_SCAN_SIZE;

		if (read_offset + len > file_size) {
			len = ut_calc_align_down(file_size - read_offset,
						OS_FILE_LOG_BLOCK_SIZE);
		}

		if (len == 0) {

			break;
		}
	
#ifdef UNIV_DEBUG
		if (log_debug_writes) {
			fprintf(stderr, 
"InnoDB: Archive read starting at lsn %lu %lu, len %lu from file %s\n",
					(ulong) ut_dulint_get_high(start_lsn),
					(ulong) ut_dulint_get_low(start_lsn),
					(ulong) len, name);
		}
#endif /* UNIV_DEBUG */

		fil_io(OS_FILE_READ | OS_FILE_LOG, TRUE,
			group->archive_space_id, read_offset / UNIV_PAGE_SIZE,
			read_offset % UNIV_PAGE_SIZE, len, buf, NULL);

		ret = recv_scan_log_recs(TRUE,
                                (buf_pool->n_frames -
                                recv_n_pool_free_frames) * UNIV_PAGE_SIZE,
				TRUE, buf, len, start_lsn,
				&dummy_lsn, &scanned_lsn);

		if (ut_dulint_cmp(scanned_lsn, file_end_lsn) == 0) {

			return(FALSE);
		}

		if (ret) {
			fprintf(stderr,
		"InnoDB: Archive log file %s does not scan right\n",
								name);	    
			return(TRUE);
		}
		
		read_offset += len;
		start_lsn = ut_dulint_add(start_lsn, len);

		ut_ad(ut_dulint_cmp(start_lsn, scanned_lsn) == 0);
	}

	return(FALSE);
}

/************************************************************
Recovers from archived log files, and also from log files, if they exist. */

ulint
recv_recovery_from_archive_start(
/*=============================*/
				/* out: error code or DB_SUCCESS */
	dulint	min_flushed_lsn,/* in: min flushed lsn field from the
				data files */
	dulint	limit_lsn,	/* in: recover up to this lsn if possible */
	ulint	first_log_no)	/* in: number of the first archived log file
				to use in the recovery; the file will be
				searched from INNOBASE_LOG_ARCH_DIR specified
				in server config file */
{
	log_group_t*	group;
	ulint		group_id;
	ulint		trunc_len;
	ibool		ret;
	ulint		err;
	
	ut_a(0);

	recv_sys_create();
	recv_sys_init(FALSE, buf_pool_get_curr_size());
	
	recv_recovery_on = TRUE;
	recv_recovery_from_backup_on = TRUE;

	recv_sys->limit_lsn = limit_lsn;

	group_id = 0;

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	while (group) {
		if (group->id == group_id) {

 			break;
		}
		
		group = UT_LIST_GET_NEXT(log_groups, group);
	}

	if (!group) {
		fprintf(stderr,
		"InnoDB: There is no log group defined with id %lu!\n",
							   (ulong) group_id);
		return(DB_ERROR);
	}

	group->archived_file_no = first_log_no;

	recv_sys->parse_start_lsn = min_flushed_lsn;

	recv_sys->scanned_lsn = ut_dulint_zero;
	recv_sys->scanned_checkpoint_no = 0;
	recv_sys->recovered_lsn = recv_sys->parse_start_lsn;

	recv_sys->archive_group = group;

	ret = FALSE;
	
	mutex_enter(&(log_sys->mutex));

	while (!ret) {
		ret = log_group_recover_from_archive_file(group);

		/* Close and truncate a possible processed archive file
		from the file space */
		
		trunc_len = UNIV_PAGE_SIZE
			    * fil_space_get_size(group->archive_space_id);
		if (trunc_len > 0) {
			fil_space_truncate_start(group->archive_space_id,
								trunc_len);
		}

		group->archived_file_no++;
	}

	if (ut_dulint_cmp(recv_sys->recovered_lsn, limit_lsn) < 0) {

		if (ut_dulint_is_zero(recv_sys->scanned_lsn)) {

			recv_sys->scanned_lsn = recv_sys->parse_start_lsn;
		}

		mutex_exit(&(log_sys->mutex));

		err = recv_recovery_from_checkpoint_start(LOG_ARCHIVE,
							limit_lsn,
							ut_dulint_max,
							ut_dulint_max);
		if (err != DB_SUCCESS) {

			return(err);
		}

		mutex_enter(&(log_sys->mutex));
	}

	if (ut_dulint_cmp(limit_lsn, ut_dulint_max) != 0) {

		recv_apply_hashed_log_recs(FALSE);

		recv_reset_logs(recv_sys->recovered_lsn, 0, FALSE);
	}

	mutex_exit(&(log_sys->mutex));

	return(DB_SUCCESS);
}

/************************************************************
Completes recovery from archive. */

void
recv_recovery_from_archive_finish(void)
/*===================================*/
{
	recv_recovery_from_checkpoint_finish();

	recv_recovery_from_backup_on = FALSE;
}
#endif /* UNIV_LOG_ARCHIVE */
