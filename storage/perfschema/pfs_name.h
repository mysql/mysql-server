/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#ifndef PFS_NAME_H
#define PFS_NAME_H

/**
  @file storage/perfschema/pfs_name.h
  Object names (declarations).
*/

#include <string.h>

#include "m_ctype.h"
#include "my_hostname.h"  // HOSTNAME_LENGTH
#include "mysql_com.h"    // NAME_LEN

/* Not used yet. */
#define ROLENAME_CHAR_LENGTH 32
#define ROLENAME_LENGTH (ROLENAME_CHAR_LENGTH * SYSTEM_CHARSET_MBMAXLEN)

struct CHARSET_INFO;
class Field;

template <int max_length>
struct PFS_any_name {
 public:
  PFS_any_name<max_length>() { m_length = 0; }

  PFS_any_name<max_length>(const PFS_any_name<max_length> &other) {
    assert(other.m_length <= max_length);

    if (0 < other.m_length && other.m_length <= max_length) {
      m_length = other.m_length;
      memcpy(m_data, other.m_data, m_length);
    } else {
      m_length = 0;
    }
  }

  PFS_any_name<max_length> &operator=(const PFS_any_name<max_length> &other) {
    assert(other.m_length <= max_length);

    if (0 < other.m_length && other.m_length <= max_length) {
      m_length = other.m_length;
      memcpy(m_data, other.m_data, m_length);
    } else {
      m_length = 0;
    }
    return *this;
  }

  void reset() { m_length = 0; }

  void set(const char *str, size_t len) {
    assert(len <= max_length);

    if (0 < len && len <= max_length) {
      m_length = len;
      memcpy(m_data, str, len);
    } else {
      m_length = 0;
    }
  }

  void casedn(const CHARSET_INFO *cs) {
    /*
      Conversion in place:
      - the string is not NUL terminated.
      - the result after conversion can be shorter.
    */
    char *data = reinterpret_cast<char *>(m_data);
    m_length = cs->cset->casedn(cs, data, m_length, data, m_length);
    assert(m_length <= max_length);
  }

  size_t length() const { return m_length; }

  const char *ptr() const { return reinterpret_cast<const char *>(m_data); }

  void hash(const CHARSET_INFO *cs, uint64 *nr1, uint64 *nr2) const {
    cs->coll->hash_sort(cs, m_data, m_length, nr1, nr2);
  }

  int sort(const CHARSET_INFO *cs,
           const PFS_any_name<max_length> *other) const {
    int cmp;
    cmp = my_strnncoll(cs, m_data, m_length, other->m_data, other->m_length);
    return cmp;
  }

  uchar m_data[max_length];
  size_t m_length;
};

struct PFS_schema_name {
 public:
  void reset() { m_name.reset(); }

  void set(const char *str, size_t len);

  void hash(uint64 *nr1, uint64 *nr2) const { m_name.hash(get_cs(), nr1, nr2); }

  int sort(const PFS_schema_name *other) const {
    return m_name.sort(get_cs(), &other->m_name);
  }

  size_t length() const { return m_name.length(); }
  const char *ptr() const { return m_name.ptr(); }
  const CHARSET_INFO *charset() const { return get_cs(); }

 private:
  static const CHARSET_INFO *get_cs();
  PFS_any_name<NAME_LEN> m_name;
};

struct PFS_table_name {
 public:
  void reset() { m_name.reset(); }

  void set(const char *str, size_t len);

  void hash(uint64 *nr1, uint64 *nr2) const { m_name.hash(get_cs(), nr1, nr2); }

  int sort(const PFS_table_name *other) const {
    return m_name.sort(get_cs(), &other->m_name);
  }

  size_t length() const { return m_name.length(); }
  const char *ptr() const { return m_name.ptr(); }
  const CHARSET_INFO *charset() const { return get_cs(); }

 private:
  friend struct PFS_object_name;

  static const CHARSET_INFO *get_cs();
  PFS_any_name<NAME_LEN> m_name;
};

struct PFS_routine_name {
 public:
  void reset() { m_name.reset(); }

  void set(const char *str, size_t len);

  void hash(uint64 *nr1, uint64 *nr2) const { m_name.hash(m_cs, nr1, nr2); }

  int sort(const PFS_routine_name *other) const {
    return m_name.sort(m_cs, &other->m_name);
  }

  size_t length() const { return m_name.length(); }
  const char *ptr() const { return m_name.ptr(); }
  const CHARSET_INFO *charset() const { return m_cs; }

 private:
  friend struct PFS_object_name;

  static const CHARSET_INFO *m_cs;
  PFS_any_name<NAME_LEN> m_name;
};

struct PFS_object_name {
 public:
  void reset() { m_name.reset(); }

  void set_as_table(const char *str, size_t len);
  void set_as_routine(const char *str, size_t len);

  void hash_as_table(uint64 *nr1, uint64 *nr2) const {
    m_name.hash(PFS_table_name::get_cs(), nr1, nr2);
  }

  int sort_as_table(const PFS_object_name *other) const {
    return m_name.sort(PFS_table_name::get_cs(), &other->m_name);
  }

  void hash_as_routine(uint64 *nr1, uint64 *nr2) const {
    m_name.hash(PFS_routine_name::m_cs, nr1, nr2);
  }

  int sort_as_routine(const PFS_object_name *other) const {
    return m_name.sort(PFS_routine_name::m_cs, &other->m_name);
  }

  PFS_object_name &operator=(const PFS_routine_name &other) {
    m_name = other.m_name;
    return *this;
  }

  PFS_object_name &operator=(const PFS_table_name &other) {
    m_name = other.m_name;
    return *this;
  }

  size_t length() const { return m_name.length(); }
  const char *ptr() const { return m_name.ptr(); }

 private:
  PFS_any_name<NAME_LEN> m_name;
};

struct PFS_user_name {
 public:
  void reset() { m_name.reset(); }

  void set(const char *str, size_t len);

  void hash(uint64 *nr1, uint64 *nr2) const { m_name.hash(m_cs, nr1, nr2); }

  int sort(const PFS_user_name *other) const {
    return m_name.sort(m_cs, &other->m_name);
  }

  size_t length() const { return m_name.length(); }
  const char *ptr() const { return m_name.ptr(); }
  const CHARSET_INFO *charset() const { return m_cs; }

 private:
  static const CHARSET_INFO *m_cs;
  PFS_any_name<USERNAME_LENGTH> m_name;
};

struct PFS_host_name {
 public:
  void reset() { m_name.reset(); }

  void set(const char *str, size_t len);

  void hash(uint64 *nr1, uint64 *nr2) const { m_name.hash(m_cs, nr1, nr2); }

  int sort(const PFS_host_name *other) const {
    return m_name.sort(m_cs, &other->m_name);
  }

  size_t length() const { return m_name.length(); }
  const char *ptr() const { return m_name.ptr(); }
  const CHARSET_INFO *charset() const { return m_cs; }

 private:
  static const CHARSET_INFO *m_cs;
  PFS_any_name<HOSTNAME_LENGTH> m_name;
};

struct PFS_role_name {
 public:
  void reset() { m_name.reset(); }

  void set(const char *str, size_t len);

  void hash(uint64 *nr1, uint64 *nr2) const { m_name.hash(m_cs, nr1, nr2); }

  int sort(const PFS_role_name *other) const {
    return m_name.sort(m_cs, &other->m_name);
  }

  size_t length() const { return m_name.length(); }
  const char *ptr() const { return m_name.ptr(); }
  const CHARSET_INFO *charset() const { return m_cs; }

 private:
  static const CHARSET_INFO *m_cs;
  PFS_any_name<ROLENAME_LENGTH> m_name;
};

struct PFS_file_name {
 public:
  void reset() { m_name.reset(); }

  void set(const char *str, size_t len);

  void hash(uint64 *nr1, uint64 *nr2) const { m_name.hash(m_cs, nr1, nr2); }

  int sort(const PFS_file_name *other) const {
    return m_name.sort(m_cs, &other->m_name);
  }

  size_t length() const { return m_name.length(); }
  const char *ptr() const { return m_name.ptr(); }
  const CHARSET_INFO *charset() const { return m_cs; }

 private:
  static const CHARSET_INFO *m_cs;
  PFS_any_name<FN_REFLEN> m_name;
};

#endif
