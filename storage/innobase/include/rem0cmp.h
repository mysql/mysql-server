/*****************************************************************************

Copyright (c) 1994, 2012, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/*******************************************************************//**
@file include/rem0cmp.h
Comparison services for records

Created 7/1/1994 Heikki Tuuri
************************************************************************/

#ifndef rem0cmp_h
#define rem0cmp_h

#include "univ.i"
#include "data0data.h"
#include "data0type.h"
#include "dict0dict.h"
#include "rem0rec.h"

/*************************************************************//**
Returns TRUE if two columns are equal for comparison purposes.
@return	TRUE if the columns are considered equal in comparisons */
UNIV_INTERN
ibool
cmp_cols_are_equal(
/*===============*/
	const dict_col_t*	col1,	/*!< in: column 1 */
	const dict_col_t*	col2,	/*!< in: column 2 */
	ibool			check_charsets);
					/*!< in: whether to check charsets */
/*************************************************************//**
This function is used to compare two data fields for which we know the
data type.
@return	1, 0, -1, if data1 is greater, equal, less than data2, respectively */
UNIV_INLINE
int
cmp_data_data(
/*==========*/
	ulint		mtype,	/*!< in: main type */
	ulint		prtype,	/*!< in: precise type */
	const byte*	data1,	/*!< in: data field (== a pointer to a memory
				buffer) */
	ulint		len1,	/*!< in: data field length or UNIV_SQL_NULL */
	const byte*	data2,	/*!< in: data field (== a pointer to a memory
				buffer) */
	ulint		len2);	/*!< in: data field length or UNIV_SQL_NULL */
/*************************************************************//**
This function is used to compare two data fields for which we know the
data type.
@return	1, 0, -1, if data1 is greater, equal, less than data2, respectively */
UNIV_INTERN
int
cmp_data_data_slow(
/*===============*/
	ulint		mtype,	/*!< in: main type */
	ulint		prtype,	/*!< in: precise type */
	const byte*	data1,	/*!< in: data field (== a pointer to a memory
				buffer) */
	ulint		len1,	/*!< in: data field length or UNIV_SQL_NULL */
	const byte*	data2,	/*!< in: data field (== a pointer to a memory
				buffer) */
	ulint		len2);	/*!< in: data field length or UNIV_SQL_NULL */

/*****************************************************************
This function is used to compare two data fields for which we know the
data type to be VARCHAR.
@return	1, 0, -1, if lhs is greater, equal, less than rhs, respectively */
UNIV_INTERN
int
cmp_data_data_slow_varchar(
/*=======================*/
	const byte*	lhs,	/* in: data field (== a pointer to a memory
				buffer) */
	ulint		lhs_len,/* in: data field length or UNIV_SQL_NULL */
	const byte*	rhs,	/* in: data field (== a pointer to a memory
				buffer) */
	ulint		rhs_len);/* in: data field length or UNIV_SQL_NULL */
/*****************************************************************
This function is used to compare two varchar/char fields. The comparison
is for the LIKE operator.
@return	1, 0, -1, if lhs is greater, equal, less than rhs, respectively */
UNIV_INTERN
int
cmp_data_data_slow_like_prefix(
/*===========================*/
	const byte*	data1,	/* in: data field (== a pointer to a memory
				buffer) */
	ulint		len1,	/* in: data field length or UNIV_SQL_NULL */
	const byte*	data2,	/* in: data field (== a pointer to a memory
				buffer) */
	ulint		len2);	/* in: data field length or UNIV_SQL_NULL */
/*****************************************************************
This function is used to compare two varchar/char fields. The comparison
is for the LIKE operator.
@return	1, 0, -1, if data1 is greater, equal, less than data2, respectively */
UNIV_INTERN
int
cmp_data_data_slow_like_suffix(
/*===========================*/
	const byte*	data1,	/* in: data field (== a pointer to a memory
				buffer) */
	ulint		len1,	/* in: data field length or UNIV_SQL_NULL */
	const byte*	data2,	/* in: data field (== a pointer to a memory
				buffer) */
	ulint		len2);	/* in: data field length or UNIV_SQL_NULL */
/*****************************************************************
This function is used to compare two varchar/char fields. The comparison
is for the LIKE operator.
@return	1, 0, -1, if data1 is greater, equal, less than data2, respectively */
UNIV_INTERN
int
cmp_data_data_slow_like_substr(
/*===========================*/
	const byte*	data1,	/* in: data field (== a pointer to a memory
				buffer) */
	ulint		len1,	/* in: data field length or UNIV_SQL_NULL */
	const byte*	data2,	/* in: data field (== a pointer to a memory
				buffer) */
	ulint		len2);	/* in: data field length or UNIV_SQL_NULL */
/*************************************************************//**
This function is used to compare two dfields where at least the first
has its data type field set.
@return 1, 0, -1, if dfield1 is greater, equal, less than dfield2,
respectively */
UNIV_INLINE
int
cmp_dfield_dfield(
/*==============*/
	const dfield_t*	dfield1,/*!< in: data field; must have type field set */
	const dfield_t*	dfield2);/*!< in: data field */
/*************************************************************//**
This function is used to compare a data tuple to a physical record.
Only dtuple->n_fields_cmp first fields are taken into account for
the data tuple! If we denote by n = n_fields_cmp, then rec must
have either m >= n fields, or it must differ from dtuple in some of
the m fields rec has. If rec has an externally stored field we do not
compare it but return with value 0 if such a comparison should be
made.
@return 1, 0, -1, if dtuple is greater, equal, less than rec,
respectively, when only the common first fields are compared, or until
the first externally stored field in rec */
UNIV_INTERN
int
cmp_dtuple_rec_with_match_low(
/*==========================*/
	const dtuple_t*	dtuple,	/*!< in: data tuple */
	const rec_t*	rec,	/*!< in: physical record which differs from
				dtuple in some of the common fields, or which
				has an equal number or more fields than
				dtuple */
	const ulint*	offsets,/*!< in: array returned by rec_get_offsets() */
	ulint		n_cmp,	/*!< in: number of fields to compare */
	ulint*		matched_fields,
				/*!< in/out: number of already completely
				matched fields; when function returns,
				contains the value for current comparison */
	ulint*		matched_bytes)
				/*!< in/out: number of already matched
				bytes within the first field not completely
				matched; when function returns, contains the
				value for current comparison */
	__attribute__((nonnull));
#define cmp_dtuple_rec_with_match(tuple,rec,offsets,fields,bytes)	\
	cmp_dtuple_rec_with_match_low(					\
		tuple,rec,offsets,dtuple_get_n_fields_cmp(tuple),fields,bytes)
/**************************************************************//**
Compares a data tuple to a physical record.
@see cmp_dtuple_rec_with_match
@return 1, 0, -1, if dtuple is greater, equal, less than rec, respectively */
UNIV_INTERN
int
cmp_dtuple_rec(
/*===========*/
	const dtuple_t*	dtuple,	/*!< in: data tuple */
	const rec_t*	rec,	/*!< in: physical record */
	const ulint*	offsets);/*!< in: array returned by rec_get_offsets() */
/**************************************************************//**
Checks if a dtuple is a prefix of a record. The last field in dtuple
is allowed to be a prefix of the corresponding field in the record.
@return	TRUE if prefix */
UNIV_INTERN
ibool
cmp_dtuple_is_prefix_of_rec(
/*========================*/
	const dtuple_t*	dtuple,	/*!< in: data tuple */
	const rec_t*	rec,	/*!< in: physical record */
	const ulint*	offsets);/*!< in: array returned by rec_get_offsets() */
/*************************************************************//**
Compare two physical records that contain the same number of columns,
none of which are stored externally.
@retval 1 if rec1 (including non-ordering columns) is greater than rec2
@retval -1 if rec1 (including non-ordering columns) is less than rec2
@retval 0 if rec1 is a duplicate of rec2 */
UNIV_INTERN
int
cmp_rec_rec_simple(
/*===============*/
	const rec_t*		rec1,	/*!< in: physical record */
	const rec_t*		rec2,	/*!< in: physical record */
	const ulint*		offsets1,/*!< in: rec_get_offsets(rec1, ...) */
	const ulint*		offsets2,/*!< in: rec_get_offsets(rec2, ...) */
	const dict_index_t*	index,	/*!< in: data dictionary index */
	struct TABLE*		table)	/*!< in: MySQL table, for reporting
					duplicate key value if applicable,
					or NULL */
	__attribute__((nonnull(1,2,3,4), warn_unused_result));
/*************************************************************//**
This function is used to compare two physical records. Only the common
first fields are compared, and if an externally stored field is
encountered, then 0 is returned.
@return 1, 0, -1 if rec1 is greater, equal, less, respectively */
UNIV_INTERN
int
cmp_rec_rec_with_match(
/*===================*/
	const rec_t*	rec1,	/*!< in: physical record */
	const rec_t*	rec2,	/*!< in: physical record */
	const ulint*	offsets1,/*!< in: rec_get_offsets(rec1, index) */
	const ulint*	offsets2,/*!< in: rec_get_offsets(rec2, index) */
	dict_index_t*	index,	/*!< in: data dictionary index */
	ibool		nulls_unequal,
				/* in: TRUE if this is for index statistics
				cardinality estimation, and innodb_stats_method
				is "nulls_unequal" or "nulls_ignored" */
	ulint*		matched_fields, /*!< in/out: number of already completely
				matched fields; when the function returns,
				contains the value the for current
				comparison */
	ulint*		matched_bytes);/*!< in/out: number of already matched
				bytes within the first field not completely
				matched; when the function returns, contains
				the value for the current comparison */
/*************************************************************//**
This function is used to compare two physical records. Only the common
first fields are compared.
@return 1, 0 , -1 if rec1 is greater, equal, less, respectively, than
rec2; only the common first fields are compared */
UNIV_INLINE
int
cmp_rec_rec(
/*========*/
	const rec_t*	rec1,	/*!< in: physical record */
	const rec_t*	rec2,	/*!< in: physical record */
	const ulint*	offsets1,/*!< in: rec_get_offsets(rec1, index) */
	const ulint*	offsets2,/*!< in: rec_get_offsets(rec2, index) */
	dict_index_t*	index);	/*!< in: data dictionary index */

/*****************************************************************
This function is used to compare two dfields where at least the first
has its data type field set. */
UNIV_INTERN
int
cmp_dfield_dfield_like_prefix(
/*==========================*/
				/* out: 1, 0, -1, if dfield1 is greater, equal,
				less than dfield2, respectively */
	dfield_t*	dfield1,/* in: data field; must have type field set */
	dfield_t*	dfield2);/* in: data field */
/*****************************************************************
This function is used to compare two dfields where at least the first
has its data type field set. */
UNIV_INLINE
int
cmp_dfield_dfield_like_substr(
/*==========================*/
				/* out: 1, 0, -1, if dfield1 is greater, equal,
				less than dfield2, respectively */
	dfield_t*	dfield1,/* in: data field; must have type field set */
	dfield_t*	dfield2);/* in: data field */
/*****************************************************************
This function is used to compare two dfields where at least the first
has its data type field set. */
UNIV_INLINE
int
cmp_dfield_dfield_like_suffix(
/*==========================*/
				/* out: 1, 0, -1, if dfield1 is greater, equal,
				less than dfield2, respectively */
	dfield_t*	dfield1,/* in: data field; must have type field set */
	dfield_t*	dfield2);/* in: data field */

#ifndef UNIV_NONINL
#include "rem0cmp.ic"
#endif

#endif
