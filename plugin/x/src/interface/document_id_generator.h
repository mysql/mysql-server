/*
 * Copyright (c) 2018, 2022, Oracle and/or its affiliates.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_SRC_INTERFACE_DOCUMENT_ID_GENERATOR_H_
#define PLUGIN_X_SRC_INTERFACE_DOCUMENT_ID_GENERATOR_H_

#include <cstdint>
#include <cstdio>
#include <string>

namespace xpl {
namespace iface {

class Document_id_generator {
 public:
  struct Variables {
    explicit Variables(const uint16_t p = 0, const uint16_t o = 1,
                       const uint16_t i = 1)
        : offset{o}, increment{i} {
      snprintf(prefix, sizeof(prefix), "%04x", p);
    }
    uint16_t offset, increment;
    char prefix[5];
  };

  virtual std::string generate(const Variables &) = 0;
  virtual ~Document_id_generator() = default;
};

}  // namespace iface
}  // namespace xpl

#endif  // PLUGIN_X_SRC_INTERFACE_DOCUMENT_ID_GENERATOR_H_
