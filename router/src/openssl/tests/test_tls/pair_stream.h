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

#ifndef ROUTER_SRC_HTTP_TESTS_TEST_TLS_PAIR_STREAM_H_
#define ROUTER_SRC_HTTP_TESTS_TEST_TLS_PAIR_STREAM_H_

#include <condition_variable>

#include "mysql/harness/stdx/monitor.h"

/*
 * Its a stub stream that emulates LocalStream
 *
 * This class, was added to minimize IFDEF in testing code
 * (windows only tests vs other tests).
 */
class Pair_stream {
  using Io_result_type = stdx::expected<size_t, std::error_code>;

  struct Waitable_result {
    Io_result_type result_;
    bool result_set_{false};
  };

  // Generic interface for handling callbacks
  class Callable_base {
   public:
    virtual ~Callable_base() = default;

    virtual size_t execute(Pair_stream *parent, const size_t bytes) = 0;
  };

  // Class implementing concrete callback for corresponding instantiation
  // of `async_send` and `async_recv`.
  template <typename Buffer, typename Handler>
  class Callable : public Callable_base {
   public:
    Callable(const Buffer &buffer, Handler &&handler)
        : b_{buffer}, handler_{std::forward<Handler>(handler)} {}

    size_t execute(Pair_stream *parent, const size_t bytes) override {
      const bool broken_pipe = parent->closed_;

      if (!broken_pipe) {
        net::mutable_buffer source_buffer{&parent->input_[0], bytes};
        auto copied = net::buffer_copy(b_, source_buffer, bytes);
        auto b = parent->input_.begin();
        parent->input_.erase(b, b + copied);
        handler_({}, copied);
        return copied;
      }

      handler_(std::make_error_code(std::errc::broken_pipe), 0);
      return 0;
    }

   private:
    Buffer b_;
    Handler handler_;
  };

 public:
  using native_handle_type = std::nullptr_t;
  using protocol_type = std::nullptr_t;
  using endpoint_type = std::nullptr_t;

  Pair_stream(net::io_context &context) : context_{context} {}
  Pair_stream(net::io_context &context, Pair_stream *other)
      : context_{context} {
    interconnect(other);
  }

  auto connect([[maybe_unused]] const endpoint_type &endpoint) {}

  template <class CompletionToken>
  auto async_connect([[maybe_unused]] const endpoint_type &endpoint,
                     [[maybe_unused]] CompletionToken &&token) {}

  Pair_stream(Pair_stream &&another) : context_{another.context_} {
    other_ = another.other_;
    callback_ = another.callback_;

    input_ = std::move(another.input_);

    if (other_ && other_->other_) {
      other_->other_ = this;
    }
  }

  template <typename Buffer, typename Handler>
  void async_send(Buffer &buffer, Handler &&handler) {
    size_t size_new = 0;
    auto size_new_elements = net::buffer_size(buffer);

    if (other_) {
      std::lock_guard<std::mutex> lock(other_->input_mutex_);

      auto size_old = other_->input_.size();

      size_new = size_old + size_new_elements;

      if (size_new_elements) {
        other_->input_.resize(size_new);

        net::mutable_buffer dst{&other_->input_[size_old], size_new_elements};

        net::buffer_copy(dst, buffer);
      }
    }

    if (other_ && size_new) {
      other_->do_callback(size_new);
    }

    Handler h{std::forward<Handler>(handler)};
    context_.get_executor().post(
        [h = std::move(h), size_new_elements]() mutable {
          h(std::error_code{}, size_new_elements);
        },
        nullptr);
  }

  template <typename Buffer, typename Handler>
  void async_receive(Buffer &buffer, Handler &&handler) {
    size_t copied = 0;
    bool broken_pipe{false};

    {
      std::lock_guard<std::mutex> lock(input_mutex_);

      auto size_src = input_.size();
      broken_pipe = closed_;

      if (!broken_pipe) {
        if (size_src) {
          net::mutable_buffer source_buffer{&input_[0], size_src};
          copied = net::buffer_copy(buffer, source_buffer, size_src);

          auto begin = input_.begin();
          input_.erase(begin, begin + copied);
        } else {
          callback_ = new Callable<Buffer, Handler>(
              buffer, std::forward<Handler>(handler));
        }
      }
    }

    if (broken_pipe) {
      handler(std::make_error_code(std::errc::broken_pipe), 0);
    } else if (copied) {
      handler({}, copied);
    }
  }

  template <typename ConstBufferSequence>
  Io_result_type write_some(const ConstBufferSequence &buffers) {
    Waitable_result waitable_result;
    WaitableMonitor<Waitable_result *> waitable{&waitable_result};

    async_send(buffers, [&waitable_result](std::error_code ec, size_t size) {
      waitable_result.result_set_ = true;
      if (ec) {
        waitable_result.result_ = stdx::unexpected(ec);
        return;
      }

      waitable_result.result_ = size;
    });

    waitable.wait([](Waitable_result *wr) { return wr->result_set_; });

    return waitable_result.result_;
  }

  template <typename MutableBufferSequence>
  Io_result_type read_some(const MutableBufferSequence &buffers) {
    Waitable_result waitable_result;
    WaitableMonitor<Waitable_result *> waitable{&waitable_result};

    async_receive(buffers, [&waitable_result](std::error_code ec, size_t size) {
      waitable_result.result_set_ = true;
      if (ec) {
        waitable_result.result_ = stdx::unexpected(ec);
        return;
      }

      waitable_result.result_ = size;
    });

    waitable.wait([](Waitable_result *wr) { return wr->result_set_; });

    return waitable_result.result_;
  }

  std::error_code close() {
    {
      std::lock_guard<std::mutex> lock1(input_mutex_);
      closed_ = true;
      if (other_) {
        std::lock_guard<std::mutex> lock2(other_->input_mutex_);
        other_->closed_ = true;
      }
    }
    other_->do_callback(0);

    return {};
  }

 private:
  void interconnect(Pair_stream *other) {
    other_ = other;
    other->other_ = this;
  }

  size_t do_callback(const size_t bytes) {
    Callable_base *callback;

    {
      std::lock_guard<std::mutex> lock(input_mutex_);
      callback = callback_;
      callback_ = nullptr;
    }

    if (callback) return callback->execute(this, bytes);

    return 0;
  }

  net::io_context &context_;
  Pair_stream *other_ = nullptr;
  Callable_base *callback_ = nullptr;
  std::mutex input_mutex_;
  std::vector<uint8_t> input_;
  std::atomic<bool> closed_{false};
};

#endif  // ROUTER_SRC_HTTP_TESTS_TEST_TLS_PAIR_STREAM_H_
