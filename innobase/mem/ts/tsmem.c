/************************************************************************
The test module for the memory management of Innobase

(c) 1994, 1995 Innobase Oy

Created 6/10/1994 Heikki Tuuri
*************************************************************************/

#include "../mem0mem.h"
#include "sync0sync.h"
#include "ut0rnd.h"	

mem_heap_t*	heap_arr[1200];

byte*		buf_arr[10000];
ulint		rnd_arr[10000];


#ifdef UNIV_DEBUG
/*********************************************************************
Debug version test. */

void
test1(void)
/*=======*/
{
	mem_heap_t*	heap_1, *heap_2;
	byte*		buf_1, *buf_2, *buf_3;
	byte		check;
	bool		error;	
	ulint		i;
	ulint		j;
	ulint		sum;
	ulint		user_size;
	ulint		phys_size, phys_size_1, phys_size_2;
	ulint		n_blocks;
	ulint		p;
	byte		block[1024];
	byte*		top_1, *top_2;
	
	/* For this test to work the memory alignment must be
	even (presumably a reasonable assumption) */
	ut_a(0 == (UNIV_MEM_ALIGNMENT & 1));

	printf("-------------------------------------------\n");
	printf("TEST 1. Basic test \n");

	heap_1 = mem_heap_create(0);
	
	buf_1 = mem_heap_alloc(heap_1, 11);

	heap_2 = mem_heap_create(0);
	
	buf_2 = mem_heap_alloc(heap_1, 15);
	
	/* Test that the field is properly initialized */
	for (i = 0; i < 11; i++) {
		ut_a((*(buf_1 + i) == 0xBA) || (*(buf_1 + i) == 0xBE));
	}
	
	check = *(buf_1 + 11);

	mem_validate();

	/* Make an advertent error in the heap */
	(*(buf_1 + 11))++;
	
	error = mem_validate_no_assert();

	ut_a(error);
	
	/* Fix the error in heap before freeing */
	*(buf_1 + 11) = check;

	mem_print_info();

	/* Free the top buffer buf_2 */
	mem_heap_free_top(heap_1, 15);

	/* Test that the field is properly erased */
	for (i = 0; i < 15; i++) {
		ut_a((*(buf_2 + i) == 0xDE) || (*(buf_2 + i) == 0xAD));
	}
	
	/* Test that a new buffer is allocated from the same position
	as buf_2 */
	buf_3 = mem_heap_alloc(heap_1, 15);
	
	ut_a(buf_3 == buf_2);

	mem_heap_free(heap_1);

	/* Test that the field is properly erased */
	for (i = 0; i < 11; i++) {
		ut_a((*(buf_1 + i) == 0xDE) || (*(buf_1 + i) == 0xAD));
	}

	mem_validate();
		
	mem_print_info();

	printf("-------------------------------------------\n");
	printf("TEST 2. Test of massive allocation and freeing\n");

	sum = 0;
	for (i = 0; i < 10000; i++) {
		
		j = ut_rnd_gen_ulint() % 16 + 15;

		sum = sum + j;
		
		buf_1 = mem_heap_alloc(heap_2, j);
		rnd_arr[i] = j;	
	
		buf_arr[i] = buf_1;

		ut_a(buf_1 == mem_heap_get_top(heap_2, j));
	}

	mem_heap_validate_or_print(heap_2, NULL, FALSE, &error, &user_size,
			 &phys_size_1,
			&n_blocks);
	
	ut_a(!error);
	ut_a(user_size == sum);
	
	(*(buf_1 - 1))++;
	
	ut_a(mem_validate_no_assert());
	
	(*(buf_1 - 1))--;

	mem_print_info();

	
	for (p = 10000; p > 0 ; p--) {
		
		j = rnd_arr[p - 1];

		ut_a(buf_arr[p - 1] == mem_heap_get_top(heap_2, j));
		mem_heap_free_top(heap_2, j);
	}
	
	mem_print_info();
	
	mem_heap_free(heap_2);
	
	mem_print_info();

	printf("-------------------------------------------\n");
	printf("TEST 3. More tests on the validating \n");
	
	heap_1 = mem_heap_create(UNIV_MEM_ALIGNMENT * 20);
	
	buf_1 = mem_heap_alloc(heap_1, UNIV_MEM_ALIGNMENT * 20);
	
	mem_heap_validate_or_print(heap_1, NULL, FALSE, &error, &user_size,
			 &phys_size_1,
			&n_blocks);

	ut_a((ulint)(buf_1 - (byte*)heap_1) == (MEM_BLOCK_HEADER_SIZE
			  	 	+ MEM_FIELD_HEADER_SIZE));

	mem_validate();
	
	mem_print_info();

	ut_a(user_size == UNIV_MEM_ALIGNMENT * 20);
	ut_a(phys_size_1 == (ulint)(ut_calc_align(MEM_FIELD_HEADER_SIZE
					+ UNIV_MEM_ALIGNMENT * 20
					+ MEM_FIELD_TRAILER_SIZE,
					UNIV_MEM_ALIGNMENT)
			  + MEM_BLOCK_HEADER_SIZE));
			  
	ut_a(n_blocks == 1);
			
	buf_2 = mem_heap_alloc(heap_1, UNIV_MEM_ALIGNMENT * 3 - 1);
	
	mem_heap_validate_or_print(heap_1, NULL, FALSE, &error, 
			&user_size, &phys_size_2,
			&n_blocks);
	
	printf("Physical size of the heap %ld\n", phys_size_2);

	ut_a(!error);
	ut_a(user_size == UNIV_MEM_ALIGNMENT * 23 - 1);
	ut_a(phys_size_2 == (ulint) (phys_size_1
			  + ut_calc_align(MEM_FIELD_HEADER_SIZE
			  		+ phys_size_1 * 2
					+ MEM_FIELD_TRAILER_SIZE,
					UNIV_MEM_ALIGNMENT)
			  + MEM_BLOCK_HEADER_SIZE));

	ut_a(n_blocks == 2);

	buf_3 = mem_heap_alloc(heap_1, UNIV_MEM_ALIGNMENT * 3 + 5);
	
	ut_a((ulint)(buf_3 - buf_2) == ut_calc_align(
					(UNIV_MEM_ALIGNMENT * 3
			    		  + MEM_FIELD_TRAILER_SIZE),
			    		  UNIV_MEM_ALIGNMENT)
			  + MEM_FIELD_HEADER_SIZE);
			    		  
	
	ut_memcpy(buf_3, buf_2, UNIV_MEM_ALIGNMENT * 3);

	mem_heap_validate_or_print(heap_1, NULL, FALSE, &error, 
			&user_size, &phys_size,
			&n_blocks);
			
	ut_a(!error);
	ut_a(user_size == UNIV_MEM_ALIGNMENT * 26 + 4);
	ut_a(phys_size == phys_size_2);
	ut_a(n_blocks == 2);
	

	/* Make an advertent error to buf_3 */
	
	(*(buf_3 - 1))++;
		
	mem_heap_validate_or_print(heap_1, NULL, FALSE, &error, 
			&user_size, &phys_size,
			&n_blocks);
			
	ut_a(error);
	ut_a(user_size == 0);
	ut_a(phys_size == 0);
	ut_a(n_blocks == 0);
	
	/* Fix the error and make another */		

	(*(buf_3 - 1))--;
	(*(buf_3 + UNIV_MEM_ALIGNMENT * 3 + 5))++;

	mem_heap_validate_or_print(heap_1, NULL, FALSE, &error, 
			&user_size, &phys_size,
			&n_blocks);
			
	ut_a(error);

	(*(buf_3 + UNIV_MEM_ALIGNMENT * 3 + 5))--;
	
	buf_1 = mem_heap_alloc(heap_1, UNIV_MEM_ALIGNMENT + 4);
	
	ut_a((ulint)(buf_1 - buf_3) == ut_calc_align(UNIV_MEM_ALIGNMENT * 3 + 5
			    		  + MEM_FIELD_TRAILER_SIZE ,
			    		  UNIV_MEM_ALIGNMENT)
			  + MEM_FIELD_HEADER_SIZE);
			    		  
	
	mem_heap_validate_or_print(heap_1, NULL, FALSE, &error, 
			&user_size, &phys_size,
			&n_blocks);
			
	ut_a(!error);
	ut_a(user_size == UNIV_MEM_ALIGNMENT * 27 + 8);
	ut_a(phys_size == phys_size_2);
	ut_a(n_blocks == 2);


	mem_print_info();
	
	mem_heap_free(heap_1);
	
	printf("-------------------------------------------\n");
	printf("TEST 4. Test of massive allocation \n");
	printf("of heaps to test the hash table\n");

	for (i = 0; i < 500; i++) {
		heap_arr[i] = mem_heap_create(i);
		buf_2 = mem_heap_alloc(heap_arr[i], 2 * i);
	}
	
	mem_validate();
	
	for (i = 0; i < 500; i++) {
		mem_heap_free(heap_arr[i]);
	}

	mem_validate();

	mem_print_info();
	
	/* Validating a freed heap should generate an error */
	
	mem_heap_validate_or_print(heap_1, NULL, FALSE, &error, 
				  NULL, NULL, NULL);

	ut_a(error);

	printf("-------------------------------------------\n");
	printf("TEST 5. Test of mem_alloc and mem_free \n");

	buf_1 = mem_alloc(11100);
	buf_2 = mem_alloc(23);
		
	ut_memcpy(buf_2, buf_1, 23);	

	mem_validate();

	mem_print_info();

	mem_free(buf_1);
	mem_free(buf_2);

	mem_validate();

	printf("-------------------------------------------\n");
	printf("TEST 6. Test of mem_heap_print \n");
	
	heap_1 = mem_heap_create(0);
	
	buf_1 = mem_heap_alloc(heap_1, 7);
	
	ut_memcpy(buf_1, "Pascal", 7);
	
	for (i = 0; i < 10; i++) {
		buf_1 = mem_heap_alloc(heap_1, 6);
		ut_memcpy(buf_1, "Cobol", 6);
	}

	printf("A heap with 1 Pascal and 10 Cobol's\n");
	mem_heap_print(heap_1);

	for (i = 0; i < 10; i++) {
		mem_heap_free_top(heap_1, 6);
	}
	
	printf("A heap with 1 Pascal and 0 Cobol's\n");
	mem_heap_print(heap_1);

	ut_a(mem_all_freed() == FALSE);

	mem_heap_free(heap_1);
	
	ut_a(mem_all_freed() == TRUE);

	mem_print_info();

	printf("-------------------------------------------\n");
	printf("TEST 7. Test of mem_heap_fast_create \n");
	
	heap_1 = mem_heap_fast_create(1024, block);
	
	buf_1 = mem_heap_alloc(heap_1, 7);
	
	ut_memcpy(buf_1, "Pascal", 7);
	
	for (i = 0; i < 1000; i++) {
		buf_1 = mem_heap_alloc(heap_1, 6);
		ut_memcpy(buf_1, "Cobol", 6);
	}

	for (i = 0; i < 1000; i++) {
		mem_heap_free_top(heap_1, 6);
	}

	ut_a(mem_all_freed() == FALSE);

	mem_heap_free(heap_1);
	
	ut_a(mem_all_freed() == TRUE);

	mem_print_info();

	printf("-------------------------------------------\n");
	printf("TEST 8. Test of heap top freeing \n");
	
	heap_1 = mem_heap_fast_create(1024, block);

	top_1 = mem_heap_get_heap_top(heap_1);
	
	buf_1 = mem_heap_alloc(heap_1, 7);
	
	ut_memcpy(buf_1, "Pascal", 7);
	
	for (i = 0; i < 500; i++) {
		buf_1 = mem_heap_alloc(heap_1, 6);
		ut_memcpy(buf_1, "Cobol", 6);
	}

	top_2 = mem_heap_get_heap_top(heap_1);

	for (i = 0; i < 500; i++) {
		buf_1 = mem_heap_alloc(heap_1, 6);
		ut_memcpy(buf_1, "Cobol", 6);
	}

	mem_heap_free_heap_top(heap_1, top_2);	

	mem_heap_free_heap_top(heap_1, top_1);	

	ut_a(mem_all_freed() == FALSE);

	for (i = 0; i < 500; i++) {
		buf_1 = mem_heap_alloc(heap_1, 6);
		ut_memcpy(buf_1, "Cobol", 6);

	}

	mem_heap_empty(heap_1);

	for (i = 0; i < 500; i++) {
		buf_1 = mem_heap_alloc(heap_1, 6);
		ut_memcpy(buf_1, "Cobol", 6);

	}
	
	mem_heap_free(heap_1);
	
	ut_a(mem_all_freed() == TRUE);

	mem_print_info();

}
#endif /* UNIV_DEBUG */

/****************************************************************
Allocation speed test. */

void
test2(void)
/*=======*/
{
	mem_heap_t*	heap;
	ulint		tm, oldtm;
	ulint		i;
	byte*		buf;
	byte		block[512];
	
	printf("-------------------------------------------\n");
	printf("TEST B1. Test of speed \n");
	
	oldtm = ut_clock();
	
	for (i = 0; i < 10000 * UNIV_DBC * UNIV_DBC; i++) {
		heap = mem_heap_create(500);
		mem_heap_free(heap);
	}
	
	tm = ut_clock();

	printf("Time for %ld heap create-free pairs %ld millisecs.\n",
		i, tm - oldtm);

	
	oldtm = ut_clock();
	
	for (i = 0; i < 10000 * UNIV_DBC * UNIV_DBC; i++) {
		heap = mem_heap_fast_create(512, block);
		mem_heap_free(heap);
	}
	
	tm = ut_clock();

	printf("Time for %ld heap fast-create-free pairs %ld millisecs.\n",
		i, tm - oldtm);


	heap = mem_heap_create(500);

	oldtm = ut_clock();
	
	for (i = 0; i < 10000 * UNIV_DBC * UNIV_DBC; i++) {
		buf = mem_heap_alloc(heap, 50);
		mem_heap_free_top(heap, 50);
	}
	
	tm = ut_clock();

	printf("Time for %ld heap alloc-free-top pairs %ld millisecs.\n",
		i, tm - oldtm);

	mem_heap_free(heap);
}


void 
main(void) 
{
	sync_init();
	mem_init(2500000);

	#ifdef UNIV_DEBUG

	test1();

	#endif

	test2();
	
	ut_ad(sync_all_freed());

	ut_ad(mem_all_freed());
	
	printf("TEST SUCCESSFULLY COMPLETED!\n");
} 
