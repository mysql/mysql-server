//
// Copyright (c) 2023-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MQTT5_TRAITS_HPP
#define BOOST_MQTT5_TRAITS_HPP

#include <boost/container/small_vector.hpp>
#include <boost/range/iterator_range_core.hpp>
#include <boost/type_traits/remove_cv_ref.hpp>

#include <optional>
#include <utility>
#include <vector>

namespace boost::mqtt5::detail {

template <typename>
constexpr bool is_optional_impl = false;

template <typename T>
constexpr bool is_optional_impl<std::optional<T>> = true;

template <typename T>
constexpr bool is_optional = is_optional_impl<boost::remove_cv_ref_t<T>>;

template <typename, template <typename...> typename>
constexpr bool is_specialization = false;

template <template <typename...> typename T, typename... Args>
constexpr bool is_specialization<T<Args...>, T> = true;

template <typename T>
constexpr bool is_vector = is_specialization<
    boost::remove_cv_ref_t<T>, std::vector
>;

template <typename... Args>
constexpr std::true_type is_small_vector_impl(
    boost::container::small_vector_base<Args...> const &
);
constexpr std::false_type is_small_vector_impl( ... );

template <typename T>
constexpr bool is_small_vector =
    decltype(is_small_vector_impl(std::declval<T>()))::value;

template <typename T>
constexpr bool is_pair = is_specialization<
    boost::remove_cv_ref_t<T>, std::pair
>;

template <typename T>
constexpr bool is_boost_iterator = is_specialization<
    boost::remove_cv_ref_t<T>, boost::iterator_range
>;

} // end namespace boost::mqtt5::detail

#endif // !BOOST_MQTT5_TRAITS_HPP
