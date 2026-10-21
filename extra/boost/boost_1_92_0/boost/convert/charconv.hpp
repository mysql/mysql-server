// Copyright (c) 2022 Dvir Yitzchaki.
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. See http://www.boost.org/LICENSE_1_0.txt.

#ifndef BOOST_CONVERT_CHARCONV_BASED_CONVERTER_HPP
#define BOOST_CONVERT_CHARCONV_BASED_CONVERTER_HPP

#ifdef BOOST_NO_CXX17_HDR_CHARCONV
#error "This header requires <charconv> which is unavailable"
#endif // BOOST_NO_CXX17_HDR_CHARCONV

#ifdef BOOST_NO_CXX17_STRUCTURED_BINDINGS
#error "This header requires structured bindings which is unavailable"
#endif // BOOST_NO_CXX17_STRUCTURED_BINDINGS

#ifdef BOOST_NO_CXX17_IF_CONSTEXPR
#error "This header requires constexpr if which is unavailable"
#endif // BOOST_NO_CXX17_IF_CONSTEXPR

#include <boost/convert/base.hpp>
#include <boost/make_default.hpp>
#include <charconv>
#include <type_traits>

namespace boost::cnv { struct charconv; }

/// @brief   std::to/from_chars-based extended converter
/// @details Good overall performance and moderate formatting facilities.
struct boost::cnv::charconv : public boost::cnv::cnvbase<boost::cnv::charconv>
{
    using this_type = boost::cnv::charconv;
    using base_type = boost::cnv::cnvbase<this_type>;

    template<typename in_type>
    cnv::range<char*>
    to_str(in_type value_in, char* buf) const
    {
        auto [ptr, err] = [&]
        {
            if constexpr (std::is_integral_v<in_type>)
                return std::to_chars(buf, buf + bufsize_, value_in, int(base_));
            else
                return std::to_chars(buf, buf + bufsize_, value_in, chars_format(), precision_);
        }();
        return cnv::range<char*>(buf, err == std::errc{} ? ptr : buf);
    }
    template<typename string_type, typename out_type>
    void
    str_to(cnv::range<string_type> range, optional<out_type>& result_out) const
    {
        out_type result = boost::make_default<out_type>();
        auto [ptr, err] = [&]
        {
            char_cptr beg = &*range.begin();
            char_cptr end = beg + range.size();

            if constexpr (std::is_integral_v<out_type>)
                return std::from_chars(beg, end, result, int(base_));
            else
                return std::from_chars(beg, end, result, chars_format());
        }();
        if (err == std::errc{})
            result_out = result;
    }
    std::chars_format chars_format() const
    {
        static constexpr std::chars_format format[] =
        {
            std::chars_format::fixed,
            std::chars_format::scientific,
            std::chars_format::hex
        };
        return format[int(notation_)];
    }
};

#endif // BOOST_CONVERT_CHARCONV_BASED_CONVERTER_HPP
