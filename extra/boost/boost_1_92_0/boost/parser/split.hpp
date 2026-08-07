#ifndef BOOST_PARSER_SPLIT_HPP
#define BOOST_PARSER_SPLIT_HPP

#include <boost/parser/search.hpp>


namespace boost::parser {

    /** Produces a sequence of subranges of the underlying sequence of type
        `V`.  the underlying sequence is split into subranges delimited by
        matches of the given parser, possibly using a given skip-parser. */
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
    struct split_view
        : detail::stl_interfaces::view_interface<
              split_view<V, Parser, GlobalState, ErrorHandler, SkipParser>>
    {
        constexpr split_view() = default;
        constexpr split_view(
            V base,
            parser_interface<Parser, GlobalState, ErrorHandler> const & parser,
            parser_interface<SkipParser> const & skip,
            trace trace_mode = trace::off) :
            base_(std::move(base)),
            parser_(parser),
            skip_(skip),
            trace_mode_(trace_mode)
        {}
        constexpr split_view(
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
                detail::maybe_const<Const, split_view> * parent) :
                parent_(parent),
                r_(parent_->base_.begin(), parent_->base_.end()),
                curr_(r_.begin(), r_.begin()),
                next_it_(r_.begin()),
                next_follows_match_(false)
            {
                ++*this;
            }

            constexpr iterator & operator++()
            {
                if (next_it_ == r_.end() && next_follows_match_) {
                    curr_ = BOOST_PARSER_SUBRANGE(next_it_, next_it_);
                    next_follows_match_ = false;
                    return *this;
                }
                r_ = BOOST_PARSER_SUBRANGE<I, S>(next_it_, r_.end());
                auto const curr_match = parser::search(
                    r_, parent_->parser_, parent_->skip_, parent_->trace_mode_);
                curr_ = BOOST_PARSER_SUBRANGE(next_it_, curr_match.begin());
                next_it_ = curr_match.end();
                next_follows_match_ = !curr_match.empty();
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
            detail::maybe_const<Const, split_view> * parent_;
            BOOST_PARSER_SUBRANGE<I, S> r_;
            BOOST_PARSER_SUBRANGE<I> curr_;
            I next_it_;
            bool next_follows_match_;
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
    split_view(
        V &&,
        parser_interface<Parser, GlobalState, ErrorHandler>,
        parser_interface<SkipParser>,
        trace)
        -> split_view<
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
    split_view(
        V &&,
        parser_interface<Parser, GlobalState, ErrorHandler>,
        parser_interface<SkipParser>)
        -> split_view<
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
    split_view(
        V &&, parser_interface<Parser, GlobalState, ErrorHandler>, trace)
        -> split_view<
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
    split_view(V &&, parser_interface<Parser, GlobalState, ErrorHandler>)
        -> split_view<
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
        using split_view_expr = decltype(split_view<
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
        constexpr bool can_split_view = is_detected_v<
            split_view_expr,
            V,
            Parser,
            GlobalState,
            ErrorHandler,
            SkipParser>;

        struct split_impl
        {
#if BOOST_PARSER_USE_CONCEPTS

            template<
                parsable_range R,
                typename Parser,
                typename GlobalState,
                typename ErrorHandler,
                typename SkipParser>
                requires(std::ranges::viewable_range<R>) && can_split_view<
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
                return split_view(
                    to_range<R>::call((R &&)r), parser, skip, trace_mode);
            }

            template<
                parsable_range R,
                typename Parser,
                typename GlobalState,
                typename ErrorHandler>
                requires(std::ranges::viewable_range<R>) &&
                        can_split_view<
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
                        "Only the signatures split(R, parser, skip, trace "
                        "= trace::off) and split(R, parser, trace = "
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
                return split_view(
                    to_range<R>::call((R &&) r), parser, skip, trace_mode);
            }

#endif
        };
    }

    /** A range adaptor object ([range.adaptor.object]).  Given subexpressions
        `E` and `P`, `Q`, and `R`, each of the expressions `split(E, P)`,
        `split(E, P, Q)`, and `split(E, P, Q, R)` are
        expression-equivalent to `split_view(E, P)`, `split_view(E,
        P, Q)`, and `split_view(E, P, Q, R)`, respectively. */
    inline constexpr detail::stl_interfaces::adaptor<detail::split_impl>
        split = detail::split_impl{};

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
        split_view<V, Parser, GlobalState, ErrorHandler, SkipParser>> =
    std::ranges::enable_borrowed_range<V>;
#endif

#endif
