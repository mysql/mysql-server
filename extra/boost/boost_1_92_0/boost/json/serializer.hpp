//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_SERIALIZER_HPP
#define BOOST_JSON_SERIALIZER_HPP

#include <boost/json/detail/config.hpp>
#include <boost/json/detail/format.hpp>
#include <boost/json/detail/stream.hpp>
#include <boost/json/detail/writer.hpp>
#include <boost/json/serialize_options.hpp>
#include <boost/json/value.hpp>

namespace boost {
namespace json {

/** A serializer for JSON.

    This class traverses an instance of a library type and emits serialized
    JSON text by filling in one or more caller-provided buffers. To use,
    declare a variable and call @ref reset with a pointer to the variable you
    want to serialize. Then call @ref read over and over until @ref done
    returns `true`.

    @par Example
    This demonstrates how the serializer may be used to print a JSON value to
    an output stream.

    @code
    void print( std::ostream& os, value const& jv)
    {
        serializer sr;
        sr.reset( &jv );
        while( ! sr.done() )
        {
            char buf[ 4000 ];
            os << sr.read( buf );
        }
    }
    @endcode

    @par Thread Safety
    The same instance may not be accessed concurrently.

    @par Non-Standard JSON
    The @ref serialize_options structure optionally provided upon construction
    is used to enable non-standard JSON extensions. A default-constructed
    `serialize_options` doesn't enable any extensions.

    @see @ref serialize.
*/
class serializer
    : detail::writer
{
    using fn_t = bool (*)(writer&, detail::stream&);

    fn_t fn0_ = nullptr;
    fn_t fn1_ = nullptr;
    bool done_ = false;

public:
    /** Destructor

        All temporary storage is deallocated.

        @par Complexity
        Constant

        @par Exception Safety
        No-throw guarantee.
    */
#ifdef BOOST_JSON_DOCS
    BOOST_JSON_DECL
    ~serializer() noexcept;
#endif // BOOST_JSON_DOCS

    /** Constructors.

        The serializer is constructed with no value to serialize The value may
        be set later by calling @ref reset. If serialization is attempted with
        no value, the output is as if a null value is serialized.

        Overload **(3)** is a move constructor. The type is neither copyable
        nor movable, so this constructor is deleted.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.

        @param opts The options for the serializer. If this parameter is
        omitted, the serializer will output only standard JSON.

        @{
    */
    BOOST_JSON_DECL
    serializer( serialize_options const& opts = {} ) noexcept;

    /** Overload

        @param sp A pointer to the @ref boost::container::pmr::memory_resource
        to use when producing partial output. Shared ownership of the memory
        resource is retained until the serializer is destroyed.

        @param buf An optional static buffer to use for temporary storage when
        producing partial output.

        @param size The number of bytes of valid memory pointed to by
        `buf`.

        @param opts
    */
    BOOST_JSON_DECL
    serializer(
        storage_ptr sp,
        unsigned char* buf = nullptr,
        std::size_t size = 0,
        serialize_options const& opts = {}) noexcept;

    /// Overload
    serializer(serializer&&) = delete;
    /// @}

    /** Check if the serialization is complete.

        This function returns `true` when all of the characters in the
        serialized representation of the value have been read.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.
    */
    bool
    done() const noexcept
    {
        return done_;
    }

    /** Reset the serializer for a new element.

        This function prepares the serializer to emit a new serialized JSON
        representing its argument: `*p` **(1)**--**(5)**, `sv` **(6)**, or
        `np` **(7)**. Ownership is not transferred. The caller is responsible
        for ensuring that the lifetime of the object pointed to by the argument
        extends until it is no longer needed.

        Any memory internally allocated for previous uses of this `serializer`
        object is preserved and re-used for the new output.

        Overload **(5)** uses \<\<direct_conversion,direct serialization\>\>.

        @param p A pointer to the element to serialize.

        @{
    */
    BOOST_JSON_DECL
    void
    reset(value const* p) noexcept;

    BOOST_JSON_DECL
    void
    reset(array const* p) noexcept;

    BOOST_JSON_DECL
    void
    reset(object const* p) noexcept;

    BOOST_JSON_DECL
    void
    reset(string const* p) noexcept;

    template<class T>
    void
    reset(T const* p) noexcept;

    /** Overload

        @param sv The characters representing a string.
    */
    BOOST_JSON_DECL
    void
    reset(string_view sv) noexcept;

    /** Overload

        @param np Represents a null value.
    */
    BOOST_JSON_DECL
    void
    reset(std::nullptr_t np) noexcept;
    /// @}

    /** Read the next buffer of serialized JSON.

        This function attempts to fill the caller provided buffer starting at
        `dest` with up to `size` characters of the serialized JSON that
        represents the value. If the buffer is not large enough, multiple calls
        may be required.

        If serialization completes during this call; that is, that all of the
        characters belonging to the serialized value have been written to
        caller-provided buffers, the function @ref done will return `true`.

        @pre
        @code
        done() == false
        @endcode

        @par Complexity
        @li **(1)** linear in `size`.
        @li **(2)** linear in `N`.

        @par Exception Safety
        Basic guarantee. Calls to `memory_resource::allocate` may throw.

        @return A @ref string_view containing the characters written, which may
        be less than `size` or `N`.

        @param dest A pointer to storage to write into.

        @param size The maximum number of characters to write to the memory
        pointed to by `dest`.

        @{
    */
    BOOST_JSON_DECL
    string_view
    read(char* dest, std::size_t size);

    /** Overload

        @tparam N The size of the array `dest`.
        @param dest
    */
    template<std::size_t N>
    string_view
    read(char(&dest)[N])
    {
        return read(dest, N);
    }
    /// @}

#ifndef BOOST_JSON_DOCS
    // Safety net for accidental buffer overflows
    template<std::size_t N>
    string_view
    read(char(&dest)[N], std::size_t n)
    {
        // If this goes off, check your parameters
        // closely, chances are you passed an array
        // thinking it was a pointer.
        BOOST_ASSERT(n <= N);
        return read(dest, n);
    }
#endif
};

} // namespace json
} // namespace boost

#include <boost/json/impl/serializer.hpp>

#endif
