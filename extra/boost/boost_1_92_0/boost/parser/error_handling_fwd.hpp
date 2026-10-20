#ifndef BOOST_PARSER_ERROR_HANDLING_FWD_HPP
#define BOOST_PARSER_ERROR_HANDLING_FWD_HPP

#include <boost/parser/config.hpp>

#include <boost/parser/detail/text/transcode_view.hpp>

#include <iostream>
#include <string_view>


namespace boost { namespace parser {

    /** The possible actions to take when a parse error is handled by an error
        handler. */
    enum class error_handler_result {
        fail,   /// Fail the top-level parse.
        rethrow /// Re-throw the parse error exception.
    };

    /** The exception thrown when a parse error is encountered, consisting of
        an iterator to the point of failure, and the name of the failed parser
        or rule in `what()`. */
    template<typename Iter>
    struct parse_error : std::runtime_error
    {
        parse_error(Iter it, std::string const & msg) :
            runtime_error(msg), iter(it)
        {}

        Iter iter;
    };

    /** A position within a line, consisting of an iterator to the start of
        the line, the line number, and the column number. */
    template<typename Iter>
    struct line_position
    {
        Iter line_start;
        int64_t line_number;
        int64_t column_number;
    };

    /** Writes a formatted message (meaning prefixed with the file name, line,
        and column number) to `os`. */
    template<typename Iter, typename Sentinel>
    std::ostream & write_formatted_message(
        std::ostream & os,
        std::string_view filename,
        Iter first,
        Iter it,
        Sentinel last,
        std::string_view message,
        int64_t preferred_max_line_length = 80,
        int64_t max_after_caret = 40);

#if defined(_MSC_VER) || defined(BOOST_PARSER_DOXYGEN)
    /** Writes a formatted message (meaning prefixed with the file name, line,
        and column number) to `os`.  This overload is Windows-only. */
    template<typename Iter, typename Sentinel>
    std::ostream & write_formatted_message(
        std::ostream & os,
        std::wstring_view filename,
        Iter first,
        Iter it,
        Sentinel last,
        std::string_view message,
        int64_t preferred_max_line_length = 80,
        int64_t max_after_caret = 40);
#endif

    /** Writes a formatted parse-expectation failure (meaning prefixed with
        the file name, line, and column number) to `os`. */
    template<typename Iter, typename Sentinel>
    std::ostream & write_formatted_expectation_failure_error_message(
        std::ostream & os,
        std::string_view filename,
        Iter first,
        Sentinel last,
        parse_error<Iter> const & e,
        int64_t preferred_max_line_length = 80,
        int64_t max_after_caret = 40);

#if defined(_MSC_VER) || defined(BOOST_PARSER_DOXYGEN)
    /** Writes a formatted parse-expectation failure (meaning prefixed with
        the file name, line, and column number) to `os`.  This overload is
        Windows-only. */
    template<typename Iter, typename Sentinel>
    std::ostream & write_formatted_expectation_failure_error_message(
        std::ostream & os,
        std::wstring_view filename,
        Iter first,
        Sentinel last,
        parse_error<Iter> const & e,
        int64_t preferred_max_line_length = 80,
        int64_t max_after_caret = 40);
#endif

    /** The kinds of diagnostics that can be handled by an error handler. */
    enum class diagnostic_kind {
        error,  /// An error diagnostic.
        warning /// A warning diagnostic.
    };

    /** The error handler used when the user does not specify a custom one.
        This error handler prints warnings and errors to `std::cerr`, and does
        not have an associated filename. */
    struct default_error_handler
    {
        constexpr default_error_handler() = default;

        /** Handles a `parse_error` exception thrown during parsing.  A
            formatted parse-expectation failure is printed to `std::cerr`.
            Always returns `error_handler_result::fail`. */
        template<typename Iter, typename Sentinel>
        error_handler_result operator()(
            Iter first, Sentinel last, parse_error<Iter> const & e) const;

        /** Prints `message` to `std::cerr`.  The diagnostic is printed with
            the given `kind`, indicating the location as being at `it`.  This
            must be called within a parser semantic action, providing the
            parse context. */
        //[ error_handler_api_1
        template<typename Context, typename Iter>
        void diagnose(
            diagnostic_kind kind,
            std::string_view message,
            Context const & context,
            Iter it) const;
        //]

        /** Prints `message` to `std::cerr`.  The diagnostic is printed with
            the given `kind`, at no particular location.  This must be called
            within a parser semantic action, providing the parse context. */
        //[ error_handler_api_2
        template<typename Context>
        void diagnose(
            diagnostic_kind kind,
            std::string_view message,
            Context const & context) const;
        //]
    };

    /** Prints warnings and errors to the `std::ostream`s provided by the
        user, or `std::cerr` if neither stream is specified.  If a filename is
        provided, that is used to print all diagnostics. */
    struct stream_error_handler
    {
        stream_error_handler() : err_os_(&std::cerr), warn_os_(err_os_) {}
        stream_error_handler(std::string_view filename) :
            filename_(filename), err_os_(&std::cerr), warn_os_(err_os_)
        {}
        stream_error_handler(std::string_view filename, std::ostream & errors) :
            filename_(filename), err_os_(&errors), warn_os_(&errors)
        {}
        stream_error_handler(
            std::string_view filename,
            std::ostream & errors,
            std::ostream & warnings) :
            filename_(filename), err_os_(&errors), warn_os_(&warnings)
        {}
#if defined(_MSC_VER) || defined(BOOST_PARSER_DOXYGEN)
        /** This overload is Windows-only. */
        stream_error_handler(std::wstring_view filename) :
            err_os_(&std::cerr), warn_os_(err_os_)
        {
            auto const r = filename | detail::text::as_utf8;
            filename_.assign(r.begin(), r.end());
        }
        /** This overload is Windows-only. */
        stream_error_handler(
            std::wstring_view filename, std::ostream & errors) :
            err_os_(&errors), warn_os_(&errors)
        {
            auto const r = filename | detail::text::as_utf8;
            filename_.assign(r.begin(), r.end());
        }
        /** This overload is Windows-only. */
        stream_error_handler(
            std::wstring_view filename,
            std::ostream & errors,
            std::ostream & warnings) :
            err_os_(&errors), warn_os_(&warnings)
        {
            auto const r = filename | detail::text::as_utf8;
            filename_.assign(r.begin(), r.end());
        }
#endif

        /** Handles a `parse_error` exception thrown during parsing.  A
            formatted parse-expectation failure is printed to `*err_os_` when
            `err_os_` is non-null, or `std::cerr` otherwise.  Always returns
            `error_handler_result::fail`. */
        template<typename Iter, typename Sentinel>
        error_handler_result
        operator()(Iter first, Sentinel last, parse_error<Iter> const & e) const;

        /** Let `std::ostream * s = kind == diagnostic_kind::error : err_os_ :
            warn_os_`; prints `message` to `*s` when `s` is non-null, or
            `std::cerr` otherwise.  The diagnostic is printed with the given
            `kind`, indicating the location as being at `it`.  This must be
            called within a parser semantic action, providing the parse
            context. */
        template<typename Context, typename Iter>
        void diagnose(
            diagnostic_kind kind,
            std::string_view message,
            Context const & context,
            Iter it) const;

        /** Let `std::ostream * s = kind == diagnostic_kind::error : err_os_ :
            warn_os_`; prints `message` to `*s` when `s` is non-null, or
            `std::cerr` otherwise.  The diagnostic is printed with the given
            `kind`, at no particular location.  This must be called within a
            parser semantic action, providing the parse context. */
        template<typename Context>
        void diagnose(
            diagnostic_kind kind,
            std::string_view message,
            Context const & context) const;

    private:
        std::string filename_;
        std::ostream * err_os_;
        std::ostream * warn_os_;
    };

}}

#endif
