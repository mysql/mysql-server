//
// Copyright (c) 2020 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_PARSE_OPTIONS_HPP
#define BOOST_JSON_PARSE_OPTIONS_HPP

#include <boost/json/detail/config.hpp>
#include <iosfwd>

namespace boost {
namespace json {

/** Enumeration of number parsing modes

    These values are used to select the way to parse numbers. The default
    mode used by parsing functions is `imprecise`. It has the precision not
    less than 15 decimal places (the number required for IEEE `double`). This
    is also the number _guaranteed_ by functions in most C++ standard library
    implementations.

    But those functions often give a more precise result for certain numbers.
    For cases where such extra precision is needed the library provides
    `precise` mode. That extra precision comes with a performance cost, though.

    Finally, users might want a to parse numbers with an external function
    when they implement a custom handler for `basic_parser`. Since it is
    wasteful to parse a number only to throw the result away, the library
    provides a mode to not parse numbers at all. Note: the library still have
    to validate that the input is a number, in order to catch syntax errors and
    to know when the number ends.

    @see
        @ref parse_options,
        @ref basic_parser,
        @ref parser.
*/
enum class number_precision : unsigned char
{
    /// Fast, but potentially less precise mode.
    imprecise,

    /// Slower, but precise mode.
    precise,

    /// The fastest mode, that only validates encountered numbers without
    /// parsing them.
    none,
};

/** Parser options.

    This structure is used for specifying maximum parsing depth, and whether to
    allow various non-standard extensions. Default-constructed options set
    maximum parsing depth to 32 and specify that only standard JSON is allowed,

    @see @ref parse, @ref parser, @ref basic_parser.
*/
struct parse_options
{
    /** Maximum nesting level of arrays and objects.

        This specifies the maximum number of nested structures allowed while
        parsing a JSON text. If this limit is exceeded during a parse, an error
        is returned.

        @see @ref basic_parser, @ref stream_parser.
    */
    std::size_t max_depth = 32;

    /** Number pasing mode.

        This selects the way to parse numbers. The default is to parse them
        fast, but with possible slight imprecision for floating point numbers
        with larger mantissas. Users can also choose to parse numbers slower
        but with full precision. Or to not parse them at all, and only validate
        numbers. The latter mode is useful for @ref basic_parser instantiations
        that wish to treat numbers in a custom way.

        @see @ref basic_parser, @ref stream_parser.
    */
    number_precision numbers = number_precision::imprecise;

    /** Non-standard extension option.

        Allow C and C++ style comments to appear anywhere that whitespace is
        permissible.

        @see @ref basic_parser, @ref stream_parser.
    */
    bool allow_comments = false;

    /** Non-standard extension option

        Allow a trailing comma to appear after the last element of any array or
        object.

        @see @ref basic_parser, @ref stream_parser.
    */
    bool allow_trailing_commas = false;

    /** Non-standard extension option

        Allow invalid UTF-8 sequences to appear in keys and strings.

        @note This increases parsing performance.

        @see @ref basic_parser, @ref stream_parser.
    */
    bool allow_invalid_utf8 = false;

    /** Non-standard extension option

        Allow invalid UTF-16 surrogate pairs to appear in strings. When
        enabled, the parser will not strictly validate the correctness of
        UTF-16 encoding, allowing for the presence of illegal leading or
        trailing surrogates. In case of invalid sequences, the parser will
        replace them with the Unicode replacement character (`U+FFFD`).

        @attention Enabling this option may result in the parsing of invalid
        UTF-16 sequences without error, potentially leading to the loss of
        information.
    */
    bool allow_invalid_utf16 = false;

    /** Non-standard extension option

        Allow `Infinity`, `-Infinity`, and `NaN` JSON literals. These values
        are produced by some popular JSON implementations for positive
        infinity, negative infinity and NaN special numbers respectively.

        @see @ref basic_parser, @ref stream_parser.
    */
    bool allow_infinity_and_nan = false;

    /** Set JSON parse options on input stream.

        The function stores parse options in the private storage of the stream.
        If the stream fails to allocate necessary private storage, `badbit`
        will be set on it.

        @return Reference to `is`.

        @par Complexity
        Amortized constant (due to potential memory allocation by the stream).

        @par Exception Safety
        Strong guarantee.
        The stream may throw as configured by @ref std::ios::exceptions.

        @param is The input stream.
        @param opts The options to store.
    */
    BOOST_JSON_DECL
    friend
    std::istream&
    operator>>( std::istream& is, parse_options const& opts );
};

} // namespace json
} // namespace boost

#endif
