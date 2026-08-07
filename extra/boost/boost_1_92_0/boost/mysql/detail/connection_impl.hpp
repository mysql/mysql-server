//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_CONNECTION_IMPL_HPP
#define BOOST_MYSQL_DETAIL_CONNECTION_IMPL_HPP

#include <boost/mysql/any_address.hpp>
#include <boost/mysql/connect_params.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/error_code.hpp>
#include <boost/mysql/execution_state.hpp>
#include <boost/mysql/field_view.hpp>
#include <boost/mysql/handshake_params.hpp>
#include <boost/mysql/metadata_mode.hpp>
#include <boost/mysql/rows_view.hpp>
#include <boost/mysql/statement.hpp>
#include <boost/mysql/string_view.hpp>

#include <boost/mysql/detail/access.hpp>
#include <boost/mysql/detail/algo_params.hpp>
#include <boost/mysql/detail/config.hpp>
#include <boost/mysql/detail/connect_params_helpers.hpp>
#include <boost/mysql/detail/engine.hpp>
#include <boost/mysql/detail/execution_processor/execution_processor.hpp>
#include <boost/mysql/detail/initiation_base.hpp>
#include <boost/mysql/detail/intermediate_handler.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/optional/optional.hpp>
#include <boost/system/result.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace boost {
namespace mysql {

// Forward decl
template <class... StaticRow>
class static_execution_state;

struct character_set;
class pipeline_request;

namespace detail {

//
// Helpers to interact with connection_state, without including its definition
//
class connection_state;

struct connection_state_deleter
{
    BOOST_MYSQL_DECL void operator()(connection_state*) const;
};

BOOST_MYSQL_DECL std::vector<field_view>& get_shared_fields(connection_state&);

template <class AlgoParams>
any_resumable_ref setup(connection_state&, diagnostics&, const AlgoParams&);

// Note: AlgoParams should have !is_void_result
template <class AlgoParams>
typename AlgoParams::result_type get_result(const connection_state&);

//
// helpers to run algos
//
template <class AlgoParams>
using has_void_result = std::is_same<typename AlgoParams::result_type, void>;

template <class AlgoParams, bool is_void>
struct completion_signature_impl;

template <class AlgoParams>
struct completion_signature_impl<AlgoParams, true>
{
    // Using typedef to workaround a msvc 14.1 bug
    typedef void(type)(error_code);
};

template <class AlgoParams>
struct completion_signature_impl<AlgoParams, false>
{
    // Using typedef to workaround a msvc 14.1 bug
    typedef void(type)(error_code, typename AlgoParams::result_type);
};

template <class AlgoParams>
using completion_signature_t = typename completion_signature_impl<
    AlgoParams,
    has_void_result<AlgoParams>::value>::type;

// Intermediate handler
template <class AlgoParams>
struct generic_algo_fn
{
    static_assert(!has_void_result<AlgoParams>::value, "Internal error: result_type should be non-void");

    using result_t = typename AlgoParams::result_type;

    template <class Handler>
    void operator()(Handler&& handler, error_code ec)
    {
        std::move(handler)(ec, ec ? result_t{} : get_result<AlgoParams>(*st));
    }

    connection_state* st;
};

// Note: the 1st async_initiate arg should be a diagnostics*,
// so completion tokens knowing how Boost.MySQL work can operate
class connection_impl
{
    std::unique_ptr<engine> engine_;
    std::unique_ptr<connection_state, connection_state_deleter> st_;

    asio::any_io_executor get_executor() const { return engine_->get_executor(); }

    // Helper for execution requests
    template <class T>
    static auto make_request(T&& input, connection_state& st)
        -> decltype(execution_request_traits<typename std::decay<T>::type>::make_request(
            std::forward<T>(input),
            get_shared_fields(st)
        ))
    {
        return execution_request_traits<typename std::decay<T>::type>::make_request(
            std::forward<T>(input),
            get_shared_fields(st)
        );
    }

    // Generic algorithm
    template <class AlgoParams>
    typename AlgoParams::result_type run_impl(
        AlgoParams params,
        error_code& ec,
        diagnostics& diag,
        std::true_type /* has_void_result */
    )
    {
        engine_->run(setup(*st_, diag, params), ec);
    }

    template <class AlgoParams>
    typename AlgoParams::result_type run_impl(
        AlgoParams params,
        error_code& ec,
        diagnostics& diag,
        std::false_type /* has_void_result */
    )
    {
        engine_->run(setup(*st_, diag, params), ec);
        return get_result<AlgoParams>(*st_);
    }

    template <class AlgoParams, class Handler>
    static void async_run_impl(
        engine& eng,
        connection_state& st,
        AlgoParams params,
        diagnostics& diag,
        Handler&& handler,
        std::true_type /* has_void_result */
    )
    {
        eng.async_run(setup(st, diag, params), std::forward<Handler>(handler));
    }

    template <class AlgoParams, class Handler>
    static void async_run_impl(
        engine& eng,
        connection_state& st,
        AlgoParams params,
        diagnostics& diag,
        Handler&& handler,
        std::false_type /* has_void_result */
    )
    {
        eng.async_run(
            setup(st, diag, params),
            make_intermediate_handler(generic_algo_fn<AlgoParams>{&st}, std::forward<Handler>(handler))
        );
    }

    template <class AlgoParams, class Handler>
    static void async_run_impl(
        engine& eng,
        connection_state& st,
        AlgoParams params,
        diagnostics& diag,
        Handler&& handler
    )
    {
        async_run_impl(eng, st, params, diag, std::forward<Handler>(handler), has_void_result<AlgoParams>{});
    }

    struct run_algo_initiation : initiation_base
    {
        using initiation_base::initiation_base;

        template <class Handler, class AlgoParams>
        void operator()(
            Handler&& handler,
            diagnostics* diag,
            engine* eng,
            connection_state* st,
            AlgoParams params
        )
        {
            async_run_impl(*eng, *st, params, *diag, std::forward<Handler>(handler));
        }
    };

    // Connect
    static connect_algo_params make_params_connect(const void* server_address, const handshake_params& params)
    {
        return connect_algo_params{server_address, params, false, {}};
    }

    static connect_algo_params make_params_connect_v2(const connect_params& params)
    {
        return connect_algo_params{
            &params.server_address,
            make_hparams(params),
            params.server_address.type() == address_type::unix_path,
            params.server_default_charset,
        };
    }

    template <class EndpointType>
    struct initiate_connect : initiation_base
    {
        using initiation_base::initiation_base;

        template <class Handler>
        void operator()(
            Handler&& handler,
            diagnostics* diag,
            engine* eng,
            connection_state* st,
            const EndpointType& endpoint,
            handshake_params params
        )
        {
            async_run_impl(
                *eng,
                *st,
                make_params_connect(&endpoint, params),
                *diag,
                std::forward<Handler>(handler)
            );
        }
    };

    struct initiate_connect_v2 : initiation_base
    {
        using initiation_base::initiation_base;

        template <class Handler>
        void operator()(
            Handler&& handler,
            diagnostics* diag,
            engine* eng,
            connection_state* st,
            const connect_params* params
        )
        {
            async_run_impl(*eng, *st, make_params_connect_v2(*params), *diag, std::forward<Handler>(handler));
        }
    };

    // execute
    struct initiate_execute : initiation_base
    {
        using initiation_base::initiation_base;

        template <class Handler, class ExecutionRequest>
        void operator()(
            Handler&& handler,
            diagnostics* diag,
            engine* eng,
            connection_state* st,
            ExecutionRequest&& req,
            execution_processor* proc
        )
        {
            async_run_impl(
                *eng,
                *st,
                execute_algo_params{make_request(std::forward<ExecutionRequest>(req), *st), proc},
                *diag,
                std::forward<Handler>(handler)
            );
        }
    };

    // start execution
    struct initiate_start_execution : initiation_base
    {
        using initiation_base::initiation_base;

        template <class Handler, class ExecutionRequest>
        void operator()(
            Handler&& handler,
            diagnostics* diag,
            engine* eng,
            connection_state* st,
            ExecutionRequest&& req,
            execution_processor* proc
        )
        {
            async_run_impl(
                *eng,
                *st,
                start_execution_algo_params{make_request(std::forward<ExecutionRequest>(req), *st), proc},
                *diag,
                std::forward<Handler>(handler)
            );
        }
    };

public:
    BOOST_MYSQL_DECL connection_impl(
        std::size_t read_buff_size,
        std::size_t max_buffer_size,
        std::unique_ptr<engine> eng
    );

    BOOST_MYSQL_DECL metadata_mode meta_mode() const;
    BOOST_MYSQL_DECL void set_meta_mode(metadata_mode m);
    BOOST_MYSQL_DECL bool ssl_active() const;
    BOOST_MYSQL_DECL bool backslash_escapes() const;
    BOOST_MYSQL_DECL system::result<character_set> current_character_set() const;
    BOOST_MYSQL_DECL boost::optional<std::uint32_t> connection_id() const;
    BOOST_MYSQL_DECL diagnostics& shared_diag();

    engine& get_engine()
    {
        BOOST_ASSERT(engine_);
        return *engine_;
    }

    const engine& get_engine() const
    {
        BOOST_ASSERT(engine_);
        return *engine_;
    }

    // Generic algorithm
    template <class AlgoParams>
    typename AlgoParams::result_type run(AlgoParams params, error_code& ec, diagnostics& diag)
    {
        return run_impl(params, ec, diag, has_void_result<AlgoParams>{});
    }

    template <class AlgoParams, class CompletionToken>
    auto async_run(AlgoParams params, diagnostics& diag, CompletionToken&& token)
        -> decltype(asio::async_initiate<CompletionToken, completion_signature_t<AlgoParams>>(
            run_algo_initiation(get_executor()),
            token,
            &diag,
            engine_.get(),
            st_.get(),
            params
        ))
    {
        return asio::async_initiate<CompletionToken, completion_signature_t<AlgoParams>>(
            run_algo_initiation(get_executor()),
            token,
            &diag,
            engine_.get(),
            st_.get(),
            params
        );
    }

    // Connect
    template <class EndpointType>
    void connect(
        const EndpointType& endpoint,
        const handshake_params& params,
        error_code& err,
        diagnostics& diag
    )
    {
        run(make_params_connect(&endpoint, params), err, diag);
    }

    void connect_v2(const connect_params& params, error_code& err, diagnostics& diag)
    {
        run(make_params_connect_v2(params), err, diag);
    }

    template <class EndpointType, class CompletionToken>
    auto async_connect(
        const EndpointType& endpoint,
        const handshake_params& params,
        diagnostics& diag,
        CompletionToken&& token
    )
        -> decltype(asio::async_initiate<CompletionToken, void(error_code)>(
            initiate_connect<EndpointType>(get_executor()),
            token,
            &diag,
            engine_.get(),
            st_.get(),
            endpoint,
            params
        ))
    {
        return asio::async_initiate<CompletionToken, void(error_code)>(
            initiate_connect<EndpointType>(get_executor()),
            token,
            &diag,
            engine_.get(),
            st_.get(),
            endpoint,
            params
        );
    }

    template <class CompletionToken>
    auto async_connect_v2(const connect_params& params, diagnostics& diag, CompletionToken&& token)
        -> decltype(asio::async_initiate<CompletionToken, void(error_code)>(
            initiate_connect_v2(get_executor()),
            token,
            &diag,
            engine_.get(),
            st_.get(),
            &params
        ))
    {
        return asio::async_initiate<CompletionToken, void(error_code)>(
            initiate_connect_v2(get_executor()),
            token,
            &diag,
            engine_.get(),
            st_.get(),
            &params
        );
    }

    // Handshake
    handshake_algo_params make_params_handshake(const handshake_params& params) const
    {
        return {params, false, {}};
    }

    // Execute
    template <class ExecutionRequest, class ResultsType>
    void execute(ExecutionRequest&& req, ResultsType& result, error_code& err, diagnostics& diag)
    {
        run(
            execute_algo_params{
                make_request(std::forward<ExecutionRequest>(req), *st_),
                &access::get_impl(result).get_interface()
            },
            err,
            diag
        );
    }

    template <class ExecutionRequest, class ResultsType, class CompletionToken>
    auto async_execute(
        ExecutionRequest&& req,
        ResultsType& result,
        diagnostics& diag,
        CompletionToken&& token
    )
        -> decltype(asio::async_initiate<CompletionToken, void(error_code)>(
            initiate_execute(get_executor()),
            token,
            &diag,
            engine_.get(),
            st_.get(),
            std::forward<ExecutionRequest>(req),
            &access::get_impl(result).get_interface()
        ))
    {
        return asio::async_initiate<CompletionToken, void(error_code)>(
            initiate_execute(get_executor()),
            token,
            &diag,
            engine_.get(),
            st_.get(),
            std::forward<ExecutionRequest>(req),
            &access::get_impl(result).get_interface()
        );
    }

    // Start execution
    template <class ExecutionRequest, class ExecutionStateType>
    void start_execution(
        ExecutionRequest&& req,
        ExecutionStateType& exec_st,
        error_code& err,
        diagnostics& diag
    )
    {
        run(
            start_execution_algo_params{
                make_request(std::forward<ExecutionRequest>(req), *st_),
                &access::get_impl(exec_st).get_interface()
            },
            err,
            diag
        );
    }

    template <class ExecutionRequest, class ExecutionStateType, class CompletionToken>
    auto async_start_execution(
        ExecutionRequest&& req,
        ExecutionStateType& exec_st,
        diagnostics& diag,
        CompletionToken&& token
    )
        -> decltype(asio::async_initiate<CompletionToken, void(error_code)>(
            initiate_start_execution(get_executor()),
            token,
            &diag,
            engine_.get(),
            st_.get(),
            std::forward<ExecutionRequest>(req),
            &access::get_impl(exec_st).get_interface()
        ))
    {
        return asio::async_initiate<CompletionToken, void(error_code)>(
            initiate_start_execution(get_executor()),
            token,
            &diag,
            engine_.get(),
            st_.get(),
            std::forward<ExecutionRequest>(req),
            &access::get_impl(exec_st).get_interface()
        );
    }

    // Read some rows (dynamic)
    read_some_rows_dynamic_algo_params make_params_read_some_rows(execution_state& st) const
    {
        return {&access::get_impl(st).get_interface()};
    }

    // Read some rows (static)
    template <class SpanElementType, class ExecutionState>
    read_some_rows_algo_params make_params_read_some_rows_static(
        ExecutionState& exec_st,
        span<SpanElementType> output
    ) const
    {
        return {
            &access::get_impl(exec_st).get_interface(),
            access::get_impl(exec_st).make_output_ref(output)
        };
    }

    // Read resultset head
    template <class ExecutionStateType>
    read_resultset_head_algo_params make_params_read_resultset_head(ExecutionStateType& st) const
    {
        return {&detail::access::get_impl(st).get_interface()};
    }

    // Close statement
    close_statement_algo_params make_params_close_statement(statement stmt) const { return {stmt.id()}; }

    // Run pipeline. Separately compiled to avoid including the pipeline header here
    BOOST_MYSQL_DECL
    static run_pipeline_algo_params make_params_pipeline(
        const pipeline_request& req,
        std::vector<stage_response>& response
    );

    // Exposed for testing
    connection_state& get_state() { return *st_; }
};

// To use some completion tokens, like deferred, in C++11, the old macros
// BOOST_ASIO_INITFN_AUTO_RESULT_TYPE are no longer enough.
// Helper typedefs to reduce duplication
template <class AlgoParams, class CompletionToken>
using async_run_t = decltype(std::declval<connection_impl&>().async_run(
    std::declval<AlgoParams>(),
    std::declval<diagnostics&>(),
    std::declval<CompletionToken>()
));

template <class EndpointType, class CompletionToken>
using async_connect_t = decltype(std::declval<connection_impl&>().async_connect(
    std::declval<const EndpointType&>(),
    std::declval<const handshake_params&>(),
    std::declval<diagnostics&>(),
    std::declval<CompletionToken>()
));

template <class CompletionToken>
using async_connect_v2_t = decltype(std::declval<connection_impl&>().async_connect_v2(
    std::declval<const connect_params&>(),
    std::declval<diagnostics&>(),
    std::declval<CompletionToken>()
));

template <class ExecutionRequest, class ResultsType, class CompletionToken>
using async_execute_t = decltype(std::declval<connection_impl&>().async_execute(
    std::declval<ExecutionRequest>(),
    std::declval<ResultsType&>(),
    std::declval<diagnostics&>(),
    std::declval<CompletionToken>()
));

template <class ExecutionRequest, class ExecutionStateType, class CompletionToken>
using async_start_execution_t = decltype(std::declval<connection_impl&>().async_start_execution(
    std::declval<ExecutionRequest>(),
    std::declval<ExecutionStateType&>(),
    std::declval<diagnostics&>(),
    std::declval<CompletionToken>()
));

template <class CompletionToken>
using async_handshake_t = async_run_t<handshake_algo_params, CompletionToken>;

template <class CompletionToken>
using async_read_resultset_head_t = async_run_t<read_resultset_head_algo_params, CompletionToken>;

template <class CompletionToken>
using async_read_some_rows_dynamic_t = async_run_t<read_some_rows_dynamic_algo_params, CompletionToken>;

template <class CompletionToken>
using async_prepare_statement_t = async_run_t<prepare_statement_algo_params, CompletionToken>;

template <class CompletionToken>
using async_close_statement_t = async_run_t<close_statement_algo_params, CompletionToken>;

template <class CompletionToken>
using async_set_character_set_t = async_run_t<set_character_set_algo_params, CompletionToken>;

template <class CompletionToken>
using async_ping_t = async_run_t<ping_algo_params, CompletionToken>;

template <class CompletionToken>
using async_reset_connection_t = async_run_t<reset_connection_algo_params, CompletionToken>;

template <class CompletionToken>
using async_quit_connection_t = async_run_t<quit_connection_algo_params, CompletionToken>;

template <class CompletionToken>
using async_close_connection_t = async_run_t<close_connection_algo_params, CompletionToken>;

template <class CompletionToken>
using async_run_pipeline_t = async_run_t<run_pipeline_algo_params, CompletionToken>;

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/connection_impl.ipp>
#endif

#endif
