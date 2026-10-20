#ifndef BOOST_PARSER_REPLACE_HPP
#define BOOST_PARSER_REPLACE_HPP

#include <boost/parser/search.hpp>

#if !defined(_MSC_VER) || BOOST_PARSER_USE_CONCEPTS


namespace boost::parser {

    namespace detail {
        template<typename T, bool = std::is_pointer_v<remove_cv_ref_t<T>>>
        constexpr auto range_value_type =
            wrapper<remove_cv_ref_t<range_value_t<T>>>{};
        template<typename T>
        constexpr auto range_value_type<T, true> = wrapper<
            remove_cv_ref_t<std::remove_pointer_t<remove_cv_ref_t<T>>>>{};

        template<typename T>
        constexpr text::format range_utf_format()
        {
#if !BOOST_PARSER_USE_CONCEPTS
            // Special case: the metafunctions above will not detect char8_t
            // in C++17 mode, since it does not exit yet!  So, we need to
            // detect utf8_view in particular, and know that its use implies
            // format::utf8.
            if constexpr (is_utf8_view<T>{}) {
                return format::utf8;
            } else {
#endif
                using value_t = typename decltype(range_value_type<T>)::type;
                if constexpr (std::is_same_v<value_t, char>) {
                    return no_format;
#if defined(__cpp_char8_t)
                } else if constexpr (std::is_same_v<value_t, char8_t>) {
                    return format::utf8;
#endif
                } else if constexpr (
                    std::is_same_v<value_t, char16_t>
#ifdef _MSC_VER
                    || std::is_same_v<T, wchar_t>
#endif
                ) {
                    return format::utf16;
                } else if constexpr (
                    std::is_same_v<value_t, char32_t>
#ifndef _MSC_VER
                    || std::is_same_v<T, wchar_t>
#endif
                ) {
                    return format::utf32;
                } else {
                    static_assert(
                        sizeof(T) && false,
                        "Looks like you're trying to pass a range to replace "
                        "or transform_replace that has a non-character type "
                        "for its value type.  This is not supported.");
                }
#if !BOOST_PARSER_USE_CONCEPTS
            }
#endif
        }

        template<typename T>
        constexpr text::format
            range_utf_format_v = detail::range_utf_format<remove_cv_ref_t<T>>();

        template<typename V1, typename V2>
        using concat_reference_t =
            std::common_type_t<range_reference_t<V1>, range_reference_t<V2>>;
        template<typename V1, typename V2>
        using concat_value_t =
            std::common_type_t<range_value_t<V1>, range_value_t<V2>>;
        template<typename V1, typename V2>
        using concat_rvalue_reference_t = std::common_type_t<
            range_rvalue_reference_t<V1>,
            range_rvalue_reference_t<V2>>;

#if BOOST_PARSER_USE_CONCEPTS
        template<typename ReplacementV, typename V>
        concept concatable = requires {
            typename detail::concat_reference_t<ReplacementV, V>;
            typename detail::concat_value_t<ReplacementV, V>;
            typename detail::concat_rvalue_reference_t<ReplacementV, V>;
        };
#else
        template<typename ReplacementV, typename V>
        using concatable_expr =
            decltype(std::declval<concat_reference_t<ReplacementV, V>>(), std::declval<concat_value_t<ReplacementV, V>>(), std::declval<concat_rvalue_reference_t<ReplacementV, V>>());
        template<typename ReplacementV, typename V>
        constexpr bool concatable =
            is_detected_v<concatable_expr, ReplacementV, V>;
#endif

        template<
            typename V1,
            typename V2
#if !BOOST_PARSER_USE_CONCEPTS
            ,
            typename Enable = std::enable_if_t<concatable<V1, V2>>
#endif
            >
#if BOOST_PARSER_USE_CONCEPTS
            requires concatable<V1, V2>
#endif
        struct either_iterator_impl
            : detail::stl_interfaces::iterator_interface<
                  either_iterator_impl<V1, V2>,
                  std::forward_iterator_tag,
                  concat_value_t<V1, V2>,
                  concat_reference_t<V1, V2>>
        {
            constexpr either_iterator_impl() = default;
            constexpr either_iterator_impl(iterator_t<V1> it) : it_(it) {}
            template<typename V = V2>
            constexpr either_iterator_impl(iterator_t<V> it) : it_(it)
            {}

            constexpr concat_reference_t<V1, V2> operator*() const
            {
                if (it_.index() == 0) {
                    return *std::get<0>(it_);
                } else {
                    return *std::get<1>(it_);
                }
            }

            constexpr either_iterator_impl & operator++()
            {
                if (it_.index() == 0)
                    ++std::get<0>(it_);
                else
                    ++std::get<1>(it_);
                return *this;
            }

            friend constexpr bool
            operator==(either_iterator_impl lhs, either_iterator_impl rhs)
            {
                if (lhs.it_.index() != rhs.it_.index())
                    return false;
                if (lhs.it_.index() == 0)
                    return std::get<0>(lhs.it_) == std::get<0>(rhs.it_);
                else
                    return std::get<1>(lhs.it_) == std::get<1>(rhs.it_);
            }

            using base_type = detail::stl_interfaces::iterator_interface<
                either_iterator_impl<V1, V2>,
                std::forward_iterator_tag,
                concat_value_t<V1, V2>,
                concat_reference_t<V1, V2>>;
            using base_type::operator++;

        private:
            std::variant<iterator_t<V1>, iterator_t<V2>> it_;
        };

        template<typename V1, typename V2>
        using either_iterator = std::conditional_t<
            std::is_same_v<iterator_t<V1>, iterator_t<V2>>,
            iterator_t<V1>,
            either_iterator_impl<V1, V2>>;

#if BOOST_PARSER_USE_CONCEPTS
        template<typename ReplacementV, typename V>
        concept replacement_for = requires(ReplacementV replacement, V base) {
            { either_iterator<V, ReplacementV>(replacement.begin()) };
            { either_iterator<V, ReplacementV>(replacement.end()) };
            { either_iterator<V, ReplacementV>(base.begin()) };
        };
#else
        template<typename ReplacementV, typename V>
        using replacement_for_expr = decltype(
            either_iterator<V, ReplacementV>(
                std::declval<ReplacementV&>().begin()),
            either_iterator<V, ReplacementV>(
                std::declval<ReplacementV&>().end()),
            either_iterator<V, ReplacementV>(std::declval<V&>().begin()));
        template<typename ReplacementV, typename V>
        constexpr bool replacement_for =
            is_detected_v<replacement_for_expr, ReplacementV, V>;
#endif
    }

    /** Produces a range of subranges of a given range `base`.  Each subrange
        is either a subrange of `base` that does not match the given parser
        `parser`, or is the given replacement for a match, `replacement`.

        In addition to the template parameter constraints, `V` and
        `ReplacementV` must be ranges of `char`, or must have the same UTF
        format, and `V` and `ReplacementV` must meet the same compatibility
        requirements as described in `std::ranges::join_view`. */
    template<
#if BOOST_PARSER_USE_CONCEPTS
        std::ranges::viewable_range V,
        std::ranges::viewable_range ReplacementV,
#else
        typename V,
        typename ReplacementV,
#endif
        typename Parser,
        typename GlobalState,
        typename ErrorHandler,
        typename SkipParser
#if !BOOST_PARSER_USE_CONCEPTS
        ,
        typename Enable = std::enable_if_t<
            detail::replacement_for<ReplacementV, V> &&
            (detail::range_utf_format_v<V> ==
             detail::range_utf_format_v<ReplacementV>)>
#endif
        >
#if BOOST_PARSER_USE_CONCEPTS
    requires detail::replacement_for<ReplacementV, V> &&
        (detail::range_utf_format_v<V> ==
         detail::range_utf_format_v<ReplacementV>)
#endif
            struct replace_view
        : detail::stl_interfaces::view_interface<replace_view<
              V,
              ReplacementV,
              Parser,
              GlobalState,
              ErrorHandler,
              SkipParser>>
    {
        constexpr replace_view() = default;
        constexpr replace_view(
            V base,
            parser_interface<Parser, GlobalState, ErrorHandler> const & parser,
            parser_interface<SkipParser> const & skip,
            ReplacementV replacement,
            trace trace_mode = trace::off) :
            base_(std::move(base)),
            replacement_(std::move(replacement)),
            parser_(parser),
            skip_(skip),
            trace_mode_(trace_mode)
        {}
        constexpr replace_view(
            V base,
            parser_interface<Parser, GlobalState, ErrorHandler> const & parser,
            ReplacementV replacement,
            trace trace_mode = trace::off) :
            base_(std::move(base)),
            replacement_(std::move(replacement)),
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

        constexpr V replacement() const &
#if BOOST_PARSER_USE_CONCEPTS
            requires std::copy_constructible<V>
#endif
        {
            return replacement_;
        }
        constexpr V replacement() && { return std::move(replacement_); }

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
        struct iterator : detail::stl_interfaces::proxy_iterator_interface<
                              iterator<Const>,
                              std::forward_iterator_tag,
                              BOOST_PARSER_SUBRANGE<detail::either_iterator<
                                  detail::maybe_const<Const, V>,
                                  detail::maybe_const<Const, ReplacementV>>>>
        {
            using I = detail::iterator_t<detail::maybe_const<Const, V>>;
            using S = detail::sentinel_t<detail::maybe_const<Const, V>>;

            using ref_t_iter = detail::either_iterator<
                detail::maybe_const<Const, V>,
                detail::maybe_const<Const, ReplacementV>>;
            using reference_type = BOOST_PARSER_SUBRANGE<ref_t_iter>;

            constexpr iterator() = default;
            constexpr iterator(
                detail::maybe_const<Const, replace_view> * parent) :
                parent_(parent),
                r_(parent_->base_.begin(), parent_->base_.end()),
                curr_(r_.begin(), r_.begin()),
                next_it_(r_.begin()),
                in_match_(true)
            {
                ++*this;
            }

            constexpr iterator & operator++()
            {
                if (in_match_) {
                    r_ = BOOST_PARSER_SUBRANGE<I, S>(next_it_, r_.end());
                    auto const new_match = parser::search(
                        r_,
                        parent_->parser_,
                        parent_->skip_,
                        parent_->trace_mode_);
                    if (new_match.begin() == curr_.end()) {
                        curr_ = new_match;
                    } else {
                        curr_ =
                            BOOST_PARSER_SUBRANGE(next_it_, new_match.begin());
                        in_match_ = false;
                    }
                    next_it_ = new_match.end();
                } else {
                    if (!curr_.empty()) {
                        curr_ = BOOST_PARSER_SUBRANGE(curr_.end(), next_it_);
                        in_match_ = true;
                    }
                    if (curr_.empty())
                        r_ = BOOST_PARSER_SUBRANGE<I, S>(next_it_, r_.end());
                }
                return *this;
            }

            constexpr reference_type operator*() const
            {
                if (in_match_) {
                    return reference_type(
                        ref_t_iter(parent_->replacement_.begin()),
                        ref_t_iter(parent_->replacement_.end()));
                } else {
                    return reference_type(
                        ref_t_iter(curr_.begin()), ref_t_iter(curr_.end()));
                }
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
                reference_type>;
            using base_type::operator++;

        private:
            detail::maybe_const<Const, replace_view> * parent_;
            BOOST_PARSER_SUBRANGE<I, S> r_;
            BOOST_PARSER_SUBRANGE<I> curr_;
            I next_it_;
            bool in_match_;
        };

        template<bool Const>
        friend struct iterator;

    private:
        V base_;
        ReplacementV replacement_;
        parser_interface<Parser, GlobalState, ErrorHandler> parser_;
        parser_interface<SkipParser> skip_;
        trace trace_mode_;
    };

    // deduction guides
    template<
        typename V,
        typename ReplacementV,
        typename Parser,
        typename GlobalState,
        typename ErrorHandler,
        typename SkipParser>
    replace_view(
        V &&,
        parser_interface<Parser, GlobalState, ErrorHandler>,
        parser_interface<SkipParser>,
        ReplacementV &&,
        trace)
        -> replace_view<
            detail::text::detail::all_t<V>,
            detail::text::detail::all_t<ReplacementV>,
            Parser,
            GlobalState,
            ErrorHandler,
            SkipParser>;

    template<
        typename V,
        typename ReplacementV,
        typename Parser,
        typename GlobalState,
        typename ErrorHandler,
        typename SkipParser>
    replace_view(
        V &&,
        parser_interface<Parser, GlobalState, ErrorHandler>,
        parser_interface<SkipParser>,
        ReplacementV &&)
        -> replace_view<
            detail::text::detail::all_t<V>,
            detail::text::detail::all_t<ReplacementV>,
            Parser,
            GlobalState,
            ErrorHandler,
            SkipParser>;

    template<
        typename V,
        typename ReplacementV,
        typename Parser,
        typename GlobalState,
        typename ErrorHandler>
    replace_view(
        V &&,
        parser_interface<Parser, GlobalState, ErrorHandler>,
        ReplacementV &&,
        trace)
        -> replace_view<
            detail::text::detail::all_t<V>,
            detail::text::detail::all_t<ReplacementV>,
            Parser,
            GlobalState,
            ErrorHandler,
            parser_interface<eps_parser<detail::phony>>>;

    template<
        typename V,
        typename ReplacementV,
        typename Parser,
        typename GlobalState,
        typename ErrorHandler>
    replace_view(
        V &&,
        parser_interface<Parser, GlobalState, ErrorHandler>,
        ReplacementV &&)
        -> replace_view<
            detail::text::detail::all_t<V>,
            detail::text::detail::all_t<ReplacementV>,
            Parser,
            GlobalState,
            ErrorHandler,
            parser_interface<eps_parser<detail::phony>>>;

    namespace detail {
        template<
            typename V,
            typename ReplacementV,
            typename Parser,
            typename GlobalState,
            typename ErrorHandler,
            typename SkipParser>
        using replace_view_expr = decltype(replace_view<
                                           V,
                                           ReplacementV,
                                           Parser,
                                           GlobalState,
                                           ErrorHandler,
                                           SkipParser>(
            std::declval<V>(),
            std::declval<
                parser_interface<Parser, GlobalState, ErrorHandler> const &>(),
            std::declval<parser_interface<SkipParser> const &>(),
            std::declval<ReplacementV>(),
            trace::on));

        template<
            typename V,
            typename ReplacementV,
            typename Parser,
            typename GlobalState,
            typename ErrorHandler,
            typename SkipParser>
        constexpr bool can_replace_view = is_detected_v<
            replace_view_expr,
            V,
            ReplacementV,
            Parser,
            GlobalState,
            ErrorHandler,
            SkipParser>;

        struct replace_impl
        {
#if BOOST_PARSER_USE_CONCEPTS

            template<
                parsable_range R,
                std::ranges::range ReplacementR,
                typename Parser,
                typename GlobalState,
                typename ErrorHandler,
                typename SkipParser>
                requires std::ranges::viewable_range<R> &&
                         std::ranges::viewable_range<ReplacementR> &&
                         can_replace_view<
                             to_range_t<R>,
                             decltype(to_range<
                                      ReplacementR,
                                      true,
                                      detail::range_utf_format_v<R>>::
                                          call(std::declval<ReplacementR>())),
                             Parser,
                             GlobalState,
                             ErrorHandler,
                             SkipParser>
            [[nodiscard]] constexpr auto operator()(
                R && r,
                parser_interface<Parser, GlobalState, ErrorHandler> const &
                    parser,
                parser_interface<SkipParser> const & skip,
                ReplacementR && replacement,
                trace trace_mode = trace::off) const
            {
                return replace_view(
                    to_range<R>::call((R &&)r),
                    parser,
                    skip,
                    to_range<
                        ReplacementR,
                        true,
                        detail::range_utf_format_v<R>>::call((ReplacementR &&)
                                                                 replacement),
                    trace_mode);
            }

            template<
                parsable_range R,
                std::ranges::range ReplacementR,
                typename Parser,
                typename GlobalState,
                typename ErrorHandler>
                requires std::ranges::viewable_range<R> &&
                         std::ranges::viewable_range<ReplacementR> &&
                         can_replace_view<
                             to_range_t<R>,
                             decltype(to_range<
                                      ReplacementR,
                                      true,
                                      detail::range_utf_format_v<R>>::
                                          call(std::declval<ReplacementR>())),
                             Parser,
                             GlobalState,
                             ErrorHandler,
                             parser_interface<eps_parser<detail::phony>>>
            [[nodiscard]] constexpr auto operator()(
                R && r,
                parser_interface<Parser, GlobalState, ErrorHandler> const &
                    parser,
                ReplacementR && replacement,
                trace trace_mode = trace::off) const
            {
                return (*this)(
                    (R &&)r,
                    parser,
                    parser_interface<eps_parser<detail::phony>>{},
                    (ReplacementR &&)replacement,
                    trace_mode);
            }

#else

            template<
                typename R,
                typename Parser,
                typename GlobalState,
                typename ErrorHandler,
                typename SkipParser,
                typename ReplacementR = trace,
                typename Trace = trace,
                typename Enable = std::enable_if_t<is_parsable_range_v<R>>>
            [[nodiscard]] constexpr auto operator()(
                R && r,
                parser_interface<Parser, GlobalState, ErrorHandler> const &
                    parser,
                SkipParser && skip,
                ReplacementR && replacement = ReplacementR{},
                Trace trace_mode = Trace{}) const
            {
                if constexpr (
                    is_parser_iface<remove_cv_ref_t<SkipParser>> &&
                    is_range<remove_cv_ref_t<ReplacementR>> &&
                    std::is_same_v<Trace, trace>) {
                    // (r, parser, skip, replacement, trace) case
                    return impl(
                        (R &&) r,
                        parser,
                        skip,
                        (ReplacementR &&) replacement,
                        trace_mode);
                } else if constexpr (
                    is_range<remove_cv_ref_t<SkipParser>> &&
                    std::is_same_v<remove_cv_ref_t<ReplacementR>, trace> &&
                    std::is_same_v<Trace, trace>) {
                    // (r, parser, replacement, trace) case
                    return impl(
                        (R &&) r,
                        parser,
                        parser_interface<eps_parser<detail::phony>>{},
                        (SkipParser &&) skip,
                        replacement);
                } else {
                    static_assert(
                        sizeof(R) == 1 && false,
                        "Only the signatures replace(R, parser, skip, "
                        "replcement trace = trace::off) and replace(R, parser, "
                        "replacement, trace = trace::off) are supported.");
                }
            }

        private:
            template<
                typename R,
                typename ReplacementR,
                typename Parser,
                typename GlobalState,
                typename ErrorHandler,
                typename SkipParser>
            [[nodiscard]] constexpr auto impl(
                R && r,
                parser_interface<Parser, GlobalState, ErrorHandler> const &
                    parser,
                parser_interface<SkipParser> const & skip,
                ReplacementR && replacement,
                trace trace_mode = trace::off) const
            {
                return replace_view(
                    to_range<R>::call((R &&) r),
                    parser,
                    skip,
                    to_range<
                        ReplacementR,
                        true,
                        detail::range_utf_format_v<R>>::call((ReplacementR &&)
                                                                 replacement),
                    trace_mode);
            }

#endif
        };
    }

    /** A range adaptor object ([range.adaptor.object]).  Given subexpressions
        `E` and `P`, `Q`, `R`, and 'S', each of the expressions `replace(E,
        P)`, `replace(E, P, Q)`. `replace(E, P, Q, R)`, and `replace(E, P, Q,
        R, S)` are expression-equivalent to `replace_view(E, P)`,
        `replace_view(E, P, Q)`, `replace_view(E, P, Q, R)`, `replace_view(E,
        P, Q, R, S)`, respectively. */
    inline constexpr detail::stl_interfaces::adaptor<detail::replace_impl>
        replace = detail::replace_impl{};

}

#if BOOST_PARSER_USE_CONCEPTS
template<
    typename V,
    typename ReplacementV,
    typename Parser,
    typename GlobalState,
    typename ErrorHandler,
    typename SkipParser>
constexpr bool std::ranges::enable_borrowed_range<boost::parser::replace_view<
    V,
    ReplacementV,
    Parser,
    GlobalState,
    ErrorHandler,
    SkipParser>> = std::ranges::enable_borrowed_range<V> &&
    std::ranges::enable_borrowed_range<ReplacementV>;
#endif

#endif

#endif
