#ifndef SQL_JOIN_CACHE_INCLUDED
#define SQL_JOIN_CACHE_INCLUDED

/* Copyright (c) 2000, 2019, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <string.h>
#include <sys/types.h>

#include "my_byteorder.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "mysql/service_mysql_alloc.h"
#include "sql/handler.h"
#include "sql/sql_executor.h"  // QEP_operation

class Field;
class Item;
class JOIN;

/**
  @file sql/sql_join_buffer.h
  Join buffer classes.
*/

/*
  Categories of data fields of variable length written into join cache buffers.
  The value of any of these fields is written into cache together with the
  prepended length of the value.
*/
#define CACHE_BLOB 1     /* blob field  */
#define CACHE_STRIPPED 2 /* field stripped of trailing spaces */
#define CACHE_VARSTR1 3  /* short string value (length takes 1 byte) */
#define CACHE_VARSTR2 4  /* long string value (length takes 2 bytes) */

/*
  The CACHE_FIELD structure used to describe fields of records that
  are written into a join cache buffer from record buffers and backward.
*/
struct CACHE_FIELD {
  uchar *str;  /**< buffer from/to where the field is to be copied */
  uint length; /**< maximal number of bytes to be copied from/to str */
  /*
    Field object for the moved field
    (0 - for a flag field, see JOIN_CACHE::create_flag_fields).
  */
  Field *field;
  uint type; /**< category of the of the copied field (CACHE_BLOB et al.) */
  /*
    The number of the record offset value for the field in the sequence
    of offsets placed after the last field of the record. These
    offset values are used to access fields referred to from other caches.
    If the value is 0 then no offset for the field is saved in the
    trailing sequence of offsets.
  */
  uint referenced_field_no;
  /// Used to chain rowid copy objects belonging to one join_tab
  CACHE_FIELD *next_copy_rowid;
  /* The remaining structure fields are used as containers for temp values */
  uint blob_length; /**< length of the blob to be copied */
  uint offset;      /**< field offset to be saved in cache buffer */

  void bind_buffer(uchar *buffer) {
    if (next_copy_rowid != NULL) next_copy_rowid->bind_buffer(buffer);
    str = buffer;
  }
  bool buffer_is_bound() const { return str != NULL; }
};

/**
  Filter the base columns of virtual generated columns if using a covering index
  scan.

  Adjust table->read_set so that it only contains the columns that are needed in
  the join operation and afterwards. A copy of the original read_set is stored
  in "saved_bitmaps".

  For a virtual generated column, all base columns are added to the read_set
  of the table. The storage engine will then copy all base column values so
  that the value of the GC can be calculated inside the executor.
  But when a virtual GC is fetched using a covering index, the actual GC
  value is fetched by the storage engine and the base column values are not
  needed. Join buffering code must not try to copy them (in
  create_remaining_fields()).
  So, we eliminate from read_set those columns that are available from the
  covering index.
*/
void filter_virtual_gcol_base_cols(
    const QEP_TAB *qep_tab, MEM_ROOT *mem_root,
    memroot_unordered_map<const QEP_TAB *, MY_BITMAP *> *saved_bitmaps);

/**
  Restore table->read_set to the value stored in "saved_bitmaps".
*/
void restore_virtual_gcol_base_cols(
    const QEP_TAB *qep_tab,
    memroot_unordered_map<const QEP_TAB *, MY_BITMAP *> *saved_bitmaps);

/**
  JOIN_CACHE is the base class to support the implementations of both
  Blocked-Based Nested Loops (BNL) Join Algorithm and Batched Key Access (BKA)
  Join Algorithm. The first algorithm is supported by the derived class
  JOIN_CACHE_BNL, while the second algorithm is supported by the derived
  class JOIN_CACHE_BKA.
  These two algorithms have a lot in common. Both algorithms first
  accumulate the records of the left join operand in a join buffer and
  then search for matching rows of the second operand for all accumulated
  records.
  For the first algorithm this strategy saves on logical I/O operations:
  the entire set of records from the join buffer requires only one look-through
  the records provided by the second operand.
  For the second algorithm the accumulation of records allows to optimize
  fetching rows of the second operand from disk for some engines (MyISAM,
  InnoDB), or to minimize the number of round-trips between the Server and
  the engine nodes (NDB Cluster).
*/

class JOIN_CACHE : public QEP_operation {
 private:
  /// Size of the offset of a record from the cache.
  uint size_of_rec_ofs;
  /// Size of the length of a record in the cache.
  uint size_of_rec_len;
  /// Size of the offset of a field within a record in the cache.
  uint size_of_fld_ofs;

  /**
     In init() there are several uses of TABLE::tmp_set, so one tmp_set isn't
     enough; this one is specific of generated column handling.
  */
  memroot_unordered_map<const QEP_TAB *, MY_BITMAP *> save_read_set_for_gcol;

 protected:
  /// @return the number of bytes used to store an offset value
  static uint offset_size(ulong len) {
    if (len <= 0xFFUL) return 1;
    if (len <= 0xFFFFUL) return 2;
    if (len <= 0xFFFFFFFFUL) return 4;
    return 8;
  }

  /**
    Decode an offset.
    @param ofs_sz   size of the encoded offset
    @param ptr      the encoded offset
    @return the decoded offset value
  */
  static ulong get_offset(uint ofs_sz, const uchar *ptr) {
    switch (ofs_sz) {
      case 1:
        return uint(*ptr);
      case 2:
        return uint2korr(ptr);
      case 4:
        return uint4korr(ptr);
      case 8:
        return static_cast<ulong>(uint8korr(ptr));
    }
    return 0;
  }

  /**
    Encode an offset.
    @param ofs_sz   size of the encoded offset
    @param ptr      the encoded offset
    @param ofs      the offset value to encode
  */
  static void store_offset(uint ofs_sz, uchar *ptr, ulong ofs) {
    switch (ofs_sz) {
      case 1:
        *ptr = (uchar)ofs;
        return;
      case 2:
        int2store(ptr, (uint16)ofs);
        return;
      case 4:
        int4store(ptr, (uint32)ofs);
        return;
      case 8:
        int8store(ptr, (uint64)ofs);
        return;
    }
  }

  /**
    The total maximum length of the fields stored for a record in the cache.
    For blob fields only the sizes of the blob lengths are taken into account.
  */
  uint length;

  /**
    Representation of the executed multi-way join through which all needed
    context can be accessed.
  */
  JOIN *join;

  /**
    Cardinality of the range of join tables whose fields can be put into the
    cache. (A table from the range not necessarily contributes to the cache.)
  */
  uint tables;

  /**
    The total number of flag and data fields that can appear in a record
    written into the cache. Fields with null values are always skipped
    to save space.
  */
  uint fields;

  /**
    The total number of flag fields in a record put into the cache. They are
    used for table null bitmaps, table null row flags, and an optional match
    flag. Flag fields go before other fields in a cache record with the match
    flag field placed always at the very beginning of the record.
  */
  uint flag_fields;

  /// The total number of blob fields that are written into the cache.
  uint blobs;

  /**
    The total number of fields referenced from field descriptors for other join
    caches. These fields are used to construct key values to access matching
    rows with index lookups. Currently the fields can be referenced only from
    descriptors for bka caches. However they may belong to a cache of any type.
  */
  uint referenced_fields;

  /**
    The current number of already created data field descriptors.
    This number can be useful for implementations of the init methods.
  */
  uint data_field_count;

  /**
    The current number of already created pointers to the data field
    descriptors. This number can be useful for implementations of
    the init methods.
  */
  uint data_field_ptr_count;
  /**
    Array of the descriptors of fields containing 'fields' elements.
    These are all fields that are stored for a record in the cache.
  */
  CACHE_FIELD *field_descr;

  /**
    Array of pointers to the blob descriptors that contains 'blobs' elements.
  */
  CACHE_FIELD **blob_ptr;

  /**
    This flag indicates that records written into the join buffer contain
    a match flag field. The flag must be set by the init method.
  */
  bool with_match_flag;
  /**
    This flag indicates that any record is prepended with the length of the
    record which allows us to skip the record or part of it without reading.
  */
  bool with_length;

  /**
    The maximum number of bytes used for a record representation in
    the cache excluding the space for blob data.
    For future derived classes this representation may contains some
    redundant info such as a key value associated with the record.
  */
  uint pack_length;
  /**
    The value of pack_length incremented by the total size of all
    pointers of a record in the cache to the blob data.
  */
  uint pack_length_with_blob_ptrs;

  /// Start of the join buffer.
  uchar *buff;
  /**
    Size of the entire memory allocated for the join buffer.

    Part of this memory may be reserved for an auxiliary buffer.
    @sa rem_space()
  */
  ulong buff_size;
#ifndef DBUG_OFF
  /// Whether the join buffer is read-only.
  bool m_read_only = false;
#endif

  /// The number of records put into the join buffer.
  uint records;

  /**
    Pointer to the current position in the join buffer.
    This member is used both when writing to buffer and
    when reading from it.
  */
  uchar *pos;
  /**
    Pointer to the first free position in the join buffer,
    right after the last record into it.
  */
  uchar *end_pos;

  /**
    Pointer to the beginning of first field of the current read/write record
    from the join buffer. The value is adjusted by the get_record/put_record
    functions.
  */
  uchar *curr_rec_pos;
  /**
    Pointer to the beginning of first field of the last record
    from the join buffer.
  */
  uchar *last_rec_pos;

  /**
    Flag is set if the blob data for the last record in the join buffer
    is in record buffers rather than in the join cache.
  */
  bool last_rec_blob_data_is_in_rec_buff;

  /**
    Pointer to the position to the current record link.
    Record links are used only with linked caches. Record links allow to set
    connections between parts of one join record that are stored in different
    join buffers.
    In the simplest case a record link is just a pointer to the beginning of
    the record stored in the buffer.
    In a more general case a link could be a reference to an array of pointers
    to records in the buffer.   */
  uchar *curr_rec_link;

  /// Cached value of calc_check_only_first_match(join_tab).
  bool check_only_first_match;

  void calc_record_fields();
  int alloc_fields(uint external_fields);
  void create_flag_fields();
  void create_remaining_fields(bool all_read_fields);
  void set_constants();
  bool alloc_buffer();

  uint get_size_of_rec_offset() const { return size_of_rec_ofs; }
  uint get_size_of_rec_length() const { return size_of_rec_len; }
  uint get_size_of_fld_offset() const { return size_of_fld_ofs; }

  uchar *get_rec_ref(uchar *ptr) {
    return buff + get_offset(size_of_rec_ofs, ptr - size_of_rec_ofs);
  }
  ulong get_rec_length(const uchar *ptr) {
    return get_offset(size_of_rec_len, ptr);
  }
  ulong get_fld_offset(const uchar *ptr) {
    return get_offset(size_of_fld_ofs, ptr);
  }

  void store_rec_ref(uchar *ptr, const uchar *ref) {
    store_offset(size_of_rec_ofs, ptr - size_of_rec_ofs, (ulong)(ref - buff));
  }

  void store_rec_length(uchar *ptr, ulong len) {
    store_offset(size_of_rec_len, ptr, len);
  }
  void store_fld_offset(uchar *ptr, ulong ofs) {
    store_offset(size_of_fld_ofs, ptr, ofs);
  }

  /// Write record fields and their required offsets into the join buffer.
  uint write_record_data(uchar *link, bool *is_full);

  /// Reserve some auxiliary space from the end of the join buffer.
  virtual void reserve_aux_buffer() {}

  /**
    @return the minimum size for the auxiliary buffer
    @retval 0 if no auxiliary buffer is needed
  */
  virtual uint aux_buffer_min_size() const { return 0; }

  /// @return how much space is remaining in the join buffer
  virtual ulong rem_space() const {
    DBUG_ASSERT(end_pos >= buff);
    DBUG_ASSERT(buff_size >= ulong(end_pos - buff));
    return ulong(buff_size - (end_pos - buff));
  }

  /**
    Skip record from the join buffer if it was flagged as matched.
    @return whether the record was skipped
  */
  virtual bool skip_record_if_match();

  /* Read some flag and data fields of a record from the join buffer */
  int read_some_record_fields();

  /* Read some flag fields of a record from the join buffer */
  void read_some_flag_fields();

  /* Read all flag fields of the record which is at position rec_ptr */
  void read_all_flag_fields_by_pos(uchar *rec_ptr);

  /* Read a data record field from the join buffer */
  uint read_record_field(CACHE_FIELD *copy, bool last_record);

  /* Read a referenced field from the join buffer */
  bool read_referenced_field(CACHE_FIELD *copy, uchar *rec_ptr, uint *len);

  /*
    True if rec_ptr points to the record whose blob data stay in
    record buffers
  */
  bool blob_data_is_in_rec_buff(uchar *rec_ptr) {
    return rec_ptr == last_rec_pos && last_rec_blob_data_is_in_rec_buff;
  }

  /**
    Find matches from the next table for records from the join buffer.
    @param skip_last  whether to ignore any matches for the
                      last partial join record
    @return nested loop state
   */
  virtual enum_nested_loop_state join_matching_records(bool skip_last) = 0;

  /* Add null complements for unmatched outer records from buffer */
  virtual enum_nested_loop_state join_null_complements(bool skip_last);

  /* Restore the fields of the last record from the join buffer */
  virtual void restore_last_record();

  /*Set match flag for a record in join buffer if it has not been set yet */
  bool set_match_flag_if_none(QEP_TAB *first_inner, uchar *rec_ptr);

  enum_nested_loop_state generate_full_extensions(uchar *rec_ptr);

  /**
    Check matching to a partial join record from the join buffer.

    @param rec_ptr pointer to the record from join buffer to check matching to
    @return whether there is a match

    @details
    The function checks whether the current record of 'join_tab' matches
    the partial join record from join buffer located at 'rec_ptr'. If this is
    the case and 'join_tab' is the last inner table of a semi-join or an outer
    join the function turns on the match flag for the 'rec_ptr' record unless
    it has been already set.

    @note
    Setting the match flag on can trigger re-evaluation of pushdown conditions
    for the record when join_tab is the last inner table of an outer join.
  */
  virtual bool check_match(uchar *rec_ptr);

  /** @returns whether we should check only the first match for this table */
  bool calc_check_only_first_match(const QEP_TAB *t) const;

  /**
    Add a record into the join buffer.
    @return whether it should be the last record in the buffer
  */
  virtual bool put_record_in_cache();

 public:
  /* Pointer to the previous join cache if there is any */
  JOIN_CACHE *prev_cache;
  /* Pointer to the next join cache if there is any */
  JOIN_CACHE *next_cache;

  // See if this operation can be replaced with a hash join. In order to be
  // replaced with hash join, the operation must be a block nested loop,
  // and the table must have at least one equi-join condition attached. If there
  // are other conditions that are not equi-join conditions, they will be
  // attached as filters after the hash join.
  virtual bool can_be_replaced_with_hash_join() const { return false; }

  /**
    Initialize the join cache.
    @retval 0 on success
    @retval 1 on failure
  */
  virtual int init() = 0;

  /// Reset the join buffer for reading/writing.
  virtual void reset_cache(bool for_writing);

  /// Add a record into join buffer and call join_records() if it's full.
  virtual enum_nested_loop_state put_record() {
    if (put_record_in_cache()) return join_records(false);
    return NESTED_LOOP_OK;
  }

  virtual bool get_record();

  void get_record_by_pos(uchar *rec_ptr);

  /**
    Read the match flag of a record.
    @param rec_ptr  record in the join buffer
    @return the match flag
   */
  bool get_match_flag_by_pos(uchar *rec_ptr);

  /// @return the position of the current record
  uchar *get_curr_rec() const { return curr_rec_pos; }

  /** Set the current record link. */
  void set_curr_rec_link(uchar *link) { curr_rec_link = link; }

  /// @return the current record link
  uchar *get_curr_rec_link() const {
    return (curr_rec_link ? curr_rec_link : get_curr_rec());
  }

  /// Join records from the join buffer with records from the next join table.
  enum_nested_loop_state end_send() { return join_records(false); }
  enum_nested_loop_state join_records(bool skip_last);

  enum_op_type type() { return OT_CACHE; }

  /**
    This constructor creates a join cache, linked or not. The cache is to be
    used to join table 'tab' to the result of joining the previous tables
    specified by the 'j' parameter. The parameter 'prev' specifies the previous
    cache object to which this cache is linked, or NULL if this cache is not
    linked.
  */
  JOIN_CACHE(JOIN *j, QEP_TAB *qep_tab_arg, JOIN_CACHE *prev)
      : QEP_operation(qep_tab_arg),
        save_read_set_for_gcol(*THR_MALLOC),
        join(j),
        buff(NULL),
        prev_cache(prev),
        next_cache(NULL) {
    if (prev_cache) prev_cache->next_cache = this;
  }
  virtual ~JOIN_CACHE() {}
  void mem_free() {
    /*
      JOIN_CACHE doesn't support unlinking cache chain. This code is needed
      only by set_join_cache_denial().
    */
    /*
      If there is a previous/next cache linked to this cache through the
      (next|prev)_cache pointer: remove the link.
    */
    if (prev_cache) prev_cache->next_cache = NULL;
    if (next_cache) next_cache->prev_cache = NULL;

    my_free(buff);
    buff = NULL;
  }

  /** Bits describing cache's type @sa setup_join_buffering() */
  enum enum_join_cache_type {
    ALG_NONE = 0,
    ALG_BNL = 1,
    ALG_BKA = 2,
    ALG_BKA_UNIQUE = 4
  };

  virtual enum_join_cache_type cache_type() const = 0;

  /* true <=> cache reads rows by key */
  bool is_key_access() const {
    return cache_type() & (ALG_BKA | ALG_BKA_UNIQUE);
  }

  friend class JOIN_CACHE_BNL;
  friend class JOIN_CACHE_BKA;
  friend class JOIN_CACHE_BKA_UNIQUE;
};

class JOIN_CACHE_BNL final : public JOIN_CACHE {
 protected:
  enum_nested_loop_state join_matching_records(bool skip_last) override;

 public:
  JOIN_CACHE_BNL(JOIN *j, QEP_TAB *qep_tab_arg, JOIN_CACHE *prev)
      : JOIN_CACHE(j, qep_tab_arg, prev), const_cond(NULL) {}

  int init() override;

  enum_join_cache_type cache_type() const override { return ALG_BNL; }

  bool can_be_replaced_with_hash_join() const override;

 private:
  Item *const_cond;
};

class JOIN_CACHE_BKA : public JOIN_CACHE {
 protected:
  /// Size of the MRR buffer at JOIN_CACHE::end_pos.
  ulong aux_buff_size;
  /// Flag to to be passed to the MRR interface.
  uint mrr_mode;

  /// MRR buffer associated with this join cache.
  HANDLER_BUFFER mrr_buff;

  /**
    Initialize the MRR buffer.

    After this point, JOIN_CACHE::write_record_data() must not be
    invoked on this object.
    rem_space() and aux_buff_size will ensure that
    there be at least some space available.

    @param end   end of the buffer

    @sa DsMrr_impl::dsmrr_init()
    @sa DsMrr_impl::dsmrr_next()
    @sa DsMrr_impl::dsmrr_fill_buffer()
  */
  void init_mrr_buff(uchar *end) {
    DBUG_ASSERT(end > end_pos);
    DBUG_ASSERT(!m_read_only);
#ifndef DBUG_OFF
    m_read_only = true;
#endif
    mrr_buff.buffer = end_pos;
    mrr_buff.buffer_end = end;
  }

  virtual void init_mrr_buff() { init_mrr_buff(buff + buff_size); }

  /**
    The number of the cache fields that are used in building keys to access
    the table join_tab
  */
  uint local_key_arg_fields;
  /**
    The total number of the fields in the previous caches that are used
    in building keys t access the table join_tab
  */
  uint external_key_arg_fields;

  /**
    Whether the key values will be read directly from the join
    buffer (whether we avoid building key values in the key buffer).
  */
  bool use_emb_key;
  /// The length of an embedded key value.
  uint emb_key_length;

  /// @return whether access keys can be read directly from the join buffer
  bool check_emb_key_usage();

  /// @return number of bytes to reserve for the MRR buffer for a record
  uint aux_buffer_incr();

  /// Reserve space for the MRR buffer for a record.
  void reserve_aux_buffer() override {
    if (auto rem = rem_space()) {
      auto incr = aux_buffer_incr();
      aux_buff_size += pack_length + incr < rem ? incr : rem;
    }
  }

  /// @return the minimum size for the MRR buffer
  uint aux_buffer_min_size() const override;

  enum_nested_loop_state join_matching_records(bool skip_last) override;

  /// Prepare to search for records that match records from the join buffer.
  bool init_join_matching_records(RANGE_SEQ_IF *seq_funcs, uint ranges);

 public:
  /// The MRR mode initially is set to 'flags'
  JOIN_CACHE_BKA(JOIN *j, QEP_TAB *qep_tab_arg, uint flags, JOIN_CACHE *prev)
      : JOIN_CACHE(j, qep_tab_arg, prev), mrr_mode(flags) {}

  int init() override;

  void reset_cache(bool for_writing) override {
    JOIN_CACHE::reset_cache(for_writing);
    aux_buff_size = 0;
  }

  /// @return how much space is remaining in the join buffer
  ulong rem_space() const override {
    const auto space = JOIN_CACHE::rem_space();
    DBUG_ASSERT(space >= aux_buff_size);
    return space - aux_buff_size;
  }

  /// @return false if a key was built successfully over the next record
  /// from the join buffer, true otherwise.
  virtual bool get_next_key(key_range *key);

  /// @return whether the record combination does not match the index condition
  bool skip_index_tuple(range_seq_t rseq, char *range_info);

  enum_join_cache_type cache_type() const override { return ALG_BKA; }
};

/*
  The class JOIN_CACHE_BKA_UNIQUE supports the variant of the BKA join algorithm
  that submits only distinct keys to the MRR interface. The records in the join
  buffer of a cache of this class that have the same access key are linked into
  a chain attached to a key entry structure that either itself contains the key
  value, or, in the case when the keys are embedded, refers to its occurance in
  one of the records from the chain.
  To build the chains with the same keys a hash table is employed. It is placed
  at the very end of the join buffer. The array of hash entries is allocated
  first at the very bottom of the join buffer, then go key entries. A hash entry
  contains a header of the list of the key entries with the same hash value.
  Each key entry is a structure of the following type:
    struct st_join_cache_key_entry {
      union {
        uchar[] value;
        cache_ref *value_ref; // offset from the beginning of the buffer
      } hash_table_key;
      key_ref next_key; // offset backward from the beginning of hash table
      cache_ref *last_rec // offset from the beginning of the buffer
    }
  The references linking the records in a chain are always placed at the very
  beginning of the record info stored in the join buffer. The records are
  linked in a circular list. A new record is always added to the end of this
  list. When a key is passed to the MRR interface it can be passed either with
  an association link containing a reference to the header of the record chain
  attached to the corresponding key entry in the hash table, or without any
  association link. When the next record is returned by a call to the MRR
  function multi_range_read_next without any association (because if was not
  passed  together with the key) then the key value is extracted from the
  returned record and searched for it in the hash table. If there is any records
  with such key the chain of them will be yielded as the result of this search.

  The following picture represents a typical layout for the info stored in the
  join buffer of a join cache object of the JOIN_CACHE_BKA_UNIQUE class.

  buff
  V
  +----------------------------------------------------------------------------+
  |     |[*]record_1_1|                                                        |
  |     ^ |                                                                    |
  |     | +--------------------------------------------------+                 |
  |     |                           |[*]record_2_1|          |                 |
  |     |                           ^ |                      V                 |
  |     |                           | +------------------+   |[*]record_1_2|   |
  |     |                           +--------------------+-+   |               |
  |+--+ +---------------------+                          | |   +-------------+ |
  ||  |                       |                          V |                 | |
  |||[*]record_3_1|         |[*]record_1_3|              |[*]record_2_2|     | |
  ||^                       ^                            ^                   | |
  ||+----------+            |                            |                   | |
  ||^          |            |<---------------------------+-------------------+ |
  |++          | | ... mrr  |   buffer ...           ... |     |               |
  |            |            |                            |                     |
  |      +-----+--------+   |                      +-----|-------+             |
  |      V     |        |   |                      V     |       |             |
  ||key_3|[/]|[*]|      |   |                |key_2|[/]|[*]|     |             |
  |                   +-+---|-----------------------+            |             |
  |                   V |   |                       |            |             |
  |             |key_1|[*]|[*]|         |   | ... |[*]|   ...  |[*]|  ...  |   |
  +----------------------------------------------------------------------------+
                                        ^           ^            ^
                                        |           i-th entry   j-th entry
                                        hash table

  i-th hash entry:
    circular record chain for key_1:
      record_1_1
      record_1_2
      record_1_3 (points to record_1_1)
    circular record chain for key_3:
      record_3_1 (points to itself)

  j-th hash entry:
    circular record chain for key_2:
      record_2_1
      record_2_2 (points to record_2_1)

*/

class JOIN_CACHE_BKA_UNIQUE final : public JOIN_CACHE_BKA {
 private:
  /// Size of the offset of a key entry in the hash table.
  uint size_of_key_ofs;

  /**
    Length of a key value.

    It is assumed that all key values have the same length.
  */
  uint key_length;
  /**
    Length of the key entry in the hash table.

    A key entry either contains the key value, or it contains a reference
    to the key value if use_emb_key flag is set for the cache.
  */
  uint key_entry_length;

  /// The beginning of the hash table in the join buffer.
  uchar *hash_table;
  /// Number of hash entries in the hash table.
  uint hash_entries;

  /// Number of key entries in the hash table (number of distinct keys).
  uint key_entries;

  /// The position of the last key entry in the hash table.
  uchar *last_key_entry;

  /// The position of the currently retrieved key entry in the hash table.
  uchar *curr_key_entry;

  /**
    The offset of the record fields from the beginning of the record
    representation. The record representation starts with a reference to
    the next record in the key record chain followed by the length of
    the trailing record data followed by a reference to the record segment
    in the previous cache, if any, followed by the record fields.
  */
  uint rec_fields_offset;
  /// The offset of the data fields from the beginning of the record fields.
  uint data_fields_offset;

  uint get_hash_idx(uchar *key, uint key_len);

  void cleanup_hash_table();

 protected:
  uint get_size_of_key_offset() { return size_of_key_ofs; }

  /**
    Get the position of the next_key_ptr field pointed to by
    a linking reference stored at the position key_ref_ptr.
    This reference is actually the offset backward from the
    beginning of hash table.
  */
  uchar *get_next_key_ref(uchar *key_ref_ptr) {
    return hash_table - get_offset(size_of_key_ofs, key_ref_ptr);
  }

  /**
    Store the linking reference to the next_key_ptr field at
    the position key_ref_ptr. The position of the next_key_ptr
    field is pointed to by ref. The stored reference is actually
    the offset backward from the beginning of the hash table.
  */
  void store_next_key_ref(uchar *key_ref_ptr, uchar *ref) {
    store_offset(size_of_key_ofs, key_ref_ptr, (ulong)(hash_table - ref));
  }

  /**
    Check whether the reference to the next_key_ptr field at the position
    key_ref_ptr contains  a nil value.
  */
  bool is_null_key_ref(uchar *key_ref_ptr) {
    ulong nil = 0;
    return memcmp(key_ref_ptr, &nil, size_of_key_ofs) == 0;
  }

  /**
    Set the reference to the next_key_ptr field at the position
    key_ref_ptr equal to nil.
  */
  void store_null_key_ref(uchar *key_ref_ptr) {
    ulong nil = 0;
    store_offset(size_of_key_ofs, key_ref_ptr, nil);
  }

  uchar *get_next_rec_ref(uchar *ref_ptr) {
    return buff + get_offset(get_size_of_rec_offset(), ref_ptr);
  }

  void store_next_rec_ref(uchar *ref_ptr, uchar *ref) {
    store_offset(get_size_of_rec_offset(), ref_ptr, (ulong)(ref - buff));
  }

  /**
    Get the position of the embedded key value for the current
    record pointed to by get_curr_rec().
  */
  uchar *get_curr_emb_key() const {
    return get_curr_rec() + data_fields_offset;
  }

  /**
    Get the position of the embedded key value.

    @param ref_ptr    reference to the key

    @return embedded key
  */
  uchar *get_emb_key(const uchar *ref_ptr) const {
    return buff + get_offset(get_size_of_rec_offset(), ref_ptr);
  }

  /**
    Store the reference to an embedded key at the position key_ref_ptr.

    @param[out] ref_ptr  stored reference (offset from JOIN_CACHE::buff)
    @param      ref      position of the embedded key
  */
  void store_emb_key_ref(uchar *ref_ptr, const uchar *ref) {
    DBUG_ASSERT(ref >= buff);
    store_offset(get_size_of_rec_offset(), ref_ptr, (ulong)(ref - buff));
  }

  /**
    @return how much space in the buffer would not be occupied by
    records, key entries and additional memory for the MRR buffer
  */
  ulong rem_space() const override {
    DBUG_ASSERT(last_key_entry >= end_pos);
    DBUG_ASSERT(buff_size >= aux_buff_size);
    DBUG_ASSERT(ulong(last_key_entry - end_pos) >= aux_buff_size);
    return ulong(last_key_entry - end_pos - aux_buff_size);
  }

  /**
    Initialize the MRR buffer allocating some space within the join buffer.
    The entire space between the last record put into the join buffer and the
    last key entry added to the hash table is used for the MRR buffer.
  */
  void init_mrr_buff() override {
    JOIN_CACHE_BKA::init_mrr_buff(last_key_entry);
  }

  bool skip_record_if_match() override;

  enum_nested_loop_state join_matching_records(bool skip_last) override;

  /**
    Search for a key in the hash table of the join buffer.

    @param key              pointer to the key value
    @param key_len          key value length
    @param[out] key_ref_ptr position of the reference to the next key from
                            the hash element for the found key, or a
                            position where the reference to the hash
                            element for the key is to be added in the
                            case when the key has not been found
    @details
    The function looks for a key in the hash table of the join buffer.
    If the key is found the functionreturns the position of the reference
    to the next key from  to the hash element for the given key.
    Otherwise the function returns the position where the reference to the
    newly created hash element for the given key is to be added.

    @return whether the key is found in the hash table
  */
  bool key_search(uchar *key, uint key_len, uchar **key_ref_ptr);

  bool check_match(uchar *rec_ptr) override;

  bool put_record_in_cache() override;

 public:
  JOIN_CACHE_BKA_UNIQUE(JOIN *j, QEP_TAB *qep_tab_arg, uint flags,
                        JOIN_CACHE *prev)
      : JOIN_CACHE_BKA(j, qep_tab_arg, flags, prev) {}

  int init() override;

  void reset_cache(bool for_writing) override;

  bool get_record() override;

  /**
    Check whether all records in a key chain are flagged as matches.

    @param key_chain_ptr key chain
    @return whether each record in the key chain has been flagged as a match
  */
  bool check_all_match_flags_for_key(uchar *key_chain_ptr);

  bool get_next_key(key_range *key) override;

  /// @return the head of the record chain attached to the current key entry
  uchar *get_curr_key_chain() {
    return get_next_rec_ref(curr_key_entry + key_entry_length -
                            get_size_of_rec_offset());
  }

  /// @return whether the record combination does not match the index condition
  bool skip_index_tuple(range_seq_t rseq, char *range_info);

  enum_join_cache_type cache_type() const override { return ALG_BKA_UNIQUE; }
};

#endif /* SQL_JOIN_CACHE_INCLUDED */
