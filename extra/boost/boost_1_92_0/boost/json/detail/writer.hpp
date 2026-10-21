//
// Copyright (c) 2023 Dmitry Arkhipov (grisumbras@yandex.ru)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_DETAIL_WRITER_HPP
#define BOOST_JSON_DETAIL_WRITER_HPP

#include <boost/json/detail/literals.hpp>
#include <boost/json/detail/stack.hpp>
#include <boost/json/detail/stream.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/serialize_options.hpp>
#include <boost/json/value.hpp>

namespace boost {
namespace json {
namespace detail {

struct writer
{
    enum class state : char;

    stack st_;
    serialize_options opts_;
    const_stream cs0_;
    void const* p_ = nullptr;
    char buf_[detail::max_number_chars + 1];

    writer(
        storage_ptr sp,
        unsigned char* buf,
        std::size_t buf_size,
        serialize_options const& opts) noexcept;

    inline
    bool
    suspend(state st);

    template<class U, class T>
    bool
    suspend(state st, U u, T const* po);
};

bool
BOOST_JSON_DECL
write_true(writer& w, stream& ss);

bool
BOOST_JSON_DECL
write_false(writer& w, stream& ss);

bool
BOOST_JSON_DECL
write_null(writer& w, stream& ss);

bool
BOOST_JSON_DECL
write_int64(writer&, stream& ss, std::int64_t i);

bool
BOOST_JSON_DECL
write_uint64(writer&, stream& ss, std::uint64_t i);

bool
BOOST_JSON_DECL
write_double(writer&, stream& ss, double d);

bool
BOOST_JSON_DECL
resume_buffer(writer&, stream& ss);

bool
BOOST_JSON_DECL
write_string(writer& w, stream& ss);

bool
BOOST_JSON_DECL
resume_string(writer& w, stream& ss);

} // namespace detail
} // namespace json
} // namespace boost

#endif // BOOST_JSON_DETAIL_WRITER_HPP
