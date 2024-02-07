/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef STRINGS_COLLATIONS_INTERNAL_H_
#define STRINGS_COLLATIONS_INTERNAL_H_

#include <cstddef>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

#include "mysql/strings/m_ctype.h"

constexpr char MY_CHARSET_INDEX[]{"Index.xml"};

typedef int myf;

namespace mysql {

namespace collation {
class Name;
}  // namespace collation

namespace collation_internals {

/**
  Helper class: implementation of character set/collation library

  @see mysql::collation_internals::entry.
*/
class Collations final {
 public:
  Collations(const Collations &) = delete;
  Collations &operator=(const Collations &) = delete;

  /**
    Constructor

    @param charset_dir  Optional "/\0"-terminated path to the directory
                        containing Index.xml
    @param loader       Optional user-specified hooks to the character
                        set/collation parser/initializer.
  */
  explicit Collations(const char *charset_dir,
                      MY_CHARSET_LOADER *loader = nullptr);

  ~Collations();

  /**
    Finds collation by its name

    @note Forces collation parsing/initialization if not done yet.

    @param name         Collation name

    @param flags        Optional mysys-specific flags

    @param [out] errmsg Optional buffer to return error message from
                        collation parser/initializer

    @returns pointer to a collation object on success, nullptr if not found
  */
  CHARSET_INFO *find_by_name(const mysql::collation::Name &name, myf flags = 0,
                             MY_CHARSET_ERRMSG *errmsg = nullptr);

  /**
    Finds collation by its number

    @note Forces collation parsing/initialization if not done yet.

    @param id           Collation id (hardcoded in library sources or
                        specified in Index.xml)

    @param flags        Optional mysys-specific flags

    @param [out] errmsg Optional buffer to return error message from
                        collation parser/initializer

    @returns pointer to a collation object on success, nullptr if not found
  */
  CHARSET_INFO *find_by_id(unsigned id, myf flags = 0,
                           MY_CHARSET_ERRMSG *errmsg = nullptr);

  /**
    Finds primary collation by its character set name

    @note Forces collation parsing/initialization if not done yet.

    @param cs_name      Character set name

    @param flags        Optional mysys-specific flags

    @param [out] errmsg Optional buffer to return error message from
                        collation parser/initializer

    @returns pointer to a collation object on success, nullptr if not found
  */
  CHARSET_INFO *find_primary(const mysql::collation::Name &cs_name,
                             myf flags = 0,
                             MY_CHARSET_ERRMSG *errmsg = nullptr);

  /**
    Finds binary collation by its character set name

    @note Forces collation parsing/initialization if not done yet.

    @param cs_name      Character set name

    @param flags        Optional mysys-specific flags

    @param [out] errmsg Optional buffer to return error message from
                        collation parser/initializer

    @returns pointer to a collation object on success, nullptr if not found
  */
  CHARSET_INFO *find_default_binary(const mysql::collation::Name &cs_name,
                                    myf flags = 0,
                                    MY_CHARSET_ERRMSG *errmsg = nullptr);

  /**
    Finds collation by its name and returns its id

    @param name         Collation name

    @returns collation id
  */
  unsigned get_collation_id(const mysql::collation::Name &name) const;

  /**
    Finds character set by its name and returns an id of its primary collation

    @param name         Collation name

    @returns primary collation id
  */
  unsigned get_primary_collation_id(const mysql::collation::Name &name) const;

  /**
    Finds character set by its name and returns an id of its default binary
    collation

    @param name         Collation name

    @returns default binary collation id
  */
  unsigned get_default_binary_collation_id(
      const mysql::collation::Name &name) const;

  /**
    If not done yet, force collation parsing/initialization under m_mutex lock

    @param cs           Pointer to collation object

    @param flags        Optional mysys-specific flags

    @param [out] errmsg Optional buffer to return error message from
                        collation parser/initializer

    @returns @p cs on success, otherwise nullptr
  */
  CHARSET_INFO *safe_init_when_necessary(CHARSET_INFO *cs, myf flags = 0,
                                         MY_CHARSET_ERRMSG *errmsg = nullptr);

  /**
    Like find_by_name but without initialization of return value

    @param name         Collation name

    @returns Pointer to CHARSET_INFO object on success, nullptr if not found.
             The resulting value can point to a half-initialized object.
             Moreover, further initialization of that object or parsing
             of its collation XML can fail.
  */
  CHARSET_INFO *find_by_name_unsafe(const mysql::collation::Name &name);

  /**
    For registering compile-time collations

    @param cs Collation object

    @returns false on success, otherwise true.
  */
  bool add_internal_collation(CHARSET_INFO *cs);

  /**
    Iterate over all collation objects known to the library

    @param f    Closure to execute on each collation object known to the library
  */
  void iterate(const std::function<void(const CHARSET_INFO *)> &f) {
    for (const auto &i : m_all_by_collation_name) {
      f(i.second);
    }
  }

 protected:
  /**
    Internals of safe_init_when_necessary()

    This function is similar to safe_init_when_necessary, but, unlike
    safe_init_when_necessary(), it doesn't acquire locks.

    @param cs           Pointer to collation object

    @param flags        Optional mysys-specific flags

    @param [out] errmsg Optional buffer to return error message from
                        collation parser/initializer

    @returns @p cs on success, otherwise nullptr
  */
  CHARSET_INFO *unsafe_init(CHARSET_INFO *cs, myf flags,
                            MY_CHARSET_ERRMSG *errmsg);

  /**
    Optional '/'-terminated path to the directory containing Index.xml
  */
  const std::string m_charset_dir;

  /**
    Common parametric type to map character set/collation names or their ids
    to CHARSET_INFO object pointers

    @tparam Key         Name or id type (std::string or unsigned respectively)

    TODO(gleb): it would be good to use mysql::collation::Name instead of
                std::string for Key.
  */
  template <typename Key>
  using Hash = std::unordered_map<Key, CHARSET_INFO *>;

  /**
    Maps collation ids to CHARSET_INFO object pointers
  */
  Hash<unsigned> m_all_by_id;

  /**
    Maps normalized strings of all known character set names, collation names,
    and their aliases to CHARSET_INFO object pointers

    @note see old_conv and get_old_charset_by_name() for exclusions
    @see old_conv(), get_old_charset_by_name()
  */
  Hash<std::string> m_all_by_collation_name;

  /**
    Maps normalized strings of character set names to CHARSET_INFO object
    pointers

    @note In MySQL, CHARSET_INFO object of character set is also an object
    of its primary collation.
  */
  Hash<std::string> m_primary_by_cs_name;

  /**
    Maps normalized strings of character set names to CHARSET_INFO objects
    of preferred binary collations

    @note utf8mb4 has two separate binary collations, so m_binary_by_cs_name
          contains a reference to utf8mb4_bin only.
  */
  Hash<std::string> m_binary_by_cs_name;

  /**
    False if m_loader references external MY_CHARSET_LOADER, otherwise true.
  */
  const bool m_owns_loader;

  /**
    Shared MY_CHARSET_LOADER implementation for use in collation parser and
    initializer

    By default references an instance of mysql::collation_internals::Loader.
  */
  MY_CHARSET_LOADER *m_loader;

 private:
  /**
    Collation parser/initializer mutex

    The library parses collations and initializes CHARSET_INFO objects in
    depth on demand, so m_mutex is necessary to guarantee a safety of
    concurrent find_... function calls.
  */
  std::mutex m_mutex;
};

/**
  Global entry point to character set/collation library internals
*/
extern Collations *entry;

}  // namespace collation_internals
}  // namespace mysql

#endif  // STRINGS_COLLATIONS_INTERNAL_H_
