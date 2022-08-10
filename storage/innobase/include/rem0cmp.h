/*****************************************************************************

Copyright (c) 1994, 2022, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/rem0cmp.h
 Comparison services for records

 Created 7/1/1994 Heikki Tuuri
 ************************************************************************/

#ifndef rem0cmp_h
#define rem0cmp_h

#include <my_sys.h>
#include "data0data.h"
#include "data0type.h"
#include "dict0dict.h"
#include "ha_prototypes.h"
#include "rem0rec.h"

/** Disable the min flag during row comparisons. */
constexpr auto DISABLE_MIN_REC_FLAG_CHECK = ULINT_UNDEFINED;

// Forward declaration
namespace dd {
class Spatial_reference_system;
}

/** Returns true if two columns are equal for comparison purposes.
@param[in] col1 Column to compare.
@param[in] col2 Column to compare.
@param[in] check_charsets if true then check the character sets.
@return true if the columns are considered equal in comparisons. */
bool cmp_cols_are_equal(const dict_col_t *col1, const dict_col_t *col2,
                        bool check_charsets);

/*!< in: whether to check charsets */
/** Compare two data fields.
@param[in]      mtype   main type
@param[in]      prtype  precise type
@param[in]      is_asc  true=ascending, false=descending order
@param[in]      data1   data field
@param[in]      len1    length of data1 in bytes, or UNIV_SQL_NULL
@param[in]      data2   data field
@param[in]      len2    length of data2 in bytes, or UNIV_SQL_NULL
@return the comparison result of data1 and data2
@retval 0 if data1 is equal to data2
@retval negative if data1 is less than data2
@retval positive if data1 is greater than data2 */
[[nodiscard]] int cmp_data_data(ulint mtype, ulint prtype, bool is_asc,
                                const byte *data1, ulint len1,
                                const byte *data2, ulint len2);

/** Compare two data fields.
@param[in]      dfield1 data field; must have type field set
@param[in]      dfield2 data field
@param[in]      is_asc  true=ASC, false=DESC
@return the comparison result of dfield1 and dfield2
@retval 0 if dfield1 is equal to dfield2
@retval negative if dfield1 is less than dfield2
@retval positive if dfield1 is greater than dfield2 */
[[nodiscard]] static inline int cmp_dfield_dfield(const dfield_t *dfield1,
                                                  const dfield_t *dfield2,
                                                  bool is_asc);

/** Compare two data fields, the first one can be of any form of multi-value
field, while the second one must be one field from multi-value index
@param[in]      dfield1 multi-value data field;
@param[in]      dfield2 data field; must have type field set
@return 0 if dfield1 has dfield2 or they are equal if both NULL, otherwise 1 */
[[nodiscard]] static inline int cmp_multi_value_dfield_dfield(
    const dfield_t *dfield1, const dfield_t *dfield2);

/** Compare a GIS data tuple to a physical record.
@param[in] dtuple data tuple
@param[in] rec B-tree record
@param[in] offsets rec_get_offsets(rec)
@param[in] mode compare mode
@param[in] srs Spatial reference system of R-tree
@retval negative if dtuple is less than rec */
int cmp_dtuple_rec_with_gis(const dtuple_t *dtuple, const rec_t *rec,
                            const ulint *offsets, page_cur_mode_t mode,
                            const dd::Spatial_reference_system *srs);

/** Compare a GIS data tuple to a physical record in rtree non-leaf node.
We need to check the page number field, since we don't store pk field in
rtree non-leaf node.
@param[in]      dtuple  data tuple
@param[in]      rec     R-tree record
@param[in]      offsets rec_get_offsets(rec)
@param[in]  srs Spatial referenxe system
@retval negative if dtuple is less than rec */
int cmp_dtuple_rec_with_gis_internal(const dtuple_t *dtuple, const rec_t *rec,
                                     const ulint *offsets,
                                     const dd::Spatial_reference_system *srs);

/** Compare a data tuple to a physical record.
@param[in]      dtuple          data tuple
@param[in]      rec             record
@param[in]      index           index
@param[in]      offsets         rec_get_offsets(rec)
@param[in]      n_cmp           number of fields to compare
@param[in,out]  matched_fields  number of completely matched fields
@return the comparison result of dtuple and rec
@retval 0 if dtuple is equal to rec
@retval negative if dtuple is less than rec
@retval positive if dtuple is greater than rec */
int cmp_dtuple_rec_with_match_low(const dtuple_t *dtuple, const rec_t *rec,
                                  const dict_index_t *index,
                                  const ulint *offsets, ulint n_cmp,
                                  ulint *matched_fields);

/** Compare a data tuple to a physical record.
@param[in]      dtuple          data tuple
@param[in]      rec             B-tree or R-tree index record
@param[in]      index           index tree
@param[in]      offsets         rec_get_offsets(rec)
@param[in,out]  matched_fields  number of completely matched fields
@param[in,out]  matched_bytes   number of matched bytes in the first
field that is not matched
@return the comparison result of dtuple and rec
@retval 0 if dtuple is equal to rec
@retval negative if dtuple is less than rec
@retval positive if dtuple is greater than rec */
[[nodiscard]] int cmp_dtuple_rec_with_match_bytes(
    const dtuple_t *dtuple, const rec_t *rec, const dict_index_t *index,
    const ulint *offsets, ulint *matched_fields, ulint *matched_bytes);
/** Compare a data tuple to a physical record.
@see cmp_dtuple_rec_with_match
@param[in]      dtuple  data tuple
@param[in]      rec     record
@param[in]      index   index
@param[in]      offsets rec_get_offsets(rec)
@return the comparison result of dtuple and rec
@retval 0 if dtuple is equal to rec
@retval negative if dtuple is less than rec
@retval positive if dtuple is greater than rec */
[[nodiscard]] int cmp_dtuple_rec(const dtuple_t *dtuple, const rec_t *rec,
                                 const dict_index_t *index,
                                 const ulint *offsets);
/** Check if a dtuple is a prefix of a record.
@param[in]      dtuple  data tuple
@param[in]      rec     B-tree record
@param[in]      index   B-tree index
@param[in]      offsets rec_get_offsets(rec)
@return true if prefix */
[[nodiscard]] bool cmp_dtuple_is_prefix_of_rec(const dtuple_t *dtuple,
                                               const rec_t *rec,
                                               const dict_index_t *index,
                                               const ulint *offsets);
/** Compare two physical records that contain the same number of columns,
none of which are stored externally.
@param[in] rec1 Physical record 1 to compare
@param[in] rec2 Physical record 2 to compare
@param[in] offsets1 rec_get_offsets(rec1, ...)
@param[in] offsets2 rec_get_offsets(rec2, ...)
@param[in] index Data dictionary index
@param[in] table MySQL table, for reporting duplicate key value if applicable,
or nullptr
@retval positive if rec1 (including non-ordering columns) is greater than rec2
@retval negative if rec1 (including non-ordering columns) is less than rec2
@retval 0 if rec1 is a duplicate of rec2 */
[[nodiscard]] int cmp_rec_rec_simple(const rec_t *rec1, const rec_t *rec2,
                                     const ulint *offsets1,
                                     const ulint *offsets2,
                                     const dict_index_t *index,
                                     struct TABLE *table);
/** Compare two B-tree records.
@param[in] rec1 B-tree record
@param[in] rec2 B-tree record
@param[in] offsets1 rec_get_offsets(rec1, index)
@param[in] offsets2 rec_get_offsets(rec2, index)
@param[in] index B-tree index
@param[in] spatial_index_non_leaf true if record is in spatial non leaf page
@param[in] nulls_unequal true if this is for index cardinality
statistics estimation, and innodb_stats_method=nulls_unequal
or innodb_stats_method=nulls_ignored
@param[out] matched_fields number of completely matched fields
within the first field not completely matched
@param[in] cmp_btree_recs true if we're comparing two b-tree records
@return the comparison result
@retval 0 if rec1 is equal to rec2
@retval negative if rec1 is less than rec2
@retval positive if rec2 is greater than rec2 */
int cmp_rec_rec_with_match(const rec_t *rec1, const rec_t *rec2,
                           const ulint *offsets1, const ulint *offsets2,
                           const dict_index_t *index,
                           bool spatial_index_non_leaf, bool nulls_unequal,
                           ulint *matched_fields, bool cmp_btree_recs = true);

/** Compare two B-tree records.
Only the common first fields are compared, and externally stored field
are treated as equal.
@param[in]      rec1                    B-tree record
@param[in]      rec2                    B-tree record
@param[in]      offsets1                rec_get_offsets(rec1, index)
@param[in]      offsets2                rec_get_offsets(rec2, index)
@param[in]      index                   B-tree index
@param[in]      spatial_index_non_leaf  true if spatial index non leaf records
@param[out]     matched_fields  number of completely matched fields
within the first field not completely matched
@param[in]  cmp_btree_recs  true if the both the records are b-tree records
@return positive, 0, negative if rec1 is greater, equal, less, than rec2,
respectively */
static inline int cmp_rec_rec(const rec_t *rec1, const rec_t *rec2,
                              const ulint *offsets1, const ulint *offsets2,
                              const dict_index_t *index,
                              bool spatial_index_non_leaf,
                              ulint *matched_fields = nullptr,
                              bool cmp_btree_recs = true);

#ifndef UNIV_HOTBACKUP
/** Compare two data fields.
@param[in] dfield1 data field
@param[in] dfield2 data field
@return the comparison result of dfield1 and dfield2
@retval true if dfield1 is equal to dfield2, or a prefix of dfield1
@retval false otherwise */
[[nodiscard]] static inline bool cmp_dfield_dfield_eq_prefix(
    const dfield_t *dfield1, const dfield_t *dfield2);
#endif /* !UNIV_HOTBACKUP */

#include "rem0cmp.ic"

#endif
