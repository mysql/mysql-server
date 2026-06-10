/*
  Copyright (c) 2026, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_MOCK_HOST_RESOLVER_SRC_MOCK_RESOLVER_H_
#define ROUTER_SRC_MOCK_HOST_RESOLVER_SRC_MOCK_RESOLVER_H_

#include <string>
#include <system_error>
#include <vector>

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/resolver/error_code.h"
#include "mysql/harness/resolver/interface.h"
#include "mysql/harness/utility/wait_variable.h"

using Hostname = std::string;

template <typename ValueType>
using WaitableVariable = mysql_harness::utility::WaitableVariable<ValueType>;

class Entry {
 public:
  Entry() {}
  Entry(const Entry &other) { *this = other; }

  Entry &operator=(const Entry &other) {
    fail_not_found_ = other.fail_not_found_;
    error_ = other.error_;
    log_ = other.log_;
    addresses_ = other.addresses_;
    called_ = other.called_.load();
    wait_.set(other.wait_.get());

    return *this;
  }

  bool fail_not_found_{false};
  bool error_{false};
  bool log_{false};
  std::vector<net::ip::address> addresses_;
  std::atomic<uint64_t> called_{0};
  WaitableVariable<uint64_t> wait_{0};
};

IMPORT_LOG_FUNCTIONS()

using ResolveActions = std::map<Hostname, Entry>;

class ChangeShouldWait {
 public:
  void operator()(uint64_t &value) const {
    if (value > 0) {
      must_wait_ = value;
      value = 0;
      return;
    }
  }

  explicit operator bool() const { return 0 != must_wait_; }
  std::chrono::seconds wait() const { return std::chrono::seconds{must_wait_}; }

 private:
  mutable uint64_t must_wait_{0};
};

class MockResolver : public mysql_harness::resolver::ResolverInterface {
 public:
  MockResolver(ResolveActions actions) : actions_{actions} {}

  static std::string action(const Entry &entry) {
    if (entry.fail_not_found_) return "not-found";
    if (entry.error_) return "error";
    return "no-of-addresses-" + std::to_string(entry.addresses_.size());
  }

  ResolveHostResult resolve_host(const std::string &hostname,
                                 [[maybe_unused]] CachePolicy cache_policy =
                                     CachePolicy::UseIfPresent) override {
    auto it = actions_.find(hostname);
    if (it == actions_.end()) {
      log_error("MockResolver - unexpected resolve of '%s'", hostname.c_str());
      return stdx::unexpected(std::make_error_code(std::errc::not_supported));
    }

    auto &entry = it->second;
    ++(entry.called_);

    ChangeShouldWait should_wait;
    entry.wait_.change(should_wait);

    if (should_wait) {
      log_info("must_wait_for other request for '%s'", hostname.c_str());
      std::this_thread::sleep_for(should_wait.wait());
    }

    if (entry.log_) {
      log_info("MockResolver - mocked resolve of host '%s', returning %s",
               hostname.c_str(), action(entry).c_str());
    }

    if (entry.error_) {
      // Any non not-found error.
      return stdx::unexpected(
          std::make_error_code(std::errc::invalid_argument));
    }

    if (entry.fail_not_found_) {
      return stdx::unexpected(mysql_harness::resolver::make_error_code(
          mysql_harness::resolver::ErrcResolveResult::NotFound));
    }

    mysql_harness::resolver::ResolvedAddresses result;
    result.addresses = entry.addresses_;
    return result;
  }

 private:
  ResolveActions actions_;
};

#endif  // ROUTER_SRC_MOCK_HOST_RESOLVER_SRC_MOCK_RESOLVER_H_
