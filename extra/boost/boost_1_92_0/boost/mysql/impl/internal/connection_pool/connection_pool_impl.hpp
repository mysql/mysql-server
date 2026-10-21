//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_INTERNAL_CONNECTION_POOL_CONNECTION_POOL_IMPL_HPP
#define BOOST_MYSQL_IMPL_INTERNAL_CONNECTION_POOL_CONNECTION_POOL_IMPL_HPP

#include <boost/mysql/any_connection.hpp>
#include <boost/mysql/character_set.hpp>
#include <boost/mysql/client_errc.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/pool_params.hpp>

#include <boost/mysql/detail/config.hpp>

#include <boost/mysql/impl/internal/connection_pool/connection_node.hpp>
#include <boost/mysql/impl/internal/connection_pool/internal_pool_params.hpp>
#include <boost/mysql/impl/internal/connection_pool/sansio_connection_node.hpp>
#include <boost/mysql/impl/internal/coroutine.hpp>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/associated_cancellation_slot.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/immediate.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>

#include <chrono>
#include <cstddef>
#include <list>
#include <memory>
#include <utility>

namespace boost {
namespace mysql {
namespace detail {

// Templating on ConnectionWrapper is useful for mocking in tests.
// Production code always uses ConnectionWrapper = pooled_connection.
template <class ConnectionType, class ClockType, class ConnectionWrapper>
class basic_pool_impl
    : public std::enable_shared_from_this<basic_pool_impl<ConnectionType, ClockType, ConnectionWrapper>>
{
    using this_type = basic_pool_impl<ConnectionType, ClockType, ConnectionWrapper>;
    using node_type = basic_connection_node<ConnectionType, ClockType>;
    using timer_type = asio::basic_waitable_timer<ClockType>;
    using shared_state_type = conn_shared_state<ConnectionType, ClockType>;

    enum class state_t
    {
        initial,
        running,
        cancelled,
    };

    // The passed pool executor, as is
    asio::any_io_executor original_pool_ex_;

    // If thread_safe, a strand wrapping inner_pool_ex_, otherwise inner_pool_ex_
    asio::any_io_executor pool_ex_;

    // executor to be used by connections
    asio::any_io_executor conn_ex_;

    // Rest of the parameters
    internal_pool_params params_;

    // State
    state_t state_{state_t::initial};
    std::list<node_type> all_conns_;
    shared_state_type shared_st_;
    timer_type cancel_timer_;

    std::shared_ptr<this_type> shared_from_this_wrapper()
    {
        // Some compilers get confused without this explicit cast
        return static_cast<std::enable_shared_from_this<this_type>*>(this)->shared_from_this();
    }

    // Create and run one connection
    void create_connection()
    {
        // Connection tasks always run in the pool's executor
        all_conns_.emplace_back(params_, pool_ex_, conn_ex_, shared_st_);
        all_conns_.back().async_run(asio::bind_executor(pool_ex_, asio::detached));
    }

    // Create and run connections as required by the current config and state
    void create_connections()
    {
        // Calculate how many we should create
        std::size_t n = num_connections_to_create(
            params_.initial_size,
            params_.max_size,
            all_conns_.size(),
            shared_st_.num_pending_connections,
            shared_st_.num_pending_requests
        );

        // Create them
        BOOST_ASSERT((all_conns_.size() + n) <= params_.max_size);
        for (std::size_t i = 0; i < n; ++i)
            create_connection();
    }

    // An async_get_connection request is about to wait for an available connection
    void enter_request_pending()
    {
        // Record that we're pending
        ++shared_st_.num_pending_requests;

        // Create new connections, if required.
        // Don't create any connections if we're not yet running,
        // since this would leave connections running after run exits
        if (state_ == state_t::running)
            create_connections();
    }

    // An async_get_connection request finished waiting
    void exit_request_pending()
    {
        // Record that we're no longer pending
        BOOST_ASSERT(shared_st_.num_pending_requests > 0u);
        --shared_st_.num_pending_requests;
    }

    node_type* try_get_connection()
    {
        if (!shared_st_.idle_list.empty())
        {
            node_type& res = shared_st_.idle_list.front();
            res.mark_as_in_use();
            return &res;
        }
        else
        {
            return nullptr;
        }
    }

    template <class OpSelf>
    void enter_strand(OpSelf& self)
    {
        asio::dispatch(asio::bind_executor(pool_ex_, std::move(self)));
    }

    template <class OpSelf>
    void exit_strand(OpSelf& self)
    {
        asio::post(get_executor(), std::move(self));
    }

    template <class OpSelf>
    void wait_for_connections(OpSelf& self)
    {
        // Having this encapsulated helps prevent subtle use-after-move errors
        if (params_.thread_safe)
        {
            shared_st_.idle_connections_cv.async_wait(asio::bind_executor(pool_ex_, std::move(self)));
        }
        else
        {
            shared_st_.idle_connections_cv.async_wait(std::move(self));
        }
    }

    struct run_op
    {
        int resume_point_{0};
        std::shared_ptr<this_type> obj_;
        asio::cancellation_slot cancel_slot_;

        run_op(std::shared_ptr<this_type> obj, asio::cancellation_slot slot) noexcept
            : obj_(std::move(obj)), cancel_slot_(slot)
        {
        }

        struct cancel_handler
        {
            this_type* self;

            void operator()(asio::cancellation_type_t type) const
            {
                if (run_supports_cancel_type(type))
                    self->cancel();
            }
        };

        template <class Self>
        void operator()(Self& self, error_code = {})
        {
            switch (resume_point_)
            {
            case 0:
                // Emplace a cancellation handler, if required
                if (cancel_slot_.is_connected())
                {
                    cancel_slot_.template emplace<cancel_handler>(cancel_handler{obj_.get()});
                }

                // Ensure we run within the strand
                if (obj_->params_.thread_safe)
                {
                    BOOST_MYSQL_YIELD(resume_point_, 1, obj_->enter_strand(self))
                }

                // Check that we're not running and set the state adequately
                BOOST_ASSERT(obj_->state_ == state_t::initial);
                obj_->state_ = state_t::running;

                // Create the initial connections
                obj_->create_connections();

                // Wait for the cancel notification to arrive.
                BOOST_MYSQL_YIELD(resume_point_, 2, obj_->cancel_timer_.async_wait(std::move(self)))

                // Ensure we run within the strand
                if (obj_->params_.thread_safe)
                {
                    BOOST_MYSQL_YIELD(resume_point_, 3, obj_->enter_strand(self))
                }

                // Deliver the cancel notification to all other tasks
                obj_->state_ = state_t::cancelled;
                for (auto& conn : obj_->all_conns_)
                    conn.cancel();
                obj_->shared_st_.idle_connections_cv.expires_at((ClockType::time_point::min)());

                // Wait for all connection tasks to exit
                BOOST_MYSQL_YIELD(
                    resume_point_,
                    4,
                    obj_->shared_st_.conns_finished_cv.async_wait(std::move(self))
                )

                // Done
                cancel_slot_.clear();
                obj_.reset();
                self.complete(error_code());
            }
        }
    };

    struct get_connection_op
    {
        // Operation arguments
        std::shared_ptr<this_type> obj;
        diagnostics* diag;

        // The proxy signal. Used in thread-safe mode. Present here only for
        // lifetime management
        std::shared_ptr<asio::cancellation_signal> sig;

        // The original cancellation slot. Needed for proper cleanup after we proxy
        // the signal in thread-safe mode
        asio::cancellation_slot parent_slot;

        // State
        int resume_point{0};
        error_code result_ec;
        node_type* result_conn{};
        bool has_waited{false};

        get_connection_op(
            std::shared_ptr<this_type> obj,
            diagnostics* diag,
            std::shared_ptr<asio::cancellation_signal> sig,
            asio::cancellation_slot parent_slot
        ) noexcept
            : obj(std::move(obj)), diag(diag), sig(std::move(sig)), parent_slot(parent_slot)
        {
        }

        bool thread_safe() const { return obj->params_.thread_safe; }

        template <class Self>
        void do_complete(Self& self)
        {
            auto wr = result_ec ? ConnectionWrapper() : ConnectionWrapper(*result_conn, std::move(obj));
            parent_slot.clear();
            sig.reset();
            self.complete(result_ec, std::move(wr));
        }

        template <class Self>
        void operator()(Self& self, error_code = {})
        {
            switch (resume_point)
            {
            case 0:
                // This op supports total cancellation. Must be explicitly enabled,
                // as composed ops only support terminal cancellation by default.
                self.reset_cancellation_state(asio::enable_total_cancellation());

                // Clear diagnostics
                if (diag)
                    diag->clear();

                // Enter the strand
                if (thread_safe())
                {
                    BOOST_MYSQL_YIELD(resume_point, 1, obj->enter_strand(self))
                }

                // This loop guards us against possible race conditions
                // between waiting on the pending request timer and getting the
                // connection
                while (true)
                {
                    if (obj->state_ == state_t::cancelled)
                    {
                        // The pool was cancelled
                        result_ec = client_errc::pool_cancelled;
                        break;
                    }
                    else if (get_connection_supports_cancel_type(self.cancelled()))
                    {
                        // The operation was cancelled. Try to provide diagnostics
                        if (obj->state_ == state_t::initial)
                        {
                            // The operation failed because the pool is not running
                            result_ec = client_errc::pool_not_running;
                        }
                        else
                        {
                            result_ec = client_errc::no_connection_available;
                            if (diag)
                                *diag = obj->shared_st_.last_connect_diag;
                        }
                        break;
                    }

                    // Try to get a connection
                    if ((result_conn = obj->try_get_connection()) != nullptr)
                    {
                        // There was a connection
                        break;
                    }

                    // No luck. Record that we're waiting for a connection.
                    obj->enter_request_pending();

                    // Wait to be notified, or until a cancellation happens
                    BOOST_MYSQL_YIELD(resume_point, 2, obj->wait_for_connections(self))

                    // Record that we're no longer pending
                    obj->exit_request_pending();

                    // Remember that we have waited, so completions are dispatched
                    // correctly
                    has_waited = true;
                }

                // Perform any required dispatching before completing
                if (thread_safe())
                {
                    // Exit the strand
                    BOOST_MYSQL_YIELD(resume_point, 3, obj->exit_strand(self))
                }
                else if (!has_waited)
                {
                    // This is an immediate completion
                    BOOST_MYSQL_YIELD(
                        resume_point,
                        4,
                        asio::async_immediate(self.get_io_executor(), std::move(self))
                    )
                }

                // Done
                do_complete(self);
            }
        }
    };

    // Cancel handler to use for get_connection in thread-safe mode.
    // This imitates what Asio does for composed ops
    struct get_connection_cancel_handler
    {
        // Pointer to the proxy cancellation signal
        // Lifetime managed by the get_connection composed op
        std::weak_ptr<asio::cancellation_signal> sig;

        // Pointer to the pool object
        std::weak_ptr<this_type> obj;

        get_connection_cancel_handler(
            std::weak_ptr<asio::cancellation_signal> sig,
            std::weak_ptr<this_type> obj
        ) noexcept
            : sig(std::move(sig)), obj(std::move(obj))
        {
        }

        void operator()(asio::cancellation_type_t type)
        {
            if (get_connection_supports_cancel_type(type))
            {
                // Try to get the pool object back
                std::shared_ptr<this_type> obj_shared = obj.lock();
                if (obj_shared)
                {
                    // Dispatch to the strand. We don't need to keep a reference to the
                    // pool because even if it was destroyed before running the handler,
                    // the strand would be alive.
                    auto sig_copy = sig;
                    asio::dispatch(asio::bind_executor(obj_shared->strand(), [sig_copy, type]() {
                        // If the operation has already completed, the weak ptr will be
                        // invalid
                        auto sig_shared = sig_copy.lock();
                        if (sig_shared)
                        {
                            sig_shared->emit(type);
                        }
                    }));
                }
            }
        }
    };

    // Not thread-safe
    void cancel_unsafe() { cancel_timer_.expires_at((std::chrono::steady_clock::time_point::min)()); }

public:
    basic_pool_impl(asio::any_io_executor ex, pool_params&& params)
        : original_pool_ex_(std::move(ex)),
          pool_ex_(params.thread_safe ? asio::make_strand(original_pool_ex_) : original_pool_ex_),
          conn_ex_(params.connection_executor ? std::move(params.connection_executor) : original_pool_ex_),
          params_(make_internal_pool_params(std::move(params))),
          shared_st_(pool_ex_),
          cancel_timer_(pool_ex_, (std::chrono::steady_clock::time_point::max)())
    {
    }

    asio::strand<asio::any_io_executor> strand()
    {
        BOOST_ASSERT(params_.thread_safe);
        return *pool_ex_.template target<asio::strand<asio::any_io_executor>>();
    }

    using executor_type = asio::any_io_executor;
    executor_type get_executor() { return original_pool_ex_; }

    void async_run(asio::any_completion_handler<void(error_code)> handler)
    {
        // Completely disable composed cancellation handling, as it's not what we
        // want
        auto slot = asio::get_associated_cancellation_slot(handler);
        auto token_without_slot = asio::bind_cancellation_slot(asio::cancellation_slot(), std::move(handler));

        // Initiate passing the original token's slot manually
        asio::async_compose<decltype(token_without_slot), void(error_code)>(
            run_op(shared_from_this_wrapper(), slot),
            token_without_slot,
            pool_ex_
        );
    }

    void async_get_connection(
        diagnostics* diag,
        asio::any_completion_handler<void(error_code, ConnectionWrapper)> handler
    )
    {
        // The slot to pass for cleanup
        asio::cancellation_slot parent_slot;

        // The signal pointer. Will only be created if required
        std::shared_ptr<asio::cancellation_signal> sig;

        // In thread-safe mode, and if we have a connected slot, create a proxy
        // signal that dispatches to the strand
        if (params_.thread_safe)
        {
            parent_slot = asio::get_associated_cancellation_slot(handler);
            if (parent_slot.is_connected())
            {
                // Create a signal. In rare cases, the memory acquired here may outlive
                // the async operation (e.g. the completion handler runs after the
                // signal is emitted and before the strand dispatch runs). This means we
                // can't use the handler's allocator.
                sig = std::make_shared<asio::cancellation_signal>();

                // Emplace the handler
                parent_slot.template emplace<get_connection_cancel_handler>(sig, shared_from_this_wrapper());

                // Bind the handler to the slot
                handler = asio::bind_cancellation_slot(sig->slot(), std::move(handler));
            }
        }

        // Start
        using handler_type = asio::any_completion_handler<void(error_code, ConnectionWrapper)>;
        asio::async_compose<handler_type, void(error_code, ConnectionWrapper)>(
            get_connection_op(shared_from_this_wrapper(), diag, std::move(sig), parent_slot),
            handler,
            pool_ex_
        );
    }

    void cancel()
    {
        if (params_.thread_safe)
        {
            // A handler to be passed to dispatch. Binds the executor
            // and keeps the pool alive
            struct dispatch_handler
            {
                std::shared_ptr<this_type> pool_ptr;

                using executor_type = asio::any_io_executor;
                executor_type get_executor() const noexcept { return pool_ptr->strand(); }

                void operator()() const { pool_ptr->cancel_unsafe(); }
            };

            asio::dispatch(dispatch_handler{shared_from_this_wrapper()});
        }
        else
        {
            cancel_unsafe();
        }
    }

    void return_connection(node_type& node, bool should_reset) noexcept
    {
        // This is safe to be called from any thread
        node.mark_as_collectable(should_reset);

        // The notification isn't thread-safe
        if (params_.thread_safe)
        {
            // A handler to be passed to dispatch. Binds the executor
            // and keeps the pool alive
            struct dispatch_handler
            {
                std::shared_ptr<this_type> pool_ptr;
                node_type* node_ptr;

                using executor_type = asio::any_io_executor;
                executor_type get_executor() const noexcept { return pool_ptr->strand(); }

                void operator()() const { node_ptr->notify_collectable(); }
            };

            // If, for any reason, this notification fails, the connection will
            // be collected when the next ping is due.
            try
            {
                asio::dispatch(dispatch_handler{shared_from_this_wrapper(), &node});
            }
            catch (...)
            {
            }
        }
        else
        {
            node.notify_collectable();
        }
    }

    // Exposed for testing
    static bool run_supports_cancel_type(asio::cancellation_type_t v)
    {
        // run doesn't support total, as the pool state is always modified
        return !!(v & (asio::cancellation_type_t::partial | asio::cancellation_type_t::terminal));
    }

    static bool get_connection_supports_cancel_type(asio::cancellation_type_t v)
    {
        // get_connection supports all cancel types
        return !!(
            v & (asio::cancellation_type_t::partial | asio::cancellation_type_t::total |
                 asio::cancellation_type_t::terminal)
        );
    }

    std::list<node_type>& nodes() noexcept { return all_conns_; }
    shared_state_type& shared_state() noexcept { return shared_st_; }
    internal_pool_params& params() noexcept { return params_; }
    asio::any_io_executor connection_ex() noexcept { return conn_ex_; }
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
