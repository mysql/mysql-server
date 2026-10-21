/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
 * Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */
#ifndef BOOST_REDIS_REDIS_STREAM_HPP
#define BOOST_REDIS_REDIS_STREAM_HPP

#include <boost/redis/config.hpp>
#include <boost/redis/detail/connect_fsm.hpp>
#include <boost/redis/detail/connect_params.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/logger.hpp>

#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/ip/basic_resolver.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/assert.hpp>
#include <boost/system/error_code.hpp>

#include <utility>

namespace boost {
namespace redis {
namespace detail {

template <class Executor>
class redis_stream {
   asio::ssl::context ssl_ctx_;
   asio::ip::basic_resolver<asio::ip::tcp, Executor> resolv_;
   asio::ssl::stream<asio::basic_stream_socket<asio::ip::tcp, Executor>> stream_;
#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
   asio::basic_stream_socket<asio::local::stream_protocol, Executor> unix_socket_;
#endif
   typename asio::steady_timer::template rebind_executor<Executor>::other timer_;
   redis_stream_state st_;

   void reset_stream() { stream_ = {resolv_.get_executor(), ssl_ctx_}; }

   struct connect_op {
      redis_stream& obj_;
      connect_fsm fsm_;
      connect_params params_;

      template <class Self>
      void execute_action(Self& self, connect_action act)
      {
         // Prevent use-after-move errors
         auto& obj = this->obj_;
         auto params = this->params_;

         switch (act.type) {
            case connect_action_type::unix_socket_close:
#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
            {
               system::error_code ec;
               obj.unix_socket_.close(ec);
               (*this)(self, ec);  // This is a sync action
            }
#else
               BOOST_ASSERT(false);
#endif
               return;
            case connect_action_type::unix_socket_connect:
#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
               obj.unix_socket_.async_connect(
                  params.addr.unix_socket(),
                  asio::cancel_after(obj.timer_, params.connect_timeout, std::move(self)));
#else
               BOOST_ASSERT(false);
#endif
               return;

            case connect_action_type::tcp_resolve:
               obj.resolv_.async_resolve(
                  params.addr.tcp_address().host,
                  params.addr.tcp_address().port,
                  asio::cancel_after(obj.timer_, params.resolve_timeout, std::move(self)));
               return;
            case connect_action_type::ssl_stream_reset:
               obj.reset_stream();
               // this action does not require yielding. Execute the next action immediately
               (*this)(self);
               return;
            case connect_action_type::ssl_handshake:
               obj.stream_.async_handshake(
                  asio::ssl::stream_base::client,
                  asio::cancel_after(obj.timer_, params.ssl_handshake_timeout, std::move(self)));
               return;
            case connect_action_type::done:        self.complete(act.ec); break;
            // Connect should use the specialized handler, where resolver results are available
            case connect_action_type::tcp_connect:
            default:                               BOOST_ASSERT(false);
         }
      }

      // This overload will be used for connects
      template <class Self>
      void operator()(
         Self& self,
         system::error_code ec,
         const asio::ip::tcp::endpoint& selected_endpoint)
      {
         auto act = fsm_.resume(
            ec,
            selected_endpoint,
            obj_.st_,
            self.get_cancellation_state().cancelled());
         execute_action(self, act);
      }

      // This overload will be used for resolves
      template <class Self>
      void operator()(
         Self& self,
         system::error_code ec,
         asio::ip::tcp::resolver::results_type endpoints)
      {
         auto act = fsm_.resume(ec, endpoints, obj_.st_, self.get_cancellation_state().cancelled());
         if (act.type == connect_action_type::tcp_connect) {
            auto& obj = this->obj_;  // prevent use-after-move errors
            asio::async_connect(
               obj.stream_.next_layer(),
               std::move(endpoints),
               asio::cancel_after(obj.timer_, params_.connect_timeout, std::move(self)));
         } else {
            execute_action(self, act);
         }
      }

      template <class Self>
      void operator()(Self& self, system::error_code ec = {})
      {
         auto act = fsm_.resume(ec, obj_.st_, self.get_cancellation_state().cancelled());
         execute_action(self, act);
      }
   };

public:
   explicit redis_stream(Executor ex, asio::ssl::context&& ssl_ctx)
   : ssl_ctx_{std::move(ssl_ctx)}
   , resolv_{ex}
   , stream_{ex, ssl_ctx_}
#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
   , unix_socket_{ex}
#endif
   , timer_{std::move(ex)}
   { }

   // Executor. Required to satisfy the AsyncStream concept
   using executor_type = Executor;
   executor_type get_executor() noexcept { return resolv_.get_executor(); }

   // Accessors
   const auto& get_ssl_context() const noexcept { return ssl_ctx_; }
   bool is_open() const
   {
#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
      if (st_.type == transport_type::unix_socket)
         return unix_socket_.is_open();
#endif
      return stream_.next_layer().is_open();
   }
   auto& next_layer() { return stream_; }
   const auto& next_layer() const { return stream_; }

   // I/O
   template <class CompletionToken>
   auto async_connect(const connect_params& params, buffered_logger& l, CompletionToken&& token)
   {
      this->st_.type = params.addr.type();
      return asio::async_compose<CompletionToken, void(system::error_code)>(
         connect_op{*this, connect_fsm{l}, params},
         token);
   }

   // These functions should only be used with callbacks (e.g. within async_compose function bodies)
   template <class ConstBufferSequence, class CompletionToken>
   void async_write_some(const ConstBufferSequence& buffers, CompletionToken&& token)
   {
      switch (st_.type) {
         case transport_type::tcp:
         {
            stream_.next_layer().async_write_some(buffers, std::forward<CompletionToken>(token));
            break;
         }
         case transport_type::tcp_tls:
         {
            stream_.async_write_some(buffers, std::forward<CompletionToken>(token));
            break;
         }
#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
         case transport_type::unix_socket:
         {
            unix_socket_.async_write_some(buffers, std::forward<CompletionToken>(token));
            break;
         }
#endif
         default: BOOST_ASSERT(false);
      }
   }

   template <class MutableBufferSequence, class CompletionToken>
   void async_read_some(const MutableBufferSequence& buffers, CompletionToken&& token)
   {
      switch (st_.type) {
         case transport_type::tcp:
         {
            return stream_.next_layer().async_read_some(
               buffers,
               std::forward<CompletionToken>(token));
            break;
         }
         case transport_type::tcp_tls:
         {
            return stream_.async_read_some(buffers, std::forward<CompletionToken>(token));
            break;
         }
#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
         case transport_type::unix_socket:
         {
            unix_socket_.async_read_some(buffers, std::forward<CompletionToken>(token));
            break;
         }
#endif
         default: BOOST_ASSERT(false);
      }
   }

   // Cancels resolve operations. Resolve operations don't support per-operation
   // cancellation, but resolvers have a cancel() function. Resolve operations are
   // in general blocking and run in a separate thread. cancel() has effect only
   // if the operation hasn't started yet. Still, trying is better than nothing
   void cancel_resolve() { resolv_.cancel(); }
};

}  // namespace detail
}  // namespace redis
}  // namespace boost

#endif
