/******************************************************
Data types

(c) 1996 Innobase Oy

Created 1/16/1996 Heikki Tuuri
*******************************************************/

#include "data0type.h"

#ifdef UNIV_NONINL
#include "data0type.ic"
#endif

dtype_t		dtype_binary_val = {DATA_BINARY, 0, 0, 0};
dtype_t* 	dtype_binary 	= &dtype_binary_val;

/*************************************************************************
Validates a data type structure. */

ibool
dtype_validate(
/*===========*/
				/* out: TRUE if ok */
	dtype_t*	type)	/* in: type struct to validate */
{
	ut_a(type);
	ut_a((type->mtype >= DATA_VARCHAR) && (type->mtype <= DATA_MYSQL));
	
	if (type->mtype == DATA_SYS) {
		ut_a(type->prtype >= DATA_ROW_ID);
		ut_a(type->prtype <= DATA_MIX_ID);
	}

	return(TRUE);
}

/*************************************************************************
Prints a data type structure. */

void
dtype_print(
/*========*/
	dtype_t*	type)	/* in: type */
{
	ulint	mtype;
	ulint	prtype;

	ut_a(type);

	printf("DATA TYPE: ");
	
	mtype = type->mtype;
	prtype = type->prtype;
	if (mtype == DATA_VARCHAR) {
		printf("DATA_VARCHAR");
	} else if (mtype == DATA_CHAR) {
		printf("DATA_CHAR");
	} else if (mtype == DATA_BINARY) {
		printf("DATA_BINARY");
	} else if (mtype == DATA_INT) {
		printf("DATA_INT");
	} else if (mtype == DATA_MYSQL) {
		printf("DATA_MYSQL");
	} else if (mtype == DATA_SYS) {
		printf("DATA_SYS");
	} else {
		printf("unknown type %lu", mtype);
	}
	
	if ((type->mtype == DATA_SYS)
	   || (type->mtype == DATA_VARCHAR)
	   || (type->mtype == DATA_CHAR)) {
		printf(" ");
		if (prtype == DATA_ROW_ID) {
			printf("DATA_ROW_ID");
		} else if (prtype == DATA_ROLL_PTR) {
			printf("DATA_ROLL_PTR");
		} else if (prtype == DATA_MIX_ID) {
			printf("DATA_MIX_ID");
		} else if (prtype == DATA_ENGLISH) {
			printf("DATA_ENGLISH");
		} else if (prtype == DATA_FINNISH) {
			printf("DATA_FINNISH");
		} else {
			printf("unknown prtype %lu", mtype);
		}
	}

	printf("; len %lu prec %lu\n", type->len, type->prec);
}
