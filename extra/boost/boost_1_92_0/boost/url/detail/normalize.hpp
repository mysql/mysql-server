//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_DETAIL_NORMALIZED_HPP
#define BOOST_URL_DETAIL_NORMALIZED_HPP

#include <boost/url/detail/config.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/url/detail/fnv_1a.hpp>
#include <boost/url/segments_encoded_view.hpp>

namespace boost {
namespace urls {
namespace detail {

// These functions are defined in src/detail/normalize.cpp.
// BOOST_URL_DECL is required because they are called from
// inline functions in public headers (url_view_base::compare,
// url_view_base::digest, authority_view::compare, etc.).

void
pop_encoded_front(
    core::string_view& s,
    char& c,
    std::size_t& n) noexcept;

// compare two core::string_views as if they are both
// percent-decoded
BOOST_URL_DECL
int
compare_encoded(
    core::string_view lhs,
    core::string_view rhs) noexcept;

// compare two core::string_views as if they are both
// percent-decoded but do not consider the special
// query chars ("&=+") equivalent unless they are
// both decoded or encoded the same way, because
// that gives them different meanings
BOOST_URL_DECL
int
compare_encoded_query(
    core::string_view lhs,
    core::string_view rhs) noexcept;

// digest a core::string_view as if it were
// percent-decoded
BOOST_URL_DECL
void
digest_encoded(
    core::string_view s,
    fnv_1a& hasher) noexcept;

void
digest(
    core::string_view s,
    fnv_1a& hasher) noexcept;

// check if core::string_view lhs starts with core::string_view
// rhs as if they are both percent-decoded. If
// lhs starts with rhs, return number of chars
// matched in the encoded core::string_view
std::size_t
path_starts_with(
    core::string_view lhs,
    core::string_view rhs) noexcept;

// check if core::string_view lhs ends with core::string_view
// rhs as if they are both percent-decoded. If
// lhs ends with rhs, return number of chars
// matched in the encoded core::string_view
std::size_t
path_ends_with(
    core::string_view lhs,
    core::string_view rhs) noexcept;

// compare two core::string_views as if they are both
// percent-decoded and lowercase
BOOST_URL_DECL
int
ci_compare_encoded(
    core::string_view lhs,
    core::string_view rhs) noexcept;

// digest a core::string_view as if it were decoded
// and lowercase
BOOST_URL_DECL
void
ci_digest_encoded(
    core::string_view s,
    fnv_1a& hasher) noexcept;

// compare two ascii core::string_views
BOOST_URL_DECL
int
compare(
    core::string_view lhs,
    core::string_view rhs) noexcept;

// compare two core::string_views as if they are both
// lowercase
BOOST_URL_DECL
int
ci_compare(
    core::string_view lhs,
    core::string_view rhs) noexcept;

// digest a core::string_view as if it were lowercase
BOOST_URL_DECL
void
ci_digest(
    core::string_view s,
    fnv_1a& hasher) noexcept;

BOOST_URL_DECL
std::size_t
remove_dot_segments(
    char* dest,
    char const* end,
    core::string_view input) noexcept;

void
pop_last_segment(
    core::string_view& str,
    core::string_view& seg,
    std::size_t& level,
    bool remove_unmatched) noexcept;

char
path_pop_back( core::string_view& s );

BOOST_URL_DECL
void
normalized_path_digest(
    core::string_view str,
    bool remove_unmatched,
    fnv_1a& hasher) noexcept;

BOOST_URL_DECL
int
segments_compare(
    segments_encoded_view seg0,
    segments_encoded_view seg1) noexcept;

} // detail
} // urls
} // boost

#endif
