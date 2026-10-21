#ifndef BOOST_PARSER_DETAIL_PRINTING_IMPL_HPP
#define BOOST_PARSER_DETAIL_PRINTING_IMPL_HPP

#include <boost/parser/detail/printing.hpp>

#if __has_include(<boost/type_index.hpp>)
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#include <boost/type_index.hpp>
#define BOOST_PARSER_HAVE_BOOST_TYPEINDEX 1
#define BOOST_PARSER_TYPE_NAME_NS boost_type_index
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
#else
#include <typeinfo>
#define BOOST_PARSER_HAVE_BOOST_TYPEINDEX 0
#define BOOST_PARSER_TYPE_NAME_NS std_typeinfo
#endif


namespace boost { namespace parser { namespace detail {

    inline namespace BOOST_PARSER_TYPE_NAME_NS {
        template<typename T>
        auto type_name()
        {
#if BOOST_PARSER_HAVE_BOOST_TYPEINDEX
            return typeindex::type_id<T>().pretty_name();
#else
            return typeid(T).name();
#endif
        }
    }

    template<typename Parser>
    struct n_aray_parser : std::false_type
    {};

    template<
        typename Parser,
        typename DelimiterParser,
        typename MinType,
        typename MaxType>
    struct n_aray_parser<
        repeat_parser<Parser, DelimiterParser, MinType, MaxType>>
        : std::true_type
    {};

    template<typename Parser, typename MinType, typename MaxType>
    struct n_aray_parser<repeat_parser<Parser, detail::nope, MinType, MaxType>>
        : std::false_type
    {};

    template<typename Parser, typename DelimiterParser>
    struct n_aray_parser<delimited_seq_parser<Parser, DelimiterParser>>
        : std::true_type
    {};

    template<typename ParserTuple>
    struct n_aray_parser<or_parser<ParserTuple>> : std::true_type
    {};

    template<typename ParserTuple, typename DelimiterParser>
    struct n_aray_parser<perm_parser<ParserTuple, DelimiterParser>>
        : std::true_type
    {};

    template<
        typename ParserTuple,
        typename BacktrackingTuple,
        typename CombiningGroups>
    struct n_aray_parser<
        seq_parser<ParserTuple, BacktrackingTuple, CombiningGroups>>
        : std::true_type
    {};

    // true iff Parser is an n-ary parser (contains N>2 subparsers).
    template<typename Parser>
    constexpr bool n_aray_parser_v = n_aray_parser<Parser>::value;

    template<typename Context, typename Expected>
    void print_expected(
        Context const & context,
        std::ostream & os,
        Expected expected,
        bool no_parens = false)
    {
        if (is_nope_v<Expected>)
            return;
        if (!no_parens)
            os << "(";
        detail::print(os, detail::resolve(context, expected));
        if (!no_parens)
            os << ")";
    }

    template<
        typename Context,
        typename Parser,
        typename DelimiterParser,
        typename MinType,
        typename MaxType>
    void print_parser(
        Context const & context,
        repeat_parser<Parser, DelimiterParser, MinType, MaxType> const & parser,
        std::ostream & os,
        int components)
    {
        if constexpr (is_nope_v<DelimiterParser>) {
            auto const min_ = detail::resolve(context, parser.min_);
            auto const max_ = detail::resolve(context, parser.max_);
            constexpr bool n_ary_child = n_aray_parser_v<Parser>;
            if (min_ == 0 && max_ == Inf) {
                os << "*";
                if (n_ary_child)
                    os << "(";
                detail::print_parser(
                    context, parser.parser_, os, components + 1);
                if (n_ary_child)
                    os << ")";
            } else if (min_ == 1 && max_ == Inf) {
                os << "+";
                if (n_ary_child)
                    os << "(";
                detail::print_parser(
                    context, parser.parser_, os, components + 1);
                if (n_ary_child)
                    os << ")";
            } else {
                os << "repeat(";
                detail::print(os, min_);
                if (min_ == max_) {
                    os << ")[";
                } else {
                    os << ", ";
                    if (max_ == unbounded)
                        os << "Inf";
                    else
                        detail::print(os, max_);
                    os << ")[";
                }
                detail::print_parser(
                    context, parser.parser_, os, components + 1);
                os << "]";
            }
        } else {
            detail::print_parser(context, parser.parser_, os, components + 1);
            os << " % ";
            detail::print_parser(
                context, parser.delimiter_parser_, os, components + 2);
        }
    }

    template<typename Context, typename Parser>
    void print_parser(
        Context const & context,
        opt_parser<Parser> const & parser,
        std::ostream & os,
        int components)
    {
        os << "-";
        constexpr bool n_ary_child = n_aray_parser_v<Parser>;
        if (n_ary_child)
            os << "(";
        detail::print_parser(context, parser.parser_, os, components + 1);
        if (n_ary_child)
            os << ")";
    }

    template<typename Context, typename Parser>
    void print_or_like_parser(
        Context const & context,
        Parser const & parser,
        std::ostream & os,
        int components,
        std::string_view or_ellipsis,
        std::string_view ws_or)
    {
        int i = 0;
        bool printed_ellipsis = false;
        hl::for_each(parser.parsers_, [&](auto const & parser) {
            if (components == parser_component_limit) {
                if (!printed_ellipsis)
                    os << or_ellipsis;
                printed_ellipsis = true;
                return;
            }
            if (i)
                os << ws_or;
            detail::print_parser(context, parser, os, components);
            ++components;
            ++i;
        });
    }

    template<typename Context, typename ParserTuple>
    void print_parser(
        Context const & context,
        or_parser<ParserTuple> const & parser,
        std::ostream & os,
        int components)
    {
        detail::print_or_like_parser(
            context, parser, os, components, " | ...", " | ");
    }

    template<typename Context, typename ParserTuple, typename DelimiterParser>
    void print_parser(
        Context const & context,
        perm_parser<ParserTuple, DelimiterParser> const & parser,
        std::ostream & os,
        int components)
    {
        if constexpr (!is_nope_v<DelimiterParser>) {
            os << "delimiter(";
            detail::print_parser(
                context, parser.delimiter_parser_, os, components);
            os << ")[";
        }
        detail::print_or_like_parser(
            context, parser, os, components, " || ...", " || ");
        if constexpr (!is_nope_v<DelimiterParser>)
            os << "]";
    }

    template<
        typename Context,
        typename ParserTuple,
        typename BacktrackingTuple,
        typename CombiningGroups>
    void print_parser(
        Context const & context,
        seq_parser<ParserTuple, BacktrackingTuple, CombiningGroups> const &
            parser,
        std::ostream & os,
        int components)
    {
        int prev_group = 0;
        int i = 0;
        bool printed_ellipsis = false;
        using combining_groups =
            detail::combining_t<ParserTuple, CombiningGroups>;
        hl::for_each(
            hl::zip(parser.parsers_, BacktrackingTuple{}, combining_groups{}),
            [&](auto const & parser_and_backtrack) {
                using namespace literals;
                auto const & parser = parser::get(parser_and_backtrack, 0_c);
                auto const backtrack = parser::get(parser_and_backtrack, 1_c);
                auto const group = parser::get(parser_and_backtrack, 2_c);

                if (components == parser_component_limit) {
                    if (!printed_ellipsis) {
                        os << (backtrack ? " >> ..." : " > ...");
                    }
                    printed_ellipsis = true;
                    return;
                }
                if (group != prev_group && prev_group)
                    os << ']';
                if (i)
                    os << (backtrack ? " >> " : " > ");
                if (group != prev_group && group)
                    os << (group == -1 ? "separate[" : "merge[");
                detail::print_parser(context, parser, os, components);
                ++components;
                ++i;
                prev_group = (int)group;
            });
        if (prev_group && !printed_ellipsis)
            os << ']';
    }

    template<typename Context, typename Parser, typename Action>
    void print_parser(
        Context const & context,
        action_parser<Parser, Action> const & parser,
        std::ostream & os,
        int components)
    {
        detail::print_parser(context, parser.parser_, os, components);
        os << "[<<action>>]";
    }

    template<typename Context, typename Parser>
    void print_directive(
        Context const & context,
        std::string_view name,
        Parser const & parser,
        std::ostream & os,
        int components)
    {
        os << name << "[";
        if (++components == parser_component_limit)
            os << "...";
        else
            detail::print_parser(context, parser, os, components + 1);
        os << "]";
    }

    template<typename Context, typename Parser, typename F>
    void print_parser(
        Context const & context,
        transform_parser<Parser, F> const & parser,
        std::ostream & os,
        int components)
    {
        detail::print_directive(
            context, "transform(<<f>>)", parser.parser_, os, components);
    }

    template<typename Context, typename Parser>
    void print_parser(
        Context const & context,
        omit_parser<Parser> const & parser,
        std::ostream & os,
        int components)
    {
        detail::print_directive(
            context, "omit", parser.parser_, os, components);
    }

    template<typename Context, typename Parser>
    void print_parser(
        Context const & context,
        raw_parser<Parser> const & parser,
        std::ostream & os,
        int components)
    {
        detail::print_directive(context, "raw", parser.parser_, os, components);
    }

#if defined(BOOST_PARSER_DOXYGEN) || BOOST_PARSER_USE_CONCEPTS
    template<typename Context, typename Parser>
    void print_parser(
        Context const & context,
        string_view_parser<Parser> const & parser,
        std::ostream & os,
        int components)
    {
        detail::print_directive(
            context, "string_view", parser.parser_, os, components);
    }
#endif

    template<typename Context, typename Parser>
    void print_parser(
        Context const & context,
        lexeme_parser<Parser> const & parser,
        std::ostream & os,
        int components)
    {
        detail::print_directive(
            context, "lexeme", parser.parser_, os, components);
    }

    template<typename Context, typename Parser>
    void print_parser(
        Context const & context,
        no_case_parser<Parser> const & parser,
        std::ostream & os,
        int components)
    {
        detail::print_directive(
            context, "no_case", parser.parser_, os, components);
    }

    template<typename Context, typename Parser, typename SkipParser>
    void print_parser(
        Context const & context,
        skip_parser<Parser, SkipParser> const & parser,
        std::ostream & os,
        int components)
    {
        if constexpr (is_nope_v<SkipParser>) {
            detail::print_directive(
                context, "skip", parser.parser_, os, components);
        } else {
            os << "skip(";
            detail::print_parser(
                context, parser.skip_parser_.parser_, os, components);
            os << ")";
            detail::print_directive(
                context, "", parser.parser_, os, components + 1);
        }
    }

    template<typename Context, typename Parser, bool FailOnMatch>
    void print_parser(
        Context const & context,
        expect_parser<Parser, FailOnMatch> const & parser,
        std::ostream & os,
        int components)
    {
        if (FailOnMatch)
            os << "!";
        else
            os << "&";
        constexpr bool n_ary_child = n_aray_parser_v<Parser>;
        if (n_ary_child)
            os << "(";
        detail::print_parser(context, parser.parser_, os, components + 1);
        if (n_ary_child)
            os << ")";
    }

    template<
        typename Context,
        bool UseCallbacks,
        typename Parser,
        typename Attribute,
        typename LocalState,
        typename ParamsTuple>
    void print_parser(
        Context const & context,
        rule_parser<
            UseCallbacks,
            Parser,
            Attribute,
            LocalState,
            ParamsTuple> const & parser,
        std::ostream & os,
        int components)
    {
        os << parser.diagnostic_text_;
        if constexpr (!is_nope_v<ParamsTuple>) {
            os << ".with(";
            int i = 0;
            hl::for_each(parser.params_, [&](auto const & param) {
                if (i++)
                    os << ", ";
                detail::print_expected(context, os, param, true);
            });
            os << ")";
        }
    }

    template<typename Context, typename T>
    void print_parser(
        Context const & context,
        symbol_parser<T> const & parser,
        std::ostream & os,
        int components)
    {
        if (parser.diagnostic_text_.empty())
            os << "symbols<" << detail::type_name<T>() << ">";
        else
            os << parser.diagnostic_text_;
    }

    template<typename Context, typename Predicate>
    void print_parser(
        Context const & context,
        eps_parser<Predicate> const & parser,
        std::ostream & os,
        int components)
    {
        os << "eps(<<pred>>)";
    }

    template<typename Context>
    void print_parser(
        Context const & context,
        eps_parser<nope> const & parser,
        std::ostream & os,
        int components)
    {
        os << "eps";
    }

    template<typename Context>
    void print_parser(
        Context const & context,
        eoi_parser const & parser,
        std::ostream & os,
        int components)
    {
        os << "eoi";
    }

    template<typename Context, typename Atribute>
    void print_parser(
        Context const & context,
        attr_parser<Atribute> const & parser,
        std::ostream & os,
        int components)
    {
        os << "attr";
        detail::print_expected(context, os, parser.attr_);
    }

    template<
        typename Context,
        typename ResolvedExpected,
        bool Integral = std::is_integral<ResolvedExpected>{}>
    struct print_expected_char_impl
    {
        static void call(
            Context const & context,
            std::ostream & os,
            ResolvedExpected expected)
        {
            detail::print(os, expected);
        }
    };

    template<typename Context>
    struct print_expected_char_impl<Context, char32_t, true>
    {
        static void
        call(Context const & context, std::ostream & os, char32_t expected)
        {
            if (expected == '\'') {
                os << "'\\''";
                return;
            }
            std::array<char32_t, 1> cps = {{expected}};
            auto const r = cps | text::as_utf8;
            os << "'";
            for (auto c : r) {
                detail::print_char(os, c);
            }
            os << "'";
        }
    };

    template<typename Context, typename Expected>
    void print_expected_char(
        Context const & context, std::ostream & os, Expected expected)
    {
        auto resolved_expected = detail::resolve(context, expected);
        detail::print_expected_char_impl<Context, decltype(resolved_expected)>::
            call(context, os, resolved_expected);
    }

    template<typename Context, typename T>
    struct char_print_parser_impl
    {
        static void call(Context const & context, std::ostream & os, T expected)
        {
            detail::print_expected_char(context, os, expected);
        }
    };

    template<typename Context, typename T, typename U>
    struct char_print_parser_impl<Context, char_pair<T, U>>
    {
        static void call(
            Context const & context,
            std::ostream & os,
            char_pair<T, U> expected)
        {
            detail::print_expected_char(context, os, expected.lo_);
            os << ", ";
            detail::print_expected_char(context, os, expected.hi_);
        }
    };

    template<typename Context, typename Iter, typename Sentinel, bool B>
    struct char_print_parser_impl<Context, char_range<Iter, Sentinel, B>>
    {
        static void call(
            Context const & context,
            std::ostream & os,
            char_range<Iter, Sentinel, B> expected)
        {
            os << "\"";
            auto const r = expected.chars_ | text::as_utf8;
            for (auto c : r) {
                detail::print_char(os, c);
            }
            os << "\"";
        }
    };

    template<typename Context, typename Expected, typename AttributeType>
    void print_parser(
        Context const & context,
        char_parser<Expected, AttributeType> const & parser,
        std::ostream & os,
        int components)
    {
        if (std::is_same_v<AttributeType, uint32_t>)
            os << "cp";
        else if (std::is_same_v<AttributeType, char>)
            os << "cu";
        else
            os << "char_";
        if constexpr (!is_nope_v<Expected>) {
            os << "(";
            char_print_parser_impl<Context, Expected>::call(
                context, os, parser.expected_);
            os << ")";
        }
    }

    template<typename Context>
    void print_parser(
        Context const & context,
        digit_parser const & parser,
        std::ostream & os,
        int components)
    {
        os << "digit";
    }

    template<typename Context>
    void print_parser(
        Context const & context,
        char_subrange_parser<hex_digit_subranges> const & parser,
        std::ostream & os,
        int components)
    {
        os << "hex_digit";
    }

    template<typename Context>
    void print_parser(
        Context const & context,
        char_subrange_parser<control_subranges> const & parser,
        std::ostream & os,
        int components)
    {
        os << "control";
    }

    template<typename Context>
    void print_parser(
        Context const & context,
        char_set_parser<punct_chars> const & parser,
        std::ostream & os,
        int components)
    {
        os << "punct";
    }

    template<typename Context>
    void print_parser(
        Context const & context,
        char_set_parser<symb_chars> const & parser,
        std::ostream & os,
        int components)
    {
        os << "symb";
    }

    template<typename Context>
    void print_parser(
        Context const & context,
        char_set_parser<lower_case_chars> const & parser,
        std::ostream & os,
        int components)
    {
        os << "lower";
    }

    template<typename Context>
    void print_parser(
        Context const & context,
        char_set_parser<upper_case_chars> const & parser,
        std::ostream & os,
        int components)
    {
        os << "upper";
    }

    template<typename Context, typename Expected, typename AttributeType>
    void print_parser(
        Context const & context,
        omit_parser<char_parser<Expected, AttributeType>> const & parser,
        std::ostream & os,
        int components)
    {
        if constexpr (is_nope_v<Expected>) {
            os << "omit[char_]";
        } else {
            char_print_parser_impl<Context, Expected>::call(
                context, os, parser.parser_.expected_);
        }
    }

    template<typename Context, typename StrIter, typename StrSentinel>
    void print_parser(
        Context const & context,
        string_parser<StrIter, StrSentinel> const & parser,
        std::ostream & os,
        int components)
    {
        os << "string(\"";
        for (auto c : BOOST_PARSER_DETAIL_TEXT_SUBRANGE(
                          parser.expected_first_, parser.expected_last_) |
                          text::as_utf8) {
            detail::print_char(os, c);
        }
        os << "\")";
    }

    template<typename Context, typename StrIter, typename StrSentinel>
    void print_parser(
        Context const & context,
        omit_parser<string_parser<StrIter, StrSentinel>> const & parser,
        std::ostream & os,
        int components)
    {
        os << "\"";
        for (auto c : BOOST_PARSER_DETAIL_TEXT_SUBRANGE(
                          parser.parser_.expected_first_,
                          parser.parser_.expected_last_) |
                          text::as_utf8) {
            detail::print_char(os, c);
        }
        os << "\"";
    }

    template<
        typename Context,
        typename Quotes,
        typename Escapes,
        typename CharParser>
    void print_parser(
        Context const & context,
        quoted_string_parser<Quotes, Escapes, CharParser> const & parser,
        std::ostream & os,
        int components)
    {
        os << "quoted_string(";
        if constexpr (is_nope_v<Quotes>) {
            detail::print_expected_char_impl<Context, char32_t>::call(
                context, os, parser.ch_);
        } else {
            os << '"';
            for (auto c : parser.chs_ | text::as_utf8) {
                detail::print_char(os, c);
            }
            os << '"';
        }
        os << ')';
    }

    template<typename Context, bool NewlinesOnly, bool NoNewlines>
    void print_parser(
        Context const & context,
        ws_parser<NewlinesOnly, NoNewlines> const & parser,
        std::ostream & os,
        int components)
    {
        if constexpr (NoNewlines)
            os << "blank";
        else if constexpr (NewlinesOnly)
            os << "eol";
        else
            os << "ws";
    }

    template<typename Context>
    void print_parser(
        Context const & context,
        bool_parser const & parser,
        std::ostream & os,
        int components)
    {
        os << "bool_";
    }

    template<
        typename Context,
        typename T,
        int Radix,
        int MinDigits,
        int MaxDigits,
        typename Expected>
    void print_parser(
        Context const & context,
        uint_parser<T, Radix, MinDigits, MaxDigits, Expected> const & parser,
        std::ostream & os,
        int components)
    {
        if (MinDigits == 1 && MaxDigits == -1) {
            if (std::is_same_v<T, unsigned short>) {
                os << "ushort_";
                detail::print_expected(context, os, parser.expected_);
                return;
            } else if (std::is_same_v<T, unsigned int>) {
                if (Radix == 2)
                    os << "bin";
                else if (Radix == 8)
                    os << "oct";
                else if (Radix == 16)
                    os << "hex";
                else if (Radix == 10)
                    os << "uint_";
                detail::print_expected(context, os, parser.expected_);
                return;
            } else if (Radix == 10 && std::is_same_v<T, unsigned long>) {
                os << "ulong_";
                detail::print_expected(context, os, parser.expected_);
                return;
            } else if (Radix == 10 && std::is_same_v<T, unsigned long long>) {
                os << "ulong_long";
                detail::print_expected(context, os, parser.expected_);
                return;
            }
        }
        os << "uint<" << detail::type_name<T>() << ", " << Radix << ", "
           << MinDigits << ", " << MaxDigits << ">";
        detail::print_expected(context, os, parser.expected_);
    }

    template<
        typename Context,
        typename T,
        int Radix,
        int MinDigits,
        int MaxDigits,
        typename Expected>
    void print_parser(
        Context const & context,
        int_parser<T, Radix, MinDigits, MaxDigits, Expected> const & parser,
        std::ostream & os,
        int components)
    {
        if (Radix == 10 && MinDigits == 1 && MaxDigits == -1) {
            if (std::is_same_v<T, short>) {
                os << "short_";
                detail::print_expected(context, os, parser.expected_);
                return;
            } else if (std::is_same_v<T, int>) {
                os << "int_";
                detail::print_expected(context, os, parser.expected_);
                return;
            } else if (std::is_same_v<T, long>) {
                os << "long_";
                detail::print_expected(context, os, parser.expected_);
                return;
            } else if (std::is_same_v<T, long long>) {
                os << "long_long";
                detail::print_expected(context, os, parser.expected_);
                return;
            }
        }
        os << "int<" << detail::type_name<T>() << ", " << Radix << ", "
           << MinDigits << ", " << MaxDigits << ">";
        detail::print_expected(context, os, parser.expected_);
    }

    template<typename Context>
    void print_parser(
        Context const & context,
        int_parser<short> const & parser,
        std::ostream & os,
        int components)
    {
        os << "short_";
    }

    template<typename Context>
    void print_parser(
        Context const & context,
        int_parser<long> const & parser,
        std::ostream & os,
        int components)
    {
        os << "long_";
    }

    template<typename Context>
    void print_parser(
        Context const & context,
        int_parser<long long> const & parser,
        std::ostream & os,
        int components)
    {
        os << "long_long";
    }

    template<typename Context, typename T>
    void print_parser(
        Context const & context,
        float_parser<T> const & parser,
        std::ostream & os,
        int components)
    {
        os << "float<" << detail::type_name<T>() << ">";
    }

    template<typename Context>
    void print_parser(
        Context const & context,
        float_parser<float> const & parser,
        std::ostream & os,
        int components)
    {
        os << "float_";
    }

    template<typename Context>
    void print_parser(
        Context const & context,
        float_parser<double> const & parser,
        std::ostream & os,
        int components)
    {
        os << "double_";
    }

    template<
        typename Context,
        typename ParserTuple,
        typename BacktrackingTuple,
        typename CombiningGroups>
    void print_switch_matchers(
        Context const & context,
        seq_parser<ParserTuple, BacktrackingTuple, CombiningGroups> const &
            parser,
        std::ostream & os,
        int components)
    {
        using namespace literals;
        os << "(";
        detail::print(
            os,
            detail::resolve(
                context, parser::get(parser.parsers_, 0_c).pred_.value_));
        os << ", ";
        detail::print_parser(
            context, parser::get(parser.parsers_, 1_c), os, components);
        os << ")";
    }

    template<typename Context, typename ParserTuple>
    void print_switch_matchers(
        Context const & context,
        or_parser<ParserTuple> const & parser,
        std::ostream & os,
        int components)
    {
        using namespace literals;

        bool printed_ellipsis = false;
        hl::for_each(parser.parsers_, [&](auto const & parser) {
            if (components == parser_component_limit) {
                if (!printed_ellipsis)
                    os << "...";
                printed_ellipsis = true;
                return;
            }
            detail::print_switch_matchers(context, parser, os, components);
            ++components;
        });
    }

    template<typename Context, typename SwitchValue, typename OrParser>
    void print_parser(
        Context const & context,
        switch_parser<SwitchValue, OrParser> const & parser,
        std::ostream & os,
        int components)
    {
        os << "switch_(";
        detail::print(os, detail::resolve(context, parser.switch_value_));
        os << ")";
        detail::print_switch_matchers(
            context, parser.or_parser_, os, components);
    }

}}}

#endif
