// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_PARSER_DETAIL_TEXT_IN_OUT_RESULT_HPP
#define BOOST_PARSER_DETAIL_TEXT_IN_OUT_RESULT_HPP

#include <boost/parser/detail/text/config.hpp>


namespace boost::parser::detail { namespace text {

    /** A replacement for C++20's `std::ranges::in_out_result` for use in
        pre-C++20 build modes. */
    template<typename I, typename O>
    struct in_out_result
    {
        [[no_unique_address]] I in;
        [[no_unique_address]] O out;
    };

}}

#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS

#include <ranges>

namespace boost::parser::detail { namespace text { BOOST_PARSER_DETAIL_TEXT_NAMESPACE_V2 {

    namespace dtl {
        template<typename R>
        std::ranges::borrowed_iterator_t<R> result_iterator(R &&);
        template<typename Ptr>
            requires std::is_pointer_v<std::remove_reference_t<Ptr>>
        Ptr result_iterator(Ptr &&);

        template<typename T>
        using uc_result_iterator =
            decltype(dtl::result_iterator(std::declval<T>()));
    }

}}}

#endif

#endif
