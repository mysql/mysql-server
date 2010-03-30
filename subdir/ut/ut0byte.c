/*******************************************************************
Byte utilities

(c) 1994, 1995 Innobase Oy

Created 5/11/1994 Heikki Tuuri
********************************************************************/

#include "ut0byte.h"

#ifdef UNIV_NONINL
#include "ut0byte.ic"
#endif

#include "ut0sort.h"

/* Zero value for a dulint */
dulint	ut_dulint_zero		= {0, 0};

/* Maximum value for a dulint */
dulint	ut_dulint_max		= {0xFFFFFFFFUL, 0xFFFFFFFFUL};

/****************************************************************
Sort function for dulint arrays. */
void
ut_dulint_sort(dulint* arr, dulint* aux_arr, ulint low, ulint high)
/*===============================================================*/
{
	UT_SORT_FUNCTION_BODY(ut_dulint_sort, arr, aux_arr, low, high,
			      ut_dulint_cmp);
}
