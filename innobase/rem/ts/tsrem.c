/************************************************************************
The test for the record manager

(c) 1994-1996 Innobase Oy

Created 1/25/1994 Heikki Tuuri
*************************************************************************/

#include "sync0sync.h"
#include "mem0mem.h"
#include "data0data.h"
#include "data0type.h"
#include "dict0dict.h"
#include "buf0buf.h"
#include "fil0fil.h"
#include "../rem0rec.h"
#include "../rem0cmp.h"

byte	buf1[100000];

/*********************************************************************
Test for data tuples. */

void 
test1(void)
/*=======*/
{
	dtype_t*	type;
	dtuple_t*	tuple, *tuple2;
	dfield_t*	field;
	mem_heap_t*	heap;	
	ulint		i, j;
	ulint 		n;
	char*		p_Pascal;
	char*		p_Cobol;

	heap = mem_heap_create(0);

        printf("-------------------------------------------\n");
	printf("DATA TUPLE-TEST 1. Basic tests.\n");

	tuple = dtuple_create(heap, 2);

	field = dtuple_get_nth_field(tuple, 0);
	dfield_set_data(field, "Pascal", 7);
	dtype_set(dfield_get_type(field), DATA_CHAR, DATA_ENGLISH, 7, 0);
	
	field = dtuple_get_nth_field(tuple, 1);
	dfield_set_data(field, "Cobol", 6);
	dtype_set(dfield_get_type(field), DATA_CHAR, DATA_ENGLISH, 6, 0);

	dtuple_validate(tuple);
	dtuple_print(tuple);
	
	tuple2 = dtuple_create(heap, 10);
	
	for (i = 0; i < 10; i++) {
		field = dtuple_get_nth_field(tuple2, i);
		dfield_set_data(field, NULL, UNIV_SQL_NULL);
		dtype_set(dfield_get_type(field), DATA_CHAR, DATA_ENGLISH,
								6, 0);
	}
	
	dtuple_print(tuple2);
	
        printf("-------------------------------------------\n");
	printf("DATA TUPLE-TEST 2. Accessor function tests.\n");

	tuple = dtuple_create(heap, 2);

	p_Pascal = "Pascal";
	p_Cobol = "Cobol";

	field = dtuple_get_nth_field(tuple, 0);
	dfield_set_data(field, p_Pascal, 7);
	dtype_set(dfield_get_type(field), DATA_CHAR, DATA_ENGLISH, 7, 0);
	
	field = dtuple_get_nth_field(tuple, 1);
	dfield_set_data(field, p_Cobol, 6);
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 16, 3);

	ut_a(dtuple_get_n_fields(tuple) == 2);
	
	field = dtuple_get_nth_field(tuple, 0);
	ut_a(p_Pascal == dfield_get_data(field));
	ut_a(7 == dfield_get_len(field));
	type = dfield_get_type(field);
	ut_a(type->mtype == DATA_CHAR);
	ut_a(type->prtype == DATA_ENGLISH);
	ut_a(type->len == 7);
	ut_a(type->prec == 0);

	field = dtuple_get_nth_field(tuple, 1);
	ut_a(p_Cobol == dfield_get_data(field));
	ut_a(6 == dfield_get_len(field));
	type = dfield_get_type(field);
	ut_a(type->mtype == DATA_VARCHAR);
	ut_a(type->prtype == DATA_ENGLISH);
	ut_a(type->len == 16);
	ut_a(type->prec == 3);

        printf("-------------------------------------------\n");
	printf("DATA TYPE-TEST 3. Other function tests\n");
				
	ut_a(dtuple_get_data_size(tuple) == 13);

	ut_a(dtuple_fold(tuple, 2) == dtuple_fold(tuple, 2));
	ut_a(dtuple_fold(tuple, 1) != dtuple_fold(tuple, 2));

        printf("-------------------------------------------\n");
	printf("DATA TUPLE-TEST 4. Random tuple generation test\n");

	for (i = 0; i < 500; i++) {
		tuple = dtuple_gen_rnd_tuple(heap);
		printf("%lu ", i);
		
		dtuple_validate(tuple);
		n = dtuple_get_n_fields(tuple);

		if (n < 25) {
			tuple2 = dtuple_create(heap, n);
			for (j = 0; j < n; j++) {
				dfield_copy(
					dtuple_get_nth_field(tuple2, j),
					dtuple_get_nth_field(tuple, j));
			}
			dtuple_validate(tuple2);
		
			ut_a(dtuple_fold(tuple, n) == 
			                 dtuple_fold(tuple2, n));	
		}
	}

	mem_print_info();
	mem_heap_free(heap);
}

/**********************************************************************
Test for physical records. */

void
test2(void)
/*=======*/
{
	dtuple_t*	tuple, *tuple2;
	dfield_t*	field;
	mem_heap_t*	heap;	
	ulint		i, n;
	char*		p_Pascal;
	char*		p_Cobol;
	rec_t*		rec, *rec2;
	byte*		data;
	ulint		len;
	byte*		buf;

	heap = mem_heap_create(0);

        printf("-------------------------------------------\n");
	printf("REC-TEST 1. Basic tests.\n");

	tuple = dtuple_create(heap, 2);

	p_Pascal = "Pascal";
	p_Cobol = "Cobol";

	field = dtuple_get_nth_field(tuple, 0);
	dfield_set_data(field, "Pascal", 7);
	dtype_set(dfield_get_type(field), DATA_CHAR, DATA_ENGLISH, 7, 0);

	field = dtuple_get_nth_field(tuple, 1);
	dfield_set_data(field, "Cobol", 6);
	dtype_set(dfield_get_type(field), DATA_CHAR, DATA_ENGLISH, 6, 0);
	
	tuple2 = dtuple_create(heap, 37);
	
	for (i = 0; i < 37; i++) {
		field = dtuple_get_nth_field(tuple2, i);
		dfield_set_data(field, NULL, UNIV_SQL_NULL);
		dtype_set(dfield_get_type(field), DATA_CHAR,
						DATA_ENGLISH, 6, 0);
	}
	
	rec = rec_convert_dtuple_to_rec(buf1, tuple);
	
	rec_validate(rec);
	rec_print(rec);

	rec2 = rec_convert_dtuple_to_rec(buf1 + 1000, tuple2);
	
	rec_validate(rec2);

	data = rec_get_nth_field(rec, 0, &len);
	
	ut_a(0 == memcmp(p_Pascal, data, 7));
	ut_a(len == 7);

	data = rec_get_nth_field(rec, 1, &len);
	
	ut_a(0 == memcmp(p_Cobol, data, 6));
	ut_a(len == 6);
	
	ut_a(2 == rec_get_n_fields(rec));	

	for (i = 0; i < 37; i++) {
		data = rec_get_nth_field(rec2, i, &len);
		ut_a(len == UNIV_SQL_NULL);
	}

        printf("-------------------------------------------\n");
	printf("REC-TEST 2. Test of accessor functions\n");

	rec_set_next_offs(rec, 8190);
	rec_set_n_owned(rec, 15);
	rec_set_heap_no(rec, 0);

	ut_a(rec_get_next_offs(rec) == 8190);
	ut_a(rec_get_n_owned(rec) == 15);
	ut_a(rec_get_heap_no(rec) == 0);
	
	rec_set_next_offs(rec, 1);
	rec_set_n_owned(rec, 1);
	rec_set_heap_no(rec, 8190);
		
	ut_a(rec_get_next_offs(rec) == 1);
	ut_a(rec_get_n_owned(rec) == 1);
	ut_a(rec_get_heap_no(rec) == 8190);
	
	buf = mem_heap_alloc(heap, 6);

	rec_copy_nth_field(buf, rec, 1, &len);
	
	ut_a(ut_memcmp(p_Cobol, buf, len) == 0);
	ut_a(len == 6);
	
	rec_set_nth_field(rec, 1, "Algol", 6);
	
	rec_validate(rec);
	
	rec_copy_nth_field(buf, rec, 1, &len);
	
	ut_a(ut_memcmp("Algol", buf, len) == 0);
	ut_a(len == 6);

	ut_a(rec_get_data_size(rec) == 13);
	ut_a((ulint)(rec_get_end(rec) - rec) == 13);
	ut_a(14 == (ulint)(rec - rec_get_start(rec)));
	
	ut_a(rec_get_size(rec) == 27);

	mem_heap_free(heap);

        printf("-------------------------------------------\n");
	printf("REC-TEST 3. Massive test of conversions \n");

	heap = mem_heap_create(0);	

	for (i = 0; i < 100; i++) {

		tuple = dtuple_gen_rnd_tuple(heap);
		
		if (i % 10 == 0) {
			printf("%lu ", i);
		}
		
		if (i % 10 == 0) {
			printf(
			"data tuple generated: %lu fields, data size %lu\n",
				dtuple_get_n_fields(tuple), 
				dtuple_get_data_size(tuple));
		}
		
		dtuple_validate(tuple);

		rec = rec_convert_dtuple_to_rec(buf1, tuple);
				
		rec_validate(rec);

		n = dtuple_get_n_fields(tuple);
		
		ut_a(cmp_dtuple_rec_prefix_equal(tuple, rec, n));
		ut_a(dtuple_fold(tuple, n) == rec_fold(rec, n));
		ut_a(rec_get_converted_size(tuple) == rec_get_size(rec));
		ut_a(rec_get_data_size(rec) == dtuple_get_data_size(tuple));
	}

	mem_print_info();
	mem_heap_free(heap);
}
	
/**********************************************************************
Test for comparisons. */

void
test3(void)
/*=======*/
{
	dtuple_t*	tuple, *tuple2, *tuple3;
	dfield_t*	field;
	mem_heap_t*	heap;	
	ulint		i, j;
	ulint		field_match, byte_match;
	rec_t*		rec;
	rec_t*		rec2;
	ulint		tm, oldtm;
	dict_index_t*	index;
	dict_table_t*	table;
	
	heap = mem_heap_create(0);

        printf("-------------------------------------------\n");
	printf("CMP-TEST 1. Basic tests.\n");

	tuple = dtuple_create(heap, 2);

	field = dtuple_get_nth_field(tuple, 0);
	dfield_set_data(field, "Pascal", 7);
	dtype_set(dfield_get_type(field), DATA_CHAR, DATA_ENGLISH, 7, 0);

	field = dtuple_get_nth_field(tuple, 1);
	dfield_set_data(field, "Cobol", 6);
	dtype_set(dfield_get_type(field), DATA_CHAR, DATA_ENGLISH, 6, 0);
	
	tuple2 = dtuple_create(heap, 2);

	field = dtuple_get_nth_field(tuple2, 0);
	dfield_set_data(field, "Pascal", 7);
	dtype_set(dfield_get_type(field), DATA_CHAR, DATA_ENGLISH, 7, 0);

	field = dtuple_get_nth_field(tuple2, 1);
	dfield_set_data(field, "Cobom", 6);
	dtype_set(dfield_get_type(field), DATA_CHAR, DATA_ENGLISH, 6, 0);

	tuple3 = dtuple_create(heap, 2);

	field = dtuple_get_nth_field(tuple3, 0);
	dfield_set_data(field, "PaSCal", 7);
	dtype_set(dfield_get_type(field), DATA_CHAR, DATA_ENGLISH, 7, 0);

	field = dtuple_get_nth_field(tuple3, 1);
	dfield_set_data(field, "CobOL", 6);
	dtype_set(dfield_get_type(field), DATA_CHAR, DATA_ENGLISH, 6, 0);

	rec = rec_convert_dtuple_to_rec(buf1, tuple);

	rec_validate(rec);
	
	ut_a(!cmp_dtuple_rec_prefix_equal(tuple2, rec, 2));
	ut_a(cmp_dtuple_rec_prefix_equal(tuple, rec, 2));
	ut_a(cmp_dtuple_rec_prefix_equal(tuple3, rec, 2));

	oldtm = ut_clock();
	j = 0;
	for (i = 0; i < 1000; i++) {
		field_match = 1;
		byte_match = 4;
		if (1 == cmp_dtuple_rec_with_match(tuple2, rec,
					&field_match, &byte_match)) {
		    	j++;
		}
	}
	tm = ut_clock();
	printf("Time for fast comp. %lu records = %lu\n", j, tm - oldtm);

	ut_a(field_match == 1);
	ut_a(byte_match == 4);
	
	oldtm = ut_clock();
	j = 0;
	for (i = 0; i < 1000; i++) {
		field_match = 0;
		byte_match = 0;
		if (1 == cmp_dtuple_rec_with_match(tuple2, rec,
					&field_match, &byte_match)) {
		    	j++;
		}
	}
	tm = ut_clock();
	printf("Time for test comp. %lu records = %lu\n", j, tm - oldtm);

	ut_a(field_match == 1);
	ut_a(byte_match == 4);
	
        printf("-------------------------------------------\n");
	printf(
	"CMP-TEST 2. A systematic test of comparisons and conversions\n");

	tuple = dtuple_create(heap, 3);
	tuple2 = dtuple_create(heap, 3);

	table = dict_table_create("TS_TABLE1", 3);

	dict_table_add_col(table, "COL1", DATA_VARCHAR, DATA_ENGLISH, 10, 0);
	dict_table_add_col(table, "COL2", DATA_VARCHAR, DATA_ENGLISH, 10, 0);
	dict_table_add_col(table, "COL3", DATA_VARCHAR, DATA_ENGLISH, 10, 0);

	ut_a(0 == dict_table_publish(table));

	index = dict_index_create("TS_TABLE1", "IND1", 0, 3, 0);

	dict_index_add_field(index, "COL1", 0);
	dict_index_add_field(index, "COL2", 0);
	dict_index_add_field(index, "COL3", 0);

	ut_a(0 == dict_index_publish(index));

	index = dict_index_get("TS_TABLE1", "IND1");
	ut_a(index);

	/* Compare all test data tuples to each other */
	for (i = 0; i < 512; i++) {
		dtuple_gen_test_tuple(tuple, i);
		rec = rec_convert_dtuple_to_rec(buf1, tuple);
		ut_a(rec_validate(rec));
		
		ut_a(0 == cmp_dtuple_rec(tuple, rec));

		for (j = 0; j < 512; j++) {
			dtuple_gen_test_tuple(tuple2, j);
			ut_a(dtuple_validate(tuple2));

			rec2 = rec_convert_dtuple_to_rec(buf1 + 500, tuple2);

			if (j < i) {
				ut_a(-1 == cmp_dtuple_rec(tuple2, rec));
				ut_a(-1 == cmp_rec_rec(rec2, rec, index));
			} else if (j == i) {
				ut_a(0 == cmp_dtuple_rec(tuple2, rec));
				ut_a(0 == cmp_rec_rec(rec2, rec, index));
			} else if (j > i) {
				ut_a(1 == cmp_dtuple_rec(tuple2, rec));
				ut_a(1 == cmp_rec_rec(rec2, rec, index));
			}
		}
	}
	mem_heap_free(heap);
}

/********************************************************************
Main test function. */

void 
main(void) 
/*======*/
{
	ulint	tm, oldtm;

	sync_init();
	mem_init();
	fil_init(25);
	buf_pool_init(100, 100);
	dict_init();
	
	oldtm = ut_clock();
	
	ut_rnd_set_seed(19);

	test1();
	test2();
	test3();

	tm = ut_clock();
	printf("CPU time for test %lu microseconds\n", tm - oldtm);
	printf("TESTS COMPLETED SUCCESSFULLY!\n");
} 
