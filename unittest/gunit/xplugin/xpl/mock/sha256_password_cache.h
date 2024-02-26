/*
 * Copyright (c) 2017, 2023, Oracle and/or its affiliates.
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

#ifndef UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_SHA256_PASSWORD_CACHE_H_
#define UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_SHA256_PASSWORD_CACHE_H_

#include <gmock/gmock.h>

#include <string>
#include <utility>

#include "plugin/x/src/cache_based_verification.h"
#include "plugin/x/src/interface/sha256_password_cache.h"

namespace xpl {
namespace test {
namespace mock {

class Sha256_password_cache : public iface::SHA256_password_cache {
 public:
  Sha256_password_cache();
  virtual ~Sha256_password_cache() override;

  MOCK_METHOD3(upsert, bool(const std::string &, const std::string &,
                            const std::string &));
  MOCK_METHOD(bool, remove, (const std::string &, const std::string &),
              (override));
  using Entry = std::pair<bool, std::string>;
  MOCK_METHOD(Entry, get_entry, (const std::string &, const std::string &),
              (const, override));
  MOCK_METHOD(bool, contains,
              (const std::string &, const std::string &, const std::string &),
              (const, override));
  MOCK_METHOD(std::size_t, size, (), (const, override));
  MOCK_METHOD(void, clear, (), (override));
  MOCK_METHOD(void, enable, (), (override));
  MOCK_METHOD(void, disable, (), (override));
};

class Cache_based_verification : public xpl::Cache_based_verification {
 public:
  explicit Cache_based_verification(xpl::iface::SHA256_password_cache *cache);
  virtual ~Cache_based_verification() override;

  MOCK_METHOD(const std::string &, get_salt, (), (const, override));
};

}  // namespace mock
}  // namespace test
}  // namespace xpl

#endif  // UNITTEST_GUNIT_XPLUGIN_XPL_MOCK_SHA256_PASSWORD_CACHE_H_
