// Copyright (C) 2022 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_PARSER_SUBRANGE_HPP
#define BOOST_PARSER_SUBRANGE_HPP

#include <boost/parser/detail/text/config.hpp>
#include <boost/parser/detail/text/detail/algorithm.hpp>

#include <boost/parser/detail/stl_interfaces/view_interface.hpp>


namespace boost::parser {

    /** A simple view type used throughout the rest of the library in C++17
        builds; similar to `std::ranges::subrange`. */
#if BOOST_PARSER_USE_CONCEPTS
    template<std::forward_iterator I, std::sentinel_for<I> S = I>
#else
    template<typename I, typename S = I>
#endif
    struct subrange : detail::stl_interfaces::view_interface<subrange<I, S>>
    {
        constexpr subrange() = default;
        constexpr subrange(I first, S last) : first_(first), last_(last) {}
        template<typename R>
        constexpr explicit subrange(R const & r) :
            first_(detail::text::detail::begin(r)),
            last_(detail::text::detail::end(r))
        {}

        constexpr I begin() const { return first_; }
        constexpr S end() const { return last_; }

        [[nodiscard]] constexpr subrange next(std::ptrdiff_t n = 1) const
        {
            return subrange{detail::text::detail::next(first_), last_};
        }
        [[nodiscard]] constexpr subrange prev(std::ptrdiff_t n = 1) const
        {
            return subrange{detail::text::detail::prev(first_), last_};
        }

        constexpr subrange & advance(std::ptrdiff_t n)
        {
            std::advance(first_, n);
            return *this;
        }

        template<
            typename I2,
            typename S2,
            typename Enable = std::enable_if_t<
                std::is_convertible<I, I2>::value &&
                std::is_convertible<S, S2>::value>>
        constexpr operator subrange<I2, S2>() const
        {
            return {first_, last_};
        }

    private:
        I first_;
        [[no_unique_address]] S last_;
    };

#if defined(__cpp_deduction_guides)
#if BOOST_PARSER_USE_CONCEPTS
    template<std::input_or_output_iterator I, std::sentinel_for<I> S>
#else
    template<typename I, typename S>
#endif
    subrange(I, S) -> subrange<I, S>;

#if BOOST_PARSER_USE_CONCEPTS
    template<std::ranges::borrowed_range R>
#else
    template<typename R>
#endif
    subrange(R &&) -> subrange<
        detail::text::detail::iterator_t<R>,
        detail::text::detail::sentinel_t<R>>;
#endif

    /** Makes a `subrange<I, S>` from an `I` and an `S`. */
#if BOOST_PARSER_USE_CONCEPTS
    template<std::forward_iterator I, std::sentinel_for<I> S = I>
#else
    template<typename I, typename S = I>
#endif
    constexpr subrange<I, S> make_subrange(I first, S last) noexcept
    {
        return subrange<I, S>(first, last);
    }

}

#if BOOST_PARSER_USE_CONCEPTS

namespace std::ranges {
    template<std::forward_iterator I, std::sentinel_for<I> S>
    inline constexpr bool enable_borrowed_range<boost::parser::subrange<I, S>> =
        true;
}

#endif

#endif
