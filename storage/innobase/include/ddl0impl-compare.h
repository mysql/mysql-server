/*****************************************************************************

Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

/** @file include/ddl0impl-compare.h
 DDL key comparison.
 Created 2020-11-01 by Sunny Bains. */

#ifndef ddl0impl_compare_h
#define ddl0impl_compare_h

#include "ddl0impl.h"
#include "rem0cmp.h"

namespace ddl {

/** Compare the keys of an index. */
struct Compare_key {
  /** Constructor.
  @param[in] index              Compare the keys of this index.
  @param[in,out] dups           For capturing the duplicate entries.
  @param[in] compare_all        If set, compare all columns in the key. */
  Compare_key(const dict_index_t *index, Dup *dups, bool compare_all) noexcept
      : m_dups(dups),
        m_n_unique(dict_index_get_n_unique(index)),
        m_n_fields(compare_all ? dict_index_get_n_fields(index) : m_n_unique),
        m_fields(index->fields) {
    ut_a(m_n_unique <= m_n_fields);
    ut_a(m_dups == nullptr || index == m_dups->m_index);
  }

  Compare_key(const Compare_key &) = default;

  /** Destructor. */
  ~Compare_key() = default;

  /** Compare two tuples.
  @param[in] lhs                Tuple to compare on the left hand side
  @param[in] rhs                Tuple to compare on the Right hand side
  @retval +ve - if lhs > rhs
  @retval -ve - if lhs < rhs
  @retval 0 - if lhs == rhs */
  int operator()(const dfield_t *lhs, const dfield_t *rhs) const noexcept {
    auto f = m_fields;
    auto lhs_f = lhs;
    auto rhs_f = rhs;
    auto n = m_n_unique;

    ut_a(n > 0);

    /* Compare the fields of the tuples until a difference is
    found or we run out of fields to compare. If cmp == 0 at
    the end, then the tuples are equal. */
    int cmp;

    do {
      cmp = cmp_dfield_dfield(lhs_f++, rhs_f++, (f++)->is_ascending);
    } while (cmp == 0 && --n);

    if (cmp != 0) {
      return cmp;
    }

    if (m_dups != nullptr) {
      bool report{true};

      /* Report a duplicate value error if the tuples are
      logically equal.  nullptr columns are logically inequal,
      although they are equal in the sorting order.  Find
      out if any of the fields are nullptr. */
      for (auto df = lhs; df != lhs_f; ++df) {
        if (dfield_is_null(df)) {
          report = false;
          break;
        }
      }

      if (report) {
        m_dups->report(lhs);
      }
    }

    /* The m_n_unique fields were equal, but we compare all fields so
    that we will get the same (internal) order as in the B-tree. */
    for (auto n = m_n_fields - m_n_unique + 1; --n;) {
      cmp = cmp_dfield_dfield(lhs_f++, rhs_f++, (f++)->is_ascending);
      if (cmp != 0) {
        return cmp;
      }
    }

    /* Creating a secondary index and a PRIMARY KEY and there is a duplicate
    in the PRIMARY KEY that has not been detected yet. Internally, an index
    must never contain duplicates. */
    return cmp;
  }

  /** For collecting duplicates. */
  Dup *m_dups{};

  /** Number of unique fields in the index key. */
  const size_t m_n_unique{};

  /** Total number of fields in the index key. */
  const size_t m_n_fields{};

  /** Index key fields. */
  const dict_field_t *m_fields{};
};

}  // namespace ddl

#endif /* !ddl0impl_compare_h */
