/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_OPENSSL_TESTS_TEST_TLS_CLIENT_ASYNC_CLIENT_H_
#define ROUTER_SRC_OPENSSL_TESTS_TEST_TLS_CLIENT_ASYNC_CLIENT_H_

#include <atomic>
#include <vector>

#include "mysql/harness/net_ts.h"
#include "test_tls/client/actions.h"
#include "tls/tls_stream.h"

template <typename StreamTest>
class AsyncClient {
 public:
  template <typename Container>
  class AdapterRange {
   public:
    using const_iterator = typename Container::const_iterator;
    AdapterRange(const const_iterator &begin, const const_iterator &end)
        : begin_{begin}, end_{end} {}

    const_iterator begin() const { return begin_; }
    const_iterator end() const { return end_; }

   private:
    const_iterator begin_;
    const_iterator end_;
  };

 public:
  using VectorOfActions = std::vector<Action>;
  using RangeOfActions = AdapterRange<VectorOfActions>;

 public:
  // Source data could be split into different actions, still the current
  // solution is easier to compare result vs expected array in the test.
  AsyncClient(net::io_context *io_context, std::atomic<int> *running_io,
              StreamTest *tls_stream, const std::vector<uint8_t> *source_data,
              const std::vector<Action> &actions)
      : io_context_{io_context},
        running_io_{running_io},
        tls_stream_{tls_stream},
        source_data_{source_data},
        actions_{actions} {
    io_context_->get_executor().post([this]() { execute(); }, nullptr);
  }

  const std::vector<uint8_t> &get_received_data() const {
    return destination_data_;
  }

 private:
  void execute() {
    bool repeat = false;

    RangeOfActions range(actions_.begin() + actions_offset_, actions_.end());
    for (auto &action : range) {
      ++actions_offset_;
      action_current_ = action;

      if (action_current_.must_disconnect()) {
        tls_stream_->close();
        break;
      }

      if (action_current_.is_read_operation()) {
        const auto size_new =
            action_current_.get_bytes_to_transfer() + destination_data_.size();
        destination_data_.resize(size_new);
        repeat = do_receive({}, 0);
      } else {
        repeat = do_send({}, 0);
      }

      if (!repeat) break;
    }

    if (0 == action_current_.get_bytes_to_transfer() &&
        actions_offset_ == actions_.size() && !decremented_.exchange(true)) {
      if (1 == running_io_->fetch_sub(1)) {
        io_context_->stop();
      }
    }
  }

  bool do_receive(std::error_code ec, size_t count) {
    action_current_.transfered(count);

    if (ec) {
      if (action_current_.expect_disconnect()) {
        if (count != 0)
          throw std::runtime_error(
              "Expected disconnection, still received data.");

        // The buffer was already expanded, still we didn't receive
        // the data.
        destination_data_.resize(destination_data_.size() -
                                 action_current_.get_bytes_to_transfer());
      }

      throw ec;
    }

    const auto bytes_needed = action_current_.get_bytes_to_transfer();
    if (0 == bytes_needed) {
      return true;
    }

    const auto size = destination_data_.size();
    const auto offset = size - bytes_needed;
    recv_buffer_ = net::buffer(&destination_data_[offset], bytes_needed);

    tls_stream_->async_receive(recv_buffer_,
                               [this](std::error_code ec, size_t s) {
                                 io_context_->get_executor().post(
                                     [ec, s, this]() {
                                       if (do_receive(ec, s)) execute();
                                     },
                                     nullptr);
                               });

    return false;
  }

  bool do_send(std::error_code ec, size_t count) {
    action_current_.transfered(count);
    source_offset_ += count;

    if (ec) throw std::logic_error(ec.message());

    if (0 == action_current_.get_bytes_to_transfer()) {
      return true;
    }

    send_buffer_ = net::buffer(&(*source_data_)[source_offset_],
                               action_current_.get_bytes_to_transfer());

    tls_stream_->async_send(send_buffer_, [this](std::error_code ec, size_t s) {
      if (do_send(ec, s)) execute();
    });
    return false;
  }

  net::mutable_buffer recv_buffer_;
  net::const_buffer send_buffer_;
  net::io_context *io_context_;
  std::atomic<int> *running_io_;
  StreamTest *tls_stream_;

  const std::vector<uint8_t> *source_data_;
  size_t source_offset_{0};

  std::vector<uint8_t> destination_data_;
  std::atomic<bool> decremented_{false};

  const VectorOfActions actions_;
  size_t actions_offset_{0};
  Action action_current_;
};

#endif  // ROUTER_SRC_OPENSSL_TESTS_TEST_TLS_CLIENT_ASYNC_CLIENT_H_
