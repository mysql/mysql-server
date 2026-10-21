//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_SEGMENTS_ENCODED_BASE_HPP
#define BOOST_URL_SEGMENTS_ENCODED_BASE_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/ignore_case.hpp>
#include <boost/url/pct_string_view.hpp>
#include <boost/url/detail/url_impl.hpp>
#include <iosfwd>

namespace boost {
namespace urls {

/** Percent-encoded path segment helper base

    Implements the shared encoded-segment
    algorithms reused by @ref segments_encoded_view
    and @ref segments_encoded_ref. It is not
    intended to be instantiated directly; use
    one of those concrete containers instead.

    @par Containers
    @li @ref segments_ref
    @li @ref segments_view
    @li @ref segments_encoded_ref
    @li @ref segments_encoded_view
*/
class BOOST_SYMBOL_VISIBLE segments_encoded_base
{
    detail::path_ref ref_;

    friend class url_view_base;
    friend class segments_encoded_ref;
    friend class segments_encoded_view;

    segments_encoded_base(
        detail::path_ref const& ref) noexcept;
    segments_encoded_base() = default;
    segments_encoded_base(
        segments_encoded_base const&) = default;
    segments_encoded_base& operator=(
        segments_encoded_base const&) = default;

public:
    /** A Bidirectional iterator to a path segment

        Objects of this type allow iteration
        through the segments in the path.
        Strings returned by iterators may
        contain percent escapes.
        The values returned are read-only;
        changes to segments must be made
        through the container instead, if the
        container supports modification.

        <br>

        The strings produced when iterators
        are dereferenced refer to the underlying
        character buffer.
        Ownership is not transferred; the caller
        is responsible for ensuring that the
        lifetime of the buffer extends until
        it is no longer referenced by any
        container or iterator.
    */
    class iterator;

    /// @copydoc iterator
    using const_iterator = iterator;

    /** The value type

        Values of this type represent a segment
        where unique ownership is retained by
        making a copy.

        @par Example
        @code
        segments_encoded_base::value_type ps( url_view( "/path/to/file.txt" ).encoded_segments().back() );
        @endcode
    */
    using value_type = std::string;

    /** The reference type

        This is the type of value returned when
        iterators of the view are dereferenced.
    */
    using reference = pct_string_view;

    /// @copydoc reference
    using const_reference = pct_string_view;

    /** An unsigned integer type used to represent size.
    */
    using size_type = std::size_t;

    /** A signed integer type used to represent differences.
    */
    using difference_type = std::ptrdiff_t;

    //--------------------------------------------
    //
    // Observers
    //
    //--------------------------------------------

    /** Return the maximum number of characters possible

        This represents the largest number of
        characters that are possible in a path,
        not including any null terminator.

        @par Exception Safety
        Throws nothing.

        @return The maximum number of characters possible.
    */
    static
    constexpr
    std::size_t
    max_size() noexcept
    {
        return BOOST_URL_MAX_SIZE;
    }

    /** Return the referenced character buffer.

        This function returns the character
        buffer referenced by the view.
        The returned string may contain
        percent escapes.

        @par Example
        @code
        assert( url_view( "/path/to/file.txt" ).encoded_segments().buffer() == "/path/to/file.txt" );
        @endcode

        @par Complexity
        Constant.

        @par Exception Safety
        Throws nothing.

        @return A string view of the buffer.
    */
    pct_string_view
    buffer() const noexcept;

    /** Returns true if this references an absolute path.

        Absolute paths always start with a
        forward slash ('/').

        @par Example
        @code
        assert( url_view( "/path/to/file.txt" ).encoded_segments().is_absolute() == true );
        @endcode

        @par Complexity
        Constant.

        @par Exception Safety
        Throws nothing.

        @return `true` if the path is absolute, otherwise `false`.
    */
    bool
    is_absolute() const noexcept;

    /** Return true if there are no segments

        @par Example
        @code
        assert( ! url_view( "/index.htm" ).encoded_segments().empty() );
        @endcode

        @par Complexity
        Constant.

        @par Exception Safety
        Throws nothing.

        @return `true` if there are no segments, otherwise `false`.
    */
    bool
    empty() const noexcept;

    /** Return the number of segments
    
        @par Example
        @code
        assert( url_view( "/path/to/file.txt" ).encoded_segments().size() == 3 );
        @endcode

        @par Complexity
        Constant.

        @par Exception Safety
        Throws nothing.

        @return The number of segments.
    */
    std::size_t
    size() const noexcept;

    /** Return the first segment

        This function returns a string with the
        first segment of the path without any
        leading or trailing '/' separators.
        The returned string may contain
        percent escapes.

        @par Preconditions
        @code
        this->empty() == false
        @endcode

        @par Effects
        @code
        return *begin();
        @endcode

        @par Example
        @code
        assert( url_view( "/path/to/file.txt" ).encoded_segments().front() == "path" );
        @endcode

        @par Complexity
        Constant.

        @par Exception Safety
        Throws nothing.

        @return The first segment.
    */
    pct_string_view
    front() const noexcept;

    /** Return the last segment

        This function returns a string with the
        last segment of the path without any
        leading or trailing '/' separators.
        The returned string may contain
        percent escapes.

        @par Preconditions
        @code
        this->empty() == false
        @endcode

        @par Example
        @code
        assert( url_view( "/path/to/file.txt" ).encoded_segments().back() == "file.txt" );
        @endcode

        @par Preconditions
        @code
        this->empty() == false
        @endcode

        @par Effects
        @code
        return *--end();
        @endcode

        @par Complexity
        Constant.

        @par Exception Safety
        Throws nothing.

        @return The last segment.
    */
    pct_string_view
    back() const noexcept;

    /** Return an iterator to the beginning

        @par Complexity
        Linear in `this->front().size()` or
        constant if `this->empty()`.

        @par Exception Safety
        Throws nothing.

        @return An iterator to the first segment.
    */
    iterator
    begin() const noexcept;

    /** Return an iterator to the end

        @par Complexity
        Constant.

        @par Exception Safety
        Throws nothing.

        @return An iterator to one past the last segment.
    */
    iterator
    end() const noexcept;
};

//------------------------------------------------

/** Format to an output stream

    Any percent-escapes are emitted as-is;
    no decoding is performed.

    @par Complexity
    Linear in `ps.buffer().size()`.

    @par Effects
    @code
    return os << ps.buffer();
    @endcode

    @param os The output stream to write to.
    @param ps The object to format.
    @return A reference to the output stream.
*/
std::ostream&
operator<<(
    std::ostream& os,
    segments_encoded_base const& ps);

} // urls
} // boost

#include <boost/url/impl/segments_encoded_base.hpp>

#endif
