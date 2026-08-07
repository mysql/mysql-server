//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_PARSER_HPP
#define BOOST_JSON_PARSER_HPP

#include <boost/json/detail/config.hpp>
#include <boost/json/basic_parser.hpp>
#include <boost/json/storage_ptr.hpp>
#include <boost/json/value.hpp>
#include <boost/json/detail/handler.hpp>
#include <type_traits>
#include <cstddef>

namespace boost {
namespace json {

//----------------------------------------------------------

/** A DOM parser for JSON contained in a single buffer.

    This class is used to parse a JSON text contained in a single character
    buffer, into a @ref value container.

    @par Usage
    To use the parser first construct it, then optionally call @ref reset to
    specify a @ref storage_ptr to use for the resulting @ref value. Then call
    @ref write to parse a character buffer containing a complete JSON text. If
    the parse is successful, call @ref release to take ownership of the value:
    @code
    parser p;                                       // construct a parser
    size_t n = p.write( "[1,2,3]" );                // parse a complete JSON text
    assert( n == 7 );                               // all characters consumed
    value jv = p.release();                         // take ownership of the value
    @endcode

    @par Extra Data
    When the character buffer provided as input contains additional data that
    is not part of the complete JSON text, an error is returned. The @ref
    write_some function is an alternative which allows the parse to finish
    early, without consuming all the characters in the buffer. This allows
    parsing of a buffer containing multiple individual JSON texts or containing
    different protocol data:
    @code
    parser p;                                       // construct a parser
    size_t n = p.write_some( "[1,2,3] null" );      // parse a complete JSON text
    assert( n == 8 );                               // only some characters consumed
    value jv = p.release();                         // take ownership of the value
    @endcode

    @par Temporary Storage
    The parser may dynamically allocate temporary storage as needed to
    accommodate the nesting level of the JSON text being parsed. Temporary
    storage is first obtained from an optional, caller-owned buffer specified
    upon construction. When that is exhausted, the next allocation uses the
    @ref boost::container::pmr::memory_resource passed to the constructor; if
    no such argument is specified, the default memory resource is used.
    Temporary storage is freed only when the parser is destroyed; The
    performance of parsing multiple JSON texts may be improved by reusing the
    same parser instance.

    It is important to note that the `boost::container::pmr::memory_resource`
    supplied upon construction is used for temporary storage only, and not for
    allocating the elements which make up the parsed value. That other memory
    resource is optionally supplied in each call to @ref reset.

    @par Duplicate Keys
    If there are object elements with duplicate keys; that is, if multiple
    elements in an object have keys that compare equal, only the last
    equivalent element will be inserted.

    @par Non-Standard JSON
    The @ref parse_options structure optionally provided upon construction is
    used to customize some parameters of the parser, including which
    non-standard JSON extensions should be allowed. A default-constructed parse
    options allows only standard JSON.

    @par Thread Safety
    Distinct instances may be accessed concurrently. Non-const member functions
    of a shared instance may not be called concurrently with any other member
    functions of that instance.

    @see @ref parse, @ref stream_parser.
*/
class parser
{
    basic_parser<detail::handler> p_;

public:
    /** Assignment operator.

        This type is neither copyable nor movable. The operator is deleted.
    */
    parser& operator=(
        parser const&) = delete;

    /** Destructor.

        All dynamically allocated memory, including
        any incomplete parsing results, is freed.

        @par Complexity
        Linear in the size of partial results.

        @par Exception Safety
        No-throw guarantee.
    */
    ~parser() = default;

    /** Constructors.

        Construct a new parser.

        The parser will only support standard JSON if overloads **(1)**
        or **(2)** are used. Otherwise the parser will support extensions
        specified by the parameter `opt`.

        The parsed value will use the \<\<default_memory_resource,default
        memory resource\>\> for storage. To use a different resource, call @ref
        reset after construction.

        The main difference between the overloads is in what the constructed
        parser will use for temporary storage:

        @li **(1)** the constructed parser uses the default memory resource for
        temporary storage.

        @li **(2)**, **(3)** the constructed parser uses the memory resource of
        `sp` for temporary storage.

        @li **(4)**, **(6)** the constructed parser first uses the caller-owned
        storage `[buffer, buffer + size)` for temporary storage, falling back
        to the memory resource of `sp` if needed.

        @li **(5)**, **(7)** the constructed parser first uses the caller-owned
        storage `[buffer, buffer + N)` for temporary storage, falling back to
        the memory resource of `sp` if needed.

        @note Ownership of `buffer` is not transferred. The caller is
        responsible for ensuring the lifetime of the storage pointed to by
        `buffer` extends until the parser is destroyed.

        Overload **(8)** is the copy constructor. The type is neither copyable
        nor movable, so the overload is deleted.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.

        @{
    */
    parser() noexcept
        : parser({}, {})
    {
    }

    /** Overload

        @param sp The memory resource to use for temporary storage.
    */
    explicit
    parser(storage_ptr sp) noexcept
        : parser(std::move(sp), {})
    {
    }

    /** Overload

        @param opt The parsing options to use.
        @param sp
    */
    BOOST_JSON_DECL
    parser(
        storage_ptr sp,
        parse_options const& opt) noexcept;

    /** Overload

        @param buffer A pointer to valid storage.
        @param size The number of valid bytes in `buffer`.
        @param sp
        @param opt
    */
    BOOST_JSON_DECL
    parser(
        storage_ptr sp,
        parse_options const& opt,
        unsigned char* buffer,
        std::size_t size) noexcept;

    /** Overload

        @tparam N The number of valid bytes in `buffer`.
        @param sp
        @param opt
        @param buffer
    */
    template<std::size_t N>
    parser(
        storage_ptr sp,
        parse_options const& opt,
        unsigned char(&buffer)[N]) noexcept
        : parser(std::move(sp),
            opt, &buffer[0], N)
    {
    }

#if defined(__cpp_lib_byte) || defined(BOOST_JSON_DOCS)
    /** Overload

        @param buffer
        @param size
        @param sp
        @param opt
    */
    parser(
        storage_ptr sp,
        parse_options const& opt,
        std::byte* buffer,
        std::size_t size) noexcept
        : parser(sp, opt, reinterpret_cast<
            unsigned char*>(buffer), size)
    {
    }

    /** Overload

        @tparam N
        @param sp
        @param opt
        @param buffer
    */
    template<std::size_t N>
    parser(
        storage_ptr sp,
        parse_options const& opt,
        std::byte(&buffer)[N]) noexcept
        : parser(std::move(sp),
            opt, &buffer[0], N)
    {
    }
#endif

#ifndef BOOST_JSON_DOCS
    // Safety net for accidental buffer overflows
    template<std::size_t N>
    parser(
        storage_ptr sp,
        parse_options const& opt,
        unsigned char(&buffer)[N],
        std::size_t n) noexcept
        : parser(std::move(sp),
            opt, &buffer[0], n)
    {
        // If this goes off, check your parameters
        // closely, chances are you passed an array
        // thinking it was a pointer.
        BOOST_ASSERT(n <= N);
    }

#ifdef __cpp_lib_byte
    // Safety net for accidental buffer overflows
    template<std::size_t N>
    parser(
        storage_ptr sp,
        parse_options const& opt,
        std::byte(&buffer)[N], std::size_t n) noexcept
        : parser(std::move(sp),
            opt, &buffer[0], n)
    {
        // If this goes off, check your parameters
        // closely, chances are you passed an array
        // thinking it was a pointer.
        BOOST_ASSERT(n <= N);
    }
#endif
#endif

    /// Overload
    parser(
        parser const&) = delete;
    /// @}


    /** Reset the parser for a new JSON text.

        This function is used to reset the parser to
        prepare it for parsing a new complete JSON text.
        Any previous partial results are destroyed.

        @par Complexity
        Constant or linear in the size of any previous
        partial parsing results.

        @par Exception Safety
        No-throw guarantee.

        @param sp A pointer to the @ref boost::container::pmr::memory_resource
        to use for the resulting @ref value. The parser will acquire shared
        ownership.
    */
    BOOST_JSON_DECL
    void
    reset(storage_ptr sp = {}) noexcept;

    /** Parse a buffer containing a complete JSON text.

        This function parses a complete JSON text contained in the specified
        character buffer. Additional characters past the end of the complete
        JSON text are ignored. The function returns the actual number of
        characters parsed, which may be less than the size of the input. This
        allows parsing of a buffer containing multiple individual JSON texts or
        containing different protocol data:

        @par Example
        @code
        parser p;                                       // construct a parser
        size_t n = p.write_some( "[1,2,3] null" );      // parse a complete JSON text
        assert( n == 8 );                               // only some characters consumed
        value jv = p.release();                         // take ownership of the value
        @endcode

        Overloads **(1)**, **(2)**, **(4)**, and **(5)** report errors by
        setting `ec`. Overloads **(3)** and **(6)** report errors by throwing
        exceptions.

        @par Complexity
        @li **(1)**--**(3)** linear in `size`.
        @li **(4)**--**(6)** linear in `s.size()`.

        @par Exception Safety
        Basic guarantee. Calls to `memory_resource::allocate` may throw. Upon
        error or exception, subsequent calls will fail until @ref reset is
        called to parse a new JSON text.

        @return The number of characters consumed from the buffer.

        @param data A pointer to a buffer of `size` characters to parse.
        @param size The number of characters pointed to by `data`.
        @param ec Set to the error, if any occurred.

        @{
    */
    BOOST_JSON_DECL
    std::size_t
    write_some(
        char const* data,
        std::size_t size,
        system::error_code& ec);

    BOOST_JSON_DECL
    std::size_t
    write_some(
        char const* data,
        std::size_t size,
        std::error_code& ec);

    /** Overload

        @param data
        @param size
    */
    BOOST_JSON_DECL
    std::size_t
    write_some(
        char const* data,
        std::size_t size);

    /** Overload

        @param s The character string to parse.
        @param ec
    */
    std::size_t
    write_some(
        string_view s,
        system::error_code& ec)
    {
        return write_some(
            s.data(), s.size(), ec);
    }

    /** Overload

        @param s
        @param ec
    */
    std::size_t
    write_some(
        string_view s,
        std::error_code& ec)
    {
        return write_some(
            s.data(), s.size(), ec);
    }

    /** Overload

        @param s
    */
    std::size_t
    write_some(
        string_view s)
    {
        return write_some(
            s.data(), s.size());
    }
    /// @}

    /** Parse a buffer containing a complete JSON text.

        This function parses a complete JSON text contained in the specified
        character buffer. The entire buffer must be consumed; if there are
        additional characters past the end of the complete JSON text, the parse
        fails and an error is returned.

        @par Example
        @code
        parser p;                                       // construct a parser
        size_t n = p.write( "[1,2,3]" );                // parse a complete JSON text
        assert( n == 7 );                               // all characters consumed
        value jv = p.release();                         // take ownership of the value
        @endcode

        Overloads **(1)**, **(2)**, **(4)**, and **(5)** report errors by
        setting `ec`. Overloads **(3)** and **(6)** report errors by throwing
        exceptions.

        @par Complexity
        @li **(1)**--**(3)** linear in `size`.
        @li **(4)**--**(6)** linear in `s.size()`.

        @par Exception Safety
        Basic guarantee. Calls to `memory_resource::allocate` may throw. Upon
        error or exception, subsequent calls will fail until @ref reset is
        called to parse a new JSON text.

        @return The number of characters consumed from the buffer.

        @param data A pointer to a buffer of `size` characters to parse.
        @param size The number of characters pointed to by `data`.
        @param ec Set to the error, if any occurred.

        @{
    */
    BOOST_JSON_DECL
    std::size_t
    write(
        char const* data,
        std::size_t size,
        system::error_code& ec);

    BOOST_JSON_DECL
    std::size_t
    write(
        char const* data,
        std::size_t size,
        std::error_code& ec);

    /** Overload

        @throw `boost::system::system_error` Thrown on error.
    */
    BOOST_JSON_DECL
    std::size_t
    write(
        char const* data,
        std::size_t size);

    /** Overload

        @param s The character string to parse.
        @param ec
    */
    std::size_t
    write(
        string_view s,
        system::error_code& ec)
    {
        return write(
            s.data(), s.size(), ec);
    }

    /** Overload

        @param s
        @param ec
    */
    std::size_t
    write(
        string_view s,
        std::error_code& ec)
    {
        return write(
            s.data(), s.size(), ec);
    }

    /** Overload

        @param s
    */
    std::size_t
    write(
        string_view s)
    {
        return write(
            s.data(), s.size());
    }
    /// @}

    /** Return the parsed JSON text as a @ref value.

        This returns the parsed value, or throws an exception if the parsing is
        incomplete or failed. It is necessary to call @ref reset after calling
        this function in order to parse another JSON text.

        @par Complexity
        Constant.

        @return The parsed value. Ownership of this value is transferred to the
        caller.

        @throw boost::system::system_error A complete JSON text hasn't been
               parsed, or parsing failed.
    */
    BOOST_JSON_DECL
    value
    release();
};

} // namespace json
} // namespace boost

#endif
