/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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

