/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_DOCUMENT_ID_GENERATOR_INTERFACE_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_DOCUMENT_ID_GENERATOR_INTERFACE_H_

#include <cstdio>
#include <string>

namespace ngs {

class Document_id_generator_interface {
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
  virtual ~Document_id_generator_interface() = default;
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_DOCUMENT_ID_GENERATOR_INTERFACE_H_
