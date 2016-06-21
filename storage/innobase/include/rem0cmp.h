/*****************************************************************************

Copyright (c) 1994, 2016, Oracle and/or its affiliates. All Rights Reserved.

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

#include "ha_prototypes.h"
#include "data0data.h"
#include "data0type.h"
#include "dict0dict.h"
#include "rem0rec.h"
#include <my_sys.h>

/*************************************************************//**
Returns TRUE if two columns are equal for comparison purposes.
@return TRUE if the columns are considered equal in comparisons */
ibool
cmp_cols_are_equal(
/*===============*/
	const dict_col_t*	col1,	/*!< in: column 1 */
	const dict_col_t*	col2,	/*!< in: column 2 */
	ibool			check_charsets);
					/*!< in: whether to check charsets */
/** Compare two data fields.
@param[in] mtype main type
@param[in] prtype precise type
@param[in] data1 data field
@param[in] len1 length of data1 in bytes, or UNIV_SQL_NULL
@param[in] data2 data field
@param[in] len2 length of data2 in bytes, or UNIV_SQL_NULL
@return the comparison result of data1 and data2
@retval 0 if data1 is equal to data2
@retval negative if data1 is less than data2
@retval positive if data1 is greater than data2 */
int
cmp_data_data(
	ulint		mtype,
	ulint		prtype,
	const byte*	data1,
	ulint		len1,
	const byte*	data2,
	ulint		len2)
	MY_ATTRIBUTE((nonnull, warn_unused_result));

/** Compare two data fields.
@param[in] dfield1 data field; must have type field set
@param[in] dfield2 data field
@return the comparison result of dfield1 and dfield2
@retval 0 if dfield1 is equal to dfield2
@retval negative if dfield1 is less than dfield2
@retval positive if dfield1 is greater than dfield2 */
UNIV_INLINE
int
cmp_dfield_dfield(
/*==============*/
	const dfield_t*	dfield1,/*!< in: data field; must have type field set */
	const dfield_t*	dfield2);/*!< in: data field */


/** Compare a GIS data tuple to a physical record.
@param[in] dtuple data tuple
@param[in] rec B-tree record
@param[in] offsets rec_get_offsets(rec)
@param[in] mode compare mode
@retval negative if dtuple is less than rec */
int
cmp_dtuple_rec_with_gis(
/*====================*/
	const dtuple_t*	dtuple,
	const rec_t*	rec,
	const ulint*	offsets,
	page_cur_mode_t	mode)
	MY_ATTRIBUTE((nonnull));

/** Compare a GIS data tuple to a physical record in rtree non-leaf node.
We need to check the page number field, since we don't store pk field in
rtree non-leaf node.
@param[in] dtuple data tuple
@param[in] rec R-tree record
@param[in] offsets rec_get_offsets(rec)
@param[in] mode compare mode
@retval negative if dtuple is less than rec */
int
cmp_dtuple_rec_with_gis_internal(
	const dtuple_t*	dtuple,
	const rec_t*	rec,
	const ulint*	offsets);

/** Compare a data tuple to a physical record.
@param[in] dtuple data tuple
@param[in] rec B-tree record
@param[in] offsets rec_get_offsets(rec)
@param[in] n_cmp number of fields to compare
@param[in,out] matched_fields number of completely matched fields
@return the comparison result of dtuple and rec
@retval 0 if dtuple is equal to rec
@retval negative if dtuple is less than rec
@retval positive if dtuple is greater than rec */
int
cmp_dtuple_rec_with_match_low(
	const dtuple_t*	dtuple,
	const rec_t*	rec,
	const ulint*	offsets,
	ulint		n_cmp,
	ulint*		matched_fields)
	MY_ATTRIBUTE((nonnull));
#define cmp_dtuple_rec_with_match(tuple,rec,offsets,fields)		\
	cmp_dtuple_rec_with_match_low(					\
		tuple,rec,offsets,dtuple_get_n_fields_cmp(tuple),fields)
/** Compare a data tuple to a physical record.
@param[in]	dtuple		data tuple
@param[in]	rec		B-tree or R-tree index record
@param[in]	index		index tree
@param[in]	offsets		rec_get_offsets(rec)
@param[in,out]	matched_fields	number of completely matched fields
@param[in,out]	matched_bytes	number of matched bytes in the first
field that is not matched
@return the comparison result of dtuple and rec
@retval 0 if dtuple is equal to rec
@retval negative if dtuple is less than rec
@retval positive if dtuple is greater than rec */
int
cmp_dtuple_rec_with_match_bytes(
	const dtuple_t*		dtuple,
	const rec_t*		rec,
	const dict_index_t*	index,
	const ulint*		offsets,
	ulint*			matched_fields,
	ulint*			matched_bytes)
	MY_ATTRIBUTE((warn_unused_result));
/** Compare a data tuple to a physical record.
@see cmp_dtuple_rec_with_match
@param[in] dtuple data tuple
@param[in] rec B-tree record
@param[in] offsets rec_get_offsets(rec)
@return the comparison result of dtuple and rec
@retval 0 if dtuple is equal to rec
@retval negative if dtuple is less than rec
@retval positive if dtuple is greater than rec */
int
cmp_dtuple_rec(
	const dtuple_t*	dtuple,
	const rec_t*	rec,
	const ulint*	offsets);
/**************************************************************//**
Checks if a dtuple is a prefix of a record. The last field in dtuple
is allowed to be a prefix of the corresponding field in the record.
@return TRUE if prefix */
ibool
cmp_dtuple_is_prefix_of_rec(
/*========================*/
	const dtuple_t*	dtuple,	/*!< in: data tuple */
	const rec_t*	rec,	/*!< in: physical record */
	const ulint*	offsets);/*!< in: array returned by rec_get_offsets() */
/** Compare two physical records that contain the same number of columns,
none of which are stored externally.
@retval positive if rec1 (including non-ordering columns) is greater than rec2
@retval negative if rec1 (including non-ordering columns) is less than rec2
@retval 0 if rec1 is a duplicate of rec2 */
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
	MY_ATTRIBUTE((nonnull(1,2,3,4), warn_unused_result));
/** Compare two B-tree records.
@param[in] rec1 B-tree record
@param[in] rec2 B-tree record
@param[in] offsets1 rec_get_offsets(rec1, index)
@param[in] offsets2 rec_get_offsets(rec2, index)
@param[in] index B-tree index
@param[in] nulls_unequal true if this is for index cardinality
statistics estimation, and innodb_stats_method=nulls_unequal
or innodb_stats_method=nulls_ignored
@param[out] matched_fields number of completely matched fields
within the first field not completely matched
@return the comparison result
@retval 0 if rec1 is equal to rec2
@retval negative if rec1 is less than rec2
@retval positive if rec2 is greater than rec2 */
int
cmp_rec_rec_with_match(
	const rec_t*		rec1,
	const rec_t*		rec2,
	const ulint*		offsets1,
	const ulint*		offsets2,
	const dict_index_t*	index,
	bool			nulls_unequal,
	ulint*			matched_fields);

/** Compare two B-tree records.
Only the common first fields are compared, and externally stored field
are treated as equal.
@param[in]	rec1		B-tree record
@param[in]	rec2		B-tree record
@param[in]	offsets1	rec_get_offsets(rec1, index)
@param[in]	offsets2	rec_get_offsets(rec2, index)
@param[out]	matched_fields	number of completely matched fields
				within the first field not completely matched
@return positive, 0, negative if rec1 is greater, equal, less, than rec2,
respectively */
UNIV_INLINE
int
cmp_rec_rec(
	const rec_t*		rec1,
	const rec_t*		rec2,
	const ulint*		offsets1,
	const ulint*		offsets2,
	const dict_index_t*	index,
	ulint*			matched_fields = NULL);

/** Compare two data fields.
@param[in] dfield1 data field
@param[in] dfield2 data field
@return the comparison result of dfield1 and dfield2
@retval 0 if dfield1 is equal to dfield2, or a prefix of dfield1
@retval negative if dfield1 is less than dfield2
@retval positive if dfield1 is greater than dfield2 */
UNIV_INLINE
int
cmp_dfield_dfield_like_prefix(
	const dfield_t*	dfield1,
	const dfield_t*	dfield2);

#ifndef UNIV_NONINL
#include "rem0cmp.ic"
#endif

#endif
