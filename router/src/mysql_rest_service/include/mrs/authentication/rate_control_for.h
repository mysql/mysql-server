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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_AUTHENTICATION_HELPER_RATE_CONTROL_FOR_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_AUTHENTICATION_HELPER_RATE_CONTROL_FOR_H_

#include <chrono>
#include <map>
#include <mutex>
#include <optional>

namespace mrs {
namespace authentication {

enum class BlockReason { kNone, kTooFast, kRateExceeded };

struct AcceptInfo {
 public:
  using milliseconds = std::chrono::milliseconds;

  BlockReason reason;
  milliseconds next_request_allowed_after;
};

template <typename ControlType, uint64_t measure_time_in_seconds = 60>
class RateControlFor {
 public:
  using clock = std::chrono::steady_clock;
  using time_point = clock::time_point;
  using milliseconds = std::chrono::milliseconds;
  using seconds = std::chrono::seconds;

  struct ControlEntry {
    uint64_t number_of_requests{0};
    time_point started_counting;
    std::optional<time_point> blocked_at;
    time_point access_time;

    void reset_blocking() {
      number_of_requests = 1;
      started_counting = clock::now();
      blocked_at.reset();
    }
  };

 public:
  RateControlFor(std::optional<uint64_t> block_after_rate, seconds block_for,
                 std::optional<milliseconds> minimum_time_between_requests)
      : block_for_{std::chrono::duration_cast<milliseconds>(block_for)},
        block_after_{block_after_rate},
        minimum_time_between_requests_{minimum_time_between_requests} {}

  RateControlFor() : RateControlFor(10, seconds{30}, {}) {}

  RateControlFor &operator=(const RateControlFor &other) {
    auto lock = std::unique_lock<std::mutex>(entries_mutex_);
    entries_ = other.entries_;
    block_for_ = other.block_for_;
    block_after_ = other.block_after_;
    minimum_time_between_requests_ = other.minimum_time_between_requests_;
    return *this;
  }

  void clear() {
    auto lock = std::unique_lock<std::mutex>(entries_mutex_);

    for (auto it = entries_.begin(); it != entries_.end();) {
      ControlEntry &entry = it->second;
      if (duration_now(entry.started_counting) >= seconds(1)) {
        it = entries_.erase(it);
        continue;
      }

      if (duration_now(entry.blocked_at) >= seconds(1)) {
        it = entries_.erase(it);
        continue;
      }

      ++it;
    }
  }

  bool allow(const ControlType &ct, AcceptInfo *info = nullptr) {
    if (!block_after_ && !minimum_time_between_requests_) return true;
    auto lock = std::unique_lock<std::mutex>(entries_mutex_);

    auto it = entries_.find(ct);
    if (it == entries_.end()) {
      entries_.emplace(
          std::make_pair(ct, ControlEntry{1, clock::now(), {}, clock::now()}));
      return true;
    }

    ControlEntry &entry = it->second;

    auto result = allow_impl(entry, info);

    entry.access_time = clock::now();

    return result;
  }

  size_t size() const { return entries_.size(); }

 private:
  bool allow_impl(ControlEntry &entry, AcceptInfo *info) {
    if (!allow_check_blocked(entry, info)) return false;

    return allow_check_too_fast(entry, info);
  }

  bool allow_check_too_fast(ControlEntry &entry, AcceptInfo *info) {
    if (!minimum_time_between_requests_) return true;

    auto time_between_requests = (clock::now() - entry.access_time);

    if (time_between_requests < minimum_time_between_requests_.value()) {
      if (info) {
        info->reason = BlockReason::kTooFast;
        info->next_request_allowed_after =
            minimum_time_between_requests_.value();
      }
      return false;
    }

    return true;
  }

  bool allow_check_blocked(ControlEntry &entry, AcceptInfo *info) {
    if (!block_after_) return true;

    if (entry.blocked_at) {
      if (duration_now(entry.blocked_at.value()) >= block_for_) {
        entry.reset_blocking();
        return true;
      }

      if (info) {
        info->reason = BlockReason::kRateExceeded;
        info->next_request_allowed_after =
            block_for_ - std::chrono::duration_cast<milliseconds>(
                             clock::now() - entry.blocked_at.value());
      }

      return false;
    }

    if (duration_now(entry.started_counting) >=
        seconds(measure_time_in_seconds)) {
      entry.reset_blocking();
      return true;
    }

    if (++entry.number_of_requests > block_after_) {
      entry.blocked_at = clock::now();
      if (info) {
        info->reason = BlockReason::kRateExceeded;
        info->next_request_allowed_after = block_for_;
      }
      return false;
    }

    return true;
  }

  static milliseconds duration_now(const std::optional<time_point> &value) {
    if (!value) return milliseconds{0};
    return duration_now(value.value());
  }

  static milliseconds duration_now(const time_point &value) {
    return std::chrono::duration_cast<milliseconds>(clock::now() - value);
  }

  std::mutex entries_mutex_;
  std::map<ControlType, ControlEntry> entries_;
  milliseconds block_for_;
  std::optional<uint64_t> block_after_{};
  std::optional<milliseconds> minimum_time_between_requests_;
};

}  // namespace authentication
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_AUTHENTICATION_HELPER_RATE_CONTROL_FOR_H_
