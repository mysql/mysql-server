/************************************************************************
The test module for the mini-transaction utilities

(c) 1995 Innobase Oy

Created 11/26/1995 Heikki Tuuri
*************************************************************************/

#include "sync0sync.h"
#include "sync0rw.h"
#include "mem0mem.h"
#include "log0log.h"
#include "..\mtr0mtr.h"
#include "..\mtr0log.h"

rw_lock_t	rwl[MTR_BUF_MEMO_SIZE];

/*********************************************************************
Test for mtr buffer */

void 
test1(void)
/*=======*/
{
	ulint		i;
	mtr_t		mtr;
	
        printf("-------------------------------------------\n");
	printf("MTR-TEST 1. Test of mtr buffer\n");

	mtr_start(&mtr);

	for (i = 0; i < MTR_BUF_MEMO_SIZE; i++) {
		rw_lock_create(rwl + i);
	}

	for (i = 0; i < MTR_BUF_MEMO_SIZE; i++) {
		rw_lock_s_lock(rwl + i);
		mtr_memo_push(&mtr, rwl + i, MTR_MEMO_S_LOCK);
	}

	mtr_commit(&mtr);
	
	rw_lock_list_print_info();
	ut_ad(rw_lock_n_locked() == 0);

}

/************************************************************************
Speed test function. */
void
speed_mtr(void)
/*===========*/
{
	mtr_t		mtr;

	mtr_start(&mtr);

	mtr_s_lock(rwl, &mtr);
	mtr_s_lock(rwl + 1, &mtr);
	mtr_s_lock(rwl + 2, &mtr);

	mtr_commit(&mtr);
}

/************************************************************************
Speed test function without mtr. */
void
speed_no_mtr(void)
/*===========*/
{
	rw_lock_s_lock(rwl);
	rw_lock_s_lock(rwl + 1);
	rw_lock_s_lock(rwl + 2);
	rw_lock_s_unlock(rwl + 2);
	rw_lock_s_unlock(rwl + 1);
	rw_lock_s_unlock(rwl);
}

/************************************************************************
Speed test function. */

void 
test2(void) 
/*======*/
{
	ulint	tm, oldtm;
	ulint	i, j;
	mtr_t	mtr;
	byte	buf[50];
	
	oldtm = ut_clock();

	for (i = 0; i < 1000 * UNIV_DBC * UNIV_DBC; i++) {
		speed_mtr();
	}

	tm = ut_clock();
	printf("Wall clock time for %lu mtrs %lu milliseconds\n",
					i, tm - oldtm);
	oldtm = ut_clock();

	for (i = 0; i < 1000 * UNIV_DBC * UNIV_DBC; i++) {
		speed_no_mtr();
	}

	tm = ut_clock();
	printf("Wall clock time for %lu no-mtrs %lu milliseconds\n",
					i, tm - oldtm);

	oldtm = ut_clock();
	for (i = 0; i < 4 * UNIV_DBC * UNIV_DBC; i++) {
		mtr_start(&mtr);
		for (j = 0; j < 250; j++) {
			mlog_catenate_ulint(&mtr, 5, MLOG_1BYTE);		
			mlog_catenate_ulint(&mtr, i, MLOG_4BYTES);		
			mlog_catenate_ulint(&mtr, i + 1, MLOG_4BYTES);
			mlog_catenate_string(&mtr, buf, 50);
		}
		mtr_commit(&mtr);
	}
	tm = ut_clock();
	printf("Wall clock time for %lu log writes %lu milliseconds\n",
					i * j, tm - oldtm);
	mtr_start(&mtr);
	for (j = 0; j < 250; j++) {
		mlog_catenate_ulint(&mtr, 5, MLOG_1BYTE);		
		mlog_catenate_ulint(&mtr, i, MLOG_4BYTES);		
		mlog_catenate_ulint(&mtr, i + 1, MLOG_4BYTES);
		mlog_catenate_string(&mtr, buf, 50);
	}

	mtr_print(&mtr);
	mtr_commit(&mtr);
} 

/************************************************************************
Main test function. */

void 
main(void) 
/*======*/
{
	ulint	tm, oldtm;

	oldtm = ut_clock();

	sync_init();
	mem_init();
	log_init();
	
	test1();
	test2();

	tm = ut_clock();
	printf("Wall clock time for test %lu milliseconds\n", tm - oldtm);
	printf("TESTS COMPLETED SUCCESSFULLY!\n");
} 
