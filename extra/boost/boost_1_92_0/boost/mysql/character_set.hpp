//
// Copyright (c) 2019-2025 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_MYSQL_CHARACTER_SET_HPP
#define BOOST_MYSQL_CHARACTER_SET_HPP

#include <boost/mysql/detail/character_set.hpp>
#include <boost/mysql/detail/config.hpp>

#include <boost/core/span.hpp>

#include <cstddef>

namespace boost {
namespace mysql {

/**
 * \brief Represents a MySQL character set.
 * \details
 * By default, you should always use \ref utf8mb4_charset, unless there is
 * a strong reason not to. This struct allows you to extend this library
 * with character sets that are not supported out of the box.
 */
struct character_set
{
    /**
     * \brief The character set name, as a NULL-terminated string.
     * \details
     * This should match the character set name in MySQL. This is the string
     * you specify when issuing `SET NAMES` statements. You can find available
     * character sets using the `SHOW CHARACTER SET` statement.
     */
    const char* name;

    /**
     * \brief Obtains the size of the first character of a string.
     * \details
     * Given a range of bytes, `r`, this function must interpret `r` as a
     * string encoded using this character set, and return the number of
     * bytes that the first character in the string spans, or 0 in case of error.
     * `r` is guaranteed to be non-empty (`r.size() > 0`).
     * \n
     * In some character sets (like UTF-8), not all byte sequences represent
     * valid characters. If this function finds an invalid byte sequence while
     * trying to interpret the first character, it should return 0 to signal the error.
     * \n
     * This function must not throw exceptions or have side effects.
     */
    std::size_t (*next_char)(span<const unsigned char>);
};

/// The utf8mb4 character set (the one you should use by default).
BOOST_INLINE_CONSTEXPR character_set utf8mb4_charset
#ifndef BOOST_MYSQL_DOXYGEN
    {"utf8mb4", detail::next_char_utf8mb4}
#endif
;

/// The ascii character set.
BOOST_INLINE_CONSTEXPR character_set ascii_charset
#ifndef BOOST_MYSQL_DOXYGEN
    {"ascii", detail::next_char_ascii};
#endif
;

/**
 * \brief Settings required to format SQL queries client-side.
 * \details
 * The recommended way to obtain a value of this type is using \ref any_connection::format_opts.
 */
struct format_options
{
    /// The connection's current character set.
    character_set charset;

    /// Whether backslashes represent escape sequences.
    bool backslash_escapes;
};

}  // namespace mysql
}  // namespace boost

#ifdef BOOST_MYSQL_HEADER_ONLY
#include <boost/mysql/impl/character_set.ipp>
#endif

#endif
