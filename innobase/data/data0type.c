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

#ifdef UNIV_DEBUG
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
		ut_a(type->prtype <= DATA_MIX_ID);
	}

	return(TRUE);
}
#endif /* UNIV_DEBUG */

/*************************************************************************
Prints a data type structure. */

void
dtype_print(
/*========*/
	dtype_t*	type)	/* in: type */
{
	ulint	mtype;
	ulint	prtype;
	ulint	len;
	
	ut_a(type);

	mtype = type->mtype;
	prtype = type->prtype;
	if (mtype == DATA_VARCHAR) {
		fputs("DATA_VARCHAR", stderr);
	} else if (mtype == DATA_CHAR) {
		fputs("DATA_CHAR", stderr);
	} else if (mtype == DATA_BINARY) {
		fputs("DATA_BINARY", stderr);
	} else if (mtype == DATA_INT) {
		fputs("DATA_INT", stderr);
	} else if (mtype == DATA_MYSQL) {
		fputs("DATA_MYSQL", stderr);
	} else if (mtype == DATA_SYS) {
		fputs("DATA_SYS", stderr);
	} else {
		fprintf(stderr, "type %lu", mtype);
	}

	len = type->len;
	
	if ((type->mtype == DATA_SYS)
	   || (type->mtype == DATA_VARCHAR)
	   || (type->mtype == DATA_CHAR)) {
		putc(' ', stderr);
		if (prtype == DATA_ROW_ID) {
			fputs("DATA_ROW_ID", stderr);
			len = DATA_ROW_ID_LEN;
		} else if (prtype == DATA_ROLL_PTR) {
			fputs("DATA_ROLL_PTR", stderr);
			len = DATA_ROLL_PTR_LEN;
		} else if (prtype == DATA_TRX_ID) {
			fputs("DATA_TRX_ID", stderr);
			len = DATA_TRX_ID_LEN;
		} else if (prtype == DATA_MIX_ID) {
			fputs("DATA_MIX_ID", stderr);
		} else if (prtype == DATA_ENGLISH) {
			fputs("DATA_ENGLISH", stderr);
		} else {
			fprintf(stderr, "prtype %lu", mtype);
		}
	}

	fprintf(stderr, " len %lu prec %lu", len, type->prec);
}
