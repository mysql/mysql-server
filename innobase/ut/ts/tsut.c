/************************************************************************
The test module for the utilities

(c) 1995 Innobase Oy

Created 10/28/1995 Heikki Tuuri
*************************************************************************/

#include "../ut0lst.h"
#include "../ut0mem.h"
#include "../ut0byte.h"
#include "../ut0sort.h"
#include "../ut0rnd.h"

typedef	struct node_struct	node_t;
struct node_struct {
	ulint			index;
	ulint			zyx;
	UT_LIST_NODE_T(node_t)	list1;
	ulint			yzx;
	UT_LIST_NODE_T(node_t)	list2;
};

/* Arrays to be sorted */
ulint	uarr[100000];
ulint	aux_uarr[100000];
dulint	duarr[100000];
dulint	aux_duarr[100000];

/*********************************************************************
Tests for two-way lists. */

void
test1(void)
/*=======*/
{
	ulint				i;
	UT_LIST_BASE_NODE_T(node_t)	base1;
	UT_LIST_BASE_NODE_T(node_t)	base2;
	node_t*				node;
	node_t*				node2;
	
	printf("-------------------------------------------\n");
	printf("TEST 1. Test of two-way lists \n");

	UT_LIST_INIT(base1);
	UT_LIST_INIT(base2);
	
	for (i = 0; i < 1000; i++) {
		node = ut_malloc(sizeof(node_t));

		node->index = 999 - i;

		UT_LIST_ADD_FIRST(list1, base1, node);
		UT_LIST_ADD_LAST(list2, base2, node);
	}

	UT_LIST_VALIDATE(list1, node_t, base1);
	UT_LIST_VALIDATE(list2, node_t, base2);

	node = UT_LIST_GET_FIRST(base1);
	
	for (i = 0; i < 1000; i++) {

		ut_a(node);
		ut_a(node->index == i);

		node = UT_LIST_GET_NEXT(list1, node);
	}

	ut_a(node == NULL);

	node = UT_LIST_GET_FIRST(base2);
	
	for (i = 0; i < 1000; i++) {

		ut_a(node);
		ut_a(node->index == 999 - i);

		node = UT_LIST_GET_NEXT(list2, node);
	}

	ut_a(node == NULL);

	UT_LIST_VALIDATE(list1, node_t, base1);
	UT_LIST_VALIDATE(list2, node_t, base2);
	
	node = UT_LIST_GET_FIRST(base1);
	
	for (i = 0; i < 500; i++) {

		ut_a(node);
		ut_a(node->index == i);

		node = UT_LIST_GET_NEXT(list1, node);
	}

	for (i = 0; i < 100; i++) {
		node2 = ut_malloc(sizeof(node_t));

		node2->index = 99 - i;
		
		UT_LIST_INSERT_AFTER(list1, base1, node, node2);
		UT_LIST_VALIDATE(list1, node_t, base1);
	}

	node2 = ut_malloc(sizeof(node_t));
	node2->index = 1000;
	UT_LIST_INSERT_AFTER(list1, base1, UT_LIST_GET_LAST(base1), node2);

	node2 = node;

	for (i = 0; i < 100; i++) {
		node2 = UT_LIST_GET_NEXT(list1, node2);

		ut_a(node2);
		ut_a(node2->index == i);
	}

	UT_LIST_VALIDATE(list1, node_t, base1);

	for (i = 0; i < 600; i++) {

		node2 = UT_LIST_GET_NEXT(list1, node);

		UT_LIST_REMOVE(list1, base1, node2);
		UT_LIST_VALIDATE(list1, node_t, base1);
	}

	node2 = UT_LIST_GET_NEXT(list1, node);

	UT_LIST_VALIDATE(list1, node_t, base1);
	UT_LIST_VALIDATE(list2, node_t, base2);

	ut_a(UT_LIST_GET_LEN(base1) == 501);
		
	ut_a(UT_LIST_GET_LAST(base1) == node);

	for (i = 0; i < 500; i++) {

		node = UT_LIST_GET_PREV(list1, node);
	}

	ut_a(UT_LIST_GET_FIRST(base1) == node);

	for (i = 0; i < 501; i++) {

		node2 = UT_LIST_GET_FIRST(base1);

		UT_LIST_REMOVE(list1, base1, node2);
	}

	UT_LIST_VALIDATE(list1, node_t, base1);
	UT_LIST_VALIDATE(list2, node_t, base2);

	ut_a(UT_LIST_GET_LEN(base1) == 0);
	ut_a(UT_LIST_GET_LEN(base2) == 1000);
}

/*********************************************************************
Tests for dulints. */

void
test2(void)
/*=======*/
{
	dulint				a, b;
	
	printf("-------------------------------------------\n");
	printf("TEST 2. Test of dulints \n");

	a = ut_dulint_create(0xFFFFFFFF, 0xFFFFFFFF);

	b = a;

	ut_a(ut_dulint_cmp(a, b) == 0);

	ut_a(ut_dulint_get_low(b) == 0xFFFFFFFF);
	ut_a(ut_dulint_get_high(b) == 0xFFFFFFFF);

	a = ut_dulint_create(0xFFFFFFFE, 0xFFFFFFFF);
	ut_a(ut_dulint_cmp(a, b) == -1);
	ut_a(ut_dulint_cmp(b, a) == 1);

	a = ut_dulint_create(0xFFFFFFFF, 0xFFFFFFFE);
	ut_a(ut_dulint_cmp(a, b) == -1);
	ut_a(ut_dulint_cmp(b, a) == 1);

	a = ut_dulint_create(5, 0xFFFFFFFF);

	a = ut_dulint_add(a, 5);

	ut_a(ut_dulint_get_low(a) == 4);
	ut_a(ut_dulint_get_high(a) == 6);
	
	a = ut_dulint_create(5, 0x80000000);

	a = ut_dulint_add(a, 0x80000000);

	ut_a(ut_dulint_get_low(a) == 0);
	ut_a(ut_dulint_get_high(a) == 6);

	a = ut_dulint_create(5, 10);

	a = ut_dulint_add(a, 20);

	ut_a(ut_dulint_get_low(a) == 30);
	ut_a(ut_dulint_get_high(a) == 5);
}

/***************************************************************
Comparison function for ulints. */
UNIV_INLINE
int
cmp_ulint(ulint a, ulint b)
/*=======================*/
{
	if (a < b) {
		return(-1);
	} else if (b < a) {
		return(1);
	} else {
		return(0);
	}
}

/****************************************************************
Sort function for ulint arrays. */
void
sort_ulint(ulint* arr, ulint* aux_arr, ulint low, ulint high)
/*=========================================================*/
{
	ut_ad(high <= 100000);
	
	UT_SORT_FUNCTION_BODY(sort_ulint, arr, aux_arr, low, high,
				cmp_ulint);
}

/****************************************************************
Sort function for dulint arrays. */
void
sort_dulint(dulint* arr, dulint* aux_arr, ulint low, ulint high)
/*=========================================================*/
{
	ut_ad(high <= 100000);
	
	UT_SORT_FUNCTION_BODY(sort_dulint, arr, aux_arr, low, high,
				ut_dulint_cmp);
}

/*********************************************************************
Tests for sorting. */

void
test3(void)
/*=======*/
{
	ulint	i, j;
	ulint	tm, oldtm;
	
	printf("-------------------------------------------\n");
	printf("TEST 3. Test of sorting \n");

	for (i = 0; i < 100000; i++) {
		uarr[i] = ut_rnd_gen_ulint();
	}

	oldtm = ut_clock();

    for (j = 0; j < 1; j++) {
	i = 100000;
	
	sort_ulint(uarr, aux_uarr, 0, i);
    }

	tm = ut_clock();

	printf("Wall clock time for sort of %lu ulints %lu millisecs\n",
			j * i, tm - oldtm);
	
	for (i = 1; i < 100000; i++) {
		ut_a(uarr[i - 1] < uarr[i]);
	}

	for (i = 0; i < 100000; i++) {
		uarr[i] = 99999 - i;
	}

	sort_ulint(uarr, aux_uarr, 0, 100000);

	for (i = 1; i < 100000; i++) {
		ut_a(uarr[i] == i);
	}

	sort_ulint(uarr, aux_uarr, 0, 100000);

	for (i = 1; i < 100000; i++) {
		ut_a(uarr[i] == i);
	}

	sort_ulint(uarr, aux_uarr, 5, 6);

	for (i = 1; i < 100000; i++) {
		ut_a(uarr[i] == i);
	}

	for (i = 0; i < 100000; i++) {
		uarr[i] = 5;
	}

	sort_ulint(uarr, aux_uarr, 0, 100000);

	for (i = 1; i < 100000; i++) {
		ut_a(uarr[i] == 5);
	}

	for (i = 0; i < 100000; i++) {
		duarr[i] = ut_dulint_create(ut_rnd_gen_ulint() & 0xFFFFFFFF,
					    ut_rnd_gen_ulint() & 0xFFFFFFFF);
	}

	oldtm = ut_clock();

	i = 100000;
	
	sort_dulint(duarr, aux_duarr, 0, i);

	tm = ut_clock();

	printf("Wall clock time for sort of %lu dulints %lu millisecs\n",
			j * i, tm - oldtm);
	
	for (i = 1; i < 100000; i++) {
		ut_a(ut_dulint_cmp(duarr[i - 1], duarr[i]) < 0);
	}

}

void 
main(void) 
{
	test1();
	test2();
	test3();

	printf("TEST SUCCESSFULLY COMPLETED!\n");
} 
