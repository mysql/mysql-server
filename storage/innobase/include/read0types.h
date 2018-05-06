/*****************************************************************************

Copyright (c) 1997, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file include/read0types.h
 Cursor read

 Created 2/16/1997 Heikki Tuuri
 *******************************************************/

#ifndef read0types_h
#define read0types_h

#include <algorithm>
#include "dict0mem.h"

#include "trx0types.h"

// Friend declaration
class MVCC;

/** Read view lists the trx ids of those transactions for which a consistent
read should not see the modifications to the database. */

class ReadView {
  /** This is similar to a std::vector but it is not a drop
  in replacement. It is specific to ReadView. */
  class ids_t {
    typedef trx_ids_t::value_type value_type;

    /**
    Constructor */
    ids_t() : m_ptr(), m_size(), m_reserved() {}

    /**
    Destructor */
    ~ids_t() { UT_DELETE_ARRAY(m_ptr); }

    /**
    Try and increase the size of the array. Old elements are
    copied across. It is a no-op if n is < current size.

    @param n 		Make space for n elements */
    void reserve(ulint n);

    /**
    Resize the array, sets the current element count.
    @param n		new size of the array, in elements */
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
    Copy and overwrite the current array contents

    @param start		Source array
    @param end		Pointer to end of array */
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
    @param value		the value to append */
    void push_back(value_type value);

    /**
    @return a pointer to the start of the array */
    trx_id_t *data() { return (m_ptr); };

    /**
    @return a const pointer to the start of the array */
    const trx_id_t *data() const { return (m_ptr); };

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
  ~ReadView();
  /** Check whether transaction id is valid.
  @param[in]	id		transaction id to check
  @param[in]	name		table name */
  static void check_trx_id_sanity(trx_id_t id, const table_name_t &name);

  /** Check whether the changes by id are visible.
  @param[in]	id	transaction id to check against the view
  @param[in]	name	table name
  @return whether the view sees the modifications of id. */
  bool changes_visible(trx_id_t id, const table_name_t &name) const
      MY_ATTRIBUTE((warn_unused_result)) {
    ut_ad(id > 0);

    if (id < m_up_limit_id || id == m_creator_trx_id) {
      return (true);
    }

    check_trx_id_sanity(id, name);

    if (id >= m_low_limit_id) {
      return (false);

    } else if (m_ids.empty()) {
      return (true);
    }

    const ids_t::value_type *p = m_ids.data();

    return (!std::binary_search(p, p + m_ids.size(), id));
  }

  /**
  @param id		transaction to check
  @return true if view sees transaction id */
  bool sees(trx_id_t id) const { return (id < m_up_limit_id); }

  /**
  Mark the view as closed */
  void close() {
    ut_ad(m_creator_trx_id != TRX_ID_MAX);
    m_creator_trx_id = TRX_ID_MAX;
  }

  /**
  @return true if the view is closed */
  bool is_closed() const { return (m_closed); }

  /**
  Write the limits to the file.
  @param file		file to write to */
  void print_limits(FILE *file) const {
    fprintf(file,
            "Trx read view will not see trx with"
            " id >= " TRX_ID_FMT ", sees < " TRX_ID_FMT "\n",
            m_low_limit_id, m_up_limit_id);
  }

  /**
  @return the low limit no */
  trx_id_t low_limit_no() const { return (m_low_limit_no); }

  /**
  @return the low limit id */
  trx_id_t low_limit_id() const { return (m_low_limit_id); }

  /**
  @return true if there are no transaction ids in the snapshot */
  bool empty() const { return (m_ids.empty()); }

#ifdef UNIV_DEBUG
  /**
  @param rhs		view to compare with
  @return truen if this view is less than or equal rhs */
  bool le(const ReadView *rhs) const {
    return (m_low_limit_no <= rhs->m_low_limit_no);
  }

  trx_id_t up_limit_id() const { return (m_up_limit_id); }
#endif /* UNIV_DEBUG */
 private:
  /**
  Copy the transaction ids from the source vector */
  inline void copy_trx_ids(const trx_ids_t &trx_ids);

  /**
  Opens a read view where exactly the transactions serialized before this
  point in time are seen in the view.
  @param id		Creator transaction id */
  inline void prepare(trx_id_t id);

  /**
  Complete the read view creation */
  inline void complete();

  /**
  Copy state from another view. Must call copy_complete() to finish.
  @param other		view to copy from */
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

  /** trx id of creating transaction, set to TRX_ID_MAX for free
  views. */
  trx_id_t m_creator_trx_id;

  /** Set of RW transactions that was active when this snapshot
  was taken */
  ids_t m_ids;

  /** The view does not need to see the undo logs for transactions
  whose transaction number is strictly smaller (<) than this value:
  they can be removed in purge if not needed by other views */
  trx_id_t m_low_limit_no;

  /** AC-NL-RO transaction view that has been "closed". */
  bool m_closed;

  typedef UT_LIST_NODE_T(ReadView) node_t;

  /** List of read views in trx_sys */
  byte pad1[64 - sizeof(node_t)];
  node_t m_view_list;
};

#endif
