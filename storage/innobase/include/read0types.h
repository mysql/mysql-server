/*****************************************************************************

Copyright (c) 1997, 2026, Oracle and/or its affiliates.

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

/** @file include/read0types.h
 Cursor read

 Created 2/16/1997 Heikki Tuuri
 *******************************************************/

#ifndef read0types_h
#define read0types_h

#include <algorithm>
#include "dict0mem.h"
#include "read0read_view_interface.h"
#include "trx0types.h"
#include "ut0cpu_cache.h"

// Friend declaration
class MVCC;

/** Read view lists the trx ids of those transactions for which a consistent
read should not see the modifications to the database. */

class ReadView : public Read_view_interface {
  /** This is similar to a std::vector but it is not a drop
  in replacement. It is specific to ReadView. */
  class ids_t {
    typedef trx_ids_t::value_type value_type;

    /**
    Constructor */
    ids_t() : m_ptr(), m_size(), m_reserved() {}

    /**
    Destructor */
    ~ids_t() { ut::delete_arr(m_ptr); }

    /** Try and increase the size of the array. Old elements are copied across.
    It is a no-op if n is < current size.
    @param n            Make space for n elements */
    void reserve(ulint n);

    /**
    Resize the array, sets the current element count.
    @param n            new size of the array, in elements */
    void resize(ulint n) {
      ut_ad(n <= capacity());

      m_size = n;
    }

    /**
    Reset the size to 0 */
    void clear() { resize(0); }

    /**
    @return the capacity of the array in elements */
    ulint capacity() const { return (m_reserved); }

    /**
    Copy and overwrite this array contents

    @param start            Source array
    @param end              Pointer to end of array */
    void assign(const value_type *start, const value_type *end);

    /**
    Insert the value in the correct slot, preserving the order.
    Doesn't check for duplicates. */
    void insert(value_type value);

    /**
    @return the value of the first element in the array */
    value_type front() const {
      ut_ad(!empty());

      return (m_ptr[0]);
    }

    /**
    @return the value of the last element in the array */
    value_type back() const {
      ut_ad(!empty());

      return (m_ptr[m_size - 1]);
    }

    /**
    Append a value to the array.
    @param value                the value to append */
    void push_back(value_type value);

    /**
    @return a pointer to the start of the array */
    trx_id_t *data() { return (m_ptr); }

    /**
    @return a const pointer to the start of the array */
    const trx_id_t *data() const { return (m_ptr); }

    /**
    @return the number of elements in the array */
    ulint size() const { return (m_size); }

    /**
    @return true if size() == 0 */
    bool empty() const { return (size() == 0); }

   private:
    // Prevent copying
    ids_t(const ids_t &);
    ids_t &operator=(const ids_t &);

   private:
    /** Memory for the array */
    value_type *m_ptr;

    /** Number of active elements in the array */
    ulint m_size;

    /** Size of m_ptr in elements */
    ulint m_reserved;

    friend class ReadView;
  };

 public:
  ReadView();
  ~ReadView() override;

  /** Check whether the changes by id are visible.
  @param[in]    id      transaction id to check against the view
  @return whether the view sees the modifications of id. */
  [[nodiscard]] bool changes_visible(trx_id_t id) const override {
    ut_ad(id > 0);

    if (id < m_up_limit_id || id == m_creator_trx_id) {
      return true;
    }
    if (id >= m_low_limit_id) {
      return false;
    }
    if (m_ids.empty()) {
      return true;
    }

    const ids_t::value_type *p = m_ids.data();

    return !std::binary_search(p, p + m_ids.size(), id);
  }

  [[nodiscard]] bool sees_all_trxs_with_id_smaller_or_equal_to(
      trx_id_t id) const override {
    return id < m_up_limit_id;
  }

  /**
  @return true if the view is closed */
  [[nodiscard]] bool is_closed() const { return m_closed.load(); }

  void print(FILE *file) const override {
    fprintf(file,
            "Trx read view will not see trx with"
            " id >= " TRX_ID_FMT ", sees < " TRX_ID_FMT "\n",
            m_low_limit_id, m_up_limit_id);
  }

  [[nodiscard]] trx_id_t get_lowest_needed_trx_no() const override {
    return m_low_limit_no;
  }

  /**
  @return true if there are no transaction ids in the snapshot */
  [[nodiscard]] bool empty() const { return (m_ids.empty()); }

#ifdef UNIV_DEBUG
  /**
  @param rhs            view to compare with
  @return true if this view is less than or equal rhs */
  [[nodiscard]] bool le(const ReadView *rhs) const {
    return (m_low_limit_no <= rhs->m_low_limit_no);
  }
#endif /* UNIV_DEBUG */
 private:
  /**
  Copy the transaction ids from the source vector */
  inline void copy_trx_ids(const trx_ids_t &trx_ids);

  /**
  Opens a read view where exactly the transactions serialized before this
  point in time are seen in the view.
  @param id             Creator transaction id */
  inline void prepare(trx_id_t id);

  /**
  Copy state from another view. Must call copy_complete() to finish.
  @param other          view to copy from */
  inline void copy_prepare(const ReadView &other);

  /**
  Complete the copy, insert the creator transaction id into the
  m_trx_ids too and adjust the m_up_limit_id *, if required */
  inline void copy_complete();

  /**
  Set the creator transaction id, existing id must be 0 */
  void creator_trx_id(trx_id_t id) {
    ut_ad(m_creator_trx_id == 0);
    m_creator_trx_id = id;
  }

  friend class MVCC;

 private:
  // Disable copying
  ReadView(const ReadView &);
  ReadView &operator=(const ReadView &);

 private:
  /** The read should not see any transaction with trx id >= this
  value. In other words, this is the "high water mark". */
  trx_id_t m_low_limit_id;

  /** The read should see all trx ids which are strictly
  smaller (<) than this value.  In other words, this is the
  low water mark". */
  trx_id_t m_up_limit_id;

  /** If the view is open, then this is a trx->id of the transaction which has
  created this view, used to let this view see the changes of this transaction.
  Note that a transaction might have no trx->id assigned in which case this
  will be 0. A transaction may also get trx->id assigned after it has already
  created a read view, in which case it should call set_view_creator_trx_id to
  update this field.
  It is 0 for read views cloned by clone_oldest_view.
  Otherwise its value doesn't matter. */
  trx_id_t m_creator_trx_id;

  /** Set of RW transactions that was active when this snapshot
  was taken */
  ids_t m_ids;

  /** The view does not need to see the undo logs for transactions
  whose transaction number is strictly smaller (<) than this value:
  they can be removed in purge if not needed by other views */
  trx_id_t m_low_limit_no;

  /** False iff this view is in use by a transaction at the moment (is open).*/
  std::atomic_bool m_closed{true};

  typedef UT_LIST_NODE_T(ReadView) node_t;

  /** List of read views in trx_sys */
  byte pad1[ut::INNODB_CACHE_LINE_SIZE];
  node_t m_view_list{};
};

#endif
