/*
  Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_OPENSSL_TESTS_TEST_TLS_CLIENT_ACTIONS_H_
#define ROUTER_SRC_OPENSSL_TESTS_TEST_TLS_CLIENT_ACTIONS_H_

#include <cassert>
#include <cstdint>
#include <vector>

enum class Opt : std::uint32_t {
  none = 0,
  read = 1u << 0,
  write = 1u << 1,
  force_disconnect = 1u << 2,
  expect_disconnect = 1u << 3,
  reconfigure = 1u << 4,
};

constexpr Opt operator|(Opt a, Opt b) {
  return static_cast<Opt>(static_cast<std::uint32_t>(a) |
                          static_cast<std::uint32_t>(b));
}
constexpr Opt operator&(Opt a, Opt b) {
  return static_cast<Opt>(static_cast<std::uint32_t>(a) &
                          static_cast<std::uint32_t>(b));
}
constexpr Opt operator~(Opt a) {
  return static_cast<Opt>(~static_cast<std::uint32_t>(a));
}
constexpr Opt &operator|=(Opt &a, Opt b) { return a = (a | b); }
constexpr Opt &operator&=(Opt &a, Opt b) { return a = (a & b); }

class Action {
 public:
  Action() = default;

  Action(Opt set, const size_t transfer = 0)
      : set_{set}, transfer_bytes_{transfer} {
    assert(!(has(set_, Opt::read) && has(set_, Opt::write)) &&
           "Use either read or write");
  }

  Action(const Action &action) { *this = action; }

  Action &operator=(const Action &action) {
    set_ = action.set_;
    transfer_bytes_ = action.transfer_bytes_;

    return *this;
  }

  bool is_read_operation() const { return has(set_, Opt::read); }
  bool is_write_operation() const { return has(set_, Opt::write); }
  bool expect_disconnect() const { return has(set_, Opt::expect_disconnect); }
  bool must_disconnect() const { return has(set_, Opt::force_disconnect); }
  bool reconfigure() const { return has(set_, Opt::reconfigure); }

  size_t get_bytes_to_transfer() const { return transfer_bytes_; }
  void set_bytes_to_transfer(const size_t transfer_bytes) {
    transfer_bytes_ = transfer_bytes;
  }

  void transfered(const size_t bytes) { transfer_bytes_ -= bytes; }

 private:
  static bool has(Opt set, Opt flag) { return (set & flag) != Opt::none; }

  Opt set_{Opt::none};
  size_t transfer_bytes_{0};
};

class ActionRead : public Action {
 public:
  ActionRead(const size_t transfer = 0) : Action(Opt::read, transfer) {}
};

class ActionWrite : public Action {
 public:
  ActionWrite(const size_t transfer = 0) : Action(Opt::write, transfer) {}
};

class ActionExpectDisconnect : public Action {
 public:
  ActionExpectDisconnect() : Action(Opt::read | Opt::expect_disconnect, 1) {}
};

class ActionDisconnect : public Action {
 public:
  ActionDisconnect() : Action(Opt::force_disconnect, 0) {}
};

class ActionReconfigure : public Action {
 public:
  ActionReconfigure() : Action(Opt::reconfigure, 0) {}
};

inline size_t action_count_send(const std::vector<Action> &actions) {
  size_t result{0};

  for (const auto &a : actions) {
    if (!a.is_read_operation()) result += a.get_bytes_to_transfer();
  }

  return result;
}

inline int action_sequence_increment(const int block_size,
                                     const int total_transfer,
                                     int &count_transfer) {
  auto bs = block_size;
  count_transfer += bs;
  if (count_transfer > total_transfer) {
    auto difference = count_transfer - total_transfer;
    count_transfer = total_transfer;
    bs -= difference;
  }
  return bs;
}

template <typename... Act>
void action_sequence_push_back(const int block_size, const int total_transfer,
                               std::vector<Action> &result, int &count_transfer,
                               int &i) {
  Action new_elements[sizeof...(Act)] = {Act()...};

  for (auto &act : new_elements) {
    if (count_transfer >= total_transfer) return;

    auto bs =
        action_sequence_increment(block_size, total_transfer, count_transfer);
    act.set_bytes_to_transfer(bs);
    result.push_back(act);
    i++;
  }
}

template <typename... Act>
std::vector<Action> generate_action_sequence(const int total_transfer,
                                             const int block_size) {
  std::vector<Action> result;
  auto no_of_blocks = total_transfer / block_size;
  auto count_transfer = 0;

  if (total_transfer % total_transfer) ++no_of_blocks;

  int i = 0;
  while (i < no_of_blocks) {
    action_sequence_push_back<Act...>(block_size, total_transfer, result,
                                      count_transfer, i);
  }

  return result;
}

#endif  // ROUTER_SRC_OPENSSL_TESTS_TEST_TLS_CLIENT_ACTIONS_H_
