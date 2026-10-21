//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#ifndef BOOST_URL_SEGMENTS_ENCODED_VIEW_HPP
#define BOOST_URL_SEGMENTS_ENCODED_VIEW_HPP

#include <boost/url/detail/config.hpp>
#include <boost/url/error_types.hpp>
#include <boost/url/segments_encoded_base.hpp>
#include <boost/url/segments_view.hpp>
#include <boost/core/detail/string_view.hpp>
#include <iosfwd>
#include <utility>

namespace boost {
namespace urls {

/** Non-owning encoded path segment view

    Exposes the raw percent-encoded segments of
    a URL path as a read-only bidirectional range.
    The view references the original buffer, so
    callers must keep that storage alive while
    iterating.

    @par Example
    @code
    url_view u( "/path/to/file.txt" );

    segments_encoded_view ps = u.encoded_segments();

    assert( ps.buffer().data() == u.buffer().data() );
    @endcode

    Elements are returned as encoded strings,
    preserving escape sequences for callers that
    need the exact byte representation.

    @par Iterator Invalidation
    Changes to the underlying character buffer
    can invalidate iterators which reference it.

    @see
        @ref segments_view,
        @ref segments_encoded_ref,
        @ref segments_ref.
*/
class BOOST_SYMBOL_VISIBLE segments_encoded_view
    : public segments_encoded_base
{
    friend class url_view_base;
    friend class segments_encoded_ref;

    segments_encoded_view(
        detail::path_ref const& ref) noexcept;

public:
    /** Constructor

        Default-constructed segments have
        zero elements.

        @par Example
        @code
        segments_encoded_view ps;
        @endcode

        @par Effects
        @code
        return segments_encoded_view( "" );
        @endcode

        @par Complexity
        Constant.

        @par Exception Safety
        Throws nothing.
    */
    segments_encoded_view() noexcept;

    /** Constructor

        After construction, both views
        reference the same character buffer.

        Ownership is not transferred; the caller
        is responsible for ensuring the lifetime
        of the buffer extends until it is no
        longer referenced.

        @par Postconditions
        @code
        this->buffer().data() == other.buffer().data()
        @endcode

        @par Complexity
        Constant.

        @par Exception Safety
        Throws nothing
    */
    segments_encoded_view(
        segments_encoded_view const&) noexcept = default;

    /** Constructor

        This function constructs segments from
        a valid path string, which can contain
        percent escapes.
        Upon construction, the view references
        the character buffer pointed to by `s`.
        caller is responsible for ensuring
        that the lifetime of the buffer
        extends until the view is destroyed.

        @par Example
        @code
        segments_encoded_view ps( "/path/to/file.txt" );
        @endcode

        @par Effects
        @code
        return parse_path( s ).value();
        @endcode

        @par Postconditions
        @code
        this->buffer().data() == s.data()
        @endcode

        @par Complexity
        Linear in `s`.

        @par Exception Safety
        Exceptions thrown on invalid input.

        @throw system_error
        `s` contains an invalid path.

        @param s The string to parse.

        @par BNF
        @code
        path = [ "/" ] [ segment *( "/" segment ) ]

        segment = *pchar
        @endcode

        @par Specification
        @li <a href="https://datatracker.ietf.org/doc/html/rfc3986#section-3.3"
            >3.3.  Path</a>
    */
    segments_encoded_view(
        core::string_view s);

    /** Constructor

        This function creates a new @ref segments_encoded_view
        from a pair of iterators referring to
        elements of another encoded segments
        view. The resulting view references
        the same underlying character buffer
        as the original.

        The constructed view preserves the
        original absolute flag when `first`
        selects the first segment and otherwise
        produces an absolute subview: if the
        source path is relative and `first ==
        ps.begin()` the new view is relative,
        and in every other case the subview is
        absolute with the separator immediately
        preceding `*first` retained at the front.
        This ensures the underlying text can be
        reconstructed by concatenating the buffers
        of adjacent subviews.

        The caller is responsible for ensuring
        that the lifetime of the original buffer
        extends until the constructed view
        is no longer referenced.

        @par Example
        @code
        segments_encoded_view ps( "/path/to/file.txt" );

        segments_encoded_view sub(
            std::next(ps.begin()),
            ps.end());

        segments_encoded_view first_half(
            ps.begin(),
            std::next(ps.begin()));

        // sub represents "/to/file.txt"
        std::string combined(
            first_half.buffer().data(),
            first_half.buffer().size());
        combined.append(
            sub.buffer().data(),
            sub.buffer().size());
        BOOST_ASSERT(combined == ps.buffer());
        @endcode

        @par Preconditions
        The iterators must be valid and belong to
        the same @ref segments_encoded_view.

        @par Postconditions
        `sub.buffer()` references characters in the
        original `ps.buffer()`.

        @par Complexity
        Constant

        @par Exception Safety
        Throws nothing.

        @param first The beginning iterator.
        @param last The ending iterator.
    */
    segments_encoded_view(
        iterator first,
        iterator last) noexcept;

    /** Assignment

        After assignment, both views
        reference the same underlying character
        buffer.

        Ownership is not transferred; the caller
        is responsible for ensuring the lifetime
        of the buffer extends until it is no
        longer referenced.

        @par Postconditions
        @code
        this->buffer().data() == other.buffer().data()
        @endcode

        @par Complexity
        Constant

        @par Exception Safety
        Throws nothing

        @param other The segments to copy.
        @return Reference to this object
    */
    segments_encoded_view&
    operator=(
        segments_encoded_view const& other) = default;

    /** Conversion

        This conversion returns a new view which
        references the same underlying character
        buffer, and whose iterators and members
        return ordinary strings with decoding
        applied to any percent escapes.

        Ownership is not transferred; the caller
        is responsible for ensuring the lifetime
        of the buffer extends until it is no
        longer referenced.

        @par Example
        @code
        segments_view ps = parse_path( "/path/to/file.txt" ).value();
        @endcode

        @par Postconditions
        @code
        segments_view( *this ).buffer().data() == this->buffer().data()
        @endcode

        @par Complexity
        Constant

        @par Exception Safety
        Throws nothing

        @return A view of the segments.
    */
    operator
    segments_view() const noexcept;

    //--------------------------------------------

    BOOST_URL_DECL
    friend
    system::result<segments_encoded_view>
    parse_path(core::string_view s) noexcept;
};

// Forward-declare for inline constructors below.
// Full declaration in parse_path.hpp; definition in
// src/parse_path.cpp.
BOOST_URL_DECL
system::result<segments_encoded_view>
parse_path(core::string_view s) noexcept;

} // urls
} // boost

#include <boost/url/impl/segments_view.hpp>
#include <boost/url/impl/segments_encoded_view.hpp>

//------------------------------------------------
//
// std::ranges::enable_borrowed_range
//
//------------------------------------------------

#ifdef BOOST_URL_HAS_CONCEPTS
#include <ranges>
namespace std::ranges {
    template<>
    inline constexpr bool
        enable_borrowed_range<
            boost::urls::segments_encoded_view> = true;
} // std::ranges
#endif

#endif
