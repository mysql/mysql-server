//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_IMPL_CONNECTION_IMPL_IPP
#define BOOST_MYSQL_IMPL_CONNECTION_IMPL_IPP

#pragma once

#include <boost/mysql/character_set.hpp>
#include <boost/mysql/diagnostics.hpp>
#include <boost/mysql/pipeline.hpp>

#include <boost/mysql/detail/connection_impl.hpp>

#include <boost/mysql/impl/internal/sansio/connection_state.hpp>
#include <boost/mysql/impl/internal/sansio/connection_state_data.hpp>

#include <boost/throw_exception.hpp>

#include <cstddef>
#include <stdexcept>

namespace boost {
namespace mysql {
namespace detail {

inline connection_state* new_connection_state(
    std::size_t initial_buffer_size,
    std::size_t max_buffer_size,
    bool engine_supports_ssl
)
{
    if (initial_buffer_size > max_buffer_size)
    {
        BOOST_THROW_EXCEPTION(std::invalid_argument(
            "any_connection::any_connection: initial_buffer_size should be <= max_buffer_size"
        ));
    }
    return new connection_state(initial_buffer_size, max_buffer_size, engine_supports_ssl);
}

}  // namespace detail
}  // namespace mysql
}  // namespace boost

void boost::mysql::detail::connection_state_deleter::operator()(connection_state* st) const { delete st; }

std::vector<boost::mysql::field_view>& boost::mysql::detail::get_shared_fields(connection_state& st)
{
    return st.data().shared_fields;
}

boost::mysql::detail::connection_impl::connection_impl(
    std::size_t read_buff_size,
    std::size_t max_buffer_size,
    std::unique_ptr<engine> eng
)
    : engine_(std::move(eng)),
      st_(new_connection_state(read_buff_size, max_buffer_size, engine_->supports_ssl()))
{
}

boost::mysql::metadata_mode boost::mysql::detail::connection_impl::meta_mode() const
{
    return st_->data().meta_mode;
}

void boost::mysql::detail::connection_impl::set_meta_mode(metadata_mode v) { st_->data().meta_mode = v; }

bool boost::mysql::detail::connection_impl::ssl_active() const { return st_->data().tls_active; }

bool boost::mysql::detail::connection_impl::backslash_escapes() const
{
    return st_->data().backslash_escapes;
}

boost::mysql::diagnostics& boost::mysql::detail::connection_impl::shared_diag()
{
    return st_->data().shared_diag;
}

boost::system::result<boost::mysql::character_set> boost::mysql::detail::connection_impl::
    current_character_set() const
{
    character_set charset = st_->data().current_charset;
    if (charset.name == nullptr)
        return client_errc::unknown_character_set;
    return charset;
}

boost::optional<std::uint32_t> boost::mysql::detail::connection_impl::connection_id() const
{
    const auto& data = st_->data();
    if (data.status == connection_status::not_connected)
        return {};
    else
        return data.connection_id;
}

boost::mysql::detail::run_pipeline_algo_params boost::mysql::detail::connection_impl::make_params_pipeline(
    const pipeline_request& req,
    std::vector<stage_response>& response
)
{
    const auto& req_impl = access::get_impl(req);
    return {req_impl.buffer_, req_impl.stages_, &response};
}

template <class AlgoParams>
boost::mysql::detail::any_resumable_ref boost::mysql::detail::setup(
    connection_state& st,
    diagnostics& diag,
    const AlgoParams& params
)
{
    return st.setup(diag, params);
}

template <class AlgoParams>
typename AlgoParams::result_type boost::mysql::detail::get_result(const connection_state& st)
{
    return st.result<AlgoParams>();
}

#ifdef BOOST_MYSQL_SEPARATE_COMPILATION

#define BOOST_MYSQL_INSTANTIATE_SETUP(op_params_type) \
    template any_resumable_ref setup<op_params_type>(connection_state&, diagnostics&, const op_params_type&);

#define BOOST_MYSQL_INSTANTIATE_GET_RESULT(op_params_type) \
    template op_params_type::result_type get_result<op_params_type>(const connection_state&);

namespace boost {
namespace mysql {
namespace detail {

BOOST_MYSQL_INSTANTIATE_SETUP(connect_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(handshake_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(execute_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(start_execution_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(read_resultset_head_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(read_some_rows_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(read_some_rows_dynamic_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(prepare_statement_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(close_statement_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(set_character_set_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(ping_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(reset_connection_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(quit_connection_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(close_connection_algo_params)
BOOST_MYSQL_INSTANTIATE_SETUP(run_pipeline_algo_params)

BOOST_MYSQL_INSTANTIATE_GET_RESULT(read_some_rows_algo_params)
BOOST_MYSQL_INSTANTIATE_GET_RESULT(read_some_rows_dynamic_algo_params)
BOOST_MYSQL_INSTANTIATE_GET_RESULT(prepare_statement_algo_params)

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif

#endif
