#ifndef BOOST_PARSER_TRANSFORM_REPLACE_HPP
#define BOOST_PARSER_TRANSFORM_REPLACE_HPP

#include <boost/parser/replace.hpp>

#if (!defined(_MSC_VER) || BOOST_PARSER_USE_CONCEPTS) &&                       \
    (!defined(BOOST_PARSER_GCC) || 12 <= __GNUC__ ||                           \
     !BOOST_PARSER_USE_CONCEPTS)


namespace boost::parser {

    namespace detail {

        template<typename F>
        constexpr bool tidy_func = std::is_trivially_copyable_v<F> &&
                                   sizeof(F) <= sizeof(void *) * 2;

        template<typename I, typename S, typename Parser>
        using attr_type = decltype(std::declval<Parser const &>().call(
            std::declval<I &>(),
            std::declval<S>(),
            std::declval<
                parse_context<false, false, I, S, default_error_handler>>(),
            ws,
            detail::default_flags(),
            std::declval<bool &>()));
        template<typename R, typename Parser>
        using range_attr_t = attr_type<iterator_t<R>, sentinel_t<R>, Parser>;

#if BOOST_PARSER_USE_CONCEPTS
        template<typename F, typename V, typename Parser>
        concept transform_replacement_for =
            std::regular_invocable<F &, range_attr_t<V, Parser>> &&
            detail::replacement_for<
                std::invoke_result_t<F &, range_attr_t<V, Parser>>,
                V> &&
            (detail::range_utf_format_v<V> ==
             detail::range_utf_format_v<
                 std::invoke_result_t<F &, range_attr_t<V, Parser>>>);
#else
        template<typename F, typename V, typename Parser>
        using transform_replacement_for_expr = decltype(std::declval<F &>()(
            std::declval<range_attr_t<V, Parser>>()));
        template<
            typename F,
            typename V,
            typename Parser,
            bool = is_detected_v<transform_replacement_for_expr, F, V, Parser>>
        constexpr bool transform_replacement_for = false;
        template<typename F, typename V, typename Parser>
        constexpr bool transform_replacement_for<F, V, Parser, true> =
            replacement_for<transform_replacement_for_expr<F, V, Parser>, V> &&
            (detail::range_utf_format_v<V> ==
             detail::range_utf_format_v<
                 transform_replacement_for_expr<F, V, Parser>>);
#endif

        template<
            typename R,
            typename Result,
            text::format OtherFormat = range_utf_format_v<remove_cv_ref_t<R>>,
            text::format Format = range_utf_format_v<remove_cv_ref_t<Result>>>
        struct utf_wrap
        {
            template<typename R_ = R>
            static auto call(R_ && r)
            {
                return (R_ &&) r | as_utf<OtherFormat>;
            }
        };
        template<typename R, typename Result, text::format Format>
        struct utf_wrap<R, Result, Format, Format>
        {
            template<typename R_ = R>
            static R_ && call(R_ && r)
            {
                return (R_ &&) r;
            }
        };
        template<typename R, typename Result>
        struct utf_wrap<R, Result, no_format, no_format>
        {
            template<typename R_ = R>
            static R_ && call(R_ && r)
            {
                return (R_ &&) r;
            }
        };
        template<typename R, typename Result, text::format Format>
        struct utf_wrap<R, Result, no_format, Format>
        {
            // Looks like you tried to use transform_replace() to replace
            // subranges of chars with subranges of some UTF-N (for N=8, 16,
            // or 32).  Transcoding from char (unkown encoding) is not
            // supported.  Check the return type of your transform function.
        };
        template<typename R, typename Result, text::format Format>
        struct utf_wrap<R, Result, Format, no_format>
        {
            // Looks like you tried to use transform_replace() to replace
            // subranges of some UTF-N (for N=8, 16, or 32) with subranges of
            // chars.  Transcoding to char (unkown encoding) is not supported.
            // Check the return type of your transform function.
        };

        template<typename T>
        struct regular_ref_wrapper
        {
            regular_ref_wrapper() = default;
            regular_ref_wrapper(T & ref) : ptr_(&ref) {}

            T & get() const { return *ptr_; }

            T * ptr_;
        };

        // This type catches results of calling F, to accommodate when F
        // returns an rvalue or a type that needs to be transcoded to a
        // different UTF.
        template<typename R, typename F, typename Attr>
        struct utf_rvalue_shim
        {
            using result_type = std::invoke_result_t<F &, Attr>;
            using maybe_wrapped_result_type =
                decltype(utf_wrap<R, result_type>::call(
                    std::declval<result_type>()));
            static constexpr bool final_type_is_reference =
                std::is_lvalue_reference_v<maybe_wrapped_result_type>;
            using final_type = std::conditional_t<
                final_type_is_reference,
                regular_ref_wrapper<
                    std::remove_reference_t<maybe_wrapped_result_type>>,
                remove_cv_ref_t<maybe_wrapped_result_type>>;

            template<typename F_ = F>
            utf_rvalue_shim(F_ && f) : f_((F_ &&) f)
            {}

            // These two only have return values for testing and metaprogramming
            // purposes.
            template<
                bool B = final_type_is_reference,
                typename Enable = std::enable_if_t<B>>
            decltype(auto) operator()(Attr && attr) const
            {
                result_ = final_type(
                    utf_wrap<R, result_type>::call((*f_)((Attr &&) attr)));
                return result_->get();
            }
            template<
                bool B = final_type_is_reference,
                typename Enable = std::enable_if_t<B>>
            decltype(auto) operator()(Attr && attr)
            {
                result_ = final_type(
                    utf_wrap<R, result_type>::call((*f_)((Attr &&) attr)));
                return result_->get();
            }
            template<
                bool B = final_type_is_reference,
                typename Enable = std::enable_if_t<!B>>
            final_type & operator()(Attr && attr) const
            {
                result_ = utf_wrap<R, result_type>::call((*f_)((Attr &&) attr));
                return *result_;
            }
            template<
                bool B = final_type_is_reference,
                typename Enable = std::enable_if_t<!B>>
            final_type & operator()(Attr && attr)
            {
                result_ = utf_wrap<R, result_type>::call((*f_)((Attr &&) attr));
                return *result_;
            }

            template<
                bool B = final_type_is_reference,
                typename Enable = std::enable_if_t<B>>
            decltype(auto) get() const
            {
                return result_->get();
            }
            template<
                bool B = final_type_is_reference,
                typename Enable = std::enable_if_t<B>>
            decltype(auto) get()
            {
                return result_->get();
            }
            template<
                bool B = final_type_is_reference,
                typename Enable = std::enable_if_t<!B>>
            final_type & get() const
            {
                return *result_;
            }
            template<
                bool B = final_type_is_reference,
                typename Enable = std::enable_if_t<!B>>
            final_type & get()
            {
                return *result_;
            }

            std::optional<F> f_;
            mutable std::optional<final_type> result_;
        };

        template<
            typename R,
            typename Parser,
            typename GlobalState,
            typename ErrorHandler,
            typename SkipParser>
        auto attr_search_impl(
            R && r,
            parser_interface<Parser, GlobalState, ErrorHandler> const & parser,
            parser_interface<SkipParser> const & skip,
            trace trace_mode)
        {
            auto first = text::detail::begin(r);
            auto const last = text::detail::end(r);

            auto match_first = first;
            auto match_last = first;
            auto before = [&match_first](auto & ctx) {
                match_first = _where(ctx).begin();
            };
            auto after = [&match_last](auto & ctx) {
                match_last = _where(ctx).begin();
            };

            auto const search_parser =
                omit[*(char_ - parser)] >>
                -lexeme[eps[before] >> parser::skip[parser] >> eps[after]];

            using parse_result_outer = decltype(parser::prefix_parse(
                first, last, search_parser, trace_mode));

            static_assert(
                !std::is_same_v<parse_result_outer, bool>,
                "If you're seeing this error, you passed a parser to "
                "transform_replace() that has no attribute.  Please fix.");

            using parse_result =
                remove_cv_ref_t<decltype(**std::declval<parse_result_outer>())>;

            using return_tuple = tuple<
                decltype(BOOST_PARSER_SUBRANGE(first, first)),
                parse_result>;

            if (first == last) {
                return return_tuple(
                    BOOST_PARSER_SUBRANGE(first, first), parse_result{});
            }

            if constexpr (std::is_same_v<SkipParser, eps_parser<phony>>) {
                auto result = parser::prefix_parse(
                    first, last, search_parser, trace_mode);
                if (*result) {
                    return return_tuple(
                        BOOST_PARSER_SUBRANGE(match_first, match_last),
                        std::move(**result));
                }
            } else {
                auto result = parser::prefix_parse(
                    first, last, search_parser, skip, trace_mode);
                if (*result) {
                    return return_tuple(
                        BOOST_PARSER_SUBRANGE(match_first, match_last),
                        std::move(**result));
                }
            }

            return return_tuple(
                BOOST_PARSER_SUBRANGE(first, first), parse_result{});
        }

        template<
            typename R,
            typename Parser,
            typename GlobalState,
            typename ErrorHandler,
            typename SkipParser>
        auto attr_search_repack_shim(
            R && r,
            parser_interface<Parser, GlobalState, ErrorHandler> const & parser,
            parser_interface<SkipParser> const & skip,
            trace trace_mode)
        {
            using value_type = range_value_t<decltype(r)>;
            if constexpr (std::is_same_v<value_type, char>) {
                return detail::attr_search_impl(
                    (R &&) r, parser, skip, trace_mode);
            } else {
                auto r_unpacked = detail::text::unpack_iterator_and_sentinel(
                    text::detail::begin(r), text::detail::end(r));
                auto result = detail::attr_search_impl(
                    r | as_utf32, parser, skip, trace_mode);
                auto subrng = parser::get(result, llong<0>{});
                auto & attr = parser::get(result, llong<1>{});
                return tuple<
                    decltype(BOOST_PARSER_SUBRANGE(
                        r_unpacked.repack(subrng.begin().base()),
                        r_unpacked.repack(subrng.end().base()))),
                    remove_cv_ref_t<decltype(attr)>>(
                    BOOST_PARSER_SUBRANGE(
                        r_unpacked.repack(subrng.begin().base()),
                        r_unpacked.repack(subrng.end().base())),
                    std::move(attr));
            }
        }
    }

    /** Produces a range of subranges of a given range `base`.  Each subrange
        is either a subrange of `base` that does not match the given parser
        `parser`, or is `f(*boost::parser::parse(match, parser))`, where `f`
        is the given invocable and `match` is the matching subrange.

        In addition to the template parameter constraints, `F` must be
        invocable with the attribute type of `Parser`; `V` and the range type
        produced by `F`, "`Rf`" must be ranges of `char`, or must have the
        same UTF format; and `V` and `Rf` must meet the same compatibility
        requirements as described in `std::ranges::join_view`. */
    template<
#if BOOST_PARSER_USE_CONCEPTS
        std::ranges::viewable_range V,
        std::move_constructible F,
#else
        typename V,
        typename F,
#endif
        typename Parser,
        typename GlobalState,
        typename ErrorHandler,
        typename SkipParser
#if !BOOST_PARSER_USE_CONCEPTS
        ,
        typename Enable =
            std::enable_if_t<detail::transform_replacement_for<F, V, Parser>>
#endif
        >
#if BOOST_PARSER_USE_CONCEPTS
    requires detail::transform_replacement_for<F, V, Parser>
#endif
    struct transform_replace_view
        : detail::stl_interfaces::view_interface<transform_replace_view<
              V,
              F,
              Parser,
              GlobalState,
              ErrorHandler,
              SkipParser>>
    {
    private:
        using attr_t = detail::range_attr_t<V, Parser>;
        using replacement_range = std::invoke_result_t<F &, attr_t>;

    public:
        constexpr transform_replace_view() = default;
        constexpr transform_replace_view(
            V base,
            parser_interface<Parser, GlobalState, ErrorHandler> const & parser,
            parser_interface<SkipParser> const & skip,
            F f,
            trace trace_mode = trace::off) :
            base_(std::move(base)),
            f_(std::move(f)),
            parser_(parser),
            skip_(skip),
            trace_mode_(trace_mode)
        {}
        constexpr transform_replace_view(
            V base,
            parser_interface<Parser, GlobalState, ErrorHandler> const & parser,
            F f,
            trace trace_mode = trace::off) :
            base_(std::move(base)),
            f_(std::move(f)),
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

        constexpr F const & f() const { return *f_.f_; }

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
                  BOOST_PARSER_SUBRANGE<detail::either_iterator<
                      detail::maybe_const<Const, V>,
                      detail::maybe_const<Const, replacement_range>>>>
        {
            using I = detail::iterator_t<detail::maybe_const<Const, V>>;
            using S = detail::sentinel_t<detail::maybe_const<Const, V>>;

            using ref_t_iter = detail::either_iterator<
                detail::maybe_const<Const, V>,
                detail::maybe_const<Const, replacement_range>>;
            using reference_type = BOOST_PARSER_SUBRANGE<ref_t_iter>;

            constexpr iterator() = default;
            constexpr iterator(
                detail::maybe_const<Const, transform_replace_view> * parent) :
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
                    auto new_match_and_attr = detail::attr_search_repack_shim(
                        r_,
                        parent_->parser_,
                        parent_->skip_,
                        parent_->trace_mode_);
                    auto const new_match =
                        parser::get(new_match_and_attr, llong<0>{});
                    parent_->f_(
                        parser::get(std::move(new_match_and_attr), llong<1>{}));
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
                        ref_t_iter(parent_->f_.get().begin()),
                        ref_t_iter(parent_->f_.get().end()));
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
            detail::maybe_const<Const, transform_replace_view> * parent_ = {};
            BOOST_PARSER_SUBRANGE<I, S> r_;
            BOOST_PARSER_SUBRANGE<I> curr_;
            I next_it_ = {};
            bool in_match_ = {};
        };

        template<bool Const>
        friend struct iterator;

    private:
        V base_;
        F f_;
        parser_interface<Parser, GlobalState, ErrorHandler> parser_;
        parser_interface<SkipParser> skip_;
        trace trace_mode_;
    };

    // deduction guides
    template<
        typename V,
        typename F,
        typename Parser,
        typename GlobalState,
        typename ErrorHandler,
        typename SkipParser>
    transform_replace_view(
        V &&,
        parser_interface<Parser, GlobalState, ErrorHandler>,
        parser_interface<SkipParser>,
        F &&,
        trace)
        -> transform_replace_view<
            detail::text::detail::all_t<V>,
            detail::remove_cv_ref_t<F>,
            Parser,
            GlobalState,
            ErrorHandler,
            SkipParser>;

    template<
        typename V,
        typename F,
        typename Parser,
        typename GlobalState,
        typename ErrorHandler,
        typename SkipParser>
    transform_replace_view(
        V &&,
        parser_interface<Parser, GlobalState, ErrorHandler>,
        parser_interface<SkipParser>,
        F &&)
        -> transform_replace_view<
            detail::text::detail::all_t<V>,
            detail::remove_cv_ref_t<F>,
            Parser,
            GlobalState,
            ErrorHandler,
            SkipParser>;

    template<
        typename V,
        typename F,
        typename Parser,
        typename GlobalState,
        typename ErrorHandler>
    transform_replace_view(
        V &&, parser_interface<Parser, GlobalState, ErrorHandler>, F &&, trace)
        -> transform_replace_view<
            detail::text::detail::all_t<V>,
            detail::remove_cv_ref_t<F>,
            Parser,
            GlobalState,
            ErrorHandler,
            parser_interface<eps_parser<detail::phony>>>;

    template<
        typename V,
        typename F,
        typename Parser,
        typename GlobalState,
        typename ErrorHandler>
    transform_replace_view(
        V &&, parser_interface<Parser, GlobalState, ErrorHandler>, F &&)
        -> transform_replace_view<
            detail::text::detail::all_t<V>,
            detail::remove_cv_ref_t<F>,
            Parser,
            GlobalState,
            ErrorHandler,
            parser_interface<eps_parser<detail::phony>>>;

    namespace detail {
        template<
            typename V,
            typename F,
            typename Parser,
            typename GlobalState,
            typename ErrorHandler,
            typename SkipParser>
        using transform_replace_view_expr = decltype(transform_replace_view<
                                                     V,
                                                     F,
                                                     Parser,
                                                     GlobalState,
                                                     ErrorHandler,
                                                     SkipParser>(
            std::declval<V>(),
            std::declval<
                parser_interface<Parser, GlobalState, ErrorHandler> const &>(),
            std::declval<parser_interface<SkipParser> const &>(),
            std::declval<F>(),
            trace::on));

        template<
            typename V,
            typename F,
            typename Parser,
            typename GlobalState,
            typename ErrorHandler,
            typename SkipParser>
        constexpr bool can_transform_replace_view = is_detected_v<
            transform_replace_view_expr,
            V,
            F,
            Parser,
            GlobalState,
            ErrorHandler,
            SkipParser>;

        struct transform_replace_impl
        {
#if BOOST_PARSER_USE_CONCEPTS

            template<
                parsable_range R,
                std::move_constructible F,
                typename Parser,
                typename GlobalState,
                typename ErrorHandler,
                typename SkipParser>
                requires std::ranges::viewable_range<R> &&
                         std::regular_invocable<
                             F &,
                             range_attr_t<to_range_t<R>, Parser>> &&
                         can_transform_replace_view<
                             to_range_t<R>,
                             utf_rvalue_shim<
                                 to_range_t<R>,
                                 std::remove_cvref_t<F>,
                                 range_attr_t<to_range_t<R>, Parser>>,
                             Parser,
                             GlobalState,
                             ErrorHandler,
                             SkipParser>
            [[nodiscard]] constexpr auto operator()(
                R && r,
                parser_interface<Parser, GlobalState, ErrorHandler> const &
                    parser,
                parser_interface<SkipParser> const & skip,
                F && f,
                trace trace_mode = trace::off) const
            {
                return transform_replace_view(
                    to_range<R>::call((R &&)r),
                    parser,
                    skip,
                    utf_rvalue_shim<
                        to_range_t<R>,
                        std::remove_cvref_t<F>,
                        range_attr_t<to_range_t<R>, Parser>>((F &&)f),
                    trace_mode);
            }

            template<
                parsable_range R,
                std::move_constructible F,
                typename Parser,
                typename GlobalState,
                typename ErrorHandler>
                requires std::ranges::viewable_range<R> &&
                         std::regular_invocable<
                             F &,
                             range_attr_t<to_range_t<R>, Parser>> &&
                         can_transform_replace_view<
                             to_range_t<R>,
                             utf_rvalue_shim<
                                 to_range_t<R>,
                                 std::remove_cvref_t<F>,
                                 range_attr_t<to_range_t<R>, Parser>>,
                             Parser,
                             GlobalState,
                             ErrorHandler,
                             parser_interface<eps_parser<detail::phony>>>
            [[nodiscard]] constexpr auto operator()(
                R && r,
                parser_interface<Parser, GlobalState, ErrorHandler> const &
                    parser,
                F && f,
                trace trace_mode = trace::off) const
            {
                return (*this)(
                    (R &&)r,
                    parser,
                    parser_interface<eps_parser<detail::phony>>{},
                    (F &&)f,
                    trace_mode);
            }

#else

            template<
                typename R,
                typename Parser,
                typename GlobalState,
                typename ErrorHandler,
                typename SkipParser,
                typename F = trace,
                typename Trace = trace,
                typename Enable = std::enable_if_t<is_parsable_range_v<R>>>
            [[nodiscard]] constexpr auto operator()(
                R && r,
                parser_interface<Parser, GlobalState, ErrorHandler> const &
                    parser,
                SkipParser && skip,
                F && f = F{},
                Trace trace_mode = Trace{}) const
            {
                if constexpr (
                    is_parser_iface<remove_cv_ref_t<SkipParser>> &&
                    std::is_invocable_v<
                        F &,
                        range_attr_t<to_range_t<R>, Parser>> &&
                    std::is_same_v<Trace, trace>) {
                    // (r, parser, skip, f, trace) case
                    return impl(
                        to_range<R>::call((R &&) r),
                        parser,
                        skip,
                        (F &&) f,
                        trace_mode);
                } else if constexpr (
                    std::is_invocable_v<
                        SkipParser &,
                        range_attr_t<to_range_t<R>, Parser>> &&
                    std::is_same_v<remove_cv_ref_t<F>, trace> &&
                    std::is_same_v<Trace, trace>) {
                    // (r, parser, f, trace) case
                    return impl(
                        to_range<R>::call((R &&) r),
                        parser,
                        parser_interface<eps_parser<detail::phony>>{},
                        (SkipParser &&) skip,
                        f);
                } else {
                    static_assert(
                        sizeof(R) == 1 && false,
                        "Only the signatures replace(R, parser, skip, "
                        "replcement trace = trace::off) and replace(R, parser, "
                        "f, trace = trace::off) are supported.");
                }
            }

        private:
            template<
                typename R,
                typename F,
                typename Parser,
                typename GlobalState,
                typename ErrorHandler,
                typename SkipParser>
            [[nodiscard]] constexpr auto impl(
                R && r,
                parser_interface<Parser, GlobalState, ErrorHandler> const &
                    parser,
                parser_interface<SkipParser> const & skip,
                F && f,
                trace trace_mode = trace::off) const
            {
                return transform_replace_view(
                    (R &&) r,
                    parser,
                    skip,
                    utf_rvalue_shim<
                        R,
                        remove_cv_ref_t<F>,
                        range_attr_t<R, Parser>>((F &&) f),
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
    inline constexpr detail::stl_interfaces::adaptor<
        detail::transform_replace_impl>
        transform_replace = detail::transform_replace_impl{};

}

#if BOOST_PARSER_USE_CONCEPTS
template<
    typename V,
    typename F,
    typename Parser,
    typename GlobalState,
    typename ErrorHandler,
    typename SkipParser>
constexpr bool
    std::ranges::enable_borrowed_range<boost::parser::transform_replace_view<
        V,
        F,
        Parser,
        GlobalState,
        ErrorHandler,
        SkipParser>> = std::ranges::enable_borrowed_range<V> &&
                       (std::ranges::enable_borrowed_range<F> ||
                        boost::parser::detail::tidy_func<F>);
#endif

#endif

#endif
