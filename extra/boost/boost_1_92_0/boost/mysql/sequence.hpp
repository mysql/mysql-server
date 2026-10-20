//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_SEQUENCE_HPP
#define BOOST_MYSQL_SEQUENCE_HPP

#include <boost/mysql/detail/sequence.hpp>

#ifdef BOOST_MYSQL_HAS_CONCEPTS
#include <concepts>
#endif

namespace boost {
namespace mysql {

/**
 * \brief The return type of \ref sequence.
 * \details
 * Contains a range, a formatter function, and a glue string.
 * This type satisfies the `Formattable` concept.
 *
 * When formatted, \ref format_function is invoked for each element
 * in \ref range. The string \ref glue is output raw (as per \ref format_context_base::append_raw)
 * between consecutive invocations of the formatter function, generating an effect
 * similar to `std::ranges::views::join`.
 *
 * Don't instantiate this struct directly - use \ref sequence, instead.
 *
 * \par Type requirements
 *
 *   - Expressions `std::begin(range)` and `std::end(range)` should return an input iterator/sentinel
 *     pair that can be compared for (in)equality.
 *   - The expression `static_cast<const FormatFn&>(fn)(* std::begin(range), ctx)`
 *     should be well formed, with `ctx` begin a `format_context_base&`.
 */
template <class Range, class FormatFn>
#if defined(BOOST_MYSQL_HAS_CONCEPTS)
    requires detail::format_fn_for_range<FormatFn, Range>
#endif
struct format_sequence
{
    /// The range to format.
    Range range;

    /// The format function to apply to each element in the range.
    FormatFn format_function;

    /// The string to output between range elements.
    constant_string_view glue;
};

/**
 * \brief The type of range produced by \ref sequence.
 * \details
 * This type trait can be used to obtain the range type produced
 * by calling \ref sequence. This type is used as the `Range` template
 * parameter in \ref format_sequence.
 *
 * By default, \ref sequence copies its input range, unless
 * using `std::ref`. C arrays are copied into `std::array` objects.
 * This type trait accounts these transformations.
 *
 * Formally, given the input range type `T` (which can be a reference with cv-qualifiers):
 *
 *  - If `T` is a C array or a reference to one (as per `std::is_array`),
 *    and the array elements' type is `U`, yields `std::array<std::remove_cv_t<U>, N>`.
 *  - If `T` is a `std::reference_wrapper<U>` object, or a reference to one,
 *    yields `U&`.
 *  - Otherwise, yields `std::remove_cvref_t<T>`.
 *
 * Examples:
 *
 *  - `sequence_range_t<const std::vector<int>&>` is `std::vector<int>`.
 *  - `sequence_range_t<std::reference_wrapper<std::vector<int>>>` is `std::vector<int>&`.
 *  - `sequence_range_t<std::reference_wrapper<const std::vector<int>>>` is `const std::vector<int>&`.
 *  - `sequence_range_t<int(&)[4]>` is `std::array<int, 4>`.
 */
template <class T>
using sequence_range_t =
#ifdef BOOST_MYSQL_DOXYGEN
    __see_below__
#else
    typename detail::sequence_range_type<T>::type;
#endif
    ;

/**
 * \brief Creates an object that, when formatted, applies a per-element function to a range.
 * \details
 * Objects returned by this function satisfy `Formattable`.
 * Formatting such objects invokes `fn` for each element
 * in `range`, outputting `glue` between invocations.
 * This generates an effect similar to `std::ranges::views::join`.
 *
 * By default, this function creates an owning object by decay-copying `range` into it.
 * C arrays are copied into `std::array` objects. This behavior can be disabled
 * by passing `std::reference_wrapper` objects, which are converted to references
 * (as `std::make_tuple` does). The \ref sequence_range_t
 * type trait accounts for these transformations.
 *
 * Formally:
 *
 *   - If `Range` is a (possibly cv-qualified) C array reference (as per `std::is_array<Range>`),
 *     and the array has `N` elements of type `U`, the output range type is
 *     `std::array<std::remove_cv< U >, N>`, and the range is created as if `std::to_array` was called.
 *   - If `Range` is a `std::reference_wrapper< U >` object, or a reference to one,
 *     the output range type is `U&`. This effectively disables copying the input range.
 *     The resulting object will be a view type, and the caller is responsible for lifetime management.
 *   - Otherwise, the output range type is `std::remove_cvref_t<Range>`, and it will be
 *     created by forwarding the passed `range`.
 *
 * `FormatFn` is always decay-copied into the resulting object.
 *
 * The glue string is always stored as a view, as it should usually point to a compile-time constant.
 *
 * \par Type requirements
 *
 * The resulting range and format function should be compatible, and any required
 * copy/move operations should be well defined. Formally:
 *
 *   - `std::decay_t<FormatFn>` should be a formatter function compatible with
 *     the elements of the output range. See \ref format_sequence for the formal requirements.
 *   - If `Range` is a `std::reference_wrapper< U >`, or a reference to one,
 *     no further requirements are placed on `U`.
 *   - If `Range` is a lvalue reference to a C array, its elements should be copy-constructible
 *     (as per `std::to_array` requirements).
 *   - If `Range` is a rvalue reference to a C array, its elements should be move-constructible
 *     (as per `std::to_array` requirements).
 *   - Performing a decay-copy of `FormatFn` should be well defined.
 *
 * \par Exception safety
 * Basic guarantee. Propagates any exception thrown when constructing the output
 * range and format function.
 */
template <class Range, class FormatFn>
#if defined(BOOST_MYSQL_HAS_CONCEPTS)
    requires std::constructible_from<typename std::decay<FormatFn>::type, FormatFn&&>
#endif
format_sequence<sequence_range_t<Range>, typename std::decay<FormatFn>::type> sequence(
    Range&& range,
    FormatFn&& fn,
    constant_string_view glue = ", "
)

{
    return {detail::cast_range(std::forward<Range>(range)), std::forward<FormatFn>(fn), glue};
}

template <class Range, class FormatFn>
struct formatter<format_sequence<Range, FormatFn>>
{
    const char* parse(const char* begin, const char*) { return begin; }

    void format(format_sequence<Range, FormatFn>& value, format_context_base& ctx) const
    {
        detail::do_format_sequence(value.range, value.format_function, value.glue, ctx);
    }

    void format(const format_sequence<Range, FormatFn>& value, format_context_base& ctx) const
    {
        detail::do_format_sequence(value.range, value.format_function, value.glue, ctx);
    }
};

}  // namespace mysql
}  // namespace boost

#endif
