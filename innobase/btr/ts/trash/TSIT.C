/************************************************************************
The test module for the record manager of MVB.

(c) 1994 Heikki Tuuri

Created 1/25/1994 Heikki Tuuri
*************************************************************************/


#include "rm0phr.h"
#include "rm0lgr.h"
#include "ut0ut.h"
#include "buf0mem.h"
#include "rm0ipg.h"
#include "../it0it.h"
#include "../it0hi.h"
#include "../it0ads.h"

byte		buf[100];
byte		buf2[100];
lint		lintbuf[2048];

byte		numbuf[6000];
byte		numlogrecbuf[100];
phr_record_t*	qs_table[100000];

lint		qs_comp = 0;

extern
void 
test1(void);

#ifdef NOT_DEFINED

void
q_sort(lint low, lint up)
{
	phr_record_t*	temp, *pivot;
	lint		i, j;

	
	pivot = qs_table[(low + up) / 2];
	
	i = low;
	j = up;

	while (i < j) {
		qs_comp++;
		if (cmp_phr_compare(qs_table[i], pivot)<= 0) {
			i++;
		} else {
			j--;
			temp = qs_table[i];
			qs_table[i] = qs_table[j];
			qs_table[j] = temp;
		}
	}
	
	if (j == up) {
		temp = qs_table[(low + up) / 2];
		qs_table[(low + up) / 2] = qs_table[up - 1];
		qs_table[up - 1] = temp;
		j--;
	}


	if (j - low <= 1) {
		/* do nothing */
	} else if (j - low == 2) {
		qs_comp++;
		if (cmp_phr_compare(qs_table[low], 
						qs_table[low + 1]) 
		    <= 0) {
			/* do nothing */
		} else {
			temp = qs_table[low];
			qs_table[low] = qs_table[low + 1];
			qs_table[low + 1] = temp;
		}
	} else {
		q_sort(low, j);
	}

	if (up - j <= 1) {
		/* do nothing */
	} else if (up - j == 2) {
		qs_comp++;
		if (cmp_phr_compare(qs_table[j], 
						qs_table[j + 1]) 
		    <= 0) {
			/* do nothing */
		} else {
			temp = qs_table[j];
			qs_table[j] = qs_table[j + 1];
			qs_table[j + 1] = temp;
		}
	} else {
		q_sort(j, up);
	}
}

#endif

extern
void 
test1(void)
{
	phr_record_t*	physrec;
	phr_record_t*	rec1;
	phr_record_t*	rec2;
	lgr_record_t*	logrec;
	lgrf_field_t*	logfield;
	lint		len;
	byte*		str;
	lint		len2;
	lint		tm;
	lint		oldtm;
	lint		i, j, k, l, m;
	bool		b;
	it_cur_cursor_t	cursor;
	ipg_cur_cursor_t*	page_cursor;
	ipg_page_t*	page;

	byte		c4, c3, c2, c1, c0;	
	lint		rand, rnd1, rnd2;
	byte*		nb;
	lgr_record_t*	numlogrec;
	byte*		pgbuf;
	mem_stream_t*	stream;	
	lint		tree1, tree2, tree3;
	lint		dummy1, dummy2;

	pgbuf = (byte*)lintbuf;

	stream = mem_stream_create(0);

        printf("-------------------------------------------\n");
	printf("TEST 1. Speed and basic tests.\n");

	logrec = lgr_create_logical_record(stream, 2);

	nb = numbuf;
	
	c4 = '0';
	c3 = '0';
	for (c2 = '0'; c2 <= '9'; c2++) {
		for (c1 = '0'; c1 <= '9'; c1++) {
			for (c0 = '0'; c0 <= '9'; c0++) {
				*nb = c4; nb++;
				*nb = c3; nb++;
				*nb = c2; nb++;
				*nb = c1; nb++;
				*nb = c0; nb++;
				*nb = '\0'; nb++;
			}
		}
	}
	
	numlogrec = lgr_create_logical_record(stream, 2);


	tree1 = it_create_index_tree();

	oldtm = ut_clock();

	rand = 99900;
	rnd1 = 67;
	for (j = 0; j < 1; j++) {
	for (i = 0 ; i < 100000; i++) {

		rand = (rand + 1) % 100000;

		logfield = lgr_get_nth_field(numlogrec, 0);
		lgrf_set_data(logfield, numbuf + 6 * (rand / 300));
		lgrf_set_len(logfield, 6);

		logfield = lgr_get_nth_field(numlogrec, 1);
		lgrf_set_data(logfield, numbuf + 6 * (rand % 300));
		lgrf_set_len(logfield, 6);
/*
		it_insert(tree1, numlogrec); 
*/



		it_cur_search_tree_to_nth_level(tree1, 1, numlogrec,
			IPG_SE_L_GE, &cursor, &dummy1, &dummy2);

/*
		it_cur_set_to_first(tree1, &cursor);
*/

		it_cur_insert_record(&cursor, numlogrec);
		
	}
	}
	tm = ut_clock();
	printf("Time for inserting %ld recs = %ld \n", i* j, tm - oldtm);

/*	it_print_tree(tree1, 10);*/
	hi_print_info();
	ads_print_info();
/*	
	oldtm = ut_clock();

	rand = 11113;
	for (i = 0; i < 5000; i++) {

		rand = (rand + 57123) % 100000;

		logfield = lgr_get_nth_field(numlogrec, 0);
		lgrf_set_data(logfield, numbuf + 6 * (rand / 300));
		lgrf_set_len(logfield, 6);

		logfield = lgr_get_nth_field(numlogrec, 1);
		lgrf_set_data(logfield, numbuf + 6 * (rand % 300));
		lgrf_set_len(logfield, 6);

		it_cur_search_tree_to_nth_level(tree1, 1, numlogrec,
			IPG_SE_L_GE, &cursor, &dummy1, &dummy2);

	}
	tm = ut_clock();
	printf("Time for searching %ld recs = %ld \n", i, tm - oldtm);
*/

	it_cur_set_to_first(tree1, &cursor);

	rec1 = ipg_cur_get_record(it_cur_get_page_cursor(&cursor));
	
	for (i = 0;; i++) {
		it_cur_move_to_next(&cursor);
		if (it_cur_end_of_level(&cursor)) {
			break;
		}
		rec2 = ipg_cur_get_record(it_cur_get_page_cursor(&cursor));
		ut_a(cmp_phr_compare(rec1, rec2) == -1);
		rec1 = rec2;
	}
	
	printf("tree1 checked for right sorted order!\n");

#ifdef not_defined

	oldtm = ut_clock();

	for (j = 0; j < 1; j++) {
	rand = 11113;
	for (i = 0; i < 3000; i++) {

		rand = (rand + 57123) % 100000;

		logfield = lgr_get_nth_field(numlogrec, 0);
		lgrf_set_data(logfield, numbuf + 6 * (rand / 300));
		lgrf_set_len(logfield, 6);

		logfield = lgr_get_nth_field(numlogrec, 1);
		lgrf_set_data(logfield, numbuf + 6 * (rand % 300));
		lgrf_set_len(logfield, 6);

		physrec = hi_search(numlogrec);

		ut_a(physrec);
	}
	
	}
	ut_a(physrec);
	tm = ut_clock();
	printf("Time for hi_search %ld recs = %ld \n", i * j,
				tm - oldtm);



	oldtm = ut_clock();

	for (i = 0; i < 100000; i++) {
/*		j += lgr_fold(numlogrec, -1, -1);*/
/*		b += phr_lgr_equal(physrec, numlogrec, -1);*/
		k += ut_hash_lint(j, HI_TABLE_SIZE);
	}


/*	ut_a(b);*/
	tm = ut_clock();
	printf("Time for fold + equal %ld recs %s = %ld \n", i, physrec, 
				tm - oldtm);
	
	printf("%ld %ld %ld\n", j, b, k);

	hi_print_info();

	tree2 = it_create_index_tree();

	rand = 90000;
	for (i = 0; i < 300; i++) {

		rand = (rand + 1) % 100000;

		logfield = lgr_get_nth_field(numlogrec, 0);
		lgrf_set_data(logfield, numbuf + 6 * (rand / 300));
		lgrf_set_len(logfield, 6);

		logfield = lgr_get_nth_field(numlogrec, 1);
		lgrf_set_data(logfield, numbuf + 6 * (rand % 300));
		lgrf_set_len(logfield, 6);

		it_cur_search_tree_to_nth_level(tree2, 1, numlogrec,
			IPG_SE_L_GE, &cursor);

		it_cur_insert_record(&cursor, numlogrec);

	}

	oldtm = ut_clock();

	rand = 10000;
	for (i = 0; i < 3000; i++) {

		rand = (rand + 1) % 100000;

		logfield = lgr_get_nth_field(numlogrec, 0);
		lgrf_set_data(logfield, numbuf + 6 * (rand / 300));
		lgrf_set_len(logfield, 6);

		logfield = lgr_get_nth_field(numlogrec, 1);
		lgrf_set_data(logfield, numbuf + 6 * (rand % 300));
		lgrf_set_len(logfield, 6);

		it_cur_search_tree_to_nth_level(tree2, 1, numlogrec,
			IPG_SE_L_GE, &cursor);

		it_cur_insert_record(&cursor, numlogrec);

	}
	tm = ut_clock();
	printf("Time for inserting sequentially %ld recs = %ld \n", 
		i, tm - oldtm);


/*	it_print_tree(tree2, 10); */


	tree3 = it_create_index_tree();

	rand = 0;
	for (i = 0; i < 300; i++) {

		rand = (rand + 1) % 100000;

		logfield = lgr_get_nth_field(numlogrec, 0);
		lgrf_set_data(logfield, numbuf + 6 * (rand / 300));
		lgrf_set_len(logfield, 6);

		logfield = lgr_get_nth_field(numlogrec, 1);
		lgrf_set_data(logfield, numbuf + 6 * (rand % 300));
		lgrf_set_len(logfield, 6);

		it_cur_search_tree_to_nth_level(tree3, 1, numlogrec,
			IPG_SE_L_GE, &cursor);

		it_cur_insert_record(&cursor, numlogrec);

	}

	oldtm = ut_clock();

	rand = 100000;
	for (i = 0; i < 3000; i++) {

		rand = (rand - 1) % 100000;

		logfield = lgr_get_nth_field(numlogrec, 0);
		lgrf_set_data(logfield, numbuf + 6 * (rand / 300));
		lgrf_set_len(logfield, 6);

		logfield = lgr_get_nth_field(numlogrec, 1);
		lgrf_set_data(logfield, numbuf + 6 * (rand % 300));
		lgrf_set_len(logfield, 6);

		it_cur_search_tree_to_nth_level(tree3, 1, numlogrec,
			IPG_SE_L_GE, &cursor);

		it_cur_insert_record(&cursor, numlogrec);

	}
	tm = ut_clock();
	printf("Time for inserting sequentially downw. %ld recs = %ld \n", 
		i, tm - oldtm);


/*	it_print_tree(tree3, 10); */

#endif

}

#ifdef NOT_DEFINED

/* Test of quicksort */
void
test2(void)
{
	mem_stream_t*	stream;
	byte*		stbuf;
	lgrf_field_t*	logfield;
	lint		tm;
	lint		oldtm;
	lint		i, j, k, l, m;
	lint		rand;
	lgr_record_t*	numlogrec;
	phr_record_t*	ph_rec;
	
	stream = mem_stream_create(1000);
	
	numlogrec = lgr_create_logical_record(stream, 2);
		
	oldtm = ut_clock();

	rand = 11113;
	for (i = 0; i < 50000; i++) {
		stbuf = mem_stream_alloc(stream, 30);
		
		rand = (rand + 57123) % 100000;

		logfield = lgr_get_nth_field(numlogrec, 0);
		lgrf_set_data(logfield, numbuf + 6 * (rand / 300));
		lgrf_set_len(logfield, 6);

		logfield = lgr_get_nth_field(numlogrec, 1);
		lgrf_set_data(logfield, numbuf + 6 * (rand % 300));
		lgrf_set_len(logfield, 6);

		ph_rec = phr_create_physical_record(stbuf, 30, numlogrec);
		
		qs_table[i] = ph_rec;

	}
	tm = ut_clock();
	printf("Time for inserting %ld recs to mem stream = %ld \n", 
			i, tm - oldtm);
			

	oldtm = ut_clock();
	
	q_sort(0, 50000);

	tm = ut_clock();
	printf("Time for quicksort of %ld recs = %ld, comps: %ld \n", 
			i, tm - oldtm, qs_comp);



	for (i = 1; i < 49999; i++) {
		ut_a(-1 == 
			cmp_phr_compare(qs_table[i], qs_table[i+1]
		));
	}
	tm = ut_clock();


	oldtm = ut_clock();
	for (i = 1; i < 50000; i++) {
		k += cmp_phr_compare(qs_table[i & 0xF], 
						qs_table[5]);
	}
	tm = ut_clock();
	printf("%ld\n", k);	

	printf("Time for cmp of %ld ph_recs = %ld \n", 
			i, tm - oldtm);
			
	mem_stream_free(stream);

}
#endif

void 
main(void) 
{
	test1();
/*	test2(); */
} 

