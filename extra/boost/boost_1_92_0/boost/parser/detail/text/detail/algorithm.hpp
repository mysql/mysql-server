// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_PARSER_DETAIL_TEXT_DETAIL_ALGORITHM_HPP
#define BOOST_PARSER_DETAIL_TEXT_DETAIL_ALGORITHM_HPP

#include <boost/parser/detail/text/concepts.hpp>
#include <boost/parser/detail/detection.hpp>
#include <boost/parser/detail/text/detail/iterator.hpp>

#include <iterator>
#include <numeric>
#include <type_traits>
#include <utility>
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
#include <ranges>
#endif

#include <cstdint>


namespace boost::parser::detail { namespace text { namespace detail {

    template<typename I>
    auto prev(I it)
    {
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
        return std::ranges::prev(it);
#else
        return std::prev(it);
#endif
    }
    template<typename I>
    auto next(I it)
    {
#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
        return std::ranges::next(it);
#else
        return std::next(it);
#endif
    }

    template<typename T>
    using remove_cv_ref_t =
        typename std::remove_cv<typename std::remove_reference<T>::type>::type;

#if BOOST_PARSER_DETAIL_TEXT_USE_CONCEPTS
    // A grapheme_range that has a sentinel type that is not an iterator, but
    // that is comparable with T's interator type.
    template<typename T>
    concept cp_sentinel_gr_rng =
        grapheme_range<T> && !grapheme_iter<sentinel_t<T>> &&
        requires(iterator_t<T> first, sentinel_t<T> last) {
            { first.base() == last } -> std::convertible_to<bool>;
        };

    template<typename T>
    using gr_rng_cp_iter_t = decltype(std::declval<iterator_t<T>>().base());
    template<typename T>
    using gr_rng_cp_sent_t = std::conditional_t<
        cp_sentinel_gr_rng<T>,
        sentinel_t<T>,
        gr_rng_cp_iter_t<T>>;

#else

    template<typename T>
    using has_base = decltype(std::declval<T>().base());
    template<typename T>
    using sentinel_comparable_to_iter_base =
        decltype(std::declval<T>().begin().base() == std::declval<T>().end());

    // A grapheme_range that has a sentinel type that is not an iterator, but
    // that is comparable with T's interator type.
    template<
        typename T,
        bool IsIt = is_detected_v<has_base, iterator_t<T>> &&
                    !is_detected_v<has_base, sentinel_t<T>> &&
                    is_detected_v<sentinel_comparable_to_iter_base, T>>
    constexpr bool is_cp_sentinel_gr_rng_v = false;
    template<typename T>
    constexpr bool is_cp_sentinel_gr_rng_v<T, true> = true;

    template<typename T>
    using gr_rng_cp_iter_t =
        decltype(detail::begin(std::declval<T &>()).base());
    template<typename T>
    using gr_rng_cp_sent_t = std::conditional_t<
        is_cp_sentinel_gr_rng_v<T>,
        sentinel_t<T>,
        gr_rng_cp_iter_t<T>>;

#endif

    template<typename T>
    using has_begin = decltype(*detail::begin(std::declval<T &>()));
    template<typename T>
    using has_end = decltype(*detail::end(std::declval<T &>()));

    template<typename T>
    using value_type_ = typename std::remove_cv<
        typename std::remove_reference<typename T::value_type>::type>::type;


    template<typename T>
    using nonpointer_iterator_category_ =
        typename T::iterator::iterator_category;

    template<typename T>
    using iterator_pointer_expr = std::is_pointer<typename T::iterator>;

    template<typename T>
    using iterator_category_ = typename std::conditional<
        detected_or_t<std::false_type, iterator_pointer_expr>::value,
        std::random_access_iterator_tag,
        detected_t<nonpointer_iterator_category_, T>>::type;



    template<typename T, typename U, int N>
    constexpr bool
        is_convertible_and_n_bytes_v = std::is_convertible<T, U>::value &&
                                       sizeof(T) == N;



    template<typename T>
    constexpr bool is_char_iter_v =
        std::is_same<char *, typename std::remove_cv<T>::type>::value ||
        std::is_same<char const *, typename std::remove_cv<T>::type>::value ||
#if defined(__cpp_char8_t)
        std::is_same<char8_t *, typename std::remove_cv<T>::type>::value ||
        std::is_same<char8_t const *, typename std::remove_cv<T>::type>::
            value ||
#endif
        is_convertible_and_n_bytes_v<detected_t<value_type_, T>, char, 1>;

    // Needed for detected_or_t.
    template<typename T>
    using is_char_iter = std::integral_constant<bool, is_char_iter_v<T>>;

    template<typename T>
    constexpr bool is_char_range_v =
        (is_convertible_and_n_bytes_v<
             remove_cv_ref_t<detected_t<has_begin, T>>,
             char,
             1> &&
         is_convertible_and_n_bytes_v<
             remove_cv_ref_t<detected_t<has_end, T>>,
             char,
             1>);



    template<
        typename T,
        typename R1,
        typename Exclude,
        bool R1IsCharRange =
            is_char_range_v<R1> && !std::is_same<R1, Exclude>::value>
    struct rng_alg_ret
    {
    };

    template<typename T, typename R1, typename Exclude>
    struct rng_alg_ret<T, R1, Exclude, true>
    {
        using type = T;
    };

    template<typename T, typename R1, typename Exclude = void>
    using rng_alg_ret_t = typename rng_alg_ret<T, R1, Exclude>::type;

    template<
        typename T,
        typename R1,
        typename R2,
        bool R1IsCharRange = is_char_range_v<R1>,
        bool R2IsCharRange = is_char_range_v<R2>>
    struct rngs_alg_ret
    {
    };

    template<typename T, typename R1, typename R2>
    struct rngs_alg_ret<T, R1, R2, true, true>
    {
        using type = T;
    };

    template<typename T, typename R1, typename R2>
    using rngs_alg_ret_t = typename rngs_alg_ret<T, R1, R2>::type;



    template<typename T>
    using has_contig_begin = decltype(&*detail::begin(std::declval<T &>()));
    template<typename T>
    using has_contig_end = decltype(&*detail::end(std::declval<T &>()));

    template<typename T>
    constexpr bool is_contig_char_range_v =
        ((std::is_same<
              fixup_ptr_t<detected_t<has_contig_begin, T>>,
              char const *>::value &&
          std::is_same<
              fixup_ptr_t<detected_t<has_contig_end, T>>,
              char const *>::value)
#if defined(__cpp_char8_t)
         || (std::is_same<
                 fixup_ptr_t<detected_t<has_contig_begin, T>>,
                 char8_t const *>::value &&
             std::is_same<
                 fixup_ptr_t<detected_t<has_contig_end, T>>,
                 char8_t const *>::value)
#endif
             ) &&
        std::is_convertible<
            iterator_category_<T>,
            std::random_access_iterator_tag>::value;



    template<
        typename T,
        typename R1,
        bool R1IsContigCharRange = is_contig_char_range_v<R1>>
    struct contig_rng_alg_ret
    {
    };

    template<typename T, typename R1>
    struct contig_rng_alg_ret<T, R1, true>
    {
        using type = T;
    };

    template<typename T, typename R1>
    using contig_rng_alg_ret_t = typename contig_rng_alg_ret<T, R1>::type;

    template<
        typename T,
        typename R1,
        typename R2,
        bool R1IsContigCharRange = is_contig_char_range_v<R1>,
        bool R2IsContigCharRange = is_contig_char_range_v<R2>>
    struct contig_rngs_alg_ret
    {
    };

    template<typename T, typename R1, typename R2>
    struct contig_rngs_alg_ret<T, R1, R2, true, true>
    {
        using type = T;
    };

    template<typename T, typename R1, typename R2>
    using contig_rngs_alg_ret_t = typename contig_rngs_alg_ret<T, R1, R2>::type;



    template<typename T>
    constexpr bool is_char16_range_v =
        (is_convertible_and_n_bytes_v<
             remove_cv_ref_t<detected_t<has_begin, T>>,
             uint16_t,
             2> &&
         is_convertible_and_n_bytes_v<
             remove_cv_ref_t<detected_t<has_end, T>>,
             uint16_t,
             2>);



    template<
        typename T,
        typename R1,
        bool R1IsChar16Range = is_char16_range_v<R1>>
    struct rng16_alg_ret
    {
    };

    template<typename T, typename R1>
    struct rng16_alg_ret<T, R1, true>
    {
        using type = T;
    };

    template<typename T, typename R1>
    using rng16_alg_ret_t = typename rng16_alg_ret<T, R1>::type;



    template<typename T, typename R1, bool R1IsCharRange = is_char_iter_v<R1>>
    struct char_iter_ret
    {
    };

    template<typename T, typename R1>
    struct char_iter_ret<T, R1, true>
    {
        using type = T;
    };

    template<typename T, typename R1>
    using char_iter_ret_t = typename char_iter_ret<T, R1>::type;



    template<typename T>
    constexpr bool is_code_point_v = std::is_integral<T>::value &&
                                     sizeof(T) == 4;

    template<typename T>
    using has_deref_and_incr =
        std::pair<decltype(*std::declval<T>()), decltype(++std::declval<T>())>;

    template<typename T>
    constexpr bool is_cp_iter_v =
        ((std::is_pointer<T>::value &&
          is_code_point_v<typename std::remove_cv<
              typename std::remove_pointer<T>::type>::type>) ||
         (is_detected_v<has_deref_and_incr, T> &&
          is_code_point_v<
              typename std::remove_cv<detected_t<value_type_, T>>::type>));

    template<typename T, typename R1, bool R1IsCPRange = is_cp_iter_v<R1>>
    struct cp_iter_ret
    {
    };

    template<typename T, typename R1>
    struct cp_iter_ret<T, R1, true>
    {
        using type = T;
    };

    template<typename T, typename R1>
    using cp_iter_ret_t = typename cp_iter_ret<T, R1>::type;

    template<
        typename T,
        typename R1,
        typename R2,
        bool R1R2AreCPRanges = is_cp_iter_v<R1> && is_cp_iter_v<R2>>
    struct cp_iters_ret
    {
    };

    template<typename T, typename R1, typename R2>
    struct cp_iters_ret<T, R1, R2, true>
    {
        using type = T;
    };

    template<typename T, typename R1, typename R2>
    using cp_iters_ret_t = typename cp_iters_ret<T, R1, R2>::type;


    template<typename T>
    constexpr bool is_16_code_unit_v = std::is_integral<T>::value &&
                                       sizeof(T) == 2;

    template<typename T>
    constexpr bool is_16_iter_v =
        ((std::is_pointer<T>::value &&
          is_16_code_unit_v<typename std::remove_cv<
              typename std::remove_pointer<T>::type>::type>) ||
         (is_detected_v<has_deref_and_incr, T> &&
          is_16_code_unit_v<
              typename std::remove_cv<detected_t<value_type_, T>>::type>));

    template<typename T, typename R1, bool R1IsCPRange = is_16_iter_v<R1>>
    struct _16_iter_ret
    {
    };

    template<typename T, typename R1>
    struct _16_iter_ret<T, R1, true>
    {
        using type = T;
    };

    template<typename T, typename R1>
    using _16_iter_ret_t = typename _16_iter_ret<T, R1>::type;


    template<typename T>
    constexpr bool is_8_code_unit_v = std::is_integral<T>::value &&
                                      sizeof(T) == 1;

    template<typename T>
    constexpr bool is_8_iter_v =
        ((std::is_pointer<T>::value &&
          is_8_code_unit_v<typename std::remove_cv<
              typename std::remove_pointer<T>::type>::type>) ||
         (is_detected_v<has_deref_and_incr, T> &&
          is_8_code_unit_v<
              typename std::remove_cv<detected_t<value_type_, T>>::type>));

    template<typename T, typename R1, bool R1IsCPRange = is_8_iter_v<R1>>
    struct _8_iter_ret
    {
    };

    template<typename T, typename R1>
    struct _8_iter_ret<T, R1, true>
    {
        using type = T;
    };

    template<typename T, typename R1>
    using _8_iter_ret_t = typename _8_iter_ret<T, R1>::type;



    template<typename T, typename U>
    using comparable_ = decltype(std::declval<T>() == std::declval<U>());

    template<
        typename T,
        typename CPIter,
        typename Sentinel,
        bool FIsWordPropFunc = is_cp_iter_v<CPIter> &&
            is_detected_v<comparable_, CPIter, Sentinel>>
    struct cp_iter_sntl_ret
    {
    };

    template<typename T, typename CPIter, typename Sentinel>
    struct cp_iter_sntl_ret<T, CPIter, Sentinel, true>
    {
        using type = T;
    };

    template<typename T, typename CPIter, typename Sentinel>
    using cp_iter_sntl_ret_t =
        typename cp_iter_sntl_ret<T, CPIter, Sentinel>::type;

    template<typename T, typename R>
    using cp_rng_alg_ret_t =
        cp_iter_sntl_ret_t<T, iterator_t<R>, sentinel_t<R>>;

    template<
        typename T,
        typename CPIter1,
        typename Sentinel1,
        typename CPIter2,
        typename Sentinel2,
        bool AreCPRanges = is_cp_iter_v<CPIter1> && is_cp_iter_v<CPIter2> &&
            is_detected_v<comparable_, CPIter1, Sentinel1> &&
                is_detected_v<comparable_, CPIter2, Sentinel2>>
    struct cp_iters_sntls_ret
    {
    };

    template<
        typename T,
        typename CPIter1,
        typename Sentinel1,
        typename CPIter2,
        typename Sentinel2>
    struct cp_iters_sntls_ret<T, CPIter1, Sentinel1, CPIter2, Sentinel2, true>
    {
        using type = T;
    };

    template<
        typename T,
        typename CPIter1,
        typename Sentinel1,
        typename CPIter2,
        typename Sentinel2>
    using cp_iters_sntls_ret_t =
        typename cp_iters_sntls_ret<T, CPIter1, Sentinel1, CPIter2, Sentinel2>::
            type;

    template<typename T, typename R1, typename R2>
    using cp_rngs_alg_ret_t = cp_iters_sntls_ret_t<
        T,
        iterator_t<R1>,
        sentinel_t<R1>,
        iterator_t<R2>,
        sentinel_t<R2>>;


    template<int Size, typename T>
    constexpr bool is_cu_iter_v =
        Size == 1 ? is_char_iter_v<T> : is_16_iter_v<T>;

    template<
        int Size,
        typename T,
        typename U,
        bool UIsCUIter = is_cu_iter_v<Size, U>>
    struct cu_iter_ret
    {
    };

    template<int Size, typename T, typename U>
    struct cu_iter_ret<Size, T, U, true>
    {
        using type = T;
    };

    template<int Size, typename T, typename U>
    using cu_iter_ret_t = typename cu_iter_ret<Size, T, U>::type;

    template<int Size, typename T>
    constexpr bool is_cu_range_v =
        Size == 1 ? is_char_range_v<T> : is_char16_range_v<T>;

    template<
        int Size,
        typename T,
        typename U,
        typename Exclude,
        bool UIsCURange =
            is_cu_range_v<Size, U> && !std::is_same<U, Exclude>::value>
    struct cu_rng_alg_ret
    {
    };

    template<int Size, typename T, typename U, typename Exclude>
    struct cu_rng_alg_ret<Size, T, U, Exclude, true>
    {
        using type = T;
    };

    template<int Size, typename T, typename U, typename Exclude = void>
    using cu_rng_alg_ret_t = typename cu_rng_alg_ret<Size, T, U, Exclude>::type;


    template<typename T>
    using is_grapheme_iter_expr = std::integral_constant<
        bool,
        is_cp_iter_v<
            remove_cv_ref_t<decltype(std::declval<const T>().base())>>>;

    template<typename T>
    using is_grapheme_iter =
        detected_or_t<std::false_type, is_grapheme_iter_expr, T>;

    template<
        typename T,
        typename Iter,
        bool R1IsGraphemeIter = is_grapheme_iter<Iter>::value>
    struct graph_iter_alg_ret
    {};

    template<typename T, typename Iter>
    struct graph_iter_alg_ret<T, Iter, true>
    {
        using type = T;
    };

    template<typename T, typename Iter>
    using graph_iter_alg_ret_t = typename graph_iter_alg_ret<T, Iter>::type;

    template<
        typename T,
        typename Iter1,
        typename Iter2,
        bool Iter1Iter2AreGraphemeIter =
            is_grapheme_iter<Iter1>::value && is_grapheme_iter<Iter2>::value>
    struct graph_iters_alg_ret
    {};

    template<typename T, typename Iter1, typename Iter2>
    struct graph_iters_alg_ret<T, Iter1, Iter2, true>
    {
        using type = T;
    };

    template<typename T, typename Iter1, typename Iter2>
    using graph_iters_alg_ret_t =
        typename graph_iters_alg_ret<T, Iter1, Iter2>::type;


    template<typename Size, typename T>
    using is_grapheme_cu_iter_expr = std::integral_constant<
        bool,
        is_cp_iter_v<
            remove_cv_ref_t<decltype(std::declval<const T>().base())>> &&
            is_cu_iter_v<
                Size::value,
                remove_cv_ref_t<
                    decltype(std::declval<const T>().base().base())>>>;

    template<int Size, typename T>
    using is_grapheme_cu_iter = detected_or_t<
        std::false_type,
        is_grapheme_cu_iter_expr,
        std::integral_constant<int, Size>,
        T>;

    template<
        int Size,
        typename T,
        typename R1,
        bool R1IsGraphemeIter = is_grapheme_cu_iter<Size, R1>::value>
    struct graph_iter_cu_alg_ret
    {};

    template<int Size, typename T, typename R1>
    struct graph_iter_cu_alg_ret<Size, T, R1, true>
    {
        using type = T;
    };

    template<int Size, typename T, typename R1>
    using graph_iter_alg_cu_ret_t =
        typename graph_iter_cu_alg_ret<Size, T, R1>::type;

    template<typename T>
    using is_grapheme_range_expr = std::integral_constant<
        bool,
        is_cp_iter_v<remove_cv_ref_t<
            decltype(std::declval<const T>().begin().base())>> &&
            is_cp_iter_v<remove_cv_ref_t<
                decltype(std::declval<const T>().end().base())>> &&
            void_<
                decltype(std::declval<const T>().begin().base().base()),
                decltype(std::declval<const T>().end().base().base())>::value>;

    template<typename T>
    using is_grapheme_range =
        detected_or_t<std::false_type, is_grapheme_range_expr, T>;

    template<
        typename T,
        typename R1,
        bool R1IsGraphemeRange = is_grapheme_range<R1>::value>
    struct graph_rng_alg_ret
    {};

    template<typename T, typename R1>
    struct graph_rng_alg_ret<T, R1, true>
    {
        using type = T;
    };

    template<typename T, typename R1>
    using graph_rng_alg_ret_t = typename graph_rng_alg_ret<T, R1>::type;


    template<typename T>
    using is_cp_grapheme_range_expr = std::integral_constant<
        bool,
        is_cp_iter_v<remove_cv_ref_t<
            decltype(std::declval<const T>().begin().base())>> &&
            is_cp_iter_v<remove_cv_ref_t<
                decltype(std::declval<const T>().end().base())>>>;

    template<typename T>
    using is_cp_grapheme_range =
        detected_or_t<std::false_type, is_cp_grapheme_range_expr, T>;

    template<
        typename T,
        typename R1,
        bool R1IsGraphemeRange = is_grapheme_range<R1>::value>
    struct cp_graph_rng_alg_ret
    {};

    template<typename T, typename R1>
    struct cp_graph_rng_alg_ret<T, R1, true>
    {
        using type = T;
    };

    template<typename T, typename R1>
    using cp_graph_rng_alg_ret_t = typename cp_graph_rng_alg_ret<T, R1>::type;

    template<
        typename T,
        typename R1,
        typename R2,
        bool R1R2AreGraphemeRanges =
            is_cp_grapheme_range<R1>::value && is_cp_grapheme_range<R2>::value>
    struct graph_rngs_alg_ret
    {};

    template<typename T, typename R1, typename R2>
    struct graph_rngs_alg_ret<T, R1, R2, true>
    {
        using type = T;
    };

    template<typename T, typename R1, typename R2>
    using graph_rngs_alg_ret_t = typename graph_rngs_alg_ret<T, R1, R2>::type;

    template<
        typename T,
        typename R1,
        typename R2,
        bool R1IsGraphemeRangeButNotR2 =
            is_cp_grapheme_range<R1>::value && !is_cp_grapheme_range<R2>::value>
    struct lazy_graph_rng_and_non_alg_ret
    {};

    template<typename T, typename R1, typename R2>
    struct lazy_graph_rng_and_non_alg_ret<T, R1, R2, true>
    {
        using type = typename T::type;
    };

    template<typename T, typename R1, typename R2>
    using lazy_graph_rng_and_non_alg_ret_t =
        typename lazy_graph_rng_and_non_alg_ret<T, R1, R2>::type;

    template<
        typename T,
        typename R1,
        typename R2,
        bool R1IsGraphemeRangeButNotR2 =
            is_cp_grapheme_range<R1>::value && !is_cp_grapheme_range<R2>::value>
    struct graph_rng_and_non_alg_ret
    {};

    template<typename T, typename R1, typename R2>
    struct graph_rng_and_non_alg_ret<T, R1, R2, true>
    {
        using type = T;
    };

    template<typename T, typename R1, typename R2>
    using graph_rng_and_non_alg_ret_t =
        typename graph_rng_and_non_alg_ret<T, R1, R2>::type;

    template<
        typename T,
        typename R1,
        typename R2,
        bool R1R2AreNotGraphemeRanges = !is_cp_grapheme_range<R1>::value &&
                                        !is_cp_grapheme_range<R2>::value>
    struct non_graph_rngs_alg_ret
    {};

    template<typename T, typename R1, typename R2>
    struct non_graph_rngs_alg_ret<T, R1, R2, true>
    {
        using type = T;
    };

    template<typename T, typename R1, typename R2>
    using non_graph_rngs_alg_ret_t =
        typename non_graph_rngs_alg_ret<T, R1, R2>::type;

    template<
        typename T,
        typename R1,
        typename R2,
        bool R1R2AreNotGraphemeRanges = !is_cp_grapheme_range<R1>::value &&
                                        !is_cp_grapheme_range<R2>::value>
    struct lazy_non_graph_rngs_alg_ret
    {};

    template<typename T, typename R1, typename R2>
    struct lazy_non_graph_rngs_alg_ret<T, R1, R2, true>
    {
        using type = typename T::type;
    };

    template<typename T, typename R1, typename R2>
    using lazy_non_graph_rngs_alg_ret_t =
        typename lazy_non_graph_rngs_alg_ret<T, R1, R2>::type;


    template<typename T>
    using is_contig_grapheme_range_expr = std::integral_constant<
        bool,
        (std::is_same<
             decltype(std::declval<const T>().begin().base().base()),
             char const *>::value ||
         std::is_same<
             decltype(std::declval<const T>().begin().base().base()),
             char *>::value) &&
            (std::is_same<
                 decltype(std::declval<const T>().end().base().base()),
                 char const *>::value ||
             std::is_same<
                 decltype(std::declval<const T>().end().base().base()),
                 char *>::value)>;

    template<typename T>
    using is_contig_grapheme_range =
        detected_or_t<std::false_type, is_contig_grapheme_range_expr, T>;

    template<
        typename T,
        typename R1,
        bool R1IsContigGraphemeRange = is_contig_grapheme_range<R1>::value>
    struct contig_graph_rng_alg_ret
    {
    };

    template<typename T, typename R1>
    struct contig_graph_rng_alg_ret<T, R1, true>
    {
        using type = T;
    };

    template<typename T, typename R1>
    using contig_graph_rng_alg_ret_t =
        typename contig_graph_rng_alg_ret<T, R1>::type;



    inline std::size_t hash_combine_(std::size_t seed, std::size_t value)
    {
        return seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    template<int N>
    struct hash_4_more_chars
    {
        template<typename Iter>
        static std::size_t call(std::size_t curr, Iter it)
        {
            return curr;
        }
    };

    template<>
    struct hash_4_more_chars<8>
    {
        template<typename Iter>
        static std::size_t call(std::size_t curr, Iter it)
        {
            curr <<= 32;
            curr += (*(it + 4) << 24) + (*(it + 5) << 16) + (*(it + 2) << 6) +
                    (*(it + 7) << 0);
            return curr;
        }
    };

    template<typename CharRange>
    std::size_t hash_char_range(CharRange const & r)
    {
        auto first = r.begin();
        auto last = r.end();
        auto const size = last - first;
        auto const remainder = size % sizeof(std::size_t);
        last -= remainder;

        std::size_t retval = size;
        for (; first != last; first += sizeof(std::size_t)) {
            std::size_t curr = (*(first + 0) << 24) + (*(first + 1) << 16) +
                               (*(first + 2) << 8) + (*(first + 3) << 0);
            curr = hash_4_more_chars<sizeof(std::size_t)>::call(curr, first);
            retval = detail::hash_combine_(retval, curr);
        }

        first = last;
        last += remainder;
        for (; first != last; ++first) {
            retval = detail::hash_combine_(retval, *first);
        }

        return retval;
    }

    template<typename GraphemeRange>
    std::size_t hash_grapheme_range(GraphemeRange const & r)
    {
        std::size_t cps = 0;
        std::size_t retval = std::accumulate(
            r.begin().base(),
            r.end().base(),
            std::size_t(0),
            [&cps](std::size_t seed, std::size_t value) {
                ++cps;
                return detail::hash_combine_(seed, value);
            });
        return detail::hash_combine_(retval, cps);
    }

    template<typename Iter>
    using char_value_expr = std::integral_constant<
        bool,
        std::is_integral<
            typename std::iterator_traits<Iter>::value_type>::value &&
            sizeof(typename std::iterator_traits<Iter>::value_type) == 1>;

    template<typename Iter>
    constexpr bool is_char_ptr_v = std::is_pointer<Iter>::value &&
        detected_or_t<std::false_type, char_value_expr, Iter>::value;

    template<typename Iter>
    using _16_value_expr = std::integral_constant<
        bool,
        std::is_integral<
            typename std::iterator_traits<Iter>::value_type>::value &&
            sizeof(typename std::iterator_traits<Iter>::value_type) == 2>;

    template<typename Iter>
    constexpr bool is_16_ptr_v = std::is_pointer<Iter>::value &&
        detected_or_t<std::false_type, _16_value_expr, Iter>::value;

    template<typename Iter>
    using cp_value_expr = std::integral_constant<
        bool,
        std::is_integral<
            typename std::iterator_traits<Iter>::value_type>::value &&
            sizeof(typename std::iterator_traits<Iter>::value_type) == 4>;

    template<typename Iter>
    constexpr bool is_cp_ptr_v = std::is_pointer<Iter>::value &&
        detected_or_t<std::false_type, cp_value_expr, Iter>::value;

}}}

#endif
