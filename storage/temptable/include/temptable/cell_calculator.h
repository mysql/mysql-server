/* Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/** @file storage/temptable/include/temptable/cell_calculator.h
TempTable Cell_calculator declaration. */

#ifndef TEMPTABLE_CELL_CALCULATOR_H
#define TEMPTABLE_CELL_CALCULATOR_H

#include <algorithm>
#include <cstdint>

#include "m_ctype.h"
#include "my_dbug.h"
#include "my_murmur3.h"
#include "sql/field.h"
#include "sql/key.h"
#include "storage/temptable/include/temptable/cell.h"

namespace temptable {

/** Utility to perform calculations for a cell. It uses cell's contents and
a stored context that describes how to interpret the data. */
class Cell_calculator {
 public:
  /** Default constructor used for std::array initialization in Index. */
  Cell_calculator() = default;

  /** Constructor to be used when creating calculators for indexed columns. */
  explicit Cell_calculator(
      /** [in] Key part (indexed column) for which calculator is created. */
      const KEY_PART_INFO &mysql_key_part);

  /** Constructor to be used when creating calculators for columns when
  comparing table rows. */
  explicit Cell_calculator(
      /** [in] Field (column) for which calculator is created. */
      const Field *mysql_field);

  /** Calculate hash value for a cell.
   * @return a hash number */
  size_t hash(
      /** [in] Cell for which hash is to be calculated. */
      const Cell &cell) const;

  /** Compare two cells.
   * @retval <0 if lhs < rhs
   * @retval  0 if lhs == rhs
   * @retval >0 if lhs > rhs */
  int compare(
      /** [in] First cell to compare. */
      const Cell &lhs,
      /** [in] Second cell to compare. */
      const Cell &rhs) const;

 private:
  enum class Mode : uint8_t {
    BINARY,
    CHARSET,
    CHARSET_AND_CHAR_LENGTH,
  };

  static const CHARSET_INFO *field_charset(const Field &field);

  /** Field for which this calculator was created. */
  const Field *m_mysql_field;

  /** Charset used by calculator. NULL for binary mode. */
  const CHARSET_INFO *m_cs;

  /** Calculation mode. */
  Mode m_mode;

  /** True if the cell is right-padded with spaces (CHAR column). */
  bool m_is_space_padded;

  /** Length in number of characters.
   * Only used in CHARSET_AND_CHAR_LENGTH mode. */
  uint32_t m_char_length;
};

/* Implementation of inlined methods. */

inline Cell_calculator::Cell_calculator(const KEY_PART_INFO &mysql_key_part)
    : m_mysql_field(mysql_key_part.field),
      m_cs(field_charset(*m_mysql_field)),
      m_is_space_padded(m_mysql_field->key_type() == HA_KEYTYPE_TEXT),
      m_char_length(0) {
  /* Mimic hp_hashnr() from storage/heap/hp_hash.c. */

  if (m_cs != nullptr) {
    /* Decide if we should use my_charpos. */
    bool use_char_length = (m_cs->mbmaxlen > 1) &&
                           (mysql_key_part.key_part_flag & HA_PART_KEY_SEG);

    DBUG_EXECUTE_IF("temptable_use_char_length", use_char_length = true;);

    if (use_char_length) {
      m_char_length = mysql_key_part.length / m_cs->mbmaxlen;
      m_mode = Mode::CHARSET_AND_CHAR_LENGTH;
    } else {
      m_mode = Mode::CHARSET;
    }
  } else {
    m_mode = Mode::BINARY;
  }
}

inline Cell_calculator::Cell_calculator(const Field *mysql_field)
    : m_mysql_field(mysql_field),
      m_cs(field_charset(*m_mysql_field)),
      m_is_space_padded(m_mysql_field->key_type() == HA_KEYTYPE_TEXT),
      m_char_length(0) {
  /* Mimic hp_hashnr() from storage/heap/hp_hash.c. */

  /* No partial keys, so no CHARSET_AND_CHAR_LENGTH here. */

  if (m_cs != nullptr) {
    m_mode = Mode::CHARSET;
  } else {
    m_mode = Mode::BINARY;
  }
}

inline const CHARSET_INFO *Cell_calculator::field_charset(const Field &field) {
  /* Decide if we should use charset+collation for comparisons, or rely on pure
   * binary data. */
  switch (field.key_type()) {
    case HA_KEYTYPE_TEXT:
    case HA_KEYTYPE_VARTEXT1:
    case HA_KEYTYPE_VARTEXT2:
    case HA_KEYTYPE_VARBINARY1:
    case HA_KEYTYPE_VARBINARY2:
      if (field.is_flag_set(ENUM_FLAG) || field.is_flag_set(SET_FLAG)) {
        return &my_charset_bin;
      } else {
        return field.charset_for_protocol();
      }
    default:
      return nullptr;
  }
}

inline size_t Cell_calculator::hash(const Cell &cell) const {
  if (cell.is_null()) {
    return 1;
  }

  auto data_length = cell.data_length();
  /*
   * If the collation of field to calculate hash is with PAD_SPACE attribute,
   * empty string '' and space ' ' will be calculated as different hash values,
   * because we handle empty string '' directly (return 0), and calculate hash
   * with cs for space ' '. But actually, for collations with PAD_SPACE
   * attribute empty string '' should be equal with space ' '. Do not return
   * hash value 0 if data_length == 0. */

  auto data = cell.data();

  size_t length = 0;

  /*
  switch (m_mode) {
    case Mode::CHARSET:
      length = ...
      break;
    case Mode::CHARSET_AND_CHAR_LENGTH:
      length = ...
      break;
    case Mode::BINARY:
      return ...
  }
  code <-- this is executed when
  indexed_column.cell_hash_function() == Mode::BINARY
  and compiled with "Studio 12.5 Sun C++ 5.14 SunOS_sparc 2016/05/31" !!!
  So we use if-else instead of switch below. */

  if (m_mode == Mode::BINARY) {
    return murmur3_32(data, data_length, 0);
  } else if (m_mode == Mode::CHARSET) {
    length = data_length;
  } else if (m_mode == Mode::CHARSET_AND_CHAR_LENGTH) {
    length =
        std::min(static_cast<size_t>(data_length),
                 my_charpos(m_cs, data, data + data_length, m_char_length));
  } else {
    my_abort();
  }

  /* If the field is space padded but collation do not want to use
   * the padding it is required to strip the spaces from the end. */
  if (m_is_space_padded && (m_cs->pad_attribute == NO_PAD)) {
    length = m_cs->cset->lengthsp(m_cs, reinterpret_cast<const char *>(data),
                                  length);
  }

  uint64 h1 = 1;
  uint64 h2 = 4;
  m_cs->coll->hash_sort(m_cs, data, length, &h1, &h2);
  return h1;
}

inline int Cell_calculator::compare(const Cell &lhs, const Cell &rhs) const {
  if (lhs.is_null()) {
    if (rhs.is_null()) {
      /* Both are NULL. */
      return 0;
    } else {
      /* NULL < whatever (not NULL). */
      return -1;
    }
  } else {
    if (rhs.is_null()) {
      /* whatever (not NULL) > NULL. */
      return 1;
    }
  }

  /* Both cells are not NULL. */
  auto lhs_data_length = lhs.data_length();
  auto rhs_data_length = rhs.data_length();

  /* If both cells' data is identical, then no need to use the expensive
   * comparisons below because we know that they will report equality. */
  if ((lhs_data_length == rhs_data_length) &&
      ((lhs_data_length == 0) ||
       (memcmp(lhs.data(), rhs.data(), lhs_data_length) == 0))) {
    return 0;
  }

  auto lhs_data = lhs.data();
  auto rhs_data = rhs.data();

  size_t lhs_length = 0;
  size_t rhs_length = 0;

  /* Note: Using if-s instead of switch due to bug mentioned in hash(). */

  if (m_mode == Mode::BINARY) {
    return const_cast<Field *>(m_mysql_field)->key_cmp(lhs_data, rhs_data);
  } else if (m_mode == Mode::CHARSET) {
    lhs_length = lhs_data_length;
    rhs_length = rhs_data_length;
  } else if (m_mode == Mode::CHARSET_AND_CHAR_LENGTH) {
    lhs_length = std::min(
        static_cast<size_t>(lhs_data_length),
        my_charpos(m_cs, lhs_data, lhs_data + lhs_data_length, m_char_length));
    rhs_length = std::min(
        static_cast<size_t>(rhs_data_length),
        my_charpos(m_cs, rhs_data, rhs_data + rhs_data_length, m_char_length));
  } else {
    my_abort();
  }

  /* If the field is space padded but collation do not want to use
   * the padding it is required to strip the spaces from the end. */
  if (m_is_space_padded && (m_cs->pad_attribute == NO_PAD)) {
    /* Strip trailing spaces. */
    lhs_length = m_cs->cset->lengthsp(
        m_cs, reinterpret_cast<const char *>(lhs_data), lhs_length);
    rhs_length = m_cs->cset->lengthsp(
        m_cs, reinterpret_cast<const char *>(rhs_data), rhs_length);
  }

  return m_cs->coll->strnncollsp(m_cs, lhs_data, lhs_length, rhs_data,
                                 rhs_length);
}

} /* namespace temptable */

#endif /* TEMPTABLE_CELL_CALCULATOR_H */
