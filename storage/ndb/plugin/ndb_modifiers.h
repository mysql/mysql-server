/*
   Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_MODIFIERS_H
#define NDB_MODIFIERS_H

#include "my_inttypes.h"

/**
 * Support for create table/column modifiers
 *   by exploiting the comment field
 */
struct NDB_Modifier {
  enum { M_BOOL, M_STRING } m_type;
  const char *m_name;
  size_t m_name_len;
  bool m_found;
  union {
    bool m_val_bool;
    struct {
      const char *str;
      size_t len;
    } m_val_str;
  };
};

/**
 * NDB_Modifiers
 *
 * This class implements a simple parser for getting modifiers out
 *   of a string (e.g a comment field), allowing them to be modified
 *   and then allowing the string to be regenerated with the
 *   modified values
 */
class NDB_Modifiers {
 public:
  NDB_Modifiers(const char *prefix, const NDB_Modifier modifiers[]);
  ~NDB_Modifiers();

  /**
   * Load comment, with length
   * (not necessailly a null terminated string
   * returns negative in case of errors,
   *   details from getErrMsg()
   */
  int loadComment(const char *str, size_t len);

  /**
   * Get modifier...returns NULL if unknown
   */
  const NDB_Modifier *get(const char *name) const;

  /**
   * return a modifier which has m_found == false
   */
  const NDB_Modifier *notfound() const;

  /**
   * Set value of modifier
   */
  bool set(const char *name, bool value);
  bool set(const char *name, const char *string);

  /**
   * Generate comment string with current set modifiers
   * Returns null in case of errors,
   *  details from getErrMsg()
   */
  const char *generateCommentString();

  /**
   * Get error detail string
   */
  const char *getErrMsg() const { return m_errmsg; }

 private:
  const char *m_prefix;
  uint32 m_prefixLen;
  uint m_len;
  struct NDB_Modifier *m_modifiers;

  char *m_comment_buf;
  uint32 m_comment_len;
  uint32 m_mod_start_offset;
  uint32 m_mod_len;

  char m_errmsg[100];

  NDB_Modifier *find(const char *name) const;

  int parse_modifier(struct NDB_Modifier *m, const char *str);
  int parseModifierListString(const char *string);
  uint32 generateModifierListString(char *buf, size_t buflen) const;
};

#endif
