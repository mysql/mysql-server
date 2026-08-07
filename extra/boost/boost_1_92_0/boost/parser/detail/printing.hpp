#ifndef BOOST_PARSER_DETAIL_PRINTING_HPP
#define BOOST_PARSER_DETAIL_PRINTING_HPP

#include <boost/parser/parser_fwd.hpp>
#include <boost/parser/tuple.hpp>
#include <boost/parser/detail/detection.hpp>
#include <boost/parser/detail/hl.hpp>
#include <boost/parser/detail/text/unpack.hpp>
#if defined(_MSC_VER) && defined(BOOST_PARSER_TRACE_TO_VS_OUTPUT)
#include <boost/parser/detail/vs_output_stream.hpp>
#endif
#include <boost/parser/detail/text/transcode_view.hpp>

#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <variant>

#include <cctype>


namespace boost { namespace parser { namespace detail {

    template<typename Context>
    decltype(auto) _indent(Context const & context);

    template<typename Char>
    std::ostream & print_char(std::ostream & os, Char c)
    {
        if constexpr (
#if defined(__cpp_char8_t)
            std::is_same_v<
                char8_t,
                std::remove_cv_t<std::remove_reference_t<Char>>>
#else
            false
#endif
        ) {
            os << char(c);
        } else {
            os << c;
        }
        return os;
    }

    enum { parser_component_limit = 4 };

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
        int components = 0);

    template<typename Context, typename Parser>
    void print_parser(
        Context const & context,
        opt_parser<Parser> const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context, typename ParserTuple>
    void print_parser(
        Context const & context,
        or_parser<ParserTuple> const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context, typename ParserTuple, typename DelimiterParser>
    void print_parser(
        Context const & context,
        perm_parser<ParserTuple, DelimiterParser> const & parser,
        std::ostream & os,
        int components = 0);

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
        int components = 0);

    template<typename Context, typename Parser, typename Action>
    void print_parser(
        Context const & context,
        action_parser<Parser, Action> const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context, typename Parser, typename F>
    void print_parser(
        Context const & context,
        transform_parser<Parser, F> const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context, typename Parser>
    void print_parser(
        Context const & context,
        omit_parser<Parser> const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context, typename Parser>
    void print_parser(
        Context const & context,
        raw_parser<Parser> const & parser,
        std::ostream & os,
        int components = 0);

#if defined(BOOST_PARSER_DOXYGEN) || BOOST_PARSER_USE_CONCEPTS
    template<typename Context, typename Parser>
    void print_parser(
        Context const & context,
        string_view_parser<Parser> const & parser,
        std::ostream & os,
        int components = 0);
#endif

    template<typename Context, typename Parser>
    void print_parser(
        Context const & context,
        lexeme_parser<Parser> const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context, typename Parser>
    void print_parser(
        Context const & context,
        no_case_parser<Parser> const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context, typename Parser, typename SkipParser>
    void print_parser(
        Context const & context,
        skip_parser<Parser, SkipParser> const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context, typename Parser, bool FailOnMatch>
    void print_parser(
        Context const & context,
        expect_parser<Parser, FailOnMatch> const & parser,
        std::ostream & os,
        int components = 0);

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
        int components = 0);

    template<typename Context, typename T>
    void print_parser(
        Context const & context,
        symbol_parser<T> const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context, typename Predicate>
    void print_parser(
        Context const & context,
        eps_parser<Predicate> const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context>
    void print_parser(
        Context const & context,
        eps_parser<nope> const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context>
    void print_parser(
        Context const & context,
        eoi_parser const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context, typename Atribute>
    void print_parser(
        Context const & context,
        attr_parser<Atribute> const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context, typename Expected, typename AttributeType>
    void print_parser(
        Context const & context,
        char_parser<Expected, AttributeType> const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context>
    void print_parser(
        Context const & context,
        digit_parser const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context>
    void print_parser(
        Context const & context,
        char_subrange_parser<hex_digit_subranges> const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context>
    void print_parser(
        Context const & context,
        char_subrange_parser<control_subranges> const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context>
    void print_parser(
        Context const & context,
        char_set_parser<punct_chars> const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context>
    void print_parser(
        Context const & context,
        char_set_parser<symb_chars> const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context>
    void print_parser(
        Context const & context,
        char_set_parser<lower_case_chars> const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context>
    void print_parser(
        Context const & context,
        char_set_parser<upper_case_chars> const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context, typename Expected, typename AttributeType>
    void print_parser(
        Context const & context,
        omit_parser<char_parser<Expected, AttributeType>> const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context, typename StrIter, typename StrSentinel>
    void print_parser(
        Context const & context,
        string_parser<StrIter, StrSentinel> const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context, typename StrIter, typename StrSentinel>
    void print_parser(
        Context const & context,
        omit_parser<string_parser<StrIter, StrSentinel>> const & parser,
        std::ostream & os,
        int components = 0);

    template<
        typename Context,
        typename Quotes,
        typename Escapes,
        typename CharParser>
    void print_parser(
        Context const & context,
        quoted_string_parser<Quotes, Escapes, CharParser> const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context, bool NewlinesOnly, bool NoNewlines>
    void print_parser(
        Context const & context,
        ws_parser<NewlinesOnly, NoNewlines> const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context>
    void print_parser(
        Context const & context,
        bool_parser const & parser,
        std::ostream & os,
        int components = 0);

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
        int components = 0);

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
        int components = 0);

    template<typename Context, typename T>
    void print_parser(
        Context const & context,
        float_parser<T> const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context>
    void print_parser(
        Context const & context,
        float_parser<float> const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context>
    void print_parser(
        Context const & context,
        float_parser<double> const & parser,
        std::ostream & os,
        int components = 0);

    template<typename Context, typename SwitchValue, typename OrParser>
    void print_parser(
        Context const & context,
        switch_parser<SwitchValue, OrParser> const & parser,
        std::ostream & os,
        int components = 0);

    enum { trace_indent_factor = 2 };

    inline void trace_indent(std::ostream & os, int indent)
    {
        for (int i = 0, end = trace_indent_factor * indent; i != end; ++i) {
            os << ' ';
        }
    }

    template<typename Iter, typename Sentinel, int SizeofValueType>
    struct trace_input_impl
    {
        static void call(
            std::ostream & os,
            Iter first_,
            Sentinel last_,
            bool quote,
            int64_t trace_input_cps)
        {
            auto utf8 = BOOST_PARSER_DETAIL_TEXT_SUBRANGE(first_, last_) | text::as_utf8;
            auto first = utf8.begin();
            auto last = utf8.end();
            if (quote)
                os << '"';
            for (int64_t i = 0; i < trace_input_cps && first != last;
                 ++i, ++first) {
                detail::print_char(os, *first);
            }
            if (quote)
                os << '"';
        }
    };

    template<typename Iter, typename Sentinel>
    struct trace_input_impl<Iter, Sentinel, 1>
    {
        static void call(
            std::ostream & os,
            Iter first_,
            Sentinel last_,
            bool quote,
            int64_t trace_input_cps)
        {
            auto r = BOOST_PARSER_DETAIL_TEXT_SUBRANGE(first_, last_);
            auto r_unpacked =
                detail::text::unpack_iterator_and_sentinel(first_, last_);
            auto utf32 = r | text::as_utf32;
            auto first = utf32.begin();
            auto const last = utf32.end();
            for (int64_t i = 0; i < trace_input_cps && first != last; ++i) {
                ++first;
            }
            if (quote)
                os << '"';
            auto first_repacked = r_unpacked.repack(first.base());
            for (Iter it = first_, end = first_repacked; it != end; ++it) {
                detail::print_char(os, *it);
            }
            if (quote)
                os << '"';
        }
    };

    template<typename Iter, typename Sentinel>
    inline void trace_input(
        std::ostream & os,
        Iter first,
        Sentinel last,
        bool quote = true,
        int64_t trace_input_cps = 8)
    {
        trace_input_impl<Iter, Sentinel, sizeof(*first)>::call(
            os, first, last, quote, trace_input_cps);
    }

    template<typename Iter, typename Sentinel>
    inline void trace_begin_match(
        std::ostream & os,
        Iter first,
        Sentinel last,
        int indent,
        std::string_view name)
    {
        detail::trace_indent(os, indent);
        os << "[begin " << name << "; input=";
        detail::trace_input(os, first, last);
        os << "]" << std::endl;
    }

    template<typename Iter, typename Sentinel>
    inline void trace_end_match(
        std::ostream & os,
        Iter first,
        Sentinel last,
        int indent,
        std::string_view name)
    {
        detail::trace_indent(os, indent);
        os << "[end " << name << "; input=";
        detail::trace_input(os, first, last);
        os << "]" << std::endl;
    }

    template<typename Iter, typename Sentinel, typename Context>
    void trace_prefix(
        std::ostream & os,
        Iter first,
        Sentinel last,
        Context const & context,
        std::string_view name)
    {
        int & indent = detail::_indent(context);
        detail::trace_begin_match(os, first, last, indent, name);
        ++indent;
    }

    template<typename Iter, typename Sentinel, typename Context>
    void trace_suffix(
        std::ostream & os,
        Iter first,
        Sentinel last,
        Context const & context,
        std::string_view name)
    {
        int & indent = detail::_indent(context);
        --indent;
        detail::trace_end_match(os, first, last, indent, name);
    }

    template<typename T>
    using streamable =
        decltype(std::declval<std::ostream &>() << std::declval<T const &>());

    template<typename T, bool Streamable = is_detected_v<streamable, T>>
    struct printer
    {
        std::ostream & operator()(std::ostream & os, T const &)
        {
            return os << "<<unprintable-value>>";
        }
    };

    template<typename T>
    void print_printable(std::ostream & os, T const & x)
    {
        os << x;
    }

    inline void print_printable(std::ostream & os, char c)
    {
        if (std::isprint(c)) {
            os << "'" << c << "'";
        } else {
            unsigned char c_ = c;
            os << "'\\x" << std::hex << std::setfill('0') << (uint32_t)c_
               << "'";
        }
    }

    inline void print_printable(std::ostream & os, char32_t c)
    {
        if (c < 256) {
            os << "U";
            detail::print_printable(os, (char)c);
        } else {
            os << "U'\\U" << std::hex << std::setfill('0') << (int32_t)c << "'";
        }
    }

    template<typename T>
    struct printer<T, true>
    {
        std::ostream & operator()(std::ostream & os, T const & x)
        {
            detail::print_printable(os, x);
            return os;
        }
    };

    template<typename T>
    constexpr bool is_variant_v = enable_variant<T>;

    template<typename Attribute>
    inline void print(std::ostream & os, Attribute const & attr)
    {
        using just_attribute =
            std::remove_cv_t<std::remove_reference_t<Attribute>>;
        if constexpr (is_tuple<just_attribute>{}) {
            os << "(";
            bool first = true;
            hl::for_each(attr, [&](auto const & a) {
                if (!first)
                    os << ", ";
                detail::print(os, a);
                first = false;
            });
            os << ")";
        } else if constexpr (is_optional_v<just_attribute>) {
            if (!attr)
                os << "<<empty>>";
            else
                detail::print(os, *attr);
        } else if constexpr (is_variant_v<just_attribute>) {
            os << "<<variant>>";
        } else {
            printer<just_attribute>{}(os, attr);
        }
    }

    template<typename Attribute>
    inline void
    print_attribute(std::ostream & os, Attribute const & attr, int indent)
    {
        detail::trace_indent(os, indent);
        os << "attribute: ";
        detail::print(os, attr);
        os << "\n";
    }

    inline void print_attribute(std::ostream &, nope const &, int) {}

    constexpr inline bool do_trace(flags f)
    {
        return (uint32_t(f) & uint32_t(flags::trace)) == uint32_t(flags::trace);
    }

    template<typename Context, typename T>
    decltype(auto) resolve(Context const & context, T const & x);

    template<typename Context>
    auto resolve(Context const &, nope n);

    template<typename DependentType, bool DoTraceMacro>
    constexpr bool trace_disabled = !DoTraceMacro;

    template<
        bool DoTrace,
        typename Iter,
        typename Sentinel,
        typename Context,
        typename Attribute>
    struct scoped_trace_t
    {
        static_assert(!trace_disabled<Iter, BOOST_PARSER_DO_TRACE>);

        scoped_trace_t(
            std::ostream & os,
            Iter & first,
            Sentinel last,
            Context const & context,
            flags f,
            Attribute const & attr,
            std::string name) :
            os_(os),
            initial_first_(first),
            first_(first),
            last_(last),
            context_(context),
            flags_(f),
            attr_(attr),
            name_(std::move(name))
        {
            if (!detail::do_trace(flags_))
                return;
            detail::trace_prefix(os, first_, last_, context_, name_);
        }

        ~scoped_trace_t()
        {
            if (!detail::do_trace(flags_))
                return;
            detail::trace_indent(os_, detail::_indent(context_));
            if (*context_.pass_) {
                os_ << "matched ";
                detail::trace_input(os_, initial_first_, first_);
                os_ << "\n";
                detail::print_attribute(
                    os_,
                    detail::resolve(context_, attr_),
                    detail::_indent(context_));
            } else {
                os_ << "no match\n";
            }
            detail::trace_suffix(os_, first_, last_, context_, name_);
        }

        std::ostream & os_;
        Iter initial_first_;
        Iter & first_;
        Sentinel last_;
        Context const & context_;
        flags flags_;
        Attribute const & attr_;
        std::string name_;
    };

    template<
        typename Iter,
        typename Sentinel,
        typename Context,
        typename Attribute>
    struct scoped_trace_t<false, Iter, Sentinel, Context, Attribute>
    {
        scoped_trace_t() {}
    };

    template<
        typename Parser,
        typename Iter,
        typename Sentinel,
        typename Context,
        typename Attribute>
    auto scoped_trace(
        Parser const & parser,
        Iter & first,
        Sentinel last,
        Context const & context,
        flags f,
        Attribute const & attr)
    {
        if constexpr (!BOOST_PARSER_DO_TRACE)
            return;

        if constexpr (Context::do_trace) {
            std::stringstream oss;
            detail::print_parser(context, parser, oss);
            std::ostream & os = BOOST_PARSER_TRACE_OSTREAM;
            return scoped_trace_t<true, Iter, Sentinel, Context, Attribute>(
                os, first, last, context, f, attr, oss.str());
        } else {
            return scoped_trace_t<false, Iter, Sentinel, Context, Attribute>{};
        }
    }

    template<typename Context, typename Attribute>
    auto final_trace(Context const & context, flags f, Attribute const & attr)
    {
        if constexpr (!BOOST_PARSER_DO_TRACE)
            return;

        if (!detail::do_trace(f))
            return;

        std::ostream & os = BOOST_PARSER_TRACE_OSTREAM;
        os << "--------------------\n";
        if (*context.pass_) {
            os << "parse succeeded\n";
            detail::print_attribute(os, detail::resolve(context, attr), 0);
        } else {
            os << "parse failed\n";
        }
        os << "--------------------" << std::endl;
    }

}}}

#endif
