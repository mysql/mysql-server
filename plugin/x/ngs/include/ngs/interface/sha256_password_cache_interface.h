/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
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

#ifndef NGS_SHA256_PASSWORD_CACHE_INTERFACE_H_
#define NGS_SHA256_PASSWORD_CACHE_INTERFACE_H_

#include <utility>
#include <string>

namespace ngs {

class SHA256_password_cache_interface {
public:
  virtual bool upsert(const std::string &user, const std::string &host,
                      const std::string &value) = 0;
  virtual bool remove(const std::string &user, const std::string &host) = 0;
  virtual std::pair<bool, std::string> get_entry(const std::string &user,
      const std::string &host) const = 0;
  virtual bool contains(const std::string &user, const std::string &host,
                        const std::string &value) const = 0;
  virtual std::size_t size() const = 0;
  virtual void clear() = 0;
  virtual void enable() = 0;
  virtual void disable() = 0;
  virtual ~SHA256_password_cache_interface() = default;
};

}  // namespace ngs

#endif  // NGS_SHA256_PASSWORD_CACHE_INTERFACE_H_

