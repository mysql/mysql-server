//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_DETAIL_PIPELINE_HPP
#define BOOST_MYSQL_DETAIL_PIPELINE_HPP

#include <boost/mysql/character_set.hpp>

#include <boost/mysql/detail/resultset_encoding.hpp>

#include <cstddef>
#include <cstdint>

namespace boost {
namespace mysql {
namespace detail {

class execution_processor;

enum class pipeline_stage_kind
{
    execute,
    prepare_statement,
    close_statement,
    reset_connection,
    set_character_set,
    ping,
};

struct pipeline_request_stage
{
    pipeline_stage_kind kind;
    std::uint8_t seqnum;
    union stage_specific_t
    {
        std::nullptr_t nothing;
        resultset_encoding enc;
        character_set charset;

        stage_specific_t() noexcept : nothing() {}
        stage_specific_t(resultset_encoding v) noexcept : enc(v) {}
        stage_specific_t(character_set v) noexcept : charset(v) {}
    } stage_specific;
};

}  // namespace detail
}  // namespace mysql
}  // namespace boost

#endif
