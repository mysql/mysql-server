/*****************************************************************************
Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

/** @file include/rem0wrec.h
 Record manager wrapper declaration.

 After INSTANT ADD/DROP feature, fields index on logical record might not be
 same as field index on physical record. So this wrapper is implemented which
 translates logical index to physical index. And then functions of low level
 record manager (rem0lrec.h) are called with physical index of the field.

 Created 13/08/2021 Mayank Prasad
******************************************************************************/

#ifndef rem0wrec_h
#define rem0wrec_h

#include "rem/rec.h"

/** Gets the value of the specified field in the record.
@param[in]      index   record descriptor
@param[in]      rec     physical record
@param[in]      offsets array returned by rec_get_offsets()
@param[in]      n       index of the field
@param[out]     len     length of the field, UNIV_SQL_NULL if SQL null
@return value of the field */
byte *rec_get_nth_field(const dict_index_t *index, const rec_t *rec,
                        const ulint *offsets, ulint n, ulint *len);

const byte *rec_get_nth_field_old(const dict_index_t *index, const rec_t *rec,
                                  ulint n, ulint *len);

/** Gets the physical size of an old-style field.
Also an SQL null may have a field of size > 0, if the data type is of a fixed
size.
@param[in]      index   record descriptor
@param[in]      rec     record
@param[in]      n       index of the field
@return field size in bytes */
[[nodiscard]] ulint rec_get_nth_field_size(const dict_index_t *index,
                                           const rec_t *rec, ulint n);

/** The following function is used to get an offset to the nth data field in a
record.
@param[in]      index   record descriptor
@param[in]      offsets array returned by rec_get_offsets()
@param[in]      n       index of the field
@param[out]     len     length of the field; UNIV_SQL_NULL if SQL null;
                        UNIV_SQL_ADD_COL_DEFAULT if it's default value and no
                        value inlined
@return offset from the origin of rec */
ulint rec_get_nth_field_offs(const dict_index_t *index, const ulint *offsets,
                             ulint n, ulint *len);

/** The following function is used to get the offset to the nth
data field in an old-style record.
@param[in]      index   record descriptor
@param[in]      rec     record
@param[in]      n       index of the field
@param[in]      len     length of the field;UNIV_SQL_NULL if SQL null
@return offset to the field */
ulint rec_get_nth_field_offs_old(const dict_index_t *index, const rec_t *rec,
                                 ulint n, ulint *len);

/** Returns nonzero if the extern bit is set in nth field of rec.
@param[in]      index           record descriptor
@param[in]      offsets         array returned by rec_get_offsets()
@param[in]      n               nth field
@return nonzero if externally stored */
[[nodiscard]] ulint rec_offs_nth_extern(const dict_index_t *index,
                                        const ulint *offsets, ulint n);

/** Mark the nth field as externally stored.
@param[in]      index           record descriptor
@param[in]      offsets         array returned by rec_get_offsets()
@param[in]      n               nth field */
void rec_offs_make_nth_extern(dict_index_t *index, ulint *offsets, ulint n);

/** Returns nonzero if the SQL NULL bit is set in nth field of rec.
@param[in]      index           record descriptor
@param[in]      offsets         array returned by rec_get_offsets()
@param[in]      n               nth field
@return nonzero if SQL NULL */
[[nodiscard]] ulint rec_offs_nth_sql_null(const dict_index_t *index,
                                          const ulint *offsets, ulint n);

/** Returns nonzero if the default bit is set in nth field of rec.
@param[in]      index           record descriptor
@param[in]      offsets         array returned by rec_get_offsets()
@param[in]      n               nth field
@return nonzero if default bit is set */
ulint rec_offs_nth_default(const dict_index_t *index, const ulint *offsets,
                           ulint n);

/** Gets the physical size of a field.
@param[in]      index           record descriptor
@param[in]      offsets         array returned by rec_get_offsets()
@param[in]      n               nth field
@return length of field */
[[nodiscard]] ulint rec_offs_nth_size(const dict_index_t *index,
                                      const ulint *offsets, ulint n);

/** This is used to modify the value of an already existing field in a record.
The previous value must have exactly the same size as the new value. If len is
UNIV_SQL_NULL then the field is treated as an SQL null.
For records in ROW_FORMAT=COMPACT (new-style records), len must not be
UNIV_SQL_NULL unless the field already is SQL null.
@param[in]      index   record descriptor
@param[in]      rec     record
@param[in]      offsets array returned by rec_get_offsets()
@param[in]      n       index number of the field
@param[in]      len     length of the data or UNIV_SQL_NULL.
                        If not SQL null, must have the same length as the
                        previous value.
                        If SQL null, previous value must be SQL null.
@param[in]      data    pointer to the data if not SQL null */
void rec_set_nth_field(const dict_index_t *index, rec_t *rec,
                       const ulint *offsets, ulint n, const void *data,
                       ulint len);

/** Returns nonzero if the field is stored off-page.
@param[in]      index   index
@param[in]      rec     record
@param[in]      n       field index
@retval 0 if the field is stored in-page
@retval REC_2BYTE_EXTERN_MASK if the field is stored externally */
[[nodiscard]] ulint rec_2_is_field_extern(const dict_index_t *index,
                                          const rec_t *rec, ulint n);

/** The following function returns the data size of an old-style physical
record, that is the sum of field lengths. SQL null fields are counted as length
0 fields. The value returned by the function is the distance from record origin
to record end in bytes.
@return size */
ulint rec_get_data_size_old(const rec_t *rec);
#endif
