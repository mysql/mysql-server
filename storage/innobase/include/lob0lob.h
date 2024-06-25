/*****************************************************************************

Copyright (c) 2015, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/
#ifndef lob0lob_h
#define lob0lob_h

#include <my_dbug.h>
#include "btr0pcur.h"
#include "dict0mem.h"
#include "page0page.h"
#include "row0log.h"
#include "univ.i"

/* Uncomment the following line to generate debug data, useful to analyze
LOB issues. */
/* #define LOB_DEBUG */
/* #define ZLOB_DEBUG */

struct upd_t;
typedef std::map<page_no_t, buf_block_t *> BlockCache;

/**
@file
@brief Implements the large objects (LOB) module.

InnoDB supports large objects (LOB).  Previously, the LOB was called as
externally stored fields. A large object contains a singly linked list of
database pages, aka LOB pages.  A reference to the first LOB page is stored
along with the clustered index record.  This reference is called the LOB
reference (lob::ref_t). A single clustered index record can have many LOB
references.  Secondary indexes cannot have LOB references.

There are two types of LOB - compressed and uncompressed.

The main operations implemented for LOB are - INSERT, DELETE and FETCH.  To
carry out these main operations the following classes are provided.

Inserter     - for inserting uncompressed LOB data.
zInserter    - for inserting compressed LOB data.
BaseInserter - a base class containing common state and functions useful for
               both Inserter and zInserter.  Inserter and zInserter derives
               from this base class.
Reader       - for reading uncompressed LOB data.
zReader      - for reading compressed LOB data.
Deleter      - for deleting both compressed and uncompressed LOB data.

For each main operation, the context information is identified separately.
They are as follows:

InsertContext - context information for doing insert of LOB. `
DeleteContext - context information for doing delete of LOB. `
ReadContext   - context information for doing fetch of LOB. `

*/

/** Provides the large objects (LOB) module.  Previously, the LOB was called as
externally stored fields. */
namespace lob {

/** The maximum size possible for an LOB */
const ulint MAX_SIZE = UINT32_MAX;

/** The compressed LOB is stored as a collection of zlib streams.  The
uncompressed LOB is divided into chunks of size Z_CHUNK_SIZE and each of
these chunks are compressed individually and stored as compressed LOB.
data. */
constexpr uint32_t KB128 = 128 * 1024;
constexpr uint32_t Z_CHUNK_SIZE = KB128;

/** The reference in a field for which data is stored on a different page.
The reference is at the end of the 'locally' stored part of the field.
'Locally' means storage in the index record.
We store locally a long enough prefix of each column so that we can determine
the ordering parts of each index record without looking into the externally
stored part. */
/*-------------------------------------- @{ */

/** Space identifier where stored. */
const ulint BTR_EXTERN_SPACE_ID = 0;

/** page number where stored */
const ulint BTR_EXTERN_PAGE_NO = 4;

/** offset of BLOB header on that page */
const ulint BTR_EXTERN_OFFSET = 8;

/** Version number of LOB (LOB in new format)*/
const ulint BTR_EXTERN_VERSION = BTR_EXTERN_OFFSET;

/** 8 bytes containing the length of the externally stored part of the LOB.
The 2 highest bits are reserved to the flags below. */
const ulint BTR_EXTERN_LEN = 12;

/*-------------------------------------- @} */

/** The most significant bit of BTR_EXTERN_LEN (i.e., the most
significant bit of the byte at smallest address) is set to 1 if this
field does not 'own' the externally stored field; only the owner field
is allowed to free the field in purge! */
const ulint BTR_EXTERN_OWNER_FLAG = 128UL;

/** If the second most significant bit of BTR_EXTERN_LEN (i.e., the
second most significant bit of the byte at smallest address) is 1 then
it means that the externally stored field was inherited from an
earlier version of the row.  In rollback we are not allowed to free an
inherited external field. */
const ulint BTR_EXTERN_INHERITED_FLAG = 64UL;

/** If the 3rd most significant bit of BTR_EXTERN_LEN is 1, then it
means that the externally stored field is currently being modified.
This is mainly used by the READ UNCOMMITTED transaction to avoid returning
inconsistent blob data. */
const ulint BTR_EXTERN_BEING_MODIFIED_FLAG = 32UL;

/** The structure of uncompressed LOB page header */

/** Offset within header of LOB length on this page. */
const ulint LOB_HDR_PART_LEN = 0;

/** Offset within header of next BLOB part page no.
FIL_NULL if none */
const ulint LOB_HDR_NEXT_PAGE_NO = 4;

/** Size of an uncompressed LOB page header, in bytes */
const ulint LOB_HDR_SIZE = 8;

/** Start of the data on an LOB page */
const uint ZLOB_PAGE_DATA = FIL_PAGE_DATA;

/** In memory representation of the LOB reference. */
struct ref_mem_t {
  /** Space Identifier of the clustered index. */
  space_id_t m_space_id;

  /** Page number of first LOB page. */
  page_no_t m_page_no;

  /** Offset within m_page_no where LOB begins. */
  ulint m_offset;

  /** Length of LOB */
  ulint m_length;

  /** Whether the LOB is null. */
  bool m_null;

  /** Whether the clustered index record owns this LOB. */
  bool m_owner;

  /** Whether the clustered index record inherited this LOB from
  another clustered index record. */
  bool m_inherit;

  /** Whether the LOB is partially updated. */
  bool m_partial;

  /** Whether the blob is being modified. */
  bool m_being_modified;

  /** Check if the LOB has already been purged.
  @return true if LOB has been purged, false otherwise. */
  bool is_purged() const {
    return ((m_page_no == FIL_NULL) && (m_length == 0));
  }
};

extern const byte field_ref_almost_zero[FIELD_REF_SIZE];

/** The struct 'lob::ref_t' represents an external field reference. The
reference in a field for which data is stored on a different page.  The
reference is at the end of the 'locally' stored part of the field.  'Locally'
means storage in the index record. We store locally a long enough prefix of
each column so that we can determine the ordering parts of each index record
without looking into the externally stored part. */
struct ref_t {
 private:
  /** If the LOB size is equal to or above this limit (in physical page
  size terms), then the LOB is big enough to be partially updated.  Only
  in this case LOB index needs to be built. */
  static const ulint LOB_BIG_THRESHOLD_SIZE = 2;

 public:
  /** If the total number of bytes modified in an LOB, in an update
  operation, is less than or equal to this threshold LOB_SMALL_CHANGE_THRESHOLD,
  then it is considered as a small change.  For small changes to LOB,
  the changes are undo logged like any other update operation. */
  static const ulint LOB_SMALL_CHANGE_THRESHOLD = 100;

  /** Constructor.
  @param[in]    ptr     Pointer to the external field reference. */
  explicit ref_t(byte *ptr) : m_ref(ptr) {}

  /** For compressed LOB, if the length is less than or equal to Z_CHUNK_SIZE
  then use the older single z stream format to store the LOB.  */
  bool use_single_z_stream() const { return (length() <= Z_CHUNK_SIZE); }

  /** For compressed LOB, if the length is less than or equal to Z_CHUNK_SIZE
  then use the older single z stream format to store the LOB.  */
  static bool use_single_z_stream(ulint len) { return (len <= Z_CHUNK_SIZE); }

  /** Check if this LOB is big enough to do partial update.
  @param[in]    page_size       the page size
  @param[in]    lob_length      the size of BLOB in bytes.
  @return true if LOB is big enough, false otherwise. */
  static bool is_big(const page_size_t &page_size [[maybe_unused]],
                     const ulint lob_length [[maybe_unused]]) {
    /* Disable a performance optimization */
    return true;
  }

  /** Check if this LOB is big enough to do partial update.
  @param[in]    page_size       the page size
  @return true if LOB is big enough, false otherwise. */
  bool is_big(const page_size_t &page_size [[maybe_unused]]) const {
    /* Disable a performance optimization */
    return true;
  }

  /** Parse the LOB reference object and copy data into the given
  ref_mem_t object.
  @param[out]   obj     LOB reference memory object. */
  void parse(ref_mem_t &obj) const {
    obj.m_space_id = space_id();
    obj.m_page_no = page_no();
    obj.m_offset = offset();
    obj.m_length = length();
    obj.m_null = is_null();
    obj.m_owner = is_owner();
    obj.m_inherit = is_inherited();
    obj.m_being_modified = is_being_modified();
  }

  /** Copy the LOB reference into the given memory location.
  @param[out]   field_ref       write LOB reference in this
                                  location.*/
  void copy(byte *field_ref) const { memcpy(field_ref, m_ref, SIZE); }

  /** Check whether the stored external field reference is equal to the
  given field reference.
  @param[in]    ptr     supplied external field reference. */
  bool is_equal(const byte *ptr) const { return (m_ref == ptr); }

  /** Set the external field reference to the given memory location.
  @param[in]    ptr     the new external field reference. */
  void set_ref(byte *ptr) { m_ref = ptr; }

  /** Check if the field reference is made of zeroes except the being_modified
  bit.
  @return true if field reference is made of zeroes, false otherwise. */
  bool is_null_relaxed() const {
    return (is_null() || memcmp(field_ref_almost_zero, m_ref, SIZE) == 0);
  }

  /** Check if the field reference is made of zeroes.
  @return true if field reference is made of zeroes, false otherwise. */
  bool is_null() const { return (memcmp(field_ref_zero, m_ref, SIZE) == 0); }

  /** Initialize the reference object with zeroes. */
  void init() { memset(m_ref, 0, SIZE); }

#ifdef UNIV_DEBUG
  /** Check if the LOB reference is null (all zeroes) except the "is being
  modified" bit.
  @param[in]    ref   the LOB reference.
  @return true if the LOB reference is null (all zeros) except the "is being
  modified" bit, false otherwise. */
  static bool is_null_relaxed(const byte *ref) {
    return (is_null(ref) || memcmp(field_ref_almost_zero, ref, SIZE) == 0);
  }

  /** Check if the LOB reference is null (all zeroes).
  @param[in]    ref   the LOB reference.
  @return true if the LOB reference is null (all zeros), false otherwise. */
  static bool is_null(const byte *ref) {
    return (memcmp(field_ref_zero, ref, SIZE) == 0);
  }
#endif /* UNIV_DEBUG */

  /** Set the ownership flag in the blob reference.
  @param[in]  owner Whether to own or disown. If owner, unset
                    the owner flag.
  @param[in]  mtr   Mini-transaction or NULL.*/
  void set_owner(bool owner, mtr_t *mtr) {
    ulint byte_val = mach_read_from_1(m_ref + BTR_EXTERN_LEN);

    if (owner) {
      /* owns the blob */
      byte_val &= ~BTR_EXTERN_OWNER_FLAG;
    } else {
      byte_val |= BTR_EXTERN_OWNER_FLAG;
    }

    mlog_write_ulint(m_ref + BTR_EXTERN_LEN, byte_val, MLOG_1BYTE, mtr);
  }

  /** Set the ownership flag in the blob reference.
  @param[in]  owner Whether to own or disown. If owner, unset
                    the owner flag. */
  void set_owner(bool owner) {
    ulint byte_val = mach_read_from_1(m_ref + BTR_EXTERN_LEN);

    if (owner) {
      /* owns the blob */
      byte_val &= ~BTR_EXTERN_OWNER_FLAG;
    } else {
      byte_val |= BTR_EXTERN_OWNER_FLAG;
    }

    mach_write_ulint(m_ref + BTR_EXTERN_LEN, byte_val, MLOG_1BYTE);
  }

  /** Set the being_modified flag in the field reference.
  @param[in,out]  ref       The LOB reference
  @param[in]      modifying true, if blob is being modified.
  @param[in]      mtr       Mini-transaction context.*/
  static void set_being_modified(byte *ref, bool modifying, mtr_t *mtr) {
    ulint byte_val = mach_read_from_1(ref + BTR_EXTERN_LEN);

    if (modifying) {
      byte_val |= BTR_EXTERN_BEING_MODIFIED_FLAG;
    } else {
      byte_val &= ~BTR_EXTERN_BEING_MODIFIED_FLAG;
    }

    mlog_write_ulint(ref + BTR_EXTERN_LEN, byte_val, MLOG_1BYTE, mtr);
  }

  /** Set the being_modified flag in the field reference.
  @param[in]  modifying true, if blob is being modified.
  @param[in]  mtr       Mini-transaction context.*/
  void set_being_modified(bool modifying, mtr_t *mtr) {
    set_being_modified(m_ref, modifying, mtr);
  }

  /** Check if the current blob is being modified
  @param[in]    field_ref       blob field reference
  @return true if it is being modified, false otherwise. */
  bool static is_being_modified(const byte *field_ref) {
    const ulint byte_val = mach_read_from_1(field_ref + BTR_EXTERN_LEN);
    return (byte_val & BTR_EXTERN_BEING_MODIFIED_FLAG);
  }

  /** Check if the current blob is being modified
  @return true if it is being modified, false otherwise. */
  bool is_being_modified() const { return (is_being_modified(m_ref)); }

  /** Set the inherited flag in the field reference.
  @param[in]  inherited true, if inherited.
  @param[in]  mtr       Mini-transaction context.*/
  void set_inherited(bool inherited, mtr_t *mtr) {
    ulint byte_val = mach_read_from_1(m_ref + BTR_EXTERN_LEN);

    if (inherited) {
      byte_val |= BTR_EXTERN_INHERITED_FLAG;
    } else {
      byte_val &= ~BTR_EXTERN_INHERITED_FLAG;
    }

    mlog_write_ulint(m_ref + BTR_EXTERN_LEN, byte_val, MLOG_1BYTE, mtr);
  }

  /** Check if the current row is the owner of the blob.
  @return true if owner, false otherwise. */
  bool is_owner() const {
    ulint byte_val = mach_read_from_1(m_ref + BTR_EXTERN_LEN);
    return (!(byte_val & BTR_EXTERN_OWNER_FLAG));
  }

  /** Check if the current row inherited the blob from parent row.
  @return true if inherited, false otherwise. */
  bool is_inherited() const {
    const ulint byte_val = mach_read_from_1(m_ref + BTR_EXTERN_LEN);
    return (byte_val & BTR_EXTERN_INHERITED_FLAG);
  }

#ifdef UNIV_DEBUG
  /** Read the space id from the given blob reference.
  @param[in]   ref   the blob reference.
  @return the space id */
  static space_id_t space_id(const byte *ref) {
    return (mach_read_from_4(ref));
  }

  /** Read the page no from the blob reference.
  @return the page no */
  static page_no_t page_no(const byte *ref) {
    return (mach_read_from_4(ref + BTR_EXTERN_PAGE_NO));
  }
#endif /* UNIV_DEBUG */

  /** Read the space id from the blob reference.
  @return the space id */
  space_id_t space_id() const { return (mach_read_from_4(m_ref)); }

  /** Read the page number from the blob reference.
  @return the page number */
  page_no_t page_no() const {
    return (mach_read_from_4(m_ref + BTR_EXTERN_PAGE_NO));
  }

  /** Read the offset of blob header from the blob reference.
  @return the offset of the blob header */
  ulint offset() const { return (mach_read_from_4(m_ref + BTR_EXTERN_OFFSET)); }

  /** Read the LOB version from the blob reference.
  @return the LOB version number. */
  uint32_t version() const {
    return (mach_read_from_4(m_ref + BTR_EXTERN_VERSION));
  }

  /** Read the length from the blob reference.
  @return length of the blob */
  ulint length() const {
    return (mach_read_from_4(m_ref + BTR_EXTERN_LEN + 4));
  }

  /** Update the information stored in the external field reference.
  @param[in]    space_id        the space identifier.
  @param[in]    page_no         the page number.
  @param[in]    offset          the offset within the page_no
  @param[in]    mtr             the mini trx or NULL. */
  void update(space_id_t space_id, ulint page_no, ulint offset, mtr_t *mtr) {
    set_space_id(space_id, mtr);
    set_page_no(page_no, mtr);
    set_offset(offset, mtr);
  }

  /** Set the space_id in the external field reference.
  @param[in]    space_id        the space identifier.
  @param[in]    mtr             mini-trx or NULL. */
  void set_space_id(const space_id_t space_id, mtr_t *mtr) {
    mlog_write_ulint(m_ref + BTR_EXTERN_SPACE_ID, space_id, MLOG_4BYTES, mtr);
  }

  /** Set the space_id in the external field reference.
  @param[in]    space_id        the space identifier. */
  void set_space_id(const space_id_t space_id) {
    mach_write_ulint(m_ref + BTR_EXTERN_SPACE_ID, space_id, MLOG_4BYTES);
  }

  /** Set the page number in the external field reference.
  @param[in]    page_no the page number.
  @param[in]    mtr     mini-trx or NULL. */
  void set_page_no(const ulint page_no, mtr_t *mtr) {
    mlog_write_ulint(m_ref + BTR_EXTERN_PAGE_NO, page_no, MLOG_4BYTES, mtr);
  }

  /** Set the page number in the external field reference.
  @param[in]    page_no the page number. */
  void set_page_no(const ulint page_no) {
    mach_write_ulint(m_ref + BTR_EXTERN_PAGE_NO, page_no, MLOG_4BYTES);
  }

  /** Set the offset information in the external field reference.
  @param[in]    offset  the offset.
  @param[in]    mtr     mini-trx or NULL. */
  void set_offset(const ulint offset, mtr_t *mtr) {
    mlog_write_ulint(m_ref + BTR_EXTERN_OFFSET, offset, MLOG_4BYTES, mtr);
  }

  /** Set the version information in the external field reference.
  @param[in]    version  the lob version. */
  void set_version(const ulint version) {
    mach_write_ulint(m_ref + BTR_EXTERN_VERSION, version, MLOG_4BYTES);
  }

  /** Set the length of blob in the external field reference.
  @param[in]    len     the blob length .
  @param[in]    mtr     mini-trx or NULL. */
  void set_length(const ulint len, mtr_t *mtr) {
    ut_ad(len <= MAX_SIZE);
    mlog_write_ulint(m_ref + BTR_EXTERN_LEN + 4, len, MLOG_4BYTES, mtr);
  }

  /** Set the length of blob in the external field reference.
  @param[in]    len     the blob length . */
  void set_length(const ulint len) {
    ut_ad(len <= MAX_SIZE);
    mach_write_ulint(m_ref + BTR_EXTERN_LEN + 4, len, MLOG_4BYTES);
  }

  /** Get the start of a page containing this blob reference.
  @return start of the page */
  page_t *page_align() const { return (::page_align(m_ref)); }

#ifdef UNIV_DEBUG
  /** Check if the given mtr has necessary latches to update this LOB
  reference.
  @param[in]  mtr Mini-transaction that needs to
                  be checked.
  @return true if valid, false otherwise. */
  bool validate(mtr_t *mtr) {
    ut_ad(m_ref != nullptr);
    ut_ad(mtr != nullptr);

    if (mtr->get_log_mode() == MTR_LOG_NO_REDO) {
      return (true);
    }

    buf_block_t *block = mtr->memo_contains_page_flagged(
        m_ref, MTR_MEMO_PAGE_X_FIX | MTR_MEMO_PAGE_SX_FIX);
    ut_ad(block != nullptr);
    return (true);
  }

  /** Check if the space_id in the LOB reference is equal to the
  space_id of the index to which it belongs.
  @param[in]  index  the index to which LOB belongs.
  @return true if space is valid in LOB reference, false otherwise. */
  bool check_space_id(dict_index_t *index) const;
#endif /* UNIV_DEBUG */

  /** Check if the LOB can be partially updated. This is done by loading
  the first page of LOB and looking at the flags.
  @param[in]    index   the index to which LOB belongs.
  @return true if LOB is partially updatable, false otherwise.*/
  bool is_lob_partially_updatable(const dict_index_t *index) const;

  /** Load the first page of the LOB and mark it as not partially
  updatable anymore.
  @param[in]  trx       Current transaction
  @param[in]  mtr       Mini-transaction context.
  @param[in]  index     Index dictionary object.
  @param[in]  page_size Page size information. */
  void mark_not_partially_updatable(trx_t *trx, mtr_t *mtr, dict_index_t *index,
                                    const page_size_t &page_size);

  /** Load the first page of LOB and read its page type.
  @param[in]    index                   the index object.
  @param[in]    page_size               the page size of LOB.
  @param[out]   is_partially_updatable  is the LOB partially updatable.
  @return the page type of first page of LOB.*/
  ulint get_lob_page_info(const dict_index_t *index,
                          const page_size_t &page_size,
                          bool &is_partially_updatable) const;

  /** Print this LOB reference into the given output stream.
  @param[in]    out     the output stream.
  @return the output stream. */
  std::ostream &print(std::ostream &out) const;

  /** The size of an LOB reference object (in bytes) */
  static const uint SIZE = BTR_EXTERN_FIELD_REF_SIZE;

 private:
  /** Pointing to a memory of size BTR_EXTERN_FIELD_REF_SIZE */
  byte *m_ref;
};

/** Overload the global output stream operator to easily print the
lob::ref_t object into the output stream.
@param[in,out]  out             the output stream.
@param[in]      obj             the lob::ref_t object to be printed
@return the output stream. */
inline std::ostream &operator<<(std::ostream &out, const ref_t &obj) {
  return (obj.print(out));
}

/** LOB operation code for btr_store_big_rec_extern_fields(). */
enum opcode {

  /** Store off-page columns for a freshly inserted record */
  OPCODE_INSERT = 0,

  /** Store off-page columns for an insert by update */
  OPCODE_INSERT_UPDATE,

  /** Store off-page columns for an update */
  OPCODE_UPDATE,

  /** Store off-page columns for a freshly inserted record by bulk */
  OPCODE_INSERT_BULK,

  /** The operation code is unknown or not important. */
  OPCODE_UNKNOWN
};

/** Stores the fields in big_rec_vec to the tablespace and puts pointers to
them in rec.  The extern flags in rec will have to be set beforehand. The
fields are stored on pages allocated from leaf node file segment of the index
tree.

TODO: If the allocation extends the tablespace, it will not be redo logged, in
any mini-transaction.  Tablespace extension should be redo-logged, so that
recovery will not fail when the big_rec was written to the extended portion of
the file, in case the file was somehow truncated in the crash.
@param[in]      trx             the trx doing LOB store. If unavailable it
                                could be nullptr.
@param[in,out]  pcur            a persistent cursor. if btr_mtr is restarted,
                                then this can be repositioned.
@param[in]      upd             update vector
@param[in,out]  offsets         rec_get_offsets() on pcur. the "external in
                                offsets will correctly correspond storage"
                                flagsin offsets will correctly correspond to
                                rec when this function returns
@param[in]      big_rec_vec     vector containing fields to be stored
                                externally
@param[in,out]  btr_mtr         mtr containing the latches to the clustered
                                index. can be committed and restarted.
@param[in]      op              operation code
@return DB_SUCCESS or DB_OUT_OF_FILE_SPACE */
[[nodiscard]] dberr_t btr_store_big_rec_extern_fields(
    trx_t *trx, btr_pcur_t *pcur, const upd_t *upd, ulint *offsets,
    const big_rec_t *big_rec_vec, mtr_t *btr_mtr, opcode op);

/** Copies an externally stored field of a record to mem heap.
@param[in]      trx             the current transaction.
@param[in]      index           the clustered index
@param[in]      rec             record in a clustered index; must be
                                protected by a lock or a page latch
@param[in]      offsets         array returned by rec_get_offsets()
@param[in]      page_size       BLOB page size
@param[in]      no              field number
@param[out]     len             length of the field
@param[out]     lob_version     version of lob that has been copied
@param[in]      is_sdi          true for SDI Indexes
@param[in,out]  heap            mem heap
@param[in]      is_rebuilt      true if rebuilt
@return the field copied to heap, or NULL if the field is incomplete */
byte *btr_rec_copy_externally_stored_field_func(
    trx_t *trx, const dict_index_t *index, const rec_t *rec,
    const ulint *offsets, const page_size_t &page_size, ulint no, ulint *len,
    size_t *lob_version, IF_DEBUG(bool is_sdi, ) mem_heap_t *heap,
    bool is_rebuilt);

static inline byte *btr_rec_copy_externally_stored_field(
    trx_t *trx, const dict_index_t *index, const rec_t *rec,
    const ulint *offsets, const page_size_t &page_size, ulint no, ulint *len,
    size_t *ver, bool is_sdi [[maybe_unused]], mem_heap_t *heap) {
  return btr_rec_copy_externally_stored_field_func(
      trx, index, rec, offsets, page_size, no, len, ver,
      IF_DEBUG(is_sdi, ) heap, false);
}

/** Gets the offset of the pointer to the externally stored part of a field.
@param[in]      index           Index
@param[in]      offsets         array returned by rec_get_offsets()
@param[in]      n               index of the external field
@return offset of the pointer to the externally stored part */
ulint btr_rec_get_field_ref_offs(const dict_index_t *index,
                                 const ulint *offsets, ulint n);

/** Gets a pointer to the externally stored part of a field.
@param[in] index index
@param rec record
@param offsets rec_get_offsets(rec)
@param n index of the externally stored field
@return pointer to the externally stored part */
static inline const byte *btr_rec_get_field_ref(const dict_index_t *index,
                                                const byte *rec,
                                                const ulint *offsets, ulint n) {
  return rec + lob::btr_rec_get_field_ref_offs(index, offsets, n);
}

/** Gets a pointer to the externally stored part of a field.
@param index record descriptor
@param rec record
@param offsets rec_get_offsets(rec)
@param n index of the externally stored field
@return pointer to the externally stored part */
static inline byte *btr_rec_get_field_ref(const dict_index_t *index, byte *rec,
                                          const ulint *offsets, ulint n) {
  return rec + lob::btr_rec_get_field_ref_offs(index, offsets, n);
}

/** Deallocate a buffer block that was reserved for a BLOB part.
@param[in]      index   Index
@param[in]      block   Buffer block
@param[in]      all     true=remove also the compressed page
                        if there is one
@param[in]      mtr     Mini-transaction to commit */
void blob_free(dict_index_t *index, buf_block_t *block, bool all, mtr_t *mtr);

/** The B-tree context under which the LOB operation is done. */
class BtrContext {
 public:
  /** Default Constructor */
  BtrContext()
      : m_mtr(nullptr),
        m_pcur(nullptr),
        m_index(nullptr),
        m_rec(nullptr),
        m_offsets(nullptr),
        m_block(nullptr),
        m_op(OPCODE_UNKNOWN),
        m_btr_page_no(FIL_NULL) {}

  /** Constructor **/
  BtrContext(mtr_t *mtr, dict_index_t *index, buf_block_t *block)
      : m_mtr(mtr),
        m_pcur(nullptr),
        m_index(index),
        m_rec(nullptr),
        m_offsets(nullptr),
        m_block(block),
        m_op(OPCODE_UNKNOWN),
        m_btr_page_no(FIL_NULL) {}

  /** Constructor **/
  BtrContext(mtr_t *mtr, btr_pcur_t *pcur, dict_index_t *index, rec_t *rec,
             ulint *offsets, buf_block_t *block)
      : m_mtr(mtr),
        m_pcur(pcur),
        m_index(index),
        m_rec(rec),
        m_offsets(offsets),
        m_block(block),
        m_op(OPCODE_UNKNOWN),
        m_btr_page_no(FIL_NULL) {
    ut_ad(m_pcur == nullptr || rec_offs_validate());
    ut_ad(m_block == nullptr || m_rec == nullptr ||
          m_block->frame == page_align(m_rec));
    ut_ad(m_pcur == nullptr || m_rec == m_pcur->get_rec());
  }

  /** Constructor **/
  BtrContext(mtr_t *mtr, btr_pcur_t *pcur, dict_index_t *index, rec_t *rec,
             ulint *offsets, buf_block_t *block, opcode op)
      : m_mtr(mtr),
        m_pcur(pcur),
        m_index(index),
        m_rec(rec),
        m_offsets(offsets),
        m_block(block),
        m_op(op),
        m_btr_page_no(FIL_NULL) {
    ut_ad(m_pcur == nullptr || rec_offs_validate());
    ut_ad(m_block->frame == page_align(m_rec));
    ut_ad(m_pcur == nullptr || m_rec == m_pcur->get_rec());
  }

  /** Copy Constructor **/
  BtrContext(const BtrContext &other)
      : m_mtr(other.m_mtr),
        m_pcur(other.m_pcur),
        m_index(other.m_index),
        m_rec(other.m_rec),
        m_offsets(other.m_offsets),
        m_block(other.m_block),
        m_op(other.m_op),
        m_btr_page_no(other.m_btr_page_no) {}

  /** Marks non-updated off-page fields as disowned by this record.
  The ownership must be transferred to the updated record which is
  inserted elsewhere in the index tree. In purge only the owner of
  externally stored field is allowed to free the field.
  @param[in]    update          update vector. */
  void disown_inherited_fields(const upd_t *update);

  /** Sets the ownership bit of an externally stored field in a record.
  @param[in]            i               field number
  @param[in]            val             value to set */
  void set_ownership_of_extern_field(ulint i, bool val) {
    byte *data;
    ulint local_len;

    data = rec_get_nth_field(m_index, m_rec, m_offsets, i, &local_len);
    ut_ad(rec_offs_nth_extern(m_index, m_offsets, i));
    ut_a(local_len >= BTR_EXTERN_FIELD_REF_SIZE);

    local_len -= BTR_EXTERN_FIELD_REF_SIZE;

    ref_t ref(data + local_len);

    ut_a(val || ref.is_owner());

    page_zip_des_t *page_zip = get_page_zip();

    if (page_zip) {
      ref.set_owner(val, nullptr);
      page_zip_write_blob_ptr(page_zip, m_rec, m_index, m_offsets, i, m_mtr);
    } else {
      ref.set_owner(val, m_mtr);
    }
  }

  /** Marks all extern fields in a record as owned by the record.
  This function should be called if the delete mark of a record is
  removed: a not delete marked record always owns all its extern
  fields.*/
  void unmark_extern_fields() {
    ut_ad(!rec_offs_comp(m_offsets) || !rec_get_node_ptr_flag(m_rec));

    ulint n = rec_offs_n_fields(m_offsets);

    if (!rec_offs_any_extern(m_offsets)) {
      return;
    }

    for (ulint i = 0; i < n; i++) {
      if (rec_offs_nth_extern(m_index, m_offsets, i)) {
        set_ownership_of_extern_field(i, true);
      }
    }
  }

  /** Frees the externally stored fields for a record.
  @param[in]    trx_id          transaction identifier whose LOB is
                                  being freed.
  @param[in]    undo_no         undo number within a transaction whose
                                  LOB is being freed.
  @param[in]    rollback        performing rollback?
  @param[in]    rec_type        undo record type.
  @param[in]    node            purge node or nullptr */
  void free_externally_stored_fields(trx_id_t trx_id, undo_no_t undo_no,
                                     bool rollback, ulint rec_type,
                                     purge_node_t *node);

  /** Frees the externally stored fields for a record, if the field
  is mentioned in the update vector.
  @param[in]    trx_id          the transaction identifier.
  @param[in]    undo_no         undo number within a transaction whose
                                  LOB is being freed.
  @param[in]    update          update vector
  @param[in]    rollback        performing rollback?
  @param[in]    big_rec_vec     big record vector */
  void free_updated_extern_fields(trx_id_t trx_id, undo_no_t undo_no,
                                  const upd_t *update, bool rollback,
                                  big_rec_t *big_rec_vec);

  /** Gets the compressed page descriptor
  @return the compressed page descriptor. */
  page_zip_des_t *get_page_zip() const {
    return (buf_block_get_page_zip(m_block));
  }

  /** Get the page number of clustered index block.
  @return the page number. */
  page_no_t get_page_no() const {
    return (page_get_page_no(buf_block_get_frame(m_block)));
  }

  /** Get the record offset within page of the clustered index record.
  @return the record offset. */
  ulint get_rec_offset() const { return (page_offset(m_rec)); }

  /** Check if there is a need to recalculate the context information.
  @return true if there is a need to recalculate, false otherwise. */
  bool need_recalc() const {
    return ((m_pcur != nullptr) && (m_rec != m_pcur->get_rec()));
  }

  /** Get the clustered index record pointer.
  @return clustered index record pointer. */
  rec_t *rec() const {
    ut_ad(m_pcur == nullptr || m_rec == m_pcur->get_rec());
    return (m_rec);
  }

  /** Get the LOB reference for the given field number.
  @param[in]    field_no        field number.
  @return LOB reference (aka external field reference).*/
  byte *get_field_ref(ulint field_no) const {
    return (btr_rec_get_field_ref(m_index, m_rec, get_offsets(), field_no));
  }

#ifdef UNIV_DEBUG
  /** Validate the current BLOB context object.  The BLOB context object
  is valid if the necessary latches are being held by the
  mini-transaction of the B-tree (btr mtr).  Does not return if the
  validation fails.
  @return true if valid */
  bool validate() const {
    rec_offs_make_valid(rec(), index(), m_offsets);

    ut_ad(m_mtr->memo_contains_page_flagged(
              m_rec, MTR_MEMO_PAGE_X_FIX | MTR_MEMO_PAGE_SX_FIX) ||
          table()->is_intrinsic());

    ut_ad(mtr_memo_contains_flagged(m_mtr, dict_index_get_lock(index()),
                                    MTR_MEMO_SX_LOCK | MTR_MEMO_X_LOCK) ||
          table()->is_intrinsic());

    return (true);
  }

  /** Check to see if all pointers to externally stored columns in
  the record must be valid.
  @return true if all blob references are valid.
  @return will not return if any blob reference is invalid. */
  bool are_all_blobrefs_valid() const {
    for (ulint i = 0; i < rec_offs_n_fields(m_offsets); i++) {
      if (!rec_offs_nth_extern(m_index, m_offsets, i)) {
        continue;
      }

      byte *field_ref = btr_rec_get_field_ref(m_index, rec(), m_offsets, i);

      ref_t blobref(field_ref);

      /* The pointer must not be zero if the operation
      succeeded. */
      ut_a(!blobref.is_null());

      /* The column must not be disowned by this record. */
      ut_a(blobref.is_owner());
    }

    return (true);
  }
#endif /* UNIV_DEBUG */

  /** Determine whether current operation is a bulk insert operation.
  @return true, if bulk insert operation, false otherwise. */
  bool is_bulk() const { return (m_op == OPCODE_INSERT_BULK); }

  /** Get the beginning of the B-tree clustered index page frame
  that contains the current clustered index record (m_rec).
  @return the page frame containing the clust rec. */
  const page_t *rec_frame() const {
    ut_ad(m_block->frame == page_align(m_rec));
    return (m_block->frame);
  }

  /** Commit the mini-transaction that is holding the latches
  of the clustered index record block. */
  void commit_btr_mtr() { m_mtr->commit(); }

  /** Start the mini-transaction that will be holding the latches
  of the clustered index record block. */
  void start_btr_mtr() {
    mtr_log_t log_mode = m_mtr->get_log_mode();
    m_mtr->start();
    m_mtr->set_log_mode(log_mode);
  }

  /** Get the page number of clustered index record.
  @return page number of clustered index record. */
  page_no_t get_btr_page_no() const {
    return (page_get_page_no(buf_block_get_frame(m_block)));
  }

#ifndef UNIV_HOTBACKUP

  /** Increment the buffer fix count of the clustered index record block.
  This is to be called before commit_btr_mtr() which decrements the count when
  you want to prevent the block from being freed:
    rec_block_fix();   // buf_fix_count++
    commit_btr_mtr();  // releasing mtr internally does buf_fix_count--
    start_btr_mtr();
    rec_block_unfix(); // calls btr_block_get() which does buf_fix_count++ and
                       // then does buf_fix_count--
  */
  void rec_block_fix() {
    m_rec_offset = page_offset(m_rec);
    m_btr_page_no = page_get_page_no(buf_block_get_frame(m_block));
    buf_block_buf_fix_inc(m_block, UT_LOCATION_HERE);
  }

  /** Decrement the buffer fix count of the clustered index record block,
  X-latching it before, so that the overall buffer_fix_count doesn't change.
  This is done to restore X-latch on the page after mtr restart:
    rec_block_fix();   // buf_fix_count++
    commit_btr_mtr();  // releasing mtr internally does buf_fix_count--
    start_btr_mtr();
    rec_block_unfix(); // calls btr_block_get() which does buf_fix_count++ and
                       // then does buf_fix_count--
  */
  void rec_block_unfix() {
    space_id_t space_id = space();
    page_id_t page_id(space_id, m_btr_page_no);
    page_size_t page_size(dict_table_page_size(table()));
    page_cur_t *page_cur = &m_pcur->m_btr_cur.page_cur;

    mtr_x_lock(dict_index_get_lock(index()), m_mtr, UT_LOCATION_HERE);

    page_cur->block = btr_block_get(page_id, page_size, RW_X_LATCH,
                                    UT_LOCATION_HERE, index(), m_mtr);

    page_cur->rec = buf_block_get_frame(page_cur->block) + m_rec_offset;

    buf_block_buf_fix_dec(page_cur->block);
    /* This decrement above is paired with increment in rec_block_fix(), and
    there is another increment done within btr_block_get(), so overall the block
    should be buffer-fixed and thus safe to be used. */
    ut_ad(page_cur->block->page.buf_fix_count > 0);
    recalc();
  }
#endif /* !UNIV_HOTBACKUP */

  /** Restore the position of the persistent cursor. */
  void restore_position() {
    ut_ad(m_pcur->m_rel_pos == BTR_PCUR_ON);
    bool ret =
        m_pcur->restore_position(m_pcur->m_latch_mode, m_mtr, UT_LOCATION_HERE);

    ut_a(ret);

    recalc();
  }

  /** Get the index object.
  @return index object */
  dict_index_t *index() const { return (m_index); }

  /** Get the table object.
  @return table object or NULL. */
  dict_table_t *table() const {
    dict_table_t *result = nullptr;

    if (m_pcur != nullptr && m_pcur->index() != nullptr) {
      result = m_pcur->index()->table;
    }

    return (result);
  }

  /** Get the space id.
  @return space id. */
  space_id_t space() const { return (index()->space); }

  /** Obtain the page size of the underlying table.
  @return page size of the underlying table. */
  const page_size_t page_size() const {
    return (dict_table_page_size(table()));
  }

  /** Determine the extent size (in pages) for the underlying table
  @return extent size in pages */
  page_no_t pages_in_extent() const {
    return (dict_table_extent_size(table()));
  }

  /** Check if there is enough space in the redo log file.  The btr
  mini-transaction will be restarted. */
  void check_redolog() {
    is_bulk() ? check_redolog_bulk() : check_redolog_normal();
  }

  /** Mark the nth field as externally stored.
  @param[in]    field_no        the field number. */
  void make_nth_extern(ulint field_no) {
    rec_offs_make_nth_extern(m_index, m_offsets, field_no);
  }

  /** Get the log mode of the btr mtr.
  @return the log mode. */
  mtr_log_t get_log_mode() { return (m_mtr->get_log_mode()); }

  /** Get flush observer
  @return flush observer */
  Flush_observer *get_flush_observer() const {
    return (m_mtr->get_flush_observer());
  }

  /** Get the record offsets array.
  @return the record offsets array. */
  ulint *get_offsets() const { return (m_offsets); }

  /** Validate the record offsets array.
  @return true if validation succeeds, false otherwise. */
  bool rec_offs_validate() const {
    if (m_rec != nullptr) {
      ut_ad(::rec_offs_validate(m_rec, m_index, m_offsets));
    }
    return (true);
  }

  /** Get the associated mini-transaction.
  @return the mini-transaction. */
  mtr_t *get_mtr() { return (m_mtr); }

  /** Get the pointer to the clustered record block.
  @return pointer to the clustered rec block. */
  buf_block_t *block() const { return (m_block); }

  /** Save the position of the persistent cursor. */
  void store_position() { m_pcur->store_position(m_mtr); }

  /** Check if there is enough space in log file. Commit and re-start the
  mini-transaction. */
  void check_redolog_normal();

  /** When bulk load is being done, check if there is enough space in redo
  log file. */
  void check_redolog_bulk();

  /** Recalculate some of the members after restoring the persistent
  cursor. */
  void recalc() {
    m_block = m_pcur->get_block();
    m_rec = m_pcur->get_rec();
    m_btr_page_no = page_get_page_no(buf_block_get_frame(m_block));
    m_rec_offset = page_offset(m_rec);

    rec_offs_make_valid(rec(), index(), m_offsets);
  }

  /** Write a blob reference of a field into a clustered index record
  in a compressed leaf page. The information must already have been
  updated on the uncompressed page.
  @param[in]  field_no  BLOB field number
  @param[in]  mtr       Mini-transaction to update blob page. */
  void zblob_write_blobref(ulint field_no, mtr_t *mtr) {
    page_zip_write_blob_ptr(get_page_zip(), m_rec, index(), m_offsets, field_no,
                            mtr);
  }

  /** The btr mtr that is holding the latch on the B-tree index page
  containing the clustered index record.*/
  mtr_t *m_mtr;

  /** Persistent cursor positioned on the clustered index record.*/
  btr_pcur_t *m_pcur;
  dict_index_t *m_index;
  rec_t *m_rec;
  ulint *m_offsets;
  buf_block_t *m_block;
  opcode m_op;

  /** Record offset within the page. */
  ulint m_rec_offset;

  /** Page number where the clust rec is present. */
  page_no_t m_btr_page_no;
};

/** The context for a LOB operation.  It contains the necessary information
to carry out a LOB operation. */
struct InsertContext : public BtrContext {
  /** Constructor
  @param[in]    btr_ctx         b-tree context for lob operation.
  @param[in]    big_rec_vec     array of blobs */
  InsertContext(const BtrContext &btr_ctx, const big_rec_t *big_rec_vec)
      : BtrContext(btr_ctx), m_big_rec_vec(big_rec_vec) {}

  /** Get the vector containing fields to be stored externally.
  @return the big record vector */
  const big_rec_t *get_big_rec_vec() { return (m_big_rec_vec); }

  /** Get the size of vector containing fields to be stored externally.
  @return the big record vector size */
  ulint get_big_rec_vec_size() { return (m_big_rec_vec->n_fields); }

  /** The B-tree Context */
  // const BtrContext   m_btr_ctx;

  /** vector containing fields to be stored externally */
  const big_rec_t *m_big_rec_vec;
};

/** Information about data stored in one BLOB page. */
struct blob_page_info_t {
  /** Constructor.
  @param[in]    page_no         the BLOB page number.
  @param[in]    bytes           amount of uncompressed BLOB data
                                  in BLOB page in bytes.
  @param[in]    zbytes          amount of compressed BLOB data
                                  in BLOB page in bytes. */
  blob_page_info_t(page_no_t page_no, uint bytes, uint zbytes)
      : m_page_no(page_no), m_bytes(bytes), m_zbytes(zbytes) {}

  /** Re-initialize the current object. */
  void reset() {
    m_page_no = 0;
    m_bytes = 0;
    m_zbytes = 0;
  }

  /** Print this blob_page_into_t object into the given output stream.
  @param[in]    out     the output stream.
  @return the output stream. */
  std::ostream &print(std::ostream &out) const;

  /** Set the compressed data size in bytes.
  @param[in]    bytes   the new compressed data size. */
  void set_compressed_size(uint bytes) { m_zbytes = bytes; }

  /** Set the uncompressed data size in bytes.
  @param[in]    bytes   the new uncompressed data size. */
  void set_uncompressed_size(uint bytes) { m_bytes = bytes; }

  /** Set the page number.
  @param[in]    page_no         the page number */
  void set_page_no(page_no_t page_no) { m_page_no = page_no; }

 private:
  /** The BLOB page number */
  page_no_t m_page_no;

  /** Amount of uncompressed data (in bytes) in the BLOB page. */
  uint m_bytes;

  /** Amount of compressed data (in bytes) in the BLOB page. */
  uint m_zbytes;
};

inline std::ostream &operator<<(std::ostream &out,
                                const blob_page_info_t &obj) {
  return (obj.print(out));
}

/** The in-memory blob directory.  Each blob contains a sequence of pages.
This directory contains a list of those pages along with their metadata. */
struct blob_dir_t {
  typedef std::vector<blob_page_info_t>::const_iterator const_iterator;

  /** Print this blob directory into the given output stream.
  @param[in]    out     the output stream.
  @return the output stream. */
  std::ostream &print(std::ostream &out) const;

  /** Clear the contents of this blob directory. */
  void clear() { m_pages.clear(); }

  /** Append the given blob page information.
  @param[in]    page    the blob page information to be added.
  @return DB_SUCCESS on success, error code on failure. */
  dberr_t add(const blob_page_info_t &page) {
    m_pages.push_back(page);
    return (DB_SUCCESS);
  }

  /** A vector of blob pages along with its metadata. */
  std::vector<blob_page_info_t> m_pages;
};

/** Overloading the global output operator to print the blob_dir_t
object into an output stream.
@param[in,out]  out     the output stream.
@param[in]      obj     the object to be printed.
@return the output stream. */
inline std::ostream &operator<<(std::ostream &out, const blob_dir_t &obj) {
  return (obj.print(out));
}

/** The context information for reading a single BLOB */
struct ReadContext {
  /** Constructor
  @param[in]    page_size       page size information.
  @param[in]    data            'internally' stored part of the field
                                  containing also the reference to the
                                  external part; must be protected by
                                  a lock or a page latch.
  @param[in]    prefix_len      length of BLOB data stored inline in
                                  the clustered index record, including
                                  the blob reference.
  @param[out]   buf             the output buffer.
  @param[in]    len             the output buffer length.
  @param[in]    is_sdi          true for SDI Indexes. */
  ReadContext(const page_size_t &page_size, const byte *data, ulint prefix_len,
              byte *buf, ulint len IF_DEBUG(, bool is_sdi))
      : m_page_size(page_size),
        m_data(data),
        m_local_len(prefix_len),
        m_blobref(const_cast<byte *>(data) + prefix_len -
                  BTR_EXTERN_FIELD_REF_SIZE),
        m_buf(buf),
        m_len(len),
        m_lob_version(0) IF_DEBUG(, m_is_sdi(is_sdi)) {
    read_blobref();
  }

  /** Read the space_id, page_no and offset information from the BLOB
  reference object and update the member variables. */
  void read_blobref() {
    m_space_id = m_blobref.space_id();
    m_page_no = m_blobref.page_no();
    m_offset = m_blobref.offset();
  }

  /** Check if the BLOB reference is valid.  For this particular check,
  if the length of the BLOB is greater than 0, it is considered
  valid.
  @return true if valid. */
  bool is_valid_blob() const { return (m_blobref.length() > 0); }

  dict_index_t *index() { return (m_index); }

  /** The page size information. */
  const page_size_t &m_page_size;

  /** The 'internally' stored part of the field containing also the
  reference to the external part; must be protected by a lock or a page
  latch */
  const byte *m_data;

  /** Length (in bytes) of BLOB prefix stored inline in clustered
  index record. */
  ulint m_local_len;

  /** The blob reference of the blob that is being read. */
  const ref_t m_blobref;

  /** Buffer into which data is read. */
  byte *m_buf;

  /** Length of the buffer m_buf. */
  ulint m_len;

  /** The identifier of the space in which blob is available. */
  space_id_t m_space_id;

  /** The page number obtained from the blob reference. */
  page_no_t m_page_no;

  /** The offset information obtained from the blob reference. */
  ulint m_offset;

  dict_index_t *m_index;

  ulint m_lob_version;

#ifdef UNIV_DEBUG
  /** Is it a space dictionary index (SDI)?
  @return true if SDI, false otherwise. */
  bool is_sdi() const { return (m_is_sdi); }

  /** Is it a tablespace dictionary index (SDI)? */
  const bool m_is_sdi;

  /** Assert that current trx is using isolation level read uncommitted.
  @return true if transaction is using read uncommitted, false otherwise. */
  bool assert_read_uncommitted() const;
#endif /* UNIV_DEBUG */

  /** The transaction that is reading. */
  trx_t *m_trx = nullptr;
};

/** Fetch compressed BLOB */
struct zReader {
  /** Constructor. */
  explicit zReader(const ReadContext &ctx) : m_rctx(ctx) {}

  /** Fetch the BLOB.
  @return DB_SUCCESS on success. */
  dberr_t fetch();

  /** Fetch one BLOB page.
  @return DB_SUCCESS on success. */
  dberr_t fetch_page();

  /** Get the length of data that has been read.
  @return the length of data that has been read. */
  ulint length() const { return (m_stream.total_out); }

 private:
  /** Do setup of the zlib stream.
  @return code returned by zlib. */
  int setup_zstream();

#ifdef UNIV_DEBUG
  /** Assert that the local prefix is empty.  For compressed row format,
  there is no local prefix stored.  This function doesn't return if the
  local prefix is non-empty.
  @return true if local prefix is empty*/
  bool assert_empty_local_prefix();
#endif /* UNIV_DEBUG */

  ReadContext m_rctx;

  /** Bytes yet to be read. */
  ulint m_remaining;

  /** The zlib stream used to uncompress while fetching blob. */
  z_stream m_stream;

  /** The memory heap that will be used by zlib allocator. */
  mem_heap_t *m_heap;

  /* There is no latch on m_bpage directly.  Instead,
  m_bpage is protected by the B-tree page latch that
  is being held on the clustered index record, or,
  in row_merge_copy_blobs(), by an exclusive table lock. */
  buf_page_t *m_bpage;

#ifdef UNIV_DEBUG
  /** The expected page type. */
  ulint m_page_type_ex;
#endif /* UNIV_DEBUG */
};

/** Fetch uncompressed BLOB */
struct Reader {
  /** Constructor. */
  Reader(const ReadContext &ctx)
      : m_rctx(ctx), m_cur_block(nullptr), m_copied_len(0) {}

  /** Fetch the complete or prefix of the uncompressed LOB data.
  @return bytes of LOB data fetched. */
  ulint fetch();

  /** Fetch one BLOB page. */
  void fetch_page();

  ReadContext m_rctx;

  /** Buffer block of the current BLOB page */
  buf_block_t *m_cur_block;

  /** Total bytes of LOB data that has been copied from multiple
  LOB pages. This is a cumulative value.  When this value reaches
  m_rctx.m_len, then the read operation is completed. */
  ulint m_copied_len;
};

/** The context information when the delete operation on LOB is
taking place. */
struct DeleteContext : public BtrContext {
  /** Constructor.
  @param[in]  btr  the B-tree context of the blob operation.
  @param[in]  field_ref  reference to a blob
  @param[in]  field_no   field containing the blob.
  @param[in]  rollback   true if it is rollback, false if it is purge. */
  DeleteContext(const BtrContext &btr, byte *field_ref, ulint field_no,
                bool rollback);

  /** Constructor.
  @param[in]  btr  the B-tree context of the blob operation.
  @param[in]  rollback   true if it is rollback, false if it is purge. */
  DeleteContext(const BtrContext &btr, bool rollback);

  /** Update the delete context to point to a different blob.
  @param[in]  field_ref  blob reference
  @param[in]  field_no   the field that contains the blob. */
  void set_blob(byte *field_ref, ulint field_no);

  /** Check if the blob reference is valid.
  @return true if valid, false otherwise. */
  bool is_ref_valid() const {
    return (m_blobref_mem.m_page_no == m_blobref.page_no());
  }

  /** Determine if it is compressed page format.
  @return true if compressed. */
  bool is_compressed() const { return (m_page_size.is_compressed()); }

  /** Check if tablespace supports atomic blobs.
  @return true if tablespace has atomic blobs. */
  bool has_atomic_blobs() const {
    space_id_t space_id = m_blobref.space_id();
    uint32_t flags = fil_space_get_flags(space_id);
    return (DICT_TF_HAS_ATOMIC_BLOBS(flags));
  }

  bool is_delete_marked() const {
    rec_t *clust_rec = rec();
    if (clust_rec == nullptr) {
      return (true);
    }
    return (rec_get_deleted_flag(clust_rec, page_rec_is_comp(clust_rec)));
  }

#ifdef UNIV_DEBUG
  /** Validate the LOB reference object.
  @return true if valid, false otherwise. */
  bool validate_blobref() const {
    rec_t *clust_rec = rec();
    if (clust_rec != nullptr) {
      const byte *v2 =
          btr_rec_get_field_ref(m_index, clust_rec, get_offsets(), m_field_no);

      ut_ad(m_blobref.is_equal(v2));
    }
    return (true);
  }
#endif /* UNIV_DEBUG */

  /** Acquire an x-latch on the index page containing the clustered
  index record, in the given mini-transaction context.
  @param[in]  mtr  Mini-transaction context. */
  void x_latch_rec_page(mtr_t *mtr);

  /** the BLOB reference or external field reference. */
  ref_t m_blobref;

  /** field number of externally stored column; ignored if rec == NULL */
  ulint m_field_no;

  /** Is this operation part of rollback? */
  bool m_rollback;

  /** The page size of the tablespace.*/
  page_size_t m_page_size;

  /** Add a buffer block that is to be freed.
  @param[in]  block  buffer block to be freed.*/
  void add_lob_block(buf_block_t *block);

  /** Free all the stored lob blocks. */
  void free_lob_blocks();

  /** Destructor. Just add some asserts to ensure that resources are freed. */
  ~DeleteContext();

 private:
  /** Memory copy of the original LOB reference. */
  ref_mem_t m_blobref_mem;

  using Block_vector = std::vector<buf_block_t *, ut::allocator<buf_block_t *>>;

  /** The buffer blocks of lob to be freed. */
  Block_vector m_lob_blocks;

  /** Obtain the page size from the tablespace flags.
  @return the page size. */
  page_size_t get_page_size() const;
};

inline DeleteContext::DeleteContext(const BtrContext &btr, bool rollback)
    : DeleteContext(btr, nullptr, 0, rollback) {}

inline DeleteContext::DeleteContext(const BtrContext &btr, byte *field_ref,
                                    ulint field_no, bool rollback)
    : BtrContext(btr),
      m_blobref(field_ref),
      m_field_no(field_no),
      m_rollback(rollback),
      m_page_size(table() == nullptr ? get_page_size()
                                     : dict_table_page_size(table())) {
  if (field_ref != nullptr) {
    m_blobref.parse(m_blobref_mem);
  }
}

inline void DeleteContext::set_blob(byte *field_ref, ulint field_no) {
  m_blobref.set_ref(field_ref);
  m_field_no = field_no;
  if (field_ref != nullptr) {
    m_blobref.parse(m_blobref_mem);
  }
}

inline page_size_t DeleteContext::get_page_size() const {
  bool found;
  space_id_t space_id = m_blobref.space_id();
  const page_size_t &tmp = fil_space_get_page_size(space_id, &found);
  ut_ad(found);
  return tmp;
}

inline DeleteContext::~DeleteContext() { ut_ad(m_lob_blocks.empty()); }

inline void DeleteContext::add_lob_block(buf_block_t *block) {
  ut_ad(mtr_memo_contains_flagged(m_mtr, block, MTR_MEMO_PAGE_X_FIX));
  m_lob_blocks.push_back(block);
}

inline void DeleteContext::free_lob_blocks() {
  for (auto block_ptr : m_lob_blocks) {
    ut_ad(mtr_memo_contains_flagged(m_mtr, block_ptr, MTR_MEMO_PAGE_X_FIX));
    btr_page_free_low(m_index, block_ptr, ULINT_UNDEFINED, m_mtr);
  }
  m_lob_blocks.clear();
}

/** Determine if an operation on off-page columns is an update.
@param[in]      op      type of BLOB operation.
@return true if op != OPCODE_INSERT */
inline bool btr_lob_op_is_update(opcode op) {
  switch (op) {
    case OPCODE_INSERT:
    case OPCODE_INSERT_BULK:
      return (false);
    case OPCODE_INSERT_UPDATE:
    case OPCODE_UPDATE:
      return (true);
    case OPCODE_UNKNOWN:
      break;
  }

  ut_d(ut_error);
  ut_o(return false);
}

/** Copies the prefix of an externally stored field of a record.
The clustered index record must be protected by a lock or a page latch.
@param[in]      trx             the current transaction object if available
or nullptr.
@param[in]      index           the clust index in which lob is read.
@param[out]     buf             the field, or a prefix of it
@param[in]      len             length of buf, in bytes
@param[in]      page_size       BLOB page size
@param[in]      data            'internally' stored part of the field
                                containing also the reference to the external
                                part; must be protected by a lock or a page
                                latch.
@param[in]      is_sdi          true for SDI indexes
@param[in]      local_len       length of data, in bytes
@return the length of the copied field, or 0 if the column was being
or has been deleted */
ulint btr_copy_externally_stored_field_prefix_func(
    trx_t *trx, const dict_index_t *index, byte *buf, ulint len,
    const page_size_t &page_size, const byte *data,
    IF_DEBUG(bool is_sdi, ) ulint local_len);

/** Copies an externally stored field of a record to mem heap.
The clustered index record must be protected by a lock or a page latch.
@param[in]      trx             the current trx object or nullptr
@param[in]      index           the clust index in which lob is read.
@param[out]     len             length of the whole field
@param[out]     lob_version     LOB version number.
@param[in]      data            'internally' stored part of the field
                                containing also the reference to the external
                                part; must be protected by a lock or a page
                                latch.
@param[in]      page_size       BLOB page size
@param[in]      local_len       length of data
@param[in]      is_sdi          true for SDI Indexes
@param[in,out]  heap            mem heap
@return the whole field copied to heap */
byte *btr_copy_externally_stored_field_func(
    trx_t *trx, const dict_index_t *index, ulint *len, size_t *lob_version,
    const byte *data, const page_size_t &page_size, ulint local_len,
    IF_DEBUG(bool is_sdi, ) mem_heap_t *heap);

static inline ulint btr_copy_externally_stored_field_prefix(
    trx_t *trx, const dict_index_t *index, byte *buf, ulint len,
    const page_size_t &page_size, const byte *data, bool is_sdi,
    ulint local_len) {
  return btr_copy_externally_stored_field_prefix_func(
      trx, index, buf, len, page_size, data, IF_DEBUG(is_sdi, ) local_len);
}
static inline byte *btr_copy_externally_stored_field(
    trx_t *trx, const dict_index_t *index, ulint *len, size_t *ver,
    const byte *data, const page_size_t &page_size, ulint local_len,
    bool is_sdi, mem_heap_t *heap) {
  return btr_copy_externally_stored_field_func(trx, index, len, ver, data,
                                               page_size, local_len,
                                               IF_DEBUG(is_sdi, ) heap);
}

/** Gets the externally stored size of a record, in units of a database page.
@param[in]      index   index
@param[in]      rec     record
@param[in]      offsets array returned by rec_get_offsets()
@return externally stored part, in units of a database page */
ulint btr_rec_get_externally_stored_len(const dict_index_t *index,
                                        const rec_t *rec, const ulint *offsets);

/** Purge an LOB (either of compressed or uncompressed).
@param[in]      ctx             the delete operation context information.
@param[in]      index           clustered index in which LOB is present
@param[in]      trxid           the transaction that is being purged.
@param[in]      undo_no         during rollback to savepoint, purge only up to
                                this undo number.
@param[in]      rec_type        undo record type.
@param[in]      uf              the update vector for the field.
@param[in]      node            the purge node or nullptr. */
void purge(lob::DeleteContext *ctx, dict_index_t *index, trx_id_t trxid,
           undo_no_t undo_no, ulint rec_type, const upd_field_t *uf,
           purge_node_t *node);

/** Update a portion of the given LOB.
@param[in]      ctx             update operation context information.
@param[in]      trx             the transaction that is doing the
modification.
@param[in]      index           the clustered index containing the LOB.
@param[in]      upd             update vector
@param[in]      field_no        the LOB field number
@param[in]      blobref         LOB reference stored in clust record.
@return DB_SUCCESS on success, error code on failure. */
dberr_t update(InsertContext &ctx, trx_t *trx, dict_index_t *index,
               const upd_t *upd, ulint field_no, ref_t blobref);

/** Update a portion of the given LOB.
@param[in]      ctx             update operation context information.
@param[in]      trx             the transaction that is doing the
modification.
@param[in]      index           the clustered index containing the LOB.
@param[in]      upd             update vector
@param[in]      field_no        the LOB field number
@param[in]      blobref         LOB reference stored in clust record.
@return DB_SUCCESS on success, error code on failure. */
dberr_t z_update(InsertContext &ctx, trx_t *trx, dict_index_t *index,
                 const upd_t *upd, ulint field_no, ref_t blobref);

/** Print information about the given LOB.
@param[in]  trx  the current transaction.
@param[in]  index  the clust index that contains the LOB.
@param[in]  out    the output stream into which LOB info is printed.
@param[in]  ref    the LOB reference
@param[in]  fatal  if true assert at end of function. */
void print(trx_t *trx, dict_index_t *index, std::ostream &out, ref_t ref,
           bool fatal);

/** Import the given LOB.  Update the creator trx id and the modifier trx
id to the given import trx id.
@param[in]      index   clustered index containing the lob.
@param[in]      field_ref       the lob reference.
@param[in]      trx_id          the import trx id. */
void z_import(const dict_index_t *index, byte *field_ref, trx_id_t trx_id);

/** Import the given LOB.  Update the creator trx id and the modifier trx
id to the given import trx id.
@param[in]      index   clustered index containing the lob.
@param[in]      field_ref       the lob reference.
@param[in]      trx_id          the import trx id. */
void import(const dict_index_t *index, byte *field_ref, trx_id_t trx_id);

#ifdef UNIV_DEBUG
/** Check if all the LOB references in the given clustered index record has
valid space_id in it.
@param[in]    index   the index to which the LOB belongs.
@param[in]    rec     the clust_rec in which the LOB references are checked.
@param[in]    offsets the field offsets of the given rec.
@return true if LOB references have valid space_id, false otherwise. */
bool rec_check_lobref_space_id(dict_index_t *index, const rec_t *rec,
                               const ulint *offsets);
#endif /* UNIV_DEBUG */

/** Mark an LOB that it is not partially updatable anymore.
@param[in]  trx     Current transaction.
@param[in]  index   Clustered index to which the LOB belongs.
@param[in]  update  Update vector.
@param[in]  btr_mtr Mini-transaction context holding latches on the B-tree.
This function does not generate redo log using this btr_mtr.  It only obtains
the log mode.
@return DB_SUCCESS on success, error code on failure. */
dberr_t mark_not_partially_updatable(trx_t *trx, dict_index_t *index,
                                     const upd_t *update, const mtr_t *btr_mtr);

}  // namespace lob

#endif /* lob0lob_h */
