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

#ifndef INCLUDE_MYSQL_STRINGS_COLLATIONS_H_
#define INCLUDE_MYSQL_STRINGS_COLLATIONS_H_

#include <cstddef>
#include <string>

#include "mysql/strings/api.h"

class MY_CHARSET_LOADER;
struct CHARSET_INFO;

namespace mysql {
namespace collation {

/**
  Initialize character set/collation library

  @note initialize() after shutdown() is UB (untested).

  @param charset_dir    Optional "/\0"-terminated path to the directory
                        containing Index.xml
  @param loader         Optional user-specified hooks to the character
                        set/collation parser/initializer.
*/
MYSQL_STRINGS_EXPORT void initialize(const char *charset_dir = nullptr,
                                     MY_CHARSET_LOADER *loader = nullptr);

/**
  Shutdown character set/collation library

  This call is mainly necessary in ASAN etc. builds.

  @note initialize() after shutdown() is UB (untested).
*/
MYSQL_STRINGS_EXPORT void shutdown();

/**
  Normalizes character set/collation names
*/
class MYSQL_STRINGS_EXPORT Name {
 public:
  /**
    Constructor

    @note throws std::bad_alloc
  */
  explicit Name(const char *name);

  /**
    Constructor

    @note throws std::bad_alloc
  */
  Name(const char *name, size_t size);

  /**
    Constructor

    @note throws std::bad_alloc
  */
  Name(const Name &);

  Name(Name &&) noexcept;

  ~Name();

  Name &operator=(const Name &);
  Name &operator=(Name &&) noexcept;

  /**
    Returns normalized name as std::string
  */
  std::string operator()() const { return m_normalized; }

 private:
  const char *m_normalized{nullptr};
};

/**
  Find collation by its name

  @param name           Collation name

  @returns pointer to a collation object on success, nullptr if not found
*/
MYSQL_STRINGS_EXPORT const CHARSET_INFO *find_by_name(const Name &name);

/**
  Find collation by its name

  @param name           '\0'-terminated string of collation name
                        (not normalized name is fine)

  @returns pointer to a collation object on success, nullptr if not found
*/
inline const CHARSET_INFO *find_by_name(const char *name) {
  return find_by_name(Name{name});
}

/**
  Find collation by its number

  @param id             Collation id (hardcoded in library sources or
                        specified in Index.xml)

  @returns pointer to a collation object on success, nullptr if not found
*/
MYSQL_STRINGS_EXPORT const CHARSET_INFO *find_by_id(unsigned id);

/**
  Find primary collation by its character set name

  @param cs_name        Character set name

  @returns pointer to a collation object on success, nullptr if not found
*/
MYSQL_STRINGS_EXPORT const CHARSET_INFO *find_primary(Name cs_name);

/**
  Find primary collation by its character set name

  @param cs_name        '\0'-terminated string of character set name
                        (not normalized name is fine)

  @returns pointer to a collation object on success, nullptr if not found
*/
inline const CHARSET_INFO *find_primary(const char *cs_name) {
  return find_primary(Name{cs_name});
}

/**
  Find binary collation by its character set name

  @param cs_name        Character set name

  @returns pointer to a collation object on success, nullptr if not found
*/
MYSQL_STRINGS_EXPORT const CHARSET_INFO *find_default_binary(
    const Name &cs_name);

/**
  Find binary collation by its character set name

  @param cs_name        '\0'-terminated character set name
                        (not normalized name is fine)

  @returns pointer to a collation object on success, nullptr if not found
*/
inline const CHARSET_INFO *find_default_binary(const char *cs_name) {
  return find_default_binary(Name{cs_name});
}

}  // namespace collation
}  // namespace mysql

#endif  // INCLUDE_MYSQL_STRINGS_COLLATIONS_H_
