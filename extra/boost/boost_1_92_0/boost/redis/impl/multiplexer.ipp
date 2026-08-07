/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/ignore.hpp>
#include <boost/redis/request.hpp>

#include <boost/asio/error.hpp>
#include <boost/assert.hpp>

#include <cstddef>
#include <memory>

namespace boost::redis::detail {

multiplexer::elem::elem(request const& req, any_adapter adapter)
: req_{&req}
, adapter_{std::move(adapter)}
, remaining_responses_{req.get_expected_responses()}
, status_{status::waiting}
, ec_{}
, read_size_{0}
{ }

auto multiplexer::elem::notify_error(system::error_code ec) noexcept -> void
{
   if (!ec_) {
      ec_ = ec;
   }

   notify_done();
}

auto multiplexer::elem::commit_response(std::size_t read_size) -> void
{
   read_size_ += read_size;
   --remaining_responses_;
}

void multiplexer::elem::mark_abandoned()
{
   req_ = nullptr;
   adapter_ = any_adapter();  // A default-constructed any_adapter ignores all nodes
   set_done_callback([] { });
}

multiplexer::multiplexer()
{
   // Reserve some memory to avoid excessive memory allocations in
   // the first reads.
   read_buffer_.reserve(4096u);
}

void multiplexer::cancel(std::shared_ptr<elem> const& ptr)
{
   if (ptr->is_waiting()) {
      // We can safely remove it from the queue, since it hasn't been sent yet
      reqs_.erase(std::remove(std::begin(reqs_), std::end(reqs_), ptr), std::end(reqs_));
   } else {
      // Removing the request would cause trouble when the response arrived.
      // Mark it as abandoned, so the response is discarded when it arrives
      ptr->mark_abandoned();
   }
}

bool multiplexer::commit_write(std::size_t bytes_written)
{
   BOOST_ASSERT(!cancel_run_called_);
   BOOST_ASSERT(bytes_written + write_offset_ <= write_buffer_.size());

   usage_.bytes_sent += bytes_written;
   write_offset_ += bytes_written;

   // Are there still more bytes to write?
   if (write_offset_ < write_buffer_.size())
      return false;

   // We've written all the bytes in the write buffer.
   write_buffer_.clear();

   // There is small optimization possible here: traverse only the
   // partition of unwritten requests instead of them all.
   std::for_each(std::begin(reqs_), std::end(reqs_), [](auto const& ptr) {
      BOOST_ASSERT_MSG(ptr != nullptr, "Expects non-null pointer.");
      if (ptr->is_staged()) {
         ptr->mark_written();
      }
   });

   release_push_requests();

   return true;
}

void multiplexer::add(std::shared_ptr<elem> const& info)
{
   BOOST_ASSERT(!info->is_abandoned());

   reqs_.push_back(info);

   if (request_access::has_priority(info->get_request())) {
      auto rend = std::partition_point(std::rbegin(reqs_), std::rend(reqs_), [](auto const& e) {
         return e->is_waiting();
      });

      std::rotate(std::rbegin(reqs_), std::rbegin(reqs_) + 1, rend);
   }
}

consume_result multiplexer::consume_impl(system::error_code& ec)
{
   // We arrive here in two states:
   //
   //    1. While we are parsing a message. In this case we
   //       don't want to determine the type of the message in the
   //       buffer (i.e. response vs push) but leave it untouched
   //       until the parsing of a complete message ends.
   //
   //    2. On a new message, in which case we have to determine
   //       whether the next message is a push or a response.
   //

   auto const data = read_buffer_.get_commited();
   BOOST_ASSERT(!data.empty());

   if (!on_push_)  // Prepare for new message.
      on_push_ = is_next_push(data);

   if (on_push_) {
      if (!resp3::parse(parser_, data, receive_adapter_, ec))
         return consume_result::needs_more;

      return consume_result::got_push;
   }

   BOOST_ASSERT(!reqs_.empty());
   BOOST_ASSERT(reqs_.front() != nullptr);
   BOOST_ASSERT(reqs_.front()->get_remaining_responses() != 0);
   BOOST_ASSERT(!reqs_.front()->is_waiting());

   if (!resp3::parse(parser_, data, reqs_.front()->get_adapter(), ec))
      return consume_result::needs_more;

   if (ec) {
      reqs_.front()->notify_error(ec);
      reqs_.pop_front();
      return consume_result::got_response;
   }

   reqs_.front()->commit_response(parser_.get_consumed());
   if (reqs_.front()->get_remaining_responses() == 0) {
      // Done with this request.
      reqs_.front()->notify_done();
      reqs_.pop_front();
   }

   return consume_result::got_response;
}

std::pair<consume_result, std::size_t> multiplexer::consume(system::error_code& ec)
{
   BOOST_ASSERT(!cancel_run_called_);

   auto const ret = consume_impl(ec);
   auto const consumed = parser_.get_consumed();
   if (ec) {
      return std::make_pair(ret, consumed);
   }

   if (ret != consume_result::needs_more) {
      parser_.reset();
      auto const res = read_buffer_.consume(consumed);
      commit_usage(ret == consume_result::got_push, res);
      return std::make_pair(ret, res);
   }

   return std::make_pair(consume_result::needs_more, consumed);
}

auto multiplexer::prepare_read() -> system::error_code
{
   auto const res = read_buffer_.prepare(append_size_);
   usage_.bytes_rotated += res.rotated;
   return res.ec;
}

auto multiplexer::get_prepared_read_buffer() noexcept -> read_buffer::span_type
{
   return read_buffer_.get_prepared();
}

void multiplexer::commit_read(std::size_t bytes_read) { read_buffer_.commit(bytes_read); }

auto multiplexer::get_read_buffer_size() const noexcept -> std::size_t
{
   return read_buffer_.get_commited().size();
}

void multiplexer::reset()
{
   read_buffer_.clear();
   write_buffer_.clear();
   write_offset_ = 0u;
   parser_.reset();
   on_push_ = false;
   cancel_run_called_ = false;
}

std::size_t multiplexer::prepare_write()
{
   BOOST_ASSERT(!cancel_run_called_);

   // Coalesces the requests and marks them staged. After a
   // successful write staged requests will be marked as written.
   auto const point = std::partition_point(
      std::cbegin(reqs_),
      std::cend(reqs_),
      [](auto const& ri) {
         return !ri->is_waiting();
      });

   std::for_each(point, std::cend(reqs_), [this](const std::shared_ptr<elem>& ri) {
      // Stage the request.
      BOOST_ASSERT(!ri->is_abandoned());
      write_buffer_ += ri->get_request().payload();
      ri->mark_staged();
      usage_.commands_sent += ri->get_request().get_commands();
   });

   write_offset_ = 0u;

   auto const d = std::distance(point, std::cend(reqs_));
   return static_cast<std::size_t>(d);
}

std::size_t multiplexer::cancel_waiting()
{
   auto f = [](auto const& ptr) {
      BOOST_ASSERT(ptr != nullptr);
      return !ptr->is_waiting();
   };

   auto point = std::stable_partition(std::begin(reqs_), std::end(reqs_), f);

   auto const ret = std::distance(point, std::end(reqs_));

   std::for_each(point, std::end(reqs_), [](auto const& ptr) {
      ptr->notify_error({asio::error::operation_aborted});
   });

   reqs_.erase(point, std::end(reqs_));
   return ret;
}

void multiplexer::cancel_on_conn_lost()
{
   // Should only be called once per reconnection.
   // See https://github.com/boostorg/redis/issues/181
   BOOST_ASSERT(!cancel_run_called_);
   cancel_run_called_ = true;

   // Must return false if the request should be removed.
   auto cond = [](const std::shared_ptr<elem>& ptr) {
      BOOST_ASSERT(ptr != nullptr);

      // Abandoned requests only make sense because a response for them might arrive.
      // They should be discarded after the connection is lost
      if (ptr->is_abandoned())
         return false;

      if (ptr->is_waiting()) {
         return !ptr->get_request().get_config().cancel_on_connection_lost;
      } else {
         return !ptr->get_request().get_config().cancel_if_unresponded;
      }
   };

   auto point = std::stable_partition(std::begin(reqs_), std::end(reqs_), cond);

   std::for_each(point, std::end(reqs_), [](auto const& ptr) {
      ptr->notify_error({asio::error::operation_aborted});
   });

   reqs_.erase(point, std::end(reqs_));

   std::for_each(std::begin(reqs_), std::end(reqs_), [](auto const& ptr) {
      return ptr->mark_waiting();
   });
}

void multiplexer::commit_usage(bool is_push, std::size_t consumed)
{
   if (is_push) {
      usage_.pushes_received += 1;
      usage_.push_bytes_received += consumed;
      on_push_ = false;
   } else {
      usage_.responses_received += 1;
      usage_.response_bytes_received += consumed;
   }
}

bool multiplexer::is_next_push(std::string_view data) const noexcept
{
   // Useful links to understand the heuristics below.
   //
   // - https://github.com/redis/redis/issues/11784
   // - https://github.com/redis/redis/issues/6426
   // - https://github.com/boostorg/redis/issues/170

   // Test if the message resp3 type is a push.
   BOOST_ASSERT(!data.empty());
   if (resp3::to_type(data.front()) == resp3::type::push)
      return true;

   // This is non-push type and the requests queue is empty. I have
   // noticed this is possible, for example with -MISCONF. I don't
   // know why they are not sent with a push type so we can
   // distinguish them from responses to commands. If we are lucky
   // enough to receive them when the command queue is empty they
   // can be treated as server pushes, otherwise it is impossible
   // to handle them properly
   if (reqs_.empty())
      return true;

   // The request does not expect any response but we got one. This
   // may happen if for example, subscribe with wrong syntax.
   if (reqs_.front()->get_remaining_responses() == 0)
      return true;

   // Added to deal with MONITOR and also to fix PR170 which
   // happens under load and on low-latency networks, where we
   // might start receiving responses before the write operation
   // completed and the request is still marked as staged and not
   // written.
   return reqs_.front()->is_waiting();
}

void multiplexer::release_push_requests()
{
   auto point = std::stable_partition(
      std::begin(reqs_),
      std::end(reqs_),
      [](const std::shared_ptr<elem>& ptr) {
         return !(ptr->is_written() && ptr->get_remaining_responses() == 0u);
      });

   std::for_each(point, std::end(reqs_), [](auto const& ptr) {
      ptr->notify_done();
   });

   reqs_.erase(point, std::end(reqs_));
}

void multiplexer::set_receive_adapter(any_adapter adapter)
{
   receive_adapter_ = std::move(adapter);
}

void multiplexer::set_config(config const& cfg)
{
   append_size_ = cfg.read_buffer_append_size;
   read_buffer_.set_config({cfg.max_read_size});
}

auto make_elem(request const& req, any_adapter adapter) -> std::shared_ptr<multiplexer::elem>
{
   return std::make_shared<multiplexer::elem>(req, std::move(adapter));
}

}  // namespace boost::redis::detail
