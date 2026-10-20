/* Copyright (c) 2018-2025 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef BOOST_REDIS_CONNECTION_HPP
#define BOOST_REDIS_CONNECTION_HPP

#include <boost/redis/adapter/adapt.hpp>
#include <boost/redis/adapter/any_adapter.hpp>
#include <boost/redis/config.hpp>
#include <boost/redis/detail/connection_state.hpp>
#include <boost/redis/detail/exec_fsm.hpp>
#include <boost/redis/detail/exec_one_fsm.hpp>
#include <boost/redis/detail/multiplexer.hpp>
#include <boost/redis/detail/reader_fsm.hpp>
#include <boost/redis/detail/receive_fsm.hpp>
#include <boost/redis/detail/redis_stream.hpp>
#include <boost/redis/detail/run_fsm.hpp>
#include <boost/redis/detail/sentinel_resolve_fsm.hpp>
#include <boost/redis/detail/writer_fsm.hpp>
#include <boost/redis/error.hpp>
#include <boost/redis/logger.hpp>
#include <boost/redis/operation.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/resp3/type.hpp>
#include <boost/redis/response.hpp>
#include <boost/redis/usage.hpp>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/cancel_at.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/experimental/cancellation_condition.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/immediate.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>
#include <boost/assert.hpp>
#include <boost/config.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>

namespace boost::redis {
namespace detail {

// Given a timeout value, compute the expiry time. A zero timeout is considered to mean "no timeout"
inline std::chrono::steady_clock::time_point compute_expiry(
   std::chrono::steady_clock::duration timeout)
{
   return timeout.count() == 0 ? (std::chrono::steady_clock::time_point::max)()
                               : std::chrono::steady_clock::now() + timeout;
}

template <class Executor>
struct connection_impl {
   using clock_type = std::chrono::steady_clock;
   using clock_traits_type = asio::wait_traits<clock_type>;
   using timer_type = asio::basic_waitable_timer<clock_type, clock_traits_type, Executor>;

   using receive_channel_type = asio::experimental::channel<
      Executor,
      void(system::error_code, std::size_t)>;
   using exec_notifier_type = asio::experimental::channel<
      Executor,
      void(system::error_code, std::size_t)>;

   redis_stream<Executor> stream_;
   timer_type writer_timer_;     // timer used for write timeouts
   timer_type writer_cv_;        // condition variable, cancelled when there is new data to write
   timer_type reader_timer_;     // timer used for read timeouts
   timer_type reconnect_timer_;  // to wait the reconnection period
   timer_type ping_timer_;       // to wait between pings
   receive_channel_type receive_channel_;
   asio::cancellation_signal run_signal_;
   connection_state st_;

   using executor_type = Executor;

   executor_type get_executor() noexcept { return writer_cv_.get_executor(); }

   struct exec_op {
      connection_impl* obj_ = nullptr;
      std::shared_ptr<exec_notifier_type> notifier_ = nullptr;
      exec_fsm fsm_;

      template <class Self>
      void operator()(Self& self, system::error_code = {}, std::size_t = 0)
      {
         while (true) {
            // Invoke the state machine
            auto act = fsm_.resume(
               obj_->is_open(),
               obj_->st_,
               self.get_cancellation_state().cancelled());

            // Do what the FSM said
            switch (act.type()) {
               case exec_action_type::setup_cancellation:
                  self.reset_cancellation_state(asio::enable_total_cancellation());
                  continue;  // this action does not require yielding
               case exec_action_type::immediate:
                  asio::async_immediate(self.get_io_executor(), std::move(self));
                  return;
               case exec_action_type::notify_writer:
                  obj_->writer_cv_.cancel();
                  continue;  // this action does not require yielding
               case exec_action_type::wait_for_response:
                  notifier_->async_receive(std::move(self));
                  return;
               case exec_action_type::done:
                  notifier_.reset();
                  self.complete(act.error(), act.bytes_read());
                  return;
            }
         }
      }
   };

   connection_impl(Executor&& ex, asio::ssl::context&& ctx, logger&& lgr)
   : stream_{ex, std::move(ctx)}
   , writer_timer_{ex}
   , writer_cv_{ex}
   , reader_timer_{ex}
   , reconnect_timer_{ex}
   , ping_timer_{ex}
   , receive_channel_{ex, 256}
   , st_{{std::move(lgr)}}
   {
      set_receive_adapter(any_adapter{ignore});
      writer_cv_.expires_at((std::chrono::steady_clock::time_point::max)());
   }

   void cancel(operation op)
   {
      switch (op) {
         case operation::exec:    st_.mpx.cancel_waiting(); break;
         case operation::receive: cancel_receive_v2(); break;
         case operation::reconnection:
            st_.cfg.reconnect_wait_interval = std::chrono::seconds::zero();
            break;
         case operation::run:
         case operation::resolve:
         case operation::connect:
         case operation::ssl_handshake:
         case operation::health_check:  cancel_run(); break;
         case operation::all:
            st_.mpx.cancel_waiting();                                        // exec
            cancel_receive_v2();                                             // receive
            st_.cfg.reconnect_wait_interval = std::chrono::seconds::zero();  // reconnect
            cancel_run();                                                    // run
            break;
         default: /* ignore */;
      }
   }

   void cancel_receive_v1() { receive_channel_.cancel(); }

   void cancel_receive_v2()
   {
      st_.receive2_cancelled = true;
      cancel_receive_v1();
   }

   void cancel_run()
   {
      // Individual operations should see a terminal cancellation, regardless
      // of what we got requested. We take enough actions to ensure that this
      // doesn't prevent the object from being re-used (e.g. we reset the TLS stream).
      run_signal_.emit(asio::cancellation_type_t::terminal);

      // Name resolution doesn't support per-operation cancellation
      stream_.cancel_resolve();

      // Receive is technically not part of run, but we also cancel it for
      // backwards compatibility. Note that this intentionally affects v1 receive, only.
      cancel_receive_v1();
   }

   bool is_open() const noexcept { return stream_.is_open(); }

   bool will_reconnect() const noexcept
   {
      return st_.cfg.reconnect_wait_interval != std::chrono::seconds::zero();
   }

   template <class CompletionToken>
   auto async_exec(request const& req, any_adapter adapter, CompletionToken&& token)
   {
      auto notifier = std::make_shared<exec_notifier_type>(get_executor(), 1);
      auto info = make_elem(req, std::move(adapter));

      info->set_done_callback([notifier]() {
         notifier->try_send(std::error_code{}, 0);
      });

      return asio::async_compose<CompletionToken, void(system::error_code, std::size_t)>(
         exec_op{this, notifier, exec_fsm(std::move(info))},
         token,
         writer_cv_);
   }

   void set_receive_adapter(any_adapter adapter)
   {
      st_.mpx.set_receive_adapter(std::move(adapter));
   }

   std::size_t receive(system::error_code& ec)
   {
      std::size_t size = 0;

      auto f = [&](system::error_code const& ec2, std::size_t n) {
         ec = ec2;
         size = n;
      };

      auto const res = receive_channel_.try_receive(f);
      if (ec)
         return 0;

      if (!res)
         ec = error::sync_receive_push_failed;

      return size;
   }
};

template <class Executor>
struct receive2_op {
   connection_impl<Executor>* conn_;
   receive_fsm fsm_{};

   void drain_receive_channel()
   {
      // We don't expect any errors here. The only errors
      // that might appear in the channel are due to cancellations,
      // and these don't make sense with try_receive
      auto f = [](system::error_code, std::size_t) { };
      while (conn_->receive_channel_.try_receive(f))
         ;
   }

   template <class Self>
   void operator()(Self& self, system::error_code ec = {}, std::size_t /* push_bytes */ = 0u)
   {
      receive_action act = fsm_.resume(conn_->st_, ec, self.get_cancellation_state().cancelled());

      switch (act.type) {
         case receive_action::action_type::setup_cancellation:
            self.reset_cancellation_state(asio::enable_total_cancellation());
            (*this)(self);  // this action does not require yielding
            return;
         case receive_action::action_type::wait:
            conn_->receive_channel_.async_receive(std::move(self));
            return;
         case receive_action::action_type::drain_channel:
            drain_receive_channel();
            (*this)(self);  // this action does not require yielding
            return;
         case receive_action::action_type::immediate:
            asio::async_immediate(self.get_io_executor(), std::move(self));
            return;
         case receive_action::action_type::done: self.complete(act.ec); return;
      }
   }
};

template <class Executor>
struct exec_one_op {
   connection_impl<Executor>* conn_;
   const request* req_;
   exec_one_fsm fsm_;

   explicit exec_one_op(connection_impl<Executor>& conn, const request& req, any_adapter resp)
   : conn_(&conn)
   , req_(&req)
   , fsm_(std::move(resp), req.get_expected_responses())
   { }

   template <class Self>
   void operator()(Self& self, system::error_code ec = {}, std::size_t bytes_written = 0u)
   {
      exec_one_action act = fsm_.resume(
         conn_->st_.mpx,
         ec,
         bytes_written,
         self.get_cancellation_state().cancelled());

      switch (act.type) {
         case exec_one_action_type::done: self.complete(act.ec); return;
         case exec_one_action_type::write:
            asio::async_write(conn_->stream_, asio::buffer(req_->payload()), std::move(self));
            return;
         case exec_one_action_type::read_some:
            conn_->stream_.async_read_some(
               conn_->st_.mpx.get_read_buffer().get_prepared(),
               std::move(self));
            return;
      }
   }
};

template <class Executor, class CompletionToken>
auto async_exec_one(
   connection_impl<Executor>& conn,
   const request& req,
   any_adapter resp,
   CompletionToken&& token)
{
   return asio::async_compose<CompletionToken, void(system::error_code)>(
      exec_one_op<Executor>{conn, req, std::move(resp)},
      token,
      conn);
}

template <class Executor>
struct sentinel_resolve_op {
   connection_impl<Executor>* conn_;
   sentinel_resolve_fsm fsm_;

   explicit sentinel_resolve_op(connection_impl<Executor>& conn)
   : conn_(&conn)
   { }

   template <class Self>
   void operator()(Self& self, system::error_code ec = {})
   {
      auto* conn = conn_;  // Prevent potential use-after-move errors with cancel_after
      sentinel_action act = fsm_.resume(conn_->st_, ec, self.get_cancellation_state().cancelled());

      switch (act.get_type()) {
         case sentinel_action::type::done: self.complete(act.error()); return;
         case sentinel_action::type::connect:
            conn->stream_.async_connect(
               make_sentinel_connect_params(conn->st_.cfg, act.connect_addr()),
               conn->st_.logger,
               std::move(self));
            return;
         case sentinel_action::type::request:
            async_exec_one(
               *conn,
               conn->st_.cfg.sentinel.setup,
               make_sentinel_adapter(conn->st_),
               asio::cancel_after(
                  conn->reconnect_timer_,  // should be safe to re-use this
                  conn->st_.cfg.sentinel.request_timeout,
                  std::move(self)));
            return;
      }
   }
};

template <class Executor, class CompletionToken>
auto async_sentinel_resolve(connection_impl<Executor>& conn, CompletionToken&& token)
{
   return asio::async_compose<CompletionToken, void(system::error_code)>(
      sentinel_resolve_op<Executor>{conn},
      token,
      conn);
}

template <class Executor>
struct writer_op {
   connection_impl<Executor>* conn_;
   writer_fsm fsm_;

   explicit writer_op(connection_impl<Executor>& conn) noexcept
   : conn_(&conn)
   { }

   template <class Self>
   void operator()(Self& self, system::error_code ec = {}, std::size_t bytes_written = 0u)
   {
      auto* conn = conn_;  // Prevent potential use-after-move errors with cancel_after
      auto act = fsm_.resume(
         conn->st_,
         ec,
         bytes_written,
         self.get_cancellation_state().cancelled());

      switch (act.type()) {
         case writer_action_type::done: self.complete(act.error()); return;
         case writer_action_type::write_some:
            conn->stream_.async_write_some(
               asio::buffer(conn->st_.mpx.get_write_buffer()),
               asio::cancel_at(
                  conn->writer_timer_,
                  compute_expiry(act.timeout()),
                  std::move(self)));
            return;
         case writer_action_type::wait:
            conn->writer_cv_.expires_at(compute_expiry(act.timeout()));
            conn->writer_cv_.async_wait(std::move(self));
            return;
      }
   }
};

template <class Executor>
struct reader_op {
   connection_impl<Executor>* conn_;
   reader_fsm fsm_;

public:
   reader_op(connection_impl<Executor>& conn) noexcept
   : conn_{&conn}
   { }

   template <class Self>
   void operator()(Self& self, system::error_code ec = {}, std::size_t n = 0)
   {
      for (;;) {
         auto* conn = conn_;  // Prevent potential use-after-move errors with cancel_after
         auto act = fsm_.resume(conn->st_, n, ec, self.get_cancellation_state().cancelled());

         switch (act.get_type()) {
            case reader_fsm::action::type::read_some:
               conn->stream_.async_read_some(
                  asio::buffer(conn->st_.mpx.get_prepared_read_buffer()),
                  asio::cancel_at(
                     conn->reader_timer_,
                     compute_expiry(act.timeout()),
                     std::move(self)));
               return;
            case reader_fsm::action::type::notify_push_receiver:
               if (conn->receive_channel_.try_send(ec, act.push_size())) {
                  continue;
               } else {
                  conn->receive_channel_.async_send(ec, act.push_size(), std::move(self));
               }
               return;
            case reader_fsm::action::type::done: self.complete(act.error()); return;
         }
      }
   }
};

template <class Executor>
class run_op {
private:
   connection_impl<Executor>* conn_;
   run_fsm fsm_{};

   template <class CompletionToken>
   auto reader(CompletionToken&& token)
   {
      return asio::async_compose<CompletionToken, void(system::error_code)>(
         reader_op<Executor>{*conn_},
         std::forward<CompletionToken>(token),
         conn_->writer_cv_);
   }

   template <class CompletionToken>
   auto writer(CompletionToken&& token)
   {
      return asio::async_compose<CompletionToken, void(system::error_code)>(
         writer_op<Executor>{*conn_},
         std::forward<CompletionToken>(token),
         conn_->writer_cv_);
   }

public:
   run_op(connection_impl<Executor>* conn) noexcept
   : conn_{conn}
   { }

   // Called after the parallel group finishes
   template <class Self>
   void operator()(
      Self& self,
      std::array<std::size_t, 2u> order,
      system::error_code reader_ec,
      system::error_code writer_ec)
   {
      (*this)(self, order[0u] == 0u ? reader_ec : writer_ec);
   }

   template <class Self>
   void operator()(Self& self, system::error_code ec = {})
   {
      auto act = fsm_.resume(conn_->st_, ec, self.get_cancellation_state().cancelled());

      switch (act.type) {
         case run_action_type::done: self.complete(act.ec); return;
         case run_action_type::immediate:
            asio::async_immediate(self.get_io_executor(), std::move(self));
            return;
         case run_action_type::sentinel_resolve:
            async_sentinel_resolve(*conn_, std::move(self));
            return;
         case run_action_type::connect:
            conn_->stream_.async_connect(
               make_run_connect_params(conn_->st_),
               conn_->st_.logger,
               std::move(self));
            return;
         case run_action_type::parallel_group:
            asio::experimental::make_parallel_group(
               [this](auto token) {
                  return this->reader(token);
               },
               [this](auto token) {
                  return this->writer(token);
               })
               .async_wait(asio::experimental::wait_for_one(), std::move(self));
            return;
         case run_action_type::cancel_receive:
            conn_->receive_channel_.cancel();
            (*this)(self);  // this action does not require suspending
            return;
         case run_action_type::wait_for_reconnection:
            conn_->reconnect_timer_.expires_after(conn_->st_.cfg.reconnect_wait_interval);
            conn_->reconnect_timer_.async_wait(std::move(self));
            return;
         default: BOOST_ASSERT(false);
      }
   }
};

logger make_stderr_logger(logger::level lvl, std::string prefix);

template <class Executor>
class run_cancel_handler {
   connection_impl<Executor>* conn_;

public:
   explicit run_cancel_handler(connection_impl<Executor>& conn) noexcept
   : conn_(&conn)
   { }

   void operator()(asio::cancellation_type_t cancel_type) const
   {
      // We support terminal and partial cancellation
      constexpr auto mask = asio::cancellation_type_t::terminal |
                            asio::cancellation_type_t::partial;

      if ((cancel_type & mask) != asio::cancellation_type_t::none) {
         conn_->cancel(operation::run);
      }
   }
};

}  // namespace detail

/** @brief A SSL connection to the Redis server.
 *
 *  This class keeps a healthy connection to the Redis instance where
 *  commands can be sent at any time. For more details, please see the
 *  documentation of each individual function.
 *
 *  @tparam Executor The executor type used to create any required I/O objects.
 */
template <class Executor>
class basic_connection {
public:
   using this_type = basic_connection<Executor>;

   /// (Deprecated) Type of the next layer
   BOOST_DEPRECATED("This typedef is deprecated, and will be removed with next_layer().")
   typedef asio::ssl::stream<asio::basic_stream_socket<asio::ip::tcp, Executor>> next_layer_type;

   /// The type of the executor associated to this object.
   using executor_type = Executor;

   /// Rebinds the socket type to another executor.
   template <class Executor1>
   struct rebind_executor {
      /// The connection type when rebound to the specified executor.
      using other = basic_connection<Executor1>;
   };

   /** @brief Constructor from an executor.
   *
   *  @param ex Executor used to create all internal I/O objects.
   *  @param ctx SSL context.
   *  @param lgr Logger configuration. It can be used to filter messages by level
   *             and customize logging. By default, `logger::level::info` messages
   *             and higher are logged to `stderr`.
   */
   explicit basic_connection(
      executor_type ex,
      asio::ssl::context ctx = asio::ssl::context{asio::ssl::context::tlsv12_client},
      logger lgr = {})
   : impl_(
        std::make_unique<detail::connection_impl<Executor>>(
           std::move(ex),
           std::move(ctx),
           std::move(lgr)))
   { }

   /** @brief Constructor from an executor and a logger.
    *
    *  @param ex Executor used to create all internal I/O objects.
    *  @param lgr Logger configuration. It can be used to filter messages by level
    *             and customize logging. By default, `logger::level::info` messages
    *             and higher are logged to `stderr`.
    *
    * An SSL context with default settings will be created.
    */
   basic_connection(executor_type ex, logger lgr)
   : basic_connection(
        std::move(ex),
        asio::ssl::context{asio::ssl::context::tlsv12_client},
        std::move(lgr))
   { }

   /**
    * @brief Constructor from an `io_context`.
    * 
    * @param ioc I/O context used to create all internal I/O objects.
    * @param ctx SSL context.
    * @param lgr Logger configuration. It can be used to filter messages by level
    *            and customize logging. By default, `logger::level::info` messages
    *            and higher are logged to `stderr`.
    */
   explicit basic_connection(
      asio::io_context& ioc,
      asio::ssl::context ctx = asio::ssl::context{asio::ssl::context::tlsv12_client},
      logger lgr = {})
   : basic_connection(ioc.get_executor(), std::move(ctx), std::move(lgr))
   { }

   /**
    * @brief Constructor from an `io_context` and a logger.
    * 
    * @param ioc I/O context used to create all internal I/O objects.
    * @param lgr Logger configuration. It can be used to filter messages by level
    *            and customize logging. By default, `logger::level::info` messages
    *            and higher are logged to `stderr`.
    */
   basic_connection(asio::io_context& ioc, logger lgr)
   : basic_connection(
        ioc.get_executor(),
        asio::ssl::context{asio::ssl::context::tlsv12_client},
        std::move(lgr))
   { }

   /// Returns the associated executor.
   executor_type get_executor() noexcept { return impl_->writer_cv_.get_executor(); }

   /** @brief Starts the underlying connection operations.
    *
    * This function establishes a connection to the Redis server and keeps
    * it healthy by performing the following operations:
    *
    *  @li For Sentinel deployments (`config::sentinel::addresses` is not empty),
    *      contacts Sentinels to obtain the address of the configured master.
    *  @li For TCP connections, resolves the server hostname passed in
    *      @ref boost::redis::config::addr.
    *  @li Establishes a physical connection to the server. For TCP connections,
    *      connects to one of the endpoints obtained during name resolution.
    *      For UNIX domain socket connections, it connects to @ref boost::redis::config::unix_socket.
    *  @li If @ref boost::redis::config::use_ssl is `true`, performs the TLS handshake.
    *  @li Executes the setup request, as defined by the passed @ref config object.
    *      By default, this is a `HELLO` command, but it can contain any other arbitrary
    *      commands. See the @ref config::setup docs for more info.
    *  @li Starts a health-check operation where `PING` commands are sent
    *      at intervals specified by
    *      @ref config::health_check_interval when the connection is idle.
    *      See the documentation of @ref config::health_check_interval for more info.
    *  @li Starts read and write operations. Requests issued using @ref async_exec
    *      before `async_run` is called will be written to the server immediately.
    *
    *  When a connection is lost for any reason, a new one is
    *  established automatically. To disable reconnection
    *  set @ref boost::redis::config::reconnect_wait_interval to zero.
    *
    *  The completion token must have the following signature
    *
    *  @code
    *  void f(system::error_code);
    *  @endcode
    *
    * @par Per-operation cancellation
    * This operation supports the following cancellation types:
    *
    *   @li `asio::cancellation_type_t::terminal`.
    *   @li `asio::cancellation_type_t::partial`.
    *
    * In both cases, cancellation is equivalent to calling @ref basic_connection::cancel
    * passing @ref operation::run as argument.
    *
    * After the operation completes, the token's associated cancellation slot
    * may still have a cancellation handler associated to this connection.
    * You should make sure to not invoke it after the connection has been destroyed.
    * This is consistent with what other Asio I/O objects do.
    *
    * For example on how to call this function refer to
    * cpp20_intro.cpp or any other example.
    *
    * @param cfg Configuration parameters.
    * @param token Completion token.
    */
   template <class CompletionToken = asio::default_completion_token_t<executor_type>>
   auto async_run(config const& cfg, CompletionToken&& token = {})
   {
      return asio::async_initiate<CompletionToken, void(system::error_code)>(
         run_initiation{impl_.get()},
         token,
         &cfg);
   }

   /**
    * @brief (Deprecated) Starts the underlying connection operations.
    * @copydetail async_run
    *
    * This function accepts an extra logger parameter. The passed `logger::lvl`
    * will be used, but `logger::fn` will be ignored. Instead, a function
    * that logs to `stderr` using `config::prefix` will be used.
    * This keeps backwards compatibility with previous versions.
    * Any logger configured in the constructor will be overriden.
    *
    * @par Deprecated
    * The logger should be passed to the connection's constructor instead of using this
    * function. Use the overload without a logger parameter, instead. This function is
    * deprecated and will be removed in subsequent releases.
    *
    * @param cfg Configuration parameters.
    * @param l Logger.
    * @param token Completion token.
    */
   template <class CompletionToken = asio::default_completion_token_t<executor_type>>
   BOOST_DEPRECATED(
      "The async_run overload taking a logger argument is deprecated. "
      "Please pass the logger to the connection's constructor, instead, "
      "and use the other async_run overloads.")
   auto async_run(config const& cfg, logger l, CompletionToken&& token = {})
   {
      set_stderr_logger(l.lvl, cfg);
      return async_run(cfg, std::forward<CompletionToken>(token));
   }

   /**
    * @brief (Deprecated) Starts the underlying connection operations.
    * @copydetail async_run
    *
    * Uses a default-constructed config object to run the connection.
    *
    * @par Deprecated
    * This function is deprecated and will be removed in subsequent releases.
    * Use the overload taking an explicit config object, instead.
    *
    * @param token Completion token.
    */
   template <class CompletionToken = asio::default_completion_token_t<executor_type>>
   BOOST_DEPRECATED(
      "Running without an explicit config object is deprecated."
      "Please create a config object and pass it to async_run.")
   auto async_run(CompletionToken&& token = {})
   {
      return async_run(config{}, std::forward<CompletionToken>(token));
   }

   /** @brief (Deprecated) Receives server side pushes asynchronously.
    *
    * When pushes arrive and there is no `async_receive` operation in
    * progress, pushed data, requests, and responses will be paused
    * until `async_receive` is called again. Apps will usually want
    * to call `async_receive` in a loop. 
    *
    * For an example see cpp20_subscriber.cpp. The completion token must
    * have the following signature
    *
    * @code
    * void f(system::error_code, std::size_t);
    * @endcode
    *
    * Where the second parameter is the size of the push received in
    * bytes.
    * 
    * @par Per-operation cancellation
    * This operation supports the following cancellation types:
    *
    *   @li `asio::cancellation_type_t::terminal`.
    *   @li `asio::cancellation_type_t::partial`.
    *   @li `asio::cancellation_type_t::total`.
    *
    * Calling `basic_connection::cancel(operation::receive)` will
    * also cancel any ongoing receive operations.
    * 
    * @param token Completion token.
    */
   template <class CompletionToken = asio::default_completion_token_t<executor_type>>
   BOOST_DEPRECATED("Please use async_receive2 instead.")
   auto async_receive(CompletionToken&& token = {})
   {
      return impl_->receive_channel_.async_receive(std::forward<CompletionToken>(token));
   }

   /** @brief Wait for server pushes asynchronously.
    *
    * This function suspends until at least one server push is received by the
    * connection. On completion an unspecified number of pushes will
    * have been added to the response object set with @ref
    * set_receive_response. Use the functions in the response object
    * to know how many messages they were received and consume them.
    *
    * To prevent receiving an unbound number of pushes the connection
    * blocks further read operations on the socket when 256 pushes
    * accumulate internally (we don't make any commitment to this
    * exact number). When that happens any `async_exec`s and
    * health-checks won't make any progress and the connection may
    * eventually timeout. To avoid this, apps that expect server pushes
    * should call this function continuously in a loop.
    *
    * This function should be used instead of the deprecated @ref async_receive.
    * It differs from `async_receive` in the following:
    *
    * @li `async_receive` is designed to consume a single push message at a time.
    *     This can be inefficient when receiving lots of server pushes.
    *     `async_receive2` is batch-oriented. All pushes that are available
    *     when `async_receive2` is called will be marked as consumed.
    * @li `async_receive` is cancelled when a reconnection happens (e.g. because
    *     of a network error). This enabled the user to re-establish subscriptions
    *     using @ref async_exec before waiting for pushes again. With the introduction of
    *     functions like @ref request::subscribe, subscriptions are automatically
    *     re-established on reconnection. Thus, `async_receive2` is not cancelled
    *     on reconnection.
    * @li `async_receive` passes the number of bytes that each received
    *     push message contains. This information is unreliable and not very useful.
    *     Equivalent information is available using functions in the response object.
    * @li `async_receive` might get cancelled if `async_run` is cancelled.
    *     This doesn't happen with `async_receive2`.
    *
    * This function does *not* remove messages from the response object
    * passed to @ref set_receive_response - use the functions in the response
    * object to achieve this.
    *
    * Only a single instance of `async_receive2` may be outstanding
    * for a given connection at any time. Trying to start a second one
    * will fail with @ref error::already_running.
    *
    * @note To avoid deadlocks the task (e.g. coroutine) calling
    * `async_receive2` should not call `async_exec` in a way where
    * they could block each other. This is, avoid the following pattern:
    *
    * @code
    * asio::awaitable<void> receiver()
    * {
    *    // Do NOT do this!!! The receive buffer might get full while 
    *    // async_exec runs, which will block all read operations until async_receive2
    *    // is called. The two operations end up waiting each other, making the connection unresponsive.
    *    // If you need to do this, use two connections, instead.
    *    co_await conn.async_receive2();
    *    co_await conn.async_exec(req, resp);
    * }
    * @endcode
    *
    * For an example see cpp20_subscriber.cpp.
    *
    * The completion token must have the following signature:
    *
    * @code
    * void f(system::error_code);
    * @endcode
    *
    * @par Per-operation cancellation
    * This operation supports the following cancellation types:
    *
    *   @li `asio::cancellation_type_t::terminal`.
    *   @li `asio::cancellation_type_t::partial`.
    *   @li `asio::cancellation_type_t::total`.
    *
    * @param token Completion token.
    */
   template <class CompletionToken = asio::default_completion_token_t<executor_type>>
   auto async_receive2(CompletionToken&& token = {})
   {
      return asio::async_compose<CompletionToken, void(system::error_code)>(
         detail::receive2_op<Executor>{impl_.get()},
         token,
         *impl_);
   }

   /** @brief (Deprecated) Receives server pushes synchronously without blocking.
    *
    *  Receives a server push synchronously by calling `try_receive` on
    *  the underlying channel. If the operation fails because
    *  `try_receive` returns `false`, `ec` will be set to
    *  @ref boost::redis::error::sync_receive_push_failed.
    *
    *  @param ec Contains the error if any occurred.
    *  @returns The number of bytes read from the socket.
    */
   BOOST_DEPRECATED("Please, use async_receive2 instead.")
   std::size_t receive(system::error_code& ec) { return impl_->receive(ec); }

   /** @brief Executes commands on the Redis server asynchronously.
    *
    * This function sends a request to the Redis server and waits for
    * the responses to each individual command in the request. If the
    * request contains only commands that don't expect a response,
    * the completion occurs after it has been written to the
    * underlying stream.  Multiple concurrent calls to this function
    * will be automatically queued by the implementation.
    *
    * For an example see cpp20_echo_server.cpp.
    *
    * The completion token must have the following signature:
    *
    * @code
    * void f(system::error_code, std::size_t);
    * @endcode
    *
    * Where the second parameter is the size of the response received
    * in bytes.
    *
    * @par Per-operation cancellation
    * This operation supports per-operation cancellation. Depending on the state of the request
    * when cancellation is requested, we can encounter two scenarios:
    *
    *   @li If the request hasn't been sent to the server yet, cancellation will prevent it
    *       from being sent to the server. In this situation, all cancellation types are supported
    *       (`asio::cancellation_type_t::terminal`, `asio::cancellation_type_t::partial` and
    *       `asio::cancellation_type_t::total`).
    *   @li If the request has been sent to the server but the response hasn't arrived yet,
    *       cancellation will cause `async_exec` to complete immediately. When the response
    *       arrives from the server, it will be ignored. In this situation, only
    *       `asio::cancellation_type_t::terminal` and `asio::cancellation_type_t::partial`
    *       are supported. Cancellation requests specifying `asio::cancellation_type_t::total`
    *       only will be ignored.
    *
    * In any case, connections can be safely used after cancelling `async_exec` operations.
    *
    * @par Object lifetimes
    * Both `req` and `res` should be kept alive until the operation completes.
    * No copies of the request object are made.
    *
    * @param req The request to be executed.
    * @param resp The response object to parse data into.
    * @param token Completion token.
    */
   template <
      class Response = ignore_t,
      class CompletionToken = asio::default_completion_token_t<executor_type>>
   auto async_exec(request const& req, Response& resp = ignore, CompletionToken&& token = {})
   {
      return this->async_exec(req, any_adapter{resp}, std::forward<CompletionToken>(token));
   }

   /** @brief Executes commands on the Redis server asynchronously.
    *
    * This function sends a request to the Redis server and waits for
    * the responses to each individual command in the request. If the
    * request contains only commands that don't expect a response,
    * the completion occurs after it has been written to the
    * underlying stream.  Multiple concurrent calls to this function
    * will be automatically queued by the implementation.
    *
    * For an example see cpp20_echo_server.cpp.
    *
    * The completion token must have the following signature:
    *
    * @code
    * void f(system::error_code, std::size_t);
    * @endcode
    *
    * Where the second parameter is the size of the response received
    * in bytes.
    *
    * @par Per-operation cancellation
    * This operation supports per-operation cancellation. Depending on the state of the request
    * when cancellation is requested, we can encounter two scenarios:
    *
    *   @li If the request hasn't been sent to the server yet, cancellation will prevent it
    *       from being sent to the server. In this situation, all cancellation types are supported
    *       (`asio::cancellation_type_t::terminal`, `asio::cancellation_type_t::partial` and
    *       `asio::cancellation_type_t::total`).
    *   @li If the request has been sent to the server but the response hasn't arrived yet,
    *       cancellation will cause `async_exec` to complete immediately. When the response
    *       arrives from the server, it will be ignored. In this situation, only
    *       `asio::cancellation_type_t::terminal` and `asio::cancellation_type_t::partial`
    *       are supported. Cancellation requests specifying `asio::cancellation_type_t::total`
    *       only will be ignored.
    *
    * In any case, connections can be safely used after cancelling `async_exec` operations.
    *
    * @par Object lifetimes
    * Both `req` and any response object referenced by `adapter`
    * should be kept alive until the operation completes.
    * No copies of the request object are made.
    *
    * @param req The request to be executed.
    * @param adapter An adapter object referencing a response to place data into.
    * @param token Completion token.
    */
   template <class CompletionToken = asio::default_completion_token_t<executor_type>>
   auto async_exec(request const& req, any_adapter adapter, CompletionToken&& token = {})
   {
      return impl_->async_exec(req, std::move(adapter), std::forward<CompletionToken>(token));
   }

   /** @brief Cancel operations.
    *
    *  @li `operation::exec`: cancels operations started with
    *  `async_exec`. Affects only requests that haven't been written
    *  yet.
    *  @li `operation::run`: cancels the `async_run` operation.
    *  @li `operation::receive`: cancels any ongoing calls to `async_receive`.
    *  @li `operation::all`: cancels all operations listed above.
    *
    *  @param op The operation to be cancelled.
    */
   void cancel(operation op = operation::all) { impl_->cancel(op); }

   /// Returns true if the connection will try to reconnect if an error is encountered.
   bool will_reconnect() const noexcept { return impl_->will_reconnect(); }

   /**
    * @brief (Deprecated) Returns the ssl context.
    * 
    * `ssl::context` has no const methods, so this function should not be called.
    * Any TLS configuration should be set up by passing an `ssl::context`
    * to the connection's constructor.
    *
    * @returns The SSL context.
    */
   BOOST_DEPRECATED(
      "ssl::context has no const methods, so this function should not be called. Set up any "
      "required TLS configuration before passing the ssl::context to the connection's constructor.")
   asio::ssl::context const& get_ssl_context() const noexcept
   {
      return impl_->stream_.get_ssl_context();
   }

   /**
    * @brief (Deprecated) Resets the underlying stream.
    * 
    * This function is no longer necessary and is currently a no-op.
    */
   BOOST_DEPRECATED(
      "This function is no longer necessary and is currently a no-op. connection resets the stream "
      "internally as required. This function will be removed in subsequent releases")
   void reset_stream() { }

   /**
    * @brief (Deprecated) Returns a reference to the next layer.
    *
    * This function returns a dummy object for connections using UNIX domain sockets.
    *
    * @par Deprecated
    * Accessing the underlying stream is deprecated and will be removed in the next release.
    * Use the other member functions to interact with the connection.
    *
    * @returns A reference to the underlying SSL stream object.
    */
   BOOST_DEPRECATED(
      "Accessing the underlying stream is deprecated and will be removed in the next release. Use "
      "the other member functions to interact with the connection.")
   auto& next_layer() noexcept { return impl_->stream_.next_layer(); }

   /**
    * @brief (Deprecated) Returns a reference to the next layer.
    *
    * This function returns a dummy object for connections using UNIX domain sockets.
    *
    * @par Deprecated
    * Accessing the underlying stream is deprecated and will be removed in the next release.
    * Use the other member functions to interact with the connection.
    *
    * @returns A reference to the underlying SSL stream object.
    */
   BOOST_DEPRECATED(
      "Accessing the underlying stream is deprecated and will be removed in the next release. Use "
      "the other member functions to interact with the connection.")
   auto const& next_layer() const noexcept { return impl_->stream_.next_layer(); }

   /**
    * @brief Sets the response object of @ref async_receive2 operations.
    *
    * Pushes received by the connection (concretely, by @ref async_run)
    * will be stored in `resp`. This happens even if @ref async_receive2
    * is not being called.
    *
    * `resp` should be able to accommodate the following message types:
    *
    *  @li Any kind of RESP3 pushes that the application might expect.
    *      This usually involves Pub/Sub messages of type `message`,
    *      `subscribe`, `unsubscribe`, `psubscribe` and `punsubscribe`.
    *      See <a href="https://redis.io/docs/latest/develop/pubsub/">this page</a>
    *      for more info.
    *  @li Any errors caused by failed `SUBSCRIBE` commands.
    *      Because of protocol oddities, these are placed in the receive buffer,
    *      rather than handed to `async_exec`.
    *  @li If your application is using `MONITOR`, simple strings.
    *
    * Because receive responses need to accommodate many different kind
    * of messages, it's advised to use one of the generic responses
    * (like @ref generic_flat_response). If a response can't accommodate
    * one of the received types, @ref async_run will exit with an error.
    *
    * Messages received before this function is called are discarded.
    *
    * @par Object lifetimes
    * `resp` should be kept alive until @ref async_run completes.
    */
   template <class Response>
   void set_receive_response(Response& resp)
   {
      impl_->set_receive_adapter(any_adapter{resp});
   }

   /// Returns connection usage information.
   usage get_usage() const noexcept { return impl_->st_.mpx.get_usage(); }

private:
   using clock_type = std::chrono::steady_clock;
   using clock_traits_type = asio::wait_traits<clock_type>;
   using timer_type = asio::basic_waitable_timer<clock_type, clock_traits_type, executor_type>;

   using receive_channel_type = asio::experimental::channel<
      executor_type,
      void(system::error_code, std::size_t)>;

   auto use_ssl() const noexcept { return impl_->cfg_.use_ssl; }

   // Used by both this class and connection
   void set_stderr_logger(logger::level lvl, const config& cfg)
   {
      impl_->st_.logger.lgr = detail::make_stderr_logger(lvl, cfg.log_prefix);
   }

   // Initiation for async_run. This is required because we need access
   // to the final handler (rather than the completion token) within the initiation,
   // to modify the handler's cancellation slot.
   struct run_initiation {
      detail::connection_impl<Executor>* self;

      using executor_type = Executor;
      executor_type get_executor() const noexcept { return self->get_executor(); }

      template <class Handler>
      void operator()(Handler&& handler, config const* cfg)
      {
         self->st_.cfg = *cfg;
         self->st_.mpx.set_config(*cfg);

         // If the token's slot has cancellation enabled, it should just emit
         // the cancellation signal in our connection. This lets us unify the cancel()
         // function and per-operation cancellation
         auto slot = asio::get_associated_cancellation_slot(handler);
         if (slot.is_connected()) {
            slot.template emplace<detail::run_cancel_handler<Executor>>(*self);
         }

         // Overwrite the token's cancellation slot: the composed operation
         // should use the signal's slot so we can generate cancellations in cancel()
         auto token_with_slot = asio::bind_cancellation_slot(
            self->run_signal_.slot(),
            std::forward<Handler>(handler));

         asio::async_compose<decltype(token_with_slot), void(system::error_code)>(
            detail::run_op<Executor>{self},
            token_with_slot,
            self->writer_cv_);
      }
   };

   friend class connection;

   std::unique_ptr<detail::connection_impl<Executor>> impl_;
};

/**  @brief A basic_connection that type erases the executor.
 *
 *  This connection type uses `asio::any_io_executor` and
 *  `asio::any_completion_token` to reduce compilation times.
 *
 *  For documentation of each member function see
 *  @ref boost::redis::basic_connection.
 */
class connection {
public:
   /// Executor type.
   using executor_type = asio::any_io_executor;

   /** @brief Constructor from an executor.
    *
    *  @param ex Executor used to create all internal I/O objects.
    *  @param ctx SSL context.
    *  @param lgr Logger configuration. It can be used to filter messages by level
    *             and customize logging. By default, `logger::level::info` messages
    *             and higher are logged to `stderr`.
    */
   explicit connection(
      executor_type ex,
      asio::ssl::context ctx = asio::ssl::context{asio::ssl::context::tlsv12_client},
      logger lgr = {});

   /** @brief Constructor from an executor and a logger.
    *
    *  @param ex Executor used to create all internal I/O objects.
    *  @param lgr Logger configuration. It can be used to filter messages by level
    *             and customize logging. By default, `logger::level::info` messages
    *             and higher are logged to `stderr`.
    *
    * An SSL context with default settings will be created.
    */
   connection(executor_type ex, logger lgr)
   : connection(
        std::move(ex),
        asio::ssl::context{asio::ssl::context::tlsv12_client},
        std::move(lgr))
   { }

   /**
    * @brief Constructor from an `io_context`.
    * 
    * @param ioc I/O context used to create all internal I/O objects.
    * @param ctx SSL context.
    * @param lgr Logger configuration. It can be used to filter messages by level
    *            and customize logging. By default, `logger::level::info` messages
    *            and higher are logged to `stderr`.
    */
   explicit connection(
      asio::io_context& ioc,
      asio::ssl::context ctx = asio::ssl::context{asio::ssl::context::tlsv12_client},
      logger lgr = {})
   : connection(ioc.get_executor(), std::move(ctx), std::move(lgr))
   { }

   /**
    * @brief Constructor from an `io_context` and a logger.
    * 
    * @param ioc I/O context used to create all internal I/O objects.
    * @param lgr Logger configuration. It can be used to filter messages by level
    *            and customize logging. By default, `logger::level::info` messages
    *            and higher are logged to `stderr`.
    */
   connection(asio::io_context& ioc, logger lgr)
   : connection(
        ioc.get_executor(),
        asio::ssl::context{asio::ssl::context::tlsv12_client},
        std::move(lgr))
   { }

   /// Returns the underlying executor.
   executor_type get_executor() noexcept { return impl_.get_executor(); }

   /**
    * @brief Calls @ref boost::redis::basic_connection::async_run.
    *
    * @param cfg Configuration parameters.
    * @param token Completion token.
    */
   template <class CompletionToken = asio::deferred_t>
   auto async_run(config const& cfg, CompletionToken&& token = {})
   {
      return asio::async_initiate<CompletionToken, void(boost::system::error_code)>(
         initiation{this},
         token,
         &cfg);
   }

   /**
    * @brief (Deprecated) Calls @ref boost::redis::basic_connection::async_run.
    *
    * This function accepts an extra logger parameter. The passed logger
    * will be used by the connection, overwriting any logger passed to the connection's
    * constructor.
    *
    * @par Deprecated
    * The logger should be passed to the connection's constructor instead of using this
    * function. Use the overload without a logger parameter, instead. This function is
    * deprecated and will be removed in subsequent releases.
    *
    * @param cfg Configuration parameters.
    * @param l Logger.
    * @param token Completion token.
    */
   template <class CompletionToken = asio::deferred_t>
   BOOST_DEPRECATED(
      "The async_run overload taking a logger argument is deprecated. "
      "Please pass the logger to the connection's constructor, instead, "
      "and use the other async_run overloads.")
   auto async_run(config const& cfg, logger l, CompletionToken&& token = {})
   {
      return asio::async_initiate<CompletionToken, void(boost::system::error_code)>(
         initiation{this},
         token,
         &cfg,
         std::move(l));
   }

   /// @copydoc basic_connection::async_receive
   template <class CompletionToken = asio::deferred_t>
   BOOST_DEPRECATED("Please use async_receive2 instead.")
   auto async_receive(CompletionToken&& token = {})
   {
      return impl_.async_receive(std::forward<CompletionToken>(token));
   }

   /// @copydoc basic_connection::async_receive2
   template <class CompletionToken = asio::deferred_t>
   auto async_receive2(CompletionToken&& token = {})
   {
      return asio::async_initiate<CompletionToken, void(boost::system::error_code)>(
         initiation{this},
         token);
   }

   /// @copydoc basic_connection::receive
   BOOST_DEPRECATED("Please use async_receive2 instead.")
   std::size_t receive(system::error_code& ec) { return impl_.impl_->receive(ec); }

   /**
    * @brief Calls @ref boost::redis::basic_connection::async_exec.
    *
    * @param req The request to be executed.
    * @param resp The response object to parse data into.
    * @param token Completion token.
    */
   template <class Response = ignore_t, class CompletionToken = asio::deferred_t>
   auto async_exec(request const& req, Response& resp = ignore, CompletionToken&& token = {})
   {
      return async_exec(req, any_adapter{resp}, std::forward<CompletionToken>(token));
   }

   /**
    * @brief Calls @ref boost::redis::basic_connection::async_exec.
    *
    * @param req The request to be executed.
    * @param adapter An adapter object referencing a response to place data into.
    * @param token Completion token.
    */
   template <class CompletionToken = asio::deferred_t>
   auto async_exec(request const& req, any_adapter adapter, CompletionToken&& token = {})
   {
      return asio::async_initiate<CompletionToken, void(boost::system::error_code, std::size_t)>(
         initiation{this},
         token,
         &req,
         std::move(adapter));
   }

   /// @copydoc basic_connection::cancel
   void cancel(operation op = operation::all);

   /// @copydoc basic_connection::will_reconnect
   bool will_reconnect() const noexcept { return impl_.will_reconnect(); }

   /// (Deprecated) Calls @ref boost::redis::basic_connection::next_layer.
   BOOST_DEPRECATED(
      "Accessing the underlying stream is deprecated and will be removed in the next release. Use "
      "the other member functions to interact with the connection.")
   asio::ssl::stream<asio::ip::tcp::socket>& next_layer() noexcept
   {
      return impl_.impl_->stream_.next_layer();
   }

   /// (Deprecated) Calls @ref boost::redis::basic_connection::next_layer.
   BOOST_DEPRECATED(
      "Accessing the underlying stream is deprecated and will be removed in the next release. Use "
      "the other member functions to interact with the connection.")
   asio::ssl::stream<asio::ip::tcp::socket> const& next_layer() const noexcept
   {
      return impl_.impl_->stream_.next_layer();
   }

   /// @copydoc basic_connection::reset_stream
   BOOST_DEPRECATED(
      "This function is no longer necessary and is currently a no-op. connection resets the stream "
      "internally as required. This function will be removed in subsequent releases")
   void reset_stream() { }

   /// @copydoc basic_connection::set_receive_response
   template <class Response>
   void set_receive_response(Response& response)
   {
      impl_.set_receive_response(response);
   }

   /// @copydoc basic_connection::get_usage
   usage get_usage() const noexcept { return impl_.get_usage(); }

   /// @copydoc basic_connection::get_ssl_context
   BOOST_DEPRECATED(
      "ssl::context has no const methods, so this function should not be called. Set up any "
      "required TLS configuration before passing the ssl::context to the connection's constructor.")
   asio::ssl::context const& get_ssl_context() const noexcept
   {
      return impl_.impl_->stream_.get_ssl_context();
   }

private:
   // Function object to initiate the async ops that use asio::any_completion_handler.
   // Required for asio::cancel_after to work.
   // Since all ops have different arguments, a single struct with different overloads is enough.
   struct initiation {
      connection* self;

      using executor_type = asio::any_io_executor;
      executor_type get_executor() const noexcept { return self->get_executor(); }

      template <class Handler>
      void operator()(Handler&& handler, config const* cfg, logger l)
      {
         self->async_run_impl(*cfg, std::move(l), std::forward<Handler>(handler));
      }

      template <class Handler>
      void operator()(Handler&& handler, config const* cfg)
      {
         self->async_run_impl(*cfg, std::forward<Handler>(handler));
      }

      template <class Handler>
      void operator()(Handler&& handler, request const* req, any_adapter&& adapter)
      {
         self->async_exec_impl(*req, std::move(adapter), std::forward<Handler>(handler));
      }

      template <class Handler>
      void operator()(Handler&& handler)
      {
         self->async_receive2_impl(std::forward<Handler>(handler));
      }
   };

   void async_run_impl(
      config const& cfg,
      logger&& l,
      asio::any_completion_handler<void(boost::system::error_code)> token);

   void async_run_impl(
      config const& cfg,
      asio::any_completion_handler<void(boost::system::error_code)> token);

   void async_exec_impl(
      request const& req,
      any_adapter&& adapter,
      asio::any_completion_handler<void(boost::system::error_code, std::size_t)> token);

   void async_receive2_impl(asio::any_completion_handler<void(boost::system::error_code)> token);

   basic_connection<executor_type> impl_;
};

}  // namespace boost::redis

#endif  // BOOST_REDIS_CONNECTION_HPP
