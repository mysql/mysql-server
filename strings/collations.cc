/* Copyright (c) 2020, 2026, Oracle and/or its affiliates.

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

#include "mysql/strings/collations.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>

#include "mysql/strings/m_ctype.h"
#include "strings/collations_internal.h"

// TODO(geb): MY_CS_BUFFER_SIZE seems unreasonably large -- MY_CS_NAME_SIZE-1
//            should be enough.
constexpr static size_t MY_CS_BUFFER_SIZE =
    static_cast<size_t>(MY_CS_NAME_SIZE) * 8;

mysql::collation::Name::Name(const char *name)
    : mysql::collation::Name::Name(name, name != nullptr ? strlen(name) : 0) {}

mysql::collation::Name::Name(const char *name, size_t size) {
  if (name == nullptr) {
    // TODO(gleb): throw std::invalid_argument instead?
    assert(false);
    name = "";
  }

  // TODO(gleb): fail instead of truncating too long names?
  const size_t truncated_size = std::min(size, MY_CS_BUFFER_SIZE);
  m_normalized.reserve(truncated_size);

  for (size_t i = 0; i < truncated_size; i++) {
    // TODO(gleb): use ASCII instead of Latin1?
    m_normalized.push_back(static_cast<char>(
        my_charset_latin1.to_lower[static_cast<uint8_t>(name[i])]));
  }
}

void mysql::collation::initialize(const char *charset_dir,
                                  MY_CHARSET_LOADER *loader) {
  assert(mysql::collation_internals::entry == nullptr);
  mysql::collation_internals::entry =
      new mysql::collation_internals::Collations{charset_dir, loader};
}

void mysql::collation::shutdown() {
  delete mysql::collation_internals::entry;
  mysql::collation_internals::entry = nullptr;
}

static auto entry() { return mysql::collation_internals::entry; }

const CHARSET_INFO *mysql::collation::find_by_name(const Name &name) {
  return entry()->find_by_name(name);
}

const CHARSET_INFO *mysql::collation::find_by_id(unsigned id) {
  return entry()->find_by_id(id);
}

const CHARSET_INFO *mysql::collation::find_primary(Name cs_name) {
  return entry()->find_primary(cs_name);
}
