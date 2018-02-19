/*****************************************************************************

Copyright (c) 1994, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file include/data0data.h
 SQL data field and tuple

 Created 5/30/1994 Heikki Tuuri
 *************************************************************************/

#ifndef data0data_h
#define data0data_h

#include "univ.i"

#include "data0type.h"
#include "data0types.h"
#include "dict0types.h"
#include "mem0mem.h"
#include "trx0types.h"

#include <ostream>

/** Storage for overflow data in a big record, that is, a clustered
index record which needs external storage of data fields */
struct big_rec_t;
struct upd_t;

#ifdef UNIV_DEBUG
/** Gets pointer to the type struct of SQL data field.
 @return pointer to the type struct */
UNIV_INLINE
dtype_t *dfield_get_type(const dfield_t *field) /*!< in: SQL data field */
    MY_ATTRIBUTE((warn_unused_result));
/** Gets pointer to the data in a field.
 @return pointer to data */
UNIV_INLINE
void *dfield_get_data(const dfield_t *field) /*!< in: field */
    MY_ATTRIBUTE((warn_unused_result));
#else /* UNIV_DEBUG */
#define dfield_get_type(field) (&(field)->type)
#define dfield_get_data(field) ((field)->data)
#endif /* UNIV_DEBUG */

/** Sets the type struct of SQL data field.
@param[in]	field	SQL data field
@param[in]	type	pointer to data type struct */
UNIV_INLINE
void dfield_set_type(dfield_t *field, const dtype_t *type);

/** Gets length of field data.
 @return length of data; UNIV_SQL_NULL if SQL null data */
UNIV_INLINE
ulint dfield_get_len(const dfield_t *field) /*!< in: field */
    MY_ATTRIBUTE((warn_unused_result));

/** Sets length in a field.
@param[in]	field	field
@param[in]	len	length or UNIV_SQL_NULL */
UNIV_INLINE
void dfield_set_len(dfield_t *field, ulint len);

/** Determines if a field is SQL NULL
 @return nonzero if SQL null data */
UNIV_INLINE
ulint dfield_is_null(const dfield_t *field) /*!< in: field */
    MY_ATTRIBUTE((warn_unused_result));
/** Determines if a field is externally stored
 @return nonzero if externally stored */
UNIV_INLINE
ulint dfield_is_ext(const dfield_t *field) /*!< in: field */
    MY_ATTRIBUTE((warn_unused_result));
/** Sets the "external storage" flag */
UNIV_INLINE
void dfield_set_ext(dfield_t *field); /*!< in/out: field */

/** Gets spatial status for "external storage"
@param[in,out]	field		field */
UNIV_INLINE
spatial_status_t dfield_get_spatial_status(const dfield_t *field);

/** Sets spatial status for "external storage"
@param[in,out]	field		field
@param[in]	spatial_status	spatial status */
UNIV_INLINE
void dfield_set_spatial_status(dfield_t *field,
                               spatial_status_t spatial_status);

/** Sets pointer to the data and length in a field.
@param[in]	field	field
@param[in]	data	data
@param[in]	len	length or UNIV_SQL_NULL */
UNIV_INLINE
void dfield_set_data(dfield_t *field, const void *data, ulint len);

/** Sets pointer to the data and length in a field.
@param[in]	field	field
@param[in]	mbr	data */
UNIV_INLINE
void dfield_write_mbr(dfield_t *field, const double *mbr);

/** Sets a data field to SQL NULL. */
UNIV_INLINE
void dfield_set_null(dfield_t *field); /*!< in/out: field */

/** Writes an SQL null field full of zeros.
@param[in]	data	pointer to a buffer of size len
@param[in]	len	SQL null size in bytes */
UNIV_INLINE
void data_write_sql_null(byte *data, ulint len);

/** Copies the data and len fields.
@param[out]	field1	field to copy to
@param[in]	field2	field to copy from */
UNIV_INLINE
void dfield_copy_data(dfield_t *field1, const dfield_t *field2);

/** Copies a data field to another.
@param[out]	field1	field to copy to
@param[in]	field2	field to copy from */
UNIV_INLINE
void dfield_copy(dfield_t *field1, const dfield_t *field2);

/** Copies the data pointed to by a data field.
@param[in,out]	field	data field
@param[in]	heap	memory heap where allocated */
UNIV_INLINE
void dfield_dup(dfield_t *field, mem_heap_t *heap);

/** Tests if two data fields are equal.
 If len==0, tests the data length and content for equality.
 If len>0, tests the first len bytes of the content for equality.
 @return true if both fields are NULL or if they are equal */
UNIV_INLINE
ibool dfield_datas_are_binary_equal(
    const dfield_t *field1, /*!< in: field */
    const dfield_t *field2, /*!< in: field */
    ulint len)              /*!< in: maximum prefix to compare,
                            or 0 to compare the whole field length */
    MY_ATTRIBUTE((warn_unused_result));
/** Tests if dfield data length and content is equal to the given.
 @return true if equal */
UNIV_INLINE
ibool dfield_data_is_binary_equal(
    const dfield_t *field, /*!< in: field */
    ulint len,             /*!< in: data length or UNIV_SQL_NULL */
    const byte *data)      /*!< in: data */
    MY_ATTRIBUTE((warn_unused_result));
/** Gets number of fields in a data tuple.
 @return number of fields */
UNIV_INLINE
ulint dtuple_get_n_fields(const dtuple_t *tuple) /*!< in: tuple */
    MY_ATTRIBUTE((warn_unused_result));

/** Gets number of virtual fields in a data tuple.
@param[in]	tuple	dtuple to check
@return number of fields */
UNIV_INLINE
ulint dtuple_get_n_v_fields(const dtuple_t *tuple);

#ifdef UNIV_DEBUG
/** Gets nth field of a tuple.
@param[in]	tuple	tuple
@param[in]	n	index of field
@return nth field */
UNIV_INLINE
dfield_t *dtuple_get_nth_field(const dtuple_t *tuple, ulint n);

/** Gets nth virtual field of a tuple.
@param[in]	tuple	tuple
@param[in]	n	the nth field to get
@return nth field */
UNIV_INLINE
dfield_t *dtuple_get_nth_v_field(const dtuple_t *tuple, ulint n);

#else /* UNIV_DEBUG */
#define dtuple_get_nth_field(tuple, n) ((tuple)->fields + (n))
#define dtuple_get_nth_v_field(tuple, n) \
  ((tuple)->fields + (tuple)->n_fields + (n))
#endif /* UNIV_DEBUG */
/** Gets info bits in a data tuple.
 @return info bits */
UNIV_INLINE
ulint dtuple_get_info_bits(const dtuple_t *tuple) /*!< in: tuple */
    MY_ATTRIBUTE((warn_unused_result));

/** Sets info bits in a data tuple.
@param[in]	tuple		tuple
@param[in]	info_bits	info bits */
UNIV_INLINE
void dtuple_set_info_bits(dtuple_t *tuple, ulint info_bits);

/** Gets number of fields used in record comparisons.
 @return number of fields used in comparisons in rem0cmp.* */
UNIV_INLINE
ulint dtuple_get_n_fields_cmp(const dtuple_t *tuple) /*!< in: tuple */
    MY_ATTRIBUTE((warn_unused_result));

/** Gets number of fields used in record comparisons.
@param[in]	tuple		tuple
@param[in]	n_fields_cmp	number of fields used in comparisons in
                                rem0cmp */
UNIV_INLINE
void dtuple_set_n_fields_cmp(dtuple_t *tuple, ulint n_fields_cmp);

/* Estimate the number of bytes that are going to be allocated when
creating a new dtuple_t object */
#define DTUPLE_EST_ALLOC(n_fields) \
  (sizeof(dtuple_t) + (n_fields) * sizeof(dfield_t))

/** Creates a data tuple from an already allocated chunk of memory.
 The size of the chunk must be at least DTUPLE_EST_ALLOC(n_fields).
 The default value for number of fields used in record comparisons
 for this tuple is n_fields.
 @param[in,out]	buf		buffer to use
 @param[in]	buf_size	buffer size
 @param[in]	n_fields	number of field
 @param[in]	n_v_fields	number of fields on virtual columns
 @return created tuple (inside buf) */
UNIV_INLINE
dtuple_t *dtuple_create_from_mem(void *buf, ulint buf_size, ulint n_fields,
                                 ulint n_v_fields)
    MY_ATTRIBUTE((warn_unused_result));
/** Creates a data tuple to a memory heap. The default value for number
 of fields used in record comparisons for this tuple is n_fields.
 @return own: created tuple */
UNIV_INLINE
dtuple_t *dtuple_create(
    mem_heap_t *heap, /*!< in: memory heap where the tuple
                      is created, DTUPLE_EST_ALLOC(n_fields)
                      bytes will be allocated from this heap */
    ulint n_fields)   /*!< in: number of fields */
    MY_ATTRIBUTE((malloc));

/** Initialize the virtual field data in a dtuple_t
@param[in,out]		vrow	dtuple contains the virtual fields */
UNIV_INLINE
void dtuple_init_v_fld(const dtuple_t *vrow);

/** Duplicate the virtual field data in a dtuple_t
@param[in,out]		vrow	dtuple contains the virtual fields
@param[in]		heap	heap memory to use */
UNIV_INLINE
void dtuple_dup_v_fld(const dtuple_t *vrow, mem_heap_t *heap);

/** Creates a data tuple with possible virtual columns to a memory heap.
@param[in]	heap		memory heap where the tuple is created
@param[in]	n_fields	number of fields
@param[in]	n_v_fields	number of fields on virtual col
@return own: created tuple */
UNIV_INLINE
dtuple_t *dtuple_create_with_vcol(mem_heap_t *heap, ulint n_fields,
                                  ulint n_v_fields);
/** Sets number of fields used in a tuple. Normally this is set in
 dtuple_create, but if you want later to set it smaller, you can use this. */
void dtuple_set_n_fields(dtuple_t *tuple, /*!< in: tuple */
                         ulint n_fields); /*!< in: number of fields */
/** Copies a data tuple's virtaul fields to another. This is a shallow copy;
@param[in,out]	d_tuple		destination tuple
@param[in]	s_tuple		source tuple */
UNIV_INLINE
void dtuple_copy_v_fields(dtuple_t *d_tuple, const dtuple_t *s_tuple);
/** Copies a data tuple to another.  This is a shallow copy; if a deep copy
 is desired, dfield_dup() will have to be invoked on each field.
 @return own: copy of tuple */
UNIV_INLINE
dtuple_t *dtuple_copy(const dtuple_t *tuple, /*!< in: tuple to copy from */
                      mem_heap_t *heap)      /*!< in: memory heap
                                             where the tuple is created */
    MY_ATTRIBUTE((malloc));

/** The following function returns the sum of data lengths of a tuple. The space
occupied by the field structs or the tuple struct is not counted.
@param[in]	tuple	typed data tuple
@param[in]	comp	nonzero=ROW_FORMAT=COMPACT
@return sum of data lens */
UNIV_INLINE
ulint dtuple_get_data_size(const dtuple_t *tuple, ulint comp);
/** Computes the number of externally stored fields in a data tuple.
 @return number of fields */
UNIV_INLINE
ulint dtuple_get_n_ext(const dtuple_t *tuple); /*!< in: tuple */
/** Compare two data tuples.
@param[in] tuple1 first data tuple
@param[in] tuple2 second data tuple
@return whether tuple1==tuple2 */
bool dtuple_coll_eq(const dtuple_t *tuple1, const dtuple_t *tuple2)
    MY_ATTRIBUTE((warn_unused_result));

/** Compute a hash value of a prefix of an index record.
@param[in]	tuple		index record
@param[in]	n_fields	number of fields to include
@param[in]	n_bytes		number of bytes to fold in the last field
@param[in]	fold		fold value of the index identifier
@return the folded value */
UNIV_INLINE
ulint dtuple_fold(const dtuple_t *tuple, ulint n_fields, ulint n_bytes,
                  ulint fold) MY_ATTRIBUTE((warn_unused_result));

/** Sets types of fields binary in a tuple.
@param[in]	tuple	data tuple
@param[in]	n	number of fields to set */
UNIV_INLINE
void dtuple_set_types_binary(dtuple_t *tuple, ulint n);

/** Checks if a dtuple contains an SQL null value.
 @return true if some field is SQL null */
UNIV_INLINE
ibool dtuple_contains_null(const dtuple_t *tuple) /*!< in: dtuple */
    MY_ATTRIBUTE((warn_unused_result));
/** Checks that a data field is typed. Asserts an error if not.
 @return true if ok */
ibool dfield_check_typed(const dfield_t *field) /*!< in: data field */
    MY_ATTRIBUTE((warn_unused_result));
/** Checks that a data tuple is typed. Asserts an error if not.
 @return true if ok */
ibool dtuple_check_typed(const dtuple_t *tuple) /*!< in: tuple */
    MY_ATTRIBUTE((warn_unused_result));
#ifdef UNIV_DEBUG
/** Validates the consistency of a tuple which must be complete, i.e,
 all fields must have been set.
 @return true if ok */
ibool dtuple_validate(const dtuple_t *tuple) /*!< in: tuple */
    MY_ATTRIBUTE((warn_unused_result));
#endif /* UNIV_DEBUG */
/** Pretty prints a dfield value according to its data type. Also the hex string
 is printed if a string contains non-printable characters. */
void dfield_print_also_hex(const dfield_t *dfield); /*!< in: dfield */
/** The following function prints the contents of a tuple. */
void dtuple_print(FILE *f,                /*!< in: output stream */
                  const dtuple_t *tuple); /*!< in: tuple */

/** Print the contents of a tuple.
@param[out]	o	output stream
@param[in]	field	array of data fields
@param[in]	n	number of data fields */
void dfield_print(std::ostream &o, const dfield_t *field, ulint n);

/** Print the contents of a tuple.
@param[out]	o	output stream
@param[in]	tuple	data tuple */
void dtuple_print(std::ostream &o, const dtuple_t *tuple);

/** Print the contents of a tuple.
@param[out]	o	output stream
@param[in]	tuple	data tuple */
inline std::ostream &operator<<(std::ostream &o, const dtuple_t &tuple) {
  dtuple_print(o, &tuple);
  return (o);
}

/** Moves parts of long fields in entry to the big record vector so that
 the size of tuple drops below the maximum record size allowed in the
 database. Moves data only from those fields which are not necessary
 to determine uniquely the insertion place of the tuple in the index.
 @return own: created big record vector, NULL if we are not able to
 shorten the entry enough, i.e., if there are too many fixed-length or
 short fields in entry or the index is clustered */
big_rec_t *dtuple_convert_big_rec(dict_index_t *index, /*!< in: index */
                                  upd_t *upd,      /*!< in/out: update vector */
                                  dtuple_t *entry, /*!< in/out: index entry */
                                  ulint *n_ext)    /*!< in/out: number of
                                                   externally stored columns */
    MY_ATTRIBUTE((malloc, warn_unused_result));
/** Puts back to entry the data stored in vector. Note that to ensure the
 fields in entry can accommodate the data, vector must have been created
 from entry with dtuple_convert_big_rec. */
void dtuple_convert_back_big_rec(
    dict_index_t *index, /*!< in: index */
    dtuple_t *entry,     /*!< in: entry whose data was put to vector */
    big_rec_t *vector);  /*!< in, own: big rec vector; it is
                 freed in this function */
/** Frees the memory in a big rec vector. */
UNIV_INLINE
void dtuple_big_rec_free(big_rec_t *vector); /*!< in, own: big rec vector; it is
                                     freed in this function */

/*######################################################################*/

/** Structure for an SQL data field */
struct dfield_t {
  void *data;       /*!< pointer to data */
  unsigned ext : 1; /*!< TRUE=externally stored, FALSE=local */
  unsigned spatial_status : 2;
  /*!< spatial status of externally stored field
  in undo log for purge */
  unsigned len; /*!< data length; UNIV_SQL_NULL if SQL null */
  dtype_t type; /*!< type of data */

  void reset() {
    data = nullptr;
    ext = FALSE;
    spatial_status = SPATIAL_UNKNOWN, len = 0;
  }

  /** Create a deep copy of this object
  @param[in]	heap	the memory heap in which the clone will be
                          created.
  @return	the cloned object. */
  dfield_t *clone(mem_heap_t *heap);

  byte *blobref() const;

  dfield_t()
      : data(nullptr), ext(0), spatial_status(0), len(0), type({0, 0, 0, 0}) {}

  /** Print the dfield_t object into the given output stream.
  @param[in]	out	the output stream.
  @return	the ouput stream. */
  std::ostream &print(std::ostream &out) const;

  /** Adjust and(or) set virtual column value which is read from undo
  or online DDL log
  @param[in]	vcol	virtual column definition
  @param[in]	comp	true if compact format
  @param[in]	field	virtual column value
  @param[in]	len	value length
  @param[in,out]	heap	memory heap to keep value when necessary */
  void adjust_v_data_mysql(const dict_v_col_t *vcol, bool comp,
                           const byte *field, ulint len, mem_heap_t *heap);
};

/** Overloading the global output operator to easily print the given dfield_t
object into the given output stream.
@param[in]	out	the output stream
@param[in]	obj	the given object to print.
@return the output stream. */
inline std::ostream &operator<<(std::ostream &out, const dfield_t &obj) {
  return (obj.print(out));
}

/** Structure for an SQL data tuple of fields (logical record) */
struct dtuple_t {
  ulint info_bits;    /*!< info bits of an index record:
                      the default is 0; this field is used
                      if an index record is built from
                      a data tuple */
  ulint n_fields;     /*!< number of fields in dtuple */
  ulint n_fields_cmp; /*!< number of fields which should
                      be used in comparison services
                      of rem0cmp.*; the index search
                      is performed by comparing only these
                      fields, others are ignored; the
                      default value in dtuple creation is
                      the same value as n_fields */
  dfield_t *fields;   /*!< fields */
  ulint n_v_fields;   /*!< number of virtual fields */
  dfield_t *v_fields; /*!< fields on virtual column */
  UT_LIST_NODE_T(dtuple_t) tuple_list;
  /*!< data tuples can be linked into a
  list using this field */
#ifdef UNIV_DEBUG
  ulint magic_n; /*!< magic number, used in
                 debug assertions */
/** Value of dtuple_t::magic_n */
#define DATA_TUPLE_MAGIC_N 65478679
#endif /* UNIV_DEBUG */

  std::ostream &print(std::ostream &out) const {
    dtuple_print(out, this);
    return (out);
  }

  /* Read the trx id from the tuple (DB_TRX_ID)
  @return transaction id of the tuple. */
  trx_id_t get_trx_id() const;
};

/** A slot for a field in a big rec vector */
struct big_rec_field_t {
  /** Constructor.
  @param[in]	field_no_	the field number
  @param[in]	len_		the data length
  @param[in]	data_		the data */
  big_rec_field_t(ulint field_no_, ulint len_, void *data_)
      : field_no(field_no_),
        len(len_),
        data(data_),
        ext_in_old(false),
        ext_in_new(false) {}

  byte *ptr() const { return (static_cast<byte *>(data)); }

  ulint field_no; /*!< field number in record */
  ulint len;      /*!< stored data length, in bytes */
  void *data;     /*!< stored data */

  /** If true, this field was stored externally in the old row.
  If false, this field was stored inline in the old row.*/
  bool ext_in_old;

  /** If true, this field is stored externally in the new row.
  If false, this field is stored inline in the new row.*/
  bool ext_in_new;

  /** Print the big_rec_field_t object into the given output stream.
  @param[in]	out	the output stream.
  @return	the ouput stream. */
  std::ostream &print(std::ostream &out) const;
};

/** Overloading the global output operator to easily print the given
big_rec_field_t object into the given output stream.
@param[in]	out	the output stream
@param[in]	obj	the given object to print.
@return the output stream. */
inline std::ostream &operator<<(std::ostream &out, const big_rec_field_t &obj) {
  return (obj.print(out));
}

/** Storage format for overflow data in a big record, that is, a
clustered index record which needs external storage of data fields */
struct big_rec_t {
  mem_heap_t *heap;        /*!< memory heap from which
                           allocated */
  const ulint capacity;    /*!< fields array size */
  ulint n_fields;          /*!< number of stored fields */
  big_rec_field_t *fields; /*!< stored fields */

  /** Constructor.
  @param[in]	max	the capacity of the array of fields. */
  explicit big_rec_t(const ulint max)
      : heap(0), capacity(max), n_fields(0), fields(0) {}

  /** Append one big_rec_field_t object to the end of array of fields */
  void append(const big_rec_field_t &field) {
    ut_ad(n_fields < capacity);
    fields[n_fields] = field;
    n_fields++;
  }

  /** Allocate a big_rec_t object in the given memory heap, and for
  storing n_fld number of fields.
  @param[in]	heap	memory heap in which this object is allocated
  @param[in]	n_fld	maximum number of fields that can be stored in
                  this object
  @return the allocated object */
  static big_rec_t *alloc(mem_heap_t *heap, ulint n_fld);

  /** Print the current object into the given output stream.
  @param[in]	out	the output stream.
  @return	the ouput stream. */
  std::ostream &print(std::ostream &out) const;
};

/** Overloading the global output operator to easily print the given
big_rec_t object into the given output stream.
@param[in]	out	the output stream
@param[in]	obj	the given object to print.
@return the output stream. */
inline std::ostream &operator<<(std::ostream &out, const big_rec_t &obj) {
  return (obj.print(out));
}

#include "data0data.ic"

#endif
