/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_GTID_MANAGER_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_GTID_MANAGER_H_

#include <chrono>
#include <map>
#include <mutex>
#include <numeric>
#include <shared_mutex>
#include <vector>

#include "helper/json/rapid_json_to_struct.h"
#include "helper/json/text_to.h"
#include "helper/to_string.h"
#include "mrs/database/helper/gtid.h"

#include "mysql/harness/logging/logging.h"
#include "tcp_address.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {

enum class GtidAction { k_is_on_server, k_not_found, k_needs_update };

class GtidManager {
 public:
  using clock = std::chrono::steady_clock;
  using duration = clock::duration;
  using Uid = database::GTIDuuid;
  using Gtid = database::Gtid;
  using GtidSet = database::GtidSet;
  using TCPaddress = mysql_harness::TCPAddress;

  class AddressContext {
   public:
    bool needs_refresh{false};
    clock::time_point last_update;
    std::map<Uid, GtidSet> all_sets;
    std::shared_mutex mutex_gtid_access_;
    uint64_t initial_size_{0};
    bool requested_initialization_{false};
    bool requested_update_{false};

    bool get_gtidset_by_gtid_uid(const Uid &uid, GtidSet **set) {
      auto it = all_sets.find(uid);
      if (it == all_sets.end()) return false;

      *set = &(*it).second;

      return true;
    }

    uint64_t calculate_gtid_items() const {
      return std::accumulate(
          all_sets.begin(), all_sets.end(), 0,
          [](auto v, auto &kv) { return v + kv.second.size(); });
    }
  };

 public:
  GtidManager() {}

  void configure(const std::string &json_config) {
    auto cnf = parse_json_options(json_config);
    enable_ = cnf.enable.value_or(true);
    refresh_timeout_ = cnf.d.value_or(std::chrono::minutes(1));
    refresh_after_ = cnf.refresh_after;
    using helper::to_string;
    using std::to_string;

    //    log_debug("GtidManager - configured with:");
    //    log_debug("GtidManager::enable_: %s", to_string(enable_).c_str());
    //    log_debug("GtidManager::timeout_: %s",
    //              to_string(refresh_timeout_.count()).c_str());
    //    log_debug("GtidManager::after_: %s",
    //    to_string(refresh_after_).c_str());
  }

  GtidAction is_executed_on_server(const TCPaddress &addr, const Gtid &gtid) {
    if (!enable_) return GtidAction::k_not_found;
    //    log_debug("GtidManager - is_executed_on_server %s - %s",
    //    addr.str().c_str(),
    //              gtid.to_string().c_str());
    GtidSet *set;
    auto ctxt = get_context(addr);

    if (!ctxt->requested_initialization_) {
      ctxt->requested_initialization_ = true;
      return GtidAction::k_needs_update;
    }

    if (!ctxt->requested_update_) {
      if (needs_update(ctxt.get())) {
        ctxt->requested_update_ = true;
        return GtidAction::k_needs_update;
      }
    }

    auto l = std::shared_lock<std::shared_mutex>(ctxt->mutex_gtid_access_);

    if (!ctxt->get_gtidset_by_gtid_uid(gtid.get_uid(), &set))
      return GtidAction::k_not_found;

    return set->contains(gtid) ? GtidAction::k_is_on_server
                               : GtidAction::k_not_found;
  }

  void remember(const TCPaddress &addr, const Gtid &gtid) {
    if (!enable_) return;

    //    log_debug("GtidManager - remember (%s - %s)", addr.str().c_str(),
    //              gtid.to_string().c_str());

    GtidSet *set;
    auto ctxt = get_context(addr);
    auto l = std::unique_lock<std::shared_mutex>(ctxt->mutex_gtid_access_);

    if (!ctxt->get_gtidset_by_gtid_uid(gtid.get_uid(), &set)) {
      set = &ctxt->all_sets[gtid.get_uid()];
      set->set(gtid);
      return;
    }

    if (!set->try_merge(gtid)) set->insert(gtid);
  }

  bool needs_update(const TCPaddress &addr) {
    if (!enable_) return false;

    auto ctxt = get_context(addr);
    return needs_update(ctxt.get());
  }

  void reinitialize(const TCPaddress &addr, const std::vector<GtidSet> &sets) {
    if (!enable_) return;

    //    log_debug("GtidManager - reinitialize %s", addr.str().c_str());

    auto ctxt = get_context(addr);
    auto l = std::unique_lock<std::shared_mutex>(ctxt->mutex_gtid_access_);

    ctxt->all_sets.clear();
    for (auto &set : sets) {
      ctxt->all_sets[set.get_uid()] = set;
    }

    ctxt->needs_refresh = false;
    ctxt->last_update = std::chrono::steady_clock::now();
    ctxt->initial_size_ = ctxt->calculate_gtid_items();
    ctxt->requested_update_ = false;

    // Overwriting, the initialization could be done, without requesting.
    ctxt->requested_initialization_ = true;
  }

 private:
  class GtidOptions {
   public:
    std::optional<bool> enable;
    std::optional<duration> d;
    std::optional<uint64_t> refresh_after;
  };

  class ParseGtidOptions
      : public helper::json::RapidReaderHandlerToStruct<GtidOptions> {
   public:
    template <typename ValueType, bool default_value = false>
    bool to_bool(const ValueType &value) {
      const static std::map<std::string, bool> allowed_values{
          {"true", true}, {"false", false}, {"1", true}, {"0", false}};
      auto it = allowed_values.find(value);
      if (it != allowed_values.end()) {
        return it->second;
      }

      return default_value;
    }

    template <typename ValueType>
    uint64_t to_uint(const ValueType &value) {
      return std::stoull(value.c_str());
    }

    template <typename ValueType>
    void handle_object_value(const std::string &key, const ValueType &vt) {
      using std::to_string;
      if (key == "gtid.cache.enable") {
        result_.enable = to_bool(vt);
      } else if (key == "gtid.cache.refresh_rate") {
        result_.d = std::chrono::seconds{to_uint(vt)};
      } else if (key == "gtid.cache.refresh_when_increases_by") {
        result_.refresh_after = to_uint(vt);
      }
    }

    template <typename ValueType>
    void handle_value(const ValueType &vt) {
      const auto &key = get_current_key();
      if (is_object_path()) {
        handle_object_value(key, vt);
      }
    }

    bool String(const Ch *v, rapidjson::SizeType v_len, bool) override {
      handle_value(std::string{v, v_len});
      return true;
    }

    bool RawNumber(const Ch *v, rapidjson::SizeType v_len, bool) override {
      handle_value(std::string{v, v_len});
      return true;
    }

    bool Bool(bool v) override {
      const static std::string k_true{"true"}, k_false{"false"};
      handle_value(v ? k_true : k_false);
      return true;
    }
  };

  GtidOptions parse_json_options(const std::string &options) {
    return helper::json::text_to_handler<ParseGtidOptions>(options);
  }

  bool needs_update(AddressContext *ctxt) {
    if (refresh_after_.has_value()) {
      if (refresh_after_.value() <
          (ctxt->calculate_gtid_items() - ctxt->initial_size_))
        return true;
    }

    if (!refresh_timeout_.count()) return false;

    return refresh_timeout_ < (clock::now() - ctxt->last_update);
  }

  std::shared_ptr<AddressContext> get_context(const TCPaddress &addr) {
    std::shared_ptr<AddressContext> result;
    auto l = std::unique_lock(mutex_address_container_);

    auto it = address_context_.find(addr);
    if (it != address_context_.end()) {
      result = it->second;
    } else {
      result = std::make_shared<AddressContext>();
      result->last_update = clock::now();
      address_context_[addr] = result;
    }

    return result;
  }

  bool enable_{true};
  duration refresh_timeout_{};
  std::optional<uint64_t> refresh_after_{};
  std::mutex mutex_address_container_;
  std::map<TCPaddress, std::shared_ptr<AddressContext>> address_context_;
};

}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_GTID_MANAGER_H_
