/************************************************************************
The test module for the machine-dependent utilities

(c) 1995 Innobase Oy

Created 11/28/1995 Heikki Tuuri
*************************************************************************/

#include "../mach0data.h"

byte	arr[4000000];


/*********************************************************************
Test for ulint write and read. */

void
test1(void)
/*=======*/
{
	ulint	a, i, j;
	ulint	tm, oldtm;
	
	printf("-------------------------------------------\n");
	printf("TEST 1. Speed test of ulint read and write \n");

	a = 0;
	
	oldtm = ut_clock();

	for (j = 0; j < 100; j++) {
	for (i = 0; i < 10000; i++) {

		a += mach_read_from_4(arr + i * 4);
	}
	} 
	
	tm = ut_clock();

	printf("Wall clock time for read of %lu ulints %lu millisecs\n",
			j * i, tm - oldtm);

	oldtm = ut_clock();

	for (j = 0; j < 100; j++) {
	for (i = 0; i < 10000; i++) {

		a += mach_read(arr + i * 4);
	}
	} 
	
	tm = ut_clock();

	printf("Wall clock time for read of %lu ulints %lu millisecs\n",
			j * i, tm - oldtm);
	
	oldtm = ut_clock();

	for (j = 0; j < 100; j++) {
	for (i = 0; i < 10000; i++) {

		a += mach_read_from_4(arr + i * 4 + 1);
	}
	} 
	
	tm = ut_clock();

	printf("Wall clock time for read of %lu ulints %lu millisecs\n",
			j * i, tm - oldtm);
	oldtm = ut_clock();

	for (j = 0; j < 100; j++) {
	for (i = 0; i < 10000; i++) {

		a += mach_read(arr + i * 4 + 1);
	}
	} 
	
	tm = ut_clock();

	printf("Wall clock time for read of %lu ulints %lu millisecs\n",
			j * i, tm - oldtm);
	
	oldtm = ut_clock();

	for (i = 0; i < 1000000; i++) {

		a += mach_read_from_4(arr + i * 4);
	}
	
	tm = ut_clock();

	printf("Wall clock time for read of %lu ulints %lu millisecs\n",
			i, tm - oldtm);
	oldtm = ut_clock();

	for (i = 0; i < 1000000; i++) {

		a += mach_read(arr + i * 4);
	}
	
	tm = ut_clock();

	printf("Wall clock time for read of %lu ulints %lu millisecs\n",
			i, tm - oldtm);
	
	oldtm = ut_clock();

	for (j = 0; j < 100; j++) {
	for (i = 0; i < 10000; i++) {

		a += mach_read_from_2(arr + i * 2);
	}
	} 
	
	tm = ut_clock();

	printf("Wall clock time for read of %lu 16-bit ints %lu millisecs\n",
			j * i, tm - oldtm);
}

/*********************************************************************
Test for ulint write and read. */

void
test2(void)
/*=======*/
{
	ulint	a[2];
	
	printf("-------------------------------------------\n");
	printf("TEST 2. Correctness test of ulint read and write \n");

	mach_write_to_4((byte*)&a, 737237727);
	
	ut_a(737237727 == mach_read_from_4((byte*)&a));
	
	mach_write_to_2((byte*)&a, 7372);
	
	ut_a(7372 == mach_read_from_2((byte*)&a));
	
	mach_write_to_1((byte*)&a, 27);
	
	ut_a(27 == mach_read_from_1((byte*)&a));
	
	mach_write((byte*)&a, 737237727);
	
	ut_a(737237727 == mach_read((byte*)&a));
}	

void 
main(void) 
{
	test1();
	test2();

	printf("TEST SUCCESSFULLY COMPLETED!\n");
} 
