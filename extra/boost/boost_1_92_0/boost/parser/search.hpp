#ifndef BOOST_PARSER_SEARCH_HPP
#define BOOST_PARSER_SEARCH_HPP

#include <boost/parser/parser.hpp>
#include <boost/parser/transcode_view.hpp>

#include <cstring>


namespace boost::parser {

    namespace detail {
        template<bool Const, typename T>
        using maybe_const = std::conditional_t<Const, const T, T>;

        inline constexpr text::format no_format = text::format::none;

        template<text::format Format = text::format::utf8>
        constexpr auto as_utf =
            text::detail::as_utf_impl<text::utf8_view, text::format::utf8>{};
        template<>
        constexpr auto as_utf<text::format::utf16> =
            text::detail::as_utf_impl<text::utf16_view, text::format::utf16>{};
        template<>
        constexpr auto as_utf<text::format::utf32> =
            text::detail::as_utf_impl<text::utf32_view, text::format::utf32>{};

        template<
            typename R_,
            bool ToCommonRange = false,
            text::format OtherRangeFormat = no_format,
            bool = std::is_same_v<sentinel_t<remove_cv_ref_t<R_>>,
                                  null_sentinel_t> ||
                   text::detail::is_bounded_array_v<remove_cv_ref_t<R_>>>
        struct to_range
        {
            template<typename R>
            static constexpr auto call(R && r)
            {
                static_assert(std::is_same_v<R, R_>);
                using T = remove_cv_ref_t<R>;
                if constexpr (std::is_same_v<sentinel_t<T>, null_sentinel_t>) {
                    auto plus_strlen = [](auto * ptr) {
                        while (*ptr) {
                            ++ptr;
                        }
                        return ptr;
                    };
                    auto const first = r.begin();
                    if constexpr (OtherRangeFormat == no_format) {
                        if constexpr (ToCommonRange) {
                            return BOOST_PARSER_SUBRANGE(
                                first, plus_strlen(first));
                        } else {
                            return (R &&) r;
                        }
                    } else {
                        if constexpr (ToCommonRange) {
                            return BOOST_PARSER_SUBRANGE(
                                first, plus_strlen(first)) |
                                   as_utf<OtherRangeFormat>;
                        } else {
                            return (R &&) r | as_utf<OtherRangeFormat>;
                        }
                    }
                } else if constexpr (text::detail::is_bounded_array_v<T>) {
                    auto const first = std::begin(r);
                    auto last = std::end(r);
                    constexpr auto n = std::extent_v<T>;
                    if (n && !r[n - 1])
                        --last;
                    if constexpr (OtherRangeFormat == no_format) {
                        return BOOST_PARSER_SUBRANGE(first, last);
                    } else {
                        return BOOST_PARSER_SUBRANGE(first, last) |
                               as_utf<OtherRangeFormat>;
                    }
                } else {
                    return (R &&) r | as_utf<OtherRangeFormat>;
                }
            }
        };

        template<typename R_, bool ToCommonRange>
        struct to_range<R_, ToCommonRange, no_format, false>
        {
            template<typename R>
            static constexpr R && call(R && r)
            {
                return (R &&) r;
            }
        };

        template<typename R>
        using to_range_t = decltype(to_range<R>::call(std::declval<R>()));

        struct phony
        {};

        template<
            typename R,
            typename Parser,
            typename GlobalState,
            typename ErrorHandler,
            typename SkipParser>
        auto search_impl(
            R && r,
            parser_interface<Parser, GlobalState, ErrorHandler> const & parser,
            parser_interface<SkipParser> const & skip,
            trace trace_mode)
        {
            auto first = text::detail::begin(r);
            auto const last = text::detail::end(r);

            if (first == last)
                return BOOST_PARSER_SUBRANGE(first, first);

            auto const search_parser = omit[*(char_ - parser)] >> -raw[parser];
            if constexpr (std::is_same_v<SkipParser, eps_parser<phony>>) {
                auto result = parser::prefix_parse(
                    first, last, search_parser, trace_mode);
                if (*result)
                    return **result;
            } else {
                auto result = parser::prefix_parse(
                    first, last, search_parser, skip, trace_mode);
                if (*result)
                    return **result;
            }

            return BOOST_PARSER_SUBRANGE(first, first);
        }

        template<
            typename R,
            typename Parser,
            typename GlobalState,
            typename ErrorHandler,
            typename SkipParser>
#if BOOST_PARSER_USE_CONCEPTS
        std::ranges::borrowed_subrange_t<R>
#else
        auto
#endif
        search_repack_shim(
            R && r,
            parser_interface<Parser, GlobalState, ErrorHandler> const & parser,
            parser_interface<SkipParser> const & skip,
            trace trace_mode)
        {
            using value_type = range_value_t<decltype(r)>;
            if constexpr (std::is_same_v<value_type, char>) {
                return detail::search_impl((R &&) r, parser, skip, trace_mode);
            } else {
                auto r_unpacked = detail::text::unpack_iterator_and_sentinel(
                    text::detail::begin(r), text::detail::end(r));
                auto result =
                    detail::search_impl(r | as_utf32, parser, skip, trace_mode);
                return BOOST_PARSER_SUBRANGE(
                    r_unpacked.repack(text::detail::begin(result).base()),
                    r_unpacked.repack(text::detail::end(result).base()));
            }
        }

        template<typename T>
        constexpr bool is_parser_iface = false;
        template<typename T>
        constexpr bool is_parser_iface<parser_interface<T>> = true;
    }

    /** Returns a subrange to the first match for parser `parser` in `r`,
        using skip-parser `skip`.  This function has a similar interface and
        semantics to `std::ranges::search()`.  Returns `std::ranges::dangling`
        in C++20 and later if `r` is a non-borrowable rvalue. */
    template<
#if BOOST_PARSER_USE_CONCEPTS
        parsable_range R,
#else
        typename R,
#endif
        typename Parser,
        typename GlobalState,
        typename ErrorHandler,
        typename SkipParser
#if !BOOST_PARSER_USE_CONCEPTS
        ,
        typename Enable = std::enable_if_t<detail::is_parsable_range_v<R>>
#endif
        >
    auto search(
        R && r,
        parser_interface<Parser, GlobalState, ErrorHandler> const & parser,
        parser_interface<SkipParser> const & skip,
        trace trace_mode = trace::off)
    {
        return detail::search_repack_shim(
            detail::to_range<R>::call((R &&) r), parser, skip, trace_mode);
    }

    /** Returns a subrange to the first match for parser `parser` in `[first,
        last)`, using skip-parser `skip`.  This function has a similar
        interface and semantics to `std::ranges::search()`. */
    template<
#if BOOST_PARSER_USE_CONCEPTS
        parsable_iter I,
        std::sentinel_for<I> S,
#else
        typename I,
        typename S,
#endif
        typename Parser,
        typename SkipParser,
        typename GlobalState,
#if BOOST_PARSER_USE_CONCEPTS
        error_handler<I, S, GlobalState> ErrorHandler
#else
        typename ErrorHandler,
        typename Enable = std::enable_if_t<
            detail::is_parsable_iter_v<I> &&
            detail::is_equality_comparable_with_v<I, S>>
#endif
        >
    auto search(
        I first,
        S last,
        parser_interface<Parser, GlobalState, ErrorHandler> const & parser,
        parser_interface<SkipParser> const & skip,
        trace trace_mode = trace::off)
    {
        return parser::search(
            BOOST_PARSER_SUBRANGE(first, last), parser, skip, trace_mode);
    }

    /** Returns a subrange to the first match for parser `parser` in `r`.
        This function has a similar interface and semantics to
        `std::ranges::search()`.  Returns `std::ranges::dangling` in C++20 and
        later if `r` is a non-borrowable rvalue. */
    template<
#if BOOST_PARSER_USE_CONCEPTS
        parsable_range R,
#else
        typename R,
#endif
        typename Parser,
        typename GlobalState,
        typename ErrorHandler
#if !BOOST_PARSER_USE_CONCEPTS
        ,
        typename Enable = std::enable_if_t<detail::is_parsable_range_v<R>>
#endif
        >
    auto search(
        R && r,
        parser_interface<Parser, GlobalState, ErrorHandler> const & parser,
        trace trace_mode = trace::off)
    {
        return parser::search(
            (R &&) r,
            parser,
            parser_interface<eps_parser<detail::phony>>{},
            trace_mode);
    }

    /** Returns a subrange to the first match for parser `parser` in `[first,
        last)`.  This function has a similar interface and semantics to
        `std::ranges::search()`. */
    template<
#if BOOST_PARSER_USE_CONCEPTS
        parsable_iter I,
        std::sentinel_for<I> S,
#else
        typename I,
        typename S,
#endif
        typename Parser,
        typename GlobalState,
#if BOOST_PARSER_USE_CONCEPTS
        error_handler<I, S, GlobalState> ErrorHandler
#else
        typename ErrorHandler,
        typename Enable = std::enable_if_t<
            detail::is_parsable_iter_v<I> &&
            detail::is_equality_comparable_with_v<I, S>>
#endif
        >
    auto search(
        I first,
        S last,
        parser_interface<Parser, GlobalState, ErrorHandler> const & parser,
        trace trace_mode = trace::off)
    {
        return parser::search(
            BOOST_PARSER_SUBRANGE(first, last),
            parser,
            parser_interface<eps_parser<detail::phony>>{},
            trace_mode);
    }

    /** Produces a sequence of subranges of the underlying sequence of type
        `V`.  Each subrange is a nonoverlapping match of the given parser,
        using a skip-parser if provided. */
    template<
#if BOOST_PARSER_USE_CONCEPTS
        std::ranges::viewable_range V,
#else
        typename V,
#endif
        typename Parser,
        typename GlobalState,
        typename ErrorHandler,
        typename SkipParser>
    struct search_all_view
        : detail::stl_interfaces::view_interface<
              search_all_view<V, Parser, GlobalState, ErrorHandler, SkipParser>>
    {
        constexpr search_all_view() = default;
        constexpr search_all_view(
            V base,
            parser_interface<Parser, GlobalState, ErrorHandler> const & parser,
            parser_interface<SkipParser> const & skip,
            trace trace_mode = trace::off) :
            base_(std::move(base)),
            parser_(parser),
            skip_(skip),
            trace_mode_(trace_mode)
        {}
        constexpr search_all_view(
            V base,
            parser_interface<Parser, GlobalState, ErrorHandler> const & parser,
            trace trace_mode = trace::off) :
            base_(std::move(base)),
            parser_(parser),
            skip_(),
            trace_mode_(trace_mode)
        {}

        constexpr V base() const &
#if BOOST_PARSER_USE_CONCEPTS
            requires std::copy_constructible<V>
#endif
        {
            return base_;
        }
        constexpr V base() && { return std::move(base_); }

        constexpr auto begin() { return iterator<false>{this}; }
        constexpr auto end() { return sentinel<false>{}; }

        constexpr auto begin() const
#if BOOST_PARSER_USE_CONCEPTS
            requires std::ranges::range<const V>
#endif
        {
            return iterator<true>{this};
        }
        constexpr auto end() const
#if BOOST_PARSER_USE_CONCEPTS
            requires std::ranges::range<const V>
#endif
        {
            return sentinel<true>{};
        }

        template<bool Const>
        struct sentinel
        {};

        template<bool Const>
        struct iterator
            : detail::stl_interfaces::proxy_iterator_interface<
                  iterator<Const>,
                  std::forward_iterator_tag,
                  BOOST_PARSER_SUBRANGE<
                      detail::iterator_t<detail::maybe_const<Const, V>>>>
        {
            using I = detail::iterator_t<detail::maybe_const<Const, V>>;
            using S = detail::sentinel_t<detail::maybe_const<Const, V>>;

            constexpr iterator() = default;
            constexpr iterator(
                detail::maybe_const<Const, search_all_view> * parent) :
                parent_(parent),
                r_(parent_->base_.begin(), parent_->base_.end()),
                curr_(r_.begin(), r_.begin()),
                next_it_(r_.begin())
            {
                ++*this;
            }

            constexpr iterator & operator++()
            {
                r_ = BOOST_PARSER_SUBRANGE<I, S>(next_it_, r_.end());
                curr_ = parser::search(
                    r_, parent_->parser_, parent_->skip_, parent_->trace_mode_);
                next_it_ = curr_.end();
                if (curr_.begin() == curr_.end())
                    r_ = BOOST_PARSER_SUBRANGE<I, S>(next_it_, r_.end());
                return *this;
            }

            constexpr BOOST_PARSER_SUBRANGE<I> operator*() const
            {
                return curr_;
            }

            friend constexpr bool operator==(iterator lhs, iterator rhs)
            {
                return lhs.r_.begin() == rhs.r_.begin();
            }
            friend constexpr bool operator==(iterator it, sentinel<Const>)
            {
                return it.r_.begin() == it.r_.end();
            }

            using base_type = detail::stl_interfaces::proxy_iterator_interface<
                iterator,
                std::forward_iterator_tag,
                BOOST_PARSER_SUBRANGE<I>>;
            using base_type::operator++;

        private:
            detail::maybe_const<Const, search_all_view> * parent_;
            BOOST_PARSER_SUBRANGE<I, S> r_;
            BOOST_PARSER_SUBRANGE<I> curr_;
            I next_it_;
        };

        template<bool Const>
        friend struct iterator;

    private:
        V base_;
        parser_interface<Parser, GlobalState, ErrorHandler> parser_;
        parser_interface<SkipParser> skip_;
        trace trace_mode_;
    };

    // deduction guides
    template<
        typename V,
        typename Parser,
        typename GlobalState,
        typename ErrorHandler,
        typename SkipParser>
    search_all_view(
        V &&,
        parser_interface<Parser, GlobalState, ErrorHandler>,
        parser_interface<SkipParser>,
        trace)
        -> search_all_view<
            detail::text::detail::all_t<V>,
            Parser,
            GlobalState,
            ErrorHandler,
            SkipParser>;

    template<
        typename V,
        typename Parser,
        typename GlobalState,
        typename ErrorHandler,
        typename SkipParser>
    search_all_view(
        V &&,
        parser_interface<Parser, GlobalState, ErrorHandler>,
        parser_interface<SkipParser>)
        -> search_all_view<
            detail::text::detail::all_t<V>,
            Parser,
            GlobalState,
            ErrorHandler,
            SkipParser>;

    template<
        typename V,
        typename Parser,
        typename GlobalState,
        typename ErrorHandler>
    search_all_view(
        V &&, parser_interface<Parser, GlobalState, ErrorHandler>, trace)
        -> search_all_view<
            detail::text::detail::all_t<V>,
            Parser,
            GlobalState,
            ErrorHandler,
            parser_interface<eps_parser<detail::phony>>>;

    template<
        typename V,
        typename Parser,
        typename GlobalState,
        typename ErrorHandler>
    search_all_view(V &&, parser_interface<Parser, GlobalState, ErrorHandler>)
        -> search_all_view<
            detail::text::detail::all_t<V>,
            Parser,
            GlobalState,
            ErrorHandler,
            parser_interface<eps_parser<detail::phony>>>;

    namespace detail {
        template<
            typename V,
            typename Parser,
            typename GlobalState,
            typename ErrorHandler,
            typename SkipParser>
        using search_all_view_expr = decltype(search_all_view<
                                              V,
                                              Parser,
                                              GlobalState,
                                              ErrorHandler,
                                              SkipParser>(
            std::declval<V>(),
            std::declval<
                parser_interface<Parser, GlobalState, ErrorHandler> const &>(),
            std::declval<parser_interface<SkipParser> const &>(),
            trace::on));

        template<
            typename V,
            typename Parser,
            typename GlobalState,
            typename ErrorHandler,
            typename SkipParser>
        constexpr bool can_search_all_view = is_detected_v<
            search_all_view_expr,
            V,
            Parser,
            GlobalState,
            ErrorHandler,
            SkipParser>;

        struct search_all_impl
        {
#if BOOST_PARSER_USE_CONCEPTS

            template<
                parsable_range R,
                typename Parser,
                typename GlobalState,
                typename ErrorHandler,
                typename SkipParser>
                requires(std::ranges::viewable_range<R>) && can_search_all_view<
                                                                to_range_t<R>,
                                                                Parser,
                                                                GlobalState,
                                                                ErrorHandler,
                                                                SkipParser>
            [[nodiscard]] constexpr auto operator()(
                R && r,
                parser_interface<Parser, GlobalState, ErrorHandler> const &
                    parser,
                parser_interface<SkipParser> const & skip,
                trace trace_mode = trace::off) const
            {
                return search_all_view(
                    to_range<R>::call((R &&)r), parser, skip, trace_mode);
            }

            template<
                parsable_range R,
                typename Parser,
                typename GlobalState,
                typename ErrorHandler>
                requires(std::ranges::viewable_range<R>) &&
                        can_search_all_view<
                            to_range_t<R>,
                            Parser,
                            GlobalState,
                            ErrorHandler,
                            parser_interface<eps_parser<detail::phony>>>
            [[nodiscard]] constexpr auto operator()(
                R && r,
                parser_interface<Parser, GlobalState, ErrorHandler> const &
                    parser,
                trace trace_mode = trace::off) const
            {
                return (*this)(
                    (R &&)r,
                    parser,
                    parser_interface<eps_parser<detail::phony>>{},
                    trace_mode);
            }

#else

            template<
                typename R,
                typename Parser,
                typename GlobalState,
                typename ErrorHandler,
                typename SkipParser =
                    parser_interface<eps_parser<detail::phony>>,
                typename Trace = trace,
                typename Enable = std::enable_if_t<is_parsable_range_v<R>>>
            [[nodiscard]] constexpr auto operator()(
                R && r,
                parser_interface<Parser, GlobalState, ErrorHandler> const &
                    parser,
                SkipParser const & skip = SkipParser{},
                Trace trace_mode = Trace{}) const
            {
                if constexpr (
                    std::
                        is_same_v<detail::remove_cv_ref_t<SkipParser>, trace> &&
                    std::is_same_v<Trace, trace>) {
                    // (r, parser, trace) case
                    return impl(
                        (R &&) r,
                        parser,
                        parser_interface<eps_parser<detail::phony>>{},
                        skip);
                } else if constexpr (
                    detail::is_parser_iface<SkipParser> &&
                    std::is_same_v<Trace, trace>) {
                    // (r, parser, skip, trace) case
                    return impl((R &&) r, parser, skip, trace_mode);
                } else {
                    static_assert(
                        sizeof(R) == 1 && false,
                        "Only the signatures search_all(R, parser, skip, trace "
                        "= trace::off) and search_all(R, parser, trace = "
                        "trace::off) are supported.");
                }
            }

        private:
            template<
                typename R,
                typename Parser,
                typename GlobalState,
                typename ErrorHandler,
                typename SkipParser>
            [[nodiscard]] constexpr auto impl(
                R && r,
                parser_interface<Parser, GlobalState, ErrorHandler> const &
                    parser,
                parser_interface<SkipParser> const & skip,
                trace trace_mode = trace::off) const
            {
                return search_all_view(
                    to_range<R>::call((R &&) r), parser, skip, trace_mode);
            }

#endif
        };
    }

    /** A range adaptor object ([range.adaptor.object]).  Given subexpressions
        `E` and `P`, `Q`, and `R`, each of the expressions `search_all(E, P)`,
        `search_all(E, P, Q)`, and `search_all(E, P, Q, R)` are
        expression-equivalent to `search_all_view(E, P)`, `search_all_view(E,
        P, Q)`, and `search_all_view(E, P, Q, R)`, respectively. */
    inline constexpr detail::stl_interfaces::adaptor<detail::search_all_impl>
        search_all = detail::search_all_impl{};

}

#if BOOST_PARSER_USE_CONCEPTS
template<
    typename V,
    typename Parser,
    typename GlobalState,
    typename ErrorHandler,
    typename SkipParser>
constexpr bool std::ranges::enable_borrowed_range<
    boost::parser::
        search_all_view<V, Parser, GlobalState, ErrorHandler, SkipParser>> =
    std::ranges::enable_borrowed_range<V>;
#endif

#endif
