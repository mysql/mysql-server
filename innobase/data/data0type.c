/******************************************************
Data types

(c) 1996 Innobase Oy

Created 1/16/1996 Heikki Tuuri
*******************************************************/

#include "data0type.h"

#ifdef UNIV_NONINL
#include "data0type.ic"
#endif

/**********************************************************************
This function is used to find the storage length in bytes of the first n
characters for prefix indexes using a multibyte character set. The function
finds charset information and returns length of prefix_len characters in the
index field in bytes.

NOTE: the prototype of this function is copied from ha_innodb.cc! If you change
this function, you MUST change also the prototype here! */

ulint
innobase_get_at_most_n_mbchars(
/*===========================*/
				/* out: number of bytes occupied by the first
				n characters */
	ulint charset_id,	/* in: character set id */
	ulint prefix_len,	/* in: prefix length in bytes of the index
				(this has to be divided by mbmaxlen to get the
				number of CHARACTERS n in the prefix) */
	ulint data_len,         /* in: length of the string in bytes */
	const char* str);	/* in: character string */

/* At the database startup we store the default-charset collation number of
this MySQL installation to this global variable. If we have < 4.1.2 format
column definitions, or records in the insert buffer, we use this
charset-collation code for them. */

ulint	data_mysql_default_charset_coll		= 99999999;
ulint	data_mysql_latin1_swedish_charset_coll	= 99999999;

dtype_t		dtype_binary_val = {DATA_BINARY, 0, 0, 0};
dtype_t* 	dtype_binary 	= &dtype_binary_val;

/*************************************************************************
Checks if a string type has to be compared by the MySQL comparison functions.
InnoDB internally only handles binary byte string comparisons, as well as
latin1_swedish_ci strings. For example, UTF-8 strings have to be compared
by MySQL. */

ibool
dtype_str_needs_mysql_cmp(
/*======================*/
				/* out: TRUE if a string type that requires
				comparison with MySQL functions */
	dtype_t*	dtype)	/* in: type struct */
{
	if (dtype->mtype == DATA_MYSQL
	    || dtype->mtype == DATA_VARMYSQL
	    || (dtype->mtype == DATA_BLOB
	        && 0 == (dtype->prtype & DATA_BINARY_TYPE)
		&& dtype_get_charset_coll(dtype->prtype) !=
				data_mysql_latin1_swedish_charset_coll)) {
		return(TRUE);
	}

	return(FALSE);
}

/*************************************************************************
For the documentation of this function, see innobase_get_at_most_n_mbchars()
in ha_innodb.cc. */

ulint
dtype_get_at_most_n_mbchars(
/*========================*/
	dtype_t*	dtype,
	ulint		prefix_len,
	ulint		data_len,
	const char*	str)
{
	ut_a(data_len != UNIV_SQL_NULL);

	if (dtype_str_needs_mysql_cmp(dtype)) {
		return(innobase_get_at_most_n_mbchars(
				dtype_get_charset_coll(dtype->prtype),
				prefix_len, data_len, str));
	}

	/* We assume here that the string types that InnoDB itself can compare
	are single-byte charsets! */

	if (prefix_len < data_len) {

		return(prefix_len);

	}

	return(data_len);
}

/*************************************************************************
Checks if a data main type is a string type. Also a BLOB is considered a
string type. */

ibool
dtype_is_string_type(
/*=================*/
			/* out: TRUE if string type */
	ulint	mtype)	/* in: InnoDB main data type code: DATA_CHAR, ... */
{
 	if (mtype <= DATA_BLOB
	    || mtype == DATA_MYSQL
	    || mtype == DATA_VARMYSQL) {

		return(TRUE);
	}

	return(FALSE);
}

/*************************************************************************
Checks if a type is a binary string type. Note that for tables created with
< 4.0.14, we do not know if a DATA_BLOB column is a BLOB or a TEXT column. For
those DATA_BLOB columns this function currently returns FALSE. */

ibool
dtype_is_binary_string_type(
/*========================*/
			/* out: TRUE if binary string type */
	ulint	mtype,	/* in: main data type */
	ulint	prtype)	/* in: precise type */
{
        if ((mtype == DATA_FIXBINARY)
	    || (mtype == DATA_BINARY)
	    || (mtype == DATA_BLOB && (prtype & DATA_BINARY_TYPE))) {

		return(TRUE);
	}

	return(FALSE);
}

/*************************************************************************
Checks if a type is a non-binary string type. That is, dtype_is_string_type is
TRUE and dtype_is_binary_string_type is FALSE. Note that for tables created
with < 4.0.14, we do not know if a DATA_BLOB column is a BLOB or a TEXT column.
For those DATA_BLOB columns this function currently returns TRUE. */

ibool
dtype_is_non_binary_string_type(
/*============================*/
			/* out: TRUE if non-binary string type */
	ulint	mtype,	/* in: main data type */
	ulint	prtype)	/* in: precise type */
{
	if (dtype_is_string_type(mtype) == TRUE
	    && dtype_is_binary_string_type(mtype, prtype) == FALSE) {
		
		return(TRUE);
	}

	return(FALSE);
}

/*************************************************************************
Forms a precise type from the < 4.1.2 format precise type plus the
charset-collation code. */

ulint
dtype_form_prtype(
/*==============*/
	ulint	old_prtype,	/* in: the MySQL type code and the flags
				DATA_BINARY_TYPE etc. */
	ulint	charset_coll)	/* in: MySQL charset-collation code */
{
	ut_a(old_prtype < 256 * 256);
	ut_a(charset_coll < 256);

	return(old_prtype + (charset_coll << 16));
}

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
		ut_a((type->prtype & DATA_MYSQL_TYPE_MASK) < DATA_N_SYS_COLS);
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
		fprintf(stderr, "type %lu", (ulong) mtype);
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
			fprintf(stderr, "prtype %lu", (ulong) mtype);
		}
	}

	fprintf(stderr, " len %lu prec %lu", (ulong) len, (ulong) type->prec);
}
