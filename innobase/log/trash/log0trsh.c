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
#include "srv0srv.h"

/* Size of block reads when the log groups are scanned forward to do
roll-forward */
#define RECV_SCAN_SIZE		(4 * UNIV_PAGE_SIZE)

/* Size of block reads when the log groups are scanned backwards to synchronize
them */
#define RECV_BACK_SCAN_SIZE	(4 * UNIV_PAGE_SIZE)

recv_sys_t*	recv_sys = NULL;

recv_recover_page(block->frame, block->space, block->offset);

/************************************************************
Creates the recovery system. */

void
recv_sys_create(void)
/*=================*/
{
	ut_a(recv_sys == NULL);

	recv_sys = mem_alloc(sizeof(recv_t));

	mutex_create(&(recv_sys->mutex));

	recv_sys->hash = NULL;
	recv_sys->heap = NULL;
}

/************************************************************
Inits the recovery system for a recovery operation. */

void
recv_sys_init(void)
/*===============*/
{
	recv_sys->hash = hash_create(buf_pool_get_curr_size() / 64);
	recv_sys->heap = mem_heap_create_in_buffer(256);
}

/************************************************************
Empties the recovery system. */

void
recv_sys_empty(void)
/*================*/
{
	mutex_enter(&(recv_sys->mutex));

	hash_free(recv_sys->hash);
	mem_heap_free(recv_sys->heap);

	recv_sys->hash = NULL;
	recv_sys->heap = NULL;

	mutex_exit(&(recv_sys->mutex));
}

/***********************************************************
For recovery purposes copies the log buffer to a group to synchronize log
data. */
static
void
recv_log_buf_flush(
/*===============*/
	log_group_t*	group,		/* in: log group */
	dulint		start_lsn,	/* in: start lsn of the log data in
					the log buffer; must be divisible by
					OS_FILE_LOG_BLOCK_SIZE */
	dulint		end_lsn)	/* in: end lsn of the log data in the
					log buffer; must be divisible by
					OS_FILE_LOG_BLOCK_SIZE */
{
	ulint	len;
	
	ut_ad(mutex_own(&(log_sys->mutex)));

	len = ut_dulint_minus(end_lsn, start_lsn);
	
	log_group_write_buf(LOG_RECOVER, group, log_sys->buf, len, start_lsn,
									0);
}

/***********************************************************
Compares two buffers containing log segments and determines the highest lsn
where they match, if any. */
static
dulint
recv_log_bufs_cmp(
/*==============*/
				/* out: if no match found, ut_dulint_zero or
				if start_lsn == LOG_START_LSN, returns
				LOG_START_LSN; otherwise the highest matching
				lsn */
	byte*	recv_buf,	/* in: buffer containing valid log data */
	byte*	buf,		/* in: buffer of data from a possibly
				incompletely written log group */
	dulint	start_lsn,	/* in: buffer start lsn, must be divisible
				by OS_FILE_LOG_BLOCK_SIZE and must be >=
				LOG_START_LSN */
	dulint	end_lsn,	/* in: buffer end lsn, must be divisible
				by OS_FILE_LOG_BLOCK_SIZE */
	dulint	recovered_lsn)	/* in: recovery succeeded up to this lsn */
{
	ulint	len;
	ulint	offset;
	byte*	log_block1;
	byte*	log_block2;
	ulint	no;
	ulint	data_len;

	ut_ad(ut_dulint_cmp(start_lsn, LOG_START_LSN) >= 0);

	if (ut_dulint_cmp(end_lsn, recovered_lsn) > 0) {
		end_lsn = ut_dulint_align_up(recovered_lsn,
						OS_FILE_LOG_BLOCK_SIZE);
	}

	len = ut_dulint_minus(end_lsn, start_lsn);

	if (len == 0) {

		goto no_match;
	}
	
	ut_ad(len % OS_FILE_LOG_BLOCK_SIZE == 0);

	log_block1 = recv_buf + len;
	log_block2 = buf + len;
	
	for (;;) {
		log_block1 -= OS_FILE_LOG_BLOCK_SIZE;
		log_block2 -= OS_FILE_LOG_BLOCK_SIZE;

		no = log_block_get_hdr_no(log_block1);
		ut_a(no == log_block_get_trl_no(log_block1));

		if ((no == log_block_get_hdr_no(log_block2))
		    && (no == log_block_get_trl_no(log_block2))) {

		    	/* Match found if the block is not corrupted */

		    	data_len = log_block_get_data_len(log_block2);

		    	if (0 == ut_memcmp(log_block1 + LOG_BLOCK_DATA,
		    			   log_block2 + LOG_BLOCK_DATA,
		    			   data_len - LOG_BLOCK_DATA)) {

		    		/* Match found */

				return(ut_dulint_add(start_lsn,
						log_block2 - buf + data_len));
			}
		}

		if (log_block1 == recv_buf) {

			/* No match found */

			break;
		}    		
	}
no_match:
	if (ut_dulint_cmp(start_lsn, LOG_START_LSN) == 0) {

		return(LOG_START_LSN);
	}

	return(ut_dulint_zero);
}

/************************************************************
Copies a log segment from the most up-to-date log group to the other log
group, so that it contains the latest log data. */
static
void
recv_copy_group(
/*============*/
	log_group_t*	up_to_date_group,	/* in: the most up-to-date
						log group */
	log_group_t*	group,			/* in: copy to this log group */
	dulint_lsn	recovered_lsn)		/* in: recovery succeeded up
						to this lsn */
{
	dulint		start_lsn;
	dulint		end_lsn;
	dulint		match;
	byte*		buf;
	byte*		buf1;

	ut_ad(mutex_own(&(log_sys->mutex)));

	if (0 == ut_dulint_cmp(LOG_START_LSN, recovered_lsn)) {

		return;
	}
					
	ut_ad(RECV_BACK_SCAN_SIZE <= log_sys->buf_size);

	buf1 = mem_alloc(2 * RECV_BACK_SCAN_SIZE);
	buf = ut_align(buf, RECV_BACK_SCAN_SIZE););

	end_lsn = ut_dulint_align_up(recovered_lsn, RECV_BACK_SCAN_SIZE);

	match = ut_dulint_zero;

    	for (;;) {
		if (ut_dulint_cmp(ut_dulint_add(LOG_START_LSN,
					RECV_BACK_SCAN_SIZE), end_lsn) >= 0) {
			start_lsn = LOG_START_LSN;
		} else {
    			start_lsn = ut_dulint_subtract(end_lsn,
							RECV_BACK_SCAN_SIZE);
		}

		log_group_read_log_seg(LOG_RECOVER, buf, group, start_lsn,
								end_lsn);
		log_group_read_log_seg(LOG_RECOVER, log_sys->buf,
					up_to_date_group, start_lsn, end_lsn);

		match = recv_log_bufs_cmp(log_sys->buf, buf, start_lsn,
						end_lsn, recovered_lsn);

		if (ut_dulint_cmp(match, recovered_lsn) != 0) {
			recv_log_buf_flush(group, start_lsn, end_lsn);
		}
				
		if (!ut_dulint_zero(match)) {

			mem_free(buf1);

			return;
		}

		end_lsn = start_lsn;
	}
}

/************************************************************
Copies a log segment from the most up-to-date log group to the other log
groups, so that they all contain the latest log data. Also writes the info
about the latest checkpoint to the groups, and inits the fields in the group
memory structs to up-to-date values. */

void
recv_synchronize_groups(
/*====================*/
	log_group_t*	up_to_date_group,	/* in: the most up-to-date
						log group */
	dulint_lsn	recovered_lsn,		/* in: recovery succeeded up
						to this lsn */
	log_group_t*	max_checkpoint_group)	/* in: the group with the most
						recent checkpoint info */
{
	log_group_t*	group;

	ut_ad(mutex_own(&(log_sys->mutex)));

	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	while (group) {
		if (group != up_to_date_group) {

			/* Copy log data */

			recv_copy_group(group, up_to_date_group,
								recovered_lsn);
		}

		if (group != max_checkpoint_group) {
		
			/* Copy the checkpoint info to the group */

			log_group_checkpoint(group);

			mutex_exit(&(log_sys->mutex));

			/* Wait for the checkpoint write to complete */
			rw_lock_s_lock(&(log_sys->checkpoint_lock));
			rw_lock_s_unlock(&(log_sys->checkpoint_lock));

			mutex_enter(&(log_sys->mutex));
		}

		/* Update the fields in the group struct to correspond to
		recovered_lsn */

		log_group_set_fields(group, recovered_lsn);

		group = UT_LIST_GET_NEXT(log_groups, group);
	}	
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
	dulint		cp_no;
	ulint		field;
	ulint		fold;
	byte*		buf;
	
	ut_ad(mutex_own(&(log_sys->mutex)));

	/* Look for the latest checkpoint from the log groups */
	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	checkpoint_no = ut_dulint_zero;
	checkpoint_lsn = ut_dulint_zero;
	*max_group = NULL;
	
	buf = log_sys->checkpoint_buf;
	
	while (group) {
		group->state = LOG_GROUP_CORRUPTED;
	
		for (field = LOG_CHECKPOINT_1; field <= LOG_CHECKPOINT_2;
				field += LOG_CHECKPOINT_2 - LOG_CHECKPOINT_1) {
	
			log_group_read_checkpoint_info(group, field);

			/* Check the consistency of the checkpoint info */
			fold = ut_fold_binary(buf, LOG_CHECKPOINT_CHECKSUM_1);

			if (fold != mach_read_from_4(buf
						+ LOG_CHECKPOINT_CHECKSUM_1)) {
				goto not_consistent;
			}

			fold = ut_fold_binary(buf + LOG_CHECKPOINT_LSN,
						LOG_CHECKPOINT_CHECKSUM_2
							- LOG_CHECKPOINT_LSN);
			if (fold != mach_read_from_4(buf
						+ LOG_CHECKPOINT_CHECKSUM_2)) {
				goto not_consistent;
			}

			group->state = LOG_GROUP_OK;

			group->lsn = mach_read_from_8(buf
						+ LOG_CHECKPOINT_LSN);
			group->lsn_offset = mach_read_from_4(buf
						+ LOG_CHECKPOINT_OFFSET);
			group->lsn_file_count = mach_read_from_4(
					     buf + LOG_CHECKPOINT_FILE_COUNT);

			cp_no = mach_read_from_8(buf + LOG_CHECKPOINT_NO);

			if (ut_dulint_cmp(cp_no, max_no) >= 0) {
				*max_group = group;
				*max_field = field;
				max_no = cp_no;
			}

		not_consistent:
		}

		group = UT_LIST_GET_NEXT(log_groups, group);
	}	

	if (*max_group == NULL) {

		return(DB_ERROR);
	}

	return(DB_SUCCESS);
}

/***********************************************************
Parses log records from a buffer and stores them to a hash table to wait
merging to file pages. If the hash table becomes too big, merges automatically
it to file pages. */
static
bool
recv_parse_and_hash_log_recs(
/*=========================*/
				/* out: TRUE if limit_lsn has been reached */
	byte*	buf,		/* in: buffer containing a log segment or
				garbage */
	ulint	len,		/* in: buffer length */
	dulint	start_lsn,	/* in: buffer start lsn */
	dulint	limit_lsn,	/* in: recover at least to this lsn */
	dulint*	recovered_lsn)	/* out: was able to parse up to this lsn */
{
	
}

/************************************************************
Recovers from a checkpoint. When this function returns, the database is able
to start processing new user transactions, but the function
recv_recovery_from_checkpoint_finish should be called later to complete
the recovery and free the resources used in it. */

ulint
recv_recovery_from_checkpoint_start(
/*================================*/
				/* out: error code or DB_SUCCESS */
	dulint	limit_lsn)	/* in: recover up to this lsn if possible */
{
	log_group_t*	max_cp_group;
	log_group_t*	up_to_date_group;
	ulint		max_cp_field;
	byte*		buf;
	ulint		err;
	dulint		checkpoint_lsn;
	dulint		checkpoint_no;
	dulint		recovered_lsn;
	dulint		old_lsn;
	dulint		end_lsn;
	dulint		start_lsn;
	bool		finished;
	dulint		flush_start_lsn;
	
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

	if (ut_dulint_cmp(limit_lsn, checkpoint_lsn) < 0) {
		mutex_exit(&(log_sys->mutex));

		return(DB_ERROR);
	}

	/* Start reading the log groups from the checkpoint lsn up. The
	variable flush_start_lsn tells a lsn up to which the log is known
	to be contiguously written in all log groups. */

	recovered_lsn = checkpoint_lsn;
	flush_start_lsn = ut_dulint_align_down(checkpoint_lsn,
						OS_FILE_LOG_BLOCK_SIZE);
	up_to_date_group = max_cp_group;

	ut_ad(RECV_SCAN_SIZE <= log_sys->buf_size);
	
	group = UT_LIST_GET_FIRST(log_sys->log_groups);

	while (group) {				
		finished = FALSE;

		if (group->state == LOG_GROUP_CORRUPTED) {
			finished = TRUE;
		}

		start_lsn = flush_start_lsn;
		
		while (!finished) {			
			end_lsn = ut_dulint_add(start_lsn, RECV_SCAN_SIZE);

			log_group_read_log_seg(LOG_RECOVER, log_sys->buf,
						group, start_lsn, end_lsn);
			old_lsn = recovered_lsn;

			finished = recv_parse_and_hash_log_recs(log_sys->buf,
						RECV_SCAN_SIZE, start_lsn,
						limit_lsn, &flush_start_lsn,
						&recovered_lsn);

			if (ut_dulint_cmp(recovered_lsn, old_lsn) > 0) {

				/* We found a more up-to-date group */
				up_to_date_group = group;
			}

			start_lsn = end_lsn;
		}
		
		group = UT_LIST_GET_NEXT(log_groups, group);
	}

	/* Delete possible corrupted or extra log records from all log
	groups */

	recv_truncate_groups(recovered_lsn);

	/* Synchronize the uncorrupted log groups to the most up-to-date log
	group; we may also have to copy checkpoint info to groups */

	log_sys->next_checkpoint_lsn = checkpoint_lsn;
	log_sys->next_checkpoint_no = checkpoint_no;

	recv_synchronize_groups(up_to_date_group, _lsn, max_cp_group);

	log_sys->next_checkpoint_no = ut_dulint_add(checkpoint_no, 1);
								
	/* The database is now ready to start almost normal processing of user
	transactions */

	return(DB_SUCCESS);
}

/************************************************************
Completes recovery from a checkpoint. */

void
recv_recovery_from_checkpoint_finish(void)
/*======================================*/
{
	/* Rollback the uncommitted transactions which have no user session */

	trx_rollback_all_without_sess();

	/* Merge the hashed log records */

	recv_merge_hashed_log_recs();

	/* Free the resources of the recovery system */

	recv_sys_empty();	
}

/****************************************************************
Writes to the log a record about incrementing the row id counter. */
UNIV_INLINE
void
log_write_row_id_incr_rec(void)
/*===========================*/
{
	log_t*	log	= log_sys;
	ulint	data_len;

	mutex_enter(&(log->mutex));

	data_len = (log->buf_free % OS_FILE_LOG_BLOCK_SIZE) + 1;

	if (data_len >= OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE) {

	    	/* The string does not fit within the current log block
	    	or the the block would become full */

		mutex_exit(&(log->mutex));

		log_write_row_id_incr_rec_slow();

		return;
	}

	*(log->buf + log->buf_free) = MLOG_INCR_ROW_ID | MLOG_SINGLE_REC_FLAG;

	log_block_set_data_len(ut_align_down(log->buf + log->buf_free,
				 		OS_FILE_LOG_BLOCK_SIZE),
				data_len);
#ifdef UNIV_LOG_DEBUG
	log->old_buf_free = log->buf_free;
	log->old_lsn = log->lsn;
	log_check_log_recs(log->buf + log->buf_free, 1, log->lsn);
#endif
	log->buf_free++;
	
	ut_ad(log->buf_free <= log->buf_size);

	UT_DULINT_INC(log->lsn);

	mutex_exit(&(log->mutex));
}

/****************************************************************
Writes to the log a record about incrementing the row id counter. */
static
void
log_write_row_id_incr_rec_slow(void)
/*================================*/
{
	byte	type;

	log_reserve_and_open(1);

	type = MLOG_INCR_ROW_ID | MLOG_SINGLE_REC_FLAG;
		
	log_write_low(&type, 1);

	log_close();

	log_release();
}

/**************************************************************************
Parses and applies a log record MLOG_SET_ROW_ID. */

byte*
dict_hdr_parse_set_row_id(
/*======================*/
			/* out: end of log record or NULL */
	byte*	ptr,	/* in: buffer */
	byte*	end_ptr,/* in: buffer end */
	page_t*	page)	/* in: page or NULL */
{
	dulint	dval;
	
	ptr = mach_dulint_parse_compressed(ptr, end_ptr, &dval);
	
	if (ptr == NULL) {

		return(NULL);
	}

	if (!page) {

		return(ptr);
	}

	mach_write_to_8(page + DICT_HDR + DICT_HDR_ROW_ID, dval);

	return(ptr);	
}				

