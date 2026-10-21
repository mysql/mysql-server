//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2020 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_STRING_HPP
#define BOOST_JSON_STRING_HPP

#include <boost/json/detail/config.hpp>
#include <boost/json/pilfer.hpp>
#include <boost/json/storage_ptr.hpp>
#include <boost/json/string_view.hpp>
#include <boost/json/detail/digest.hpp>
#include <boost/json/detail/except.hpp>
#include <boost/json/detail/string_impl.hpp>
#include <boost/json/detail/value.hpp>
#include <boost/system/result.hpp>
#include <cstring>
#include <iosfwd>
#include <iterator>
#include <new>
#include <type_traits>
#include <utility>

namespace boost {
namespace json {

class value;

/** The native type of string values.

    Instances of string store and manipulate sequences of `char` using the
    UTF-8 encoding. The elements of a string are stored contiguously. A pointer
    to any character in a string may be passed to functions that expect
    a pointer to the first element of a null-terminated `char` array. The type
    uses small buffer optimisation to avoid allocations for small strings.

    String iterators are regular `char` pointers.

    @attention `string` member functions do not validate any UTF-8 byte sequences
    passed to them.

    @par Thread Safety
    Non-const member functions may not be called concurrently with any other
    member functions.

    @par Satisfies
        [_ContiguousContainer_](https://en.cppreference.com/w/cpp/named_req/ContiguousContainer),
        [_ReversibleContainer_](https://en.cppreference.com/w/cpp/named_req/ReversibleContainer),
        and {req_SequenceContainer}.
*/
class string
{
    friend class value;
#ifndef BOOST_JSON_DOCS
    // VFALCO doc toolchain shouldn't show this but does
    friend struct detail::access;
#endif

    using string_impl = detail::string_impl;

    inline
    string(
        detail::key_t const&,
        string_view s,
        storage_ptr sp);

    inline
    string(
        detail::key_t const&,
        string_view s1,
        string_view s2,
        storage_ptr sp);

public:
    /// Associated [Allocator](https://en.cppreference.com/w/cpp/named_req/Allocator)
    using allocator_type = container::pmr::polymorphic_allocator<value>;

    /// The type of a character
    using value_type        = char;

    /// The type used to represent unsigned integers
    using size_type         = std::size_t;

    /// The type used to represent signed integers
    using difference_type   = std::ptrdiff_t;

    /// A pointer to an element
    using pointer           = char*;

    /// A const pointer to an element
    using const_pointer     = char const*;

    /// A reference to an element
    using reference         = char&;

    /// A const reference to an element
    using const_reference   = const char&;

    /// A random access iterator to an element
    using iterator          = char*;

    /// A random access const iterator to an element
    using const_iterator    = char const*;

    /// A reverse random access iterator to an element
    using reverse_iterator =
        std::reverse_iterator<iterator>;

    /// A reverse random access const iterator to an element
    using const_reverse_iterator =
        std::reverse_iterator<const_iterator>;

    /** A special index

        Represents the end of the string.
    */
    static constexpr std::size_t npos =
        string_view::npos;

private:
    template<class T>
    using is_inputit = typename std::enable_if<
        std::is_convertible<typename
            std::iterator_traits<T>::reference,
            char>::value>::type;

    storage_ptr sp_; // must come first
    string_impl impl_;

public:
    /** Destructor.

        Any dynamically allocated internal storage is freed.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.
    */
    ~string() noexcept
    {
        impl_.destroy(sp_);
    }

    //------------------------------------------------------
    //
    // Construction
    //
    //------------------------------------------------------

    /** Constructors.

        Construct a string.

        @li **(1)**, **(2)** the string is empty with a non-zero,
            unspecified capacity.

        @li **(3)** the string is filled with `count` copies of character `ch`.

        @li **(4)** the string will contain a copy of the characters of `s`.

        @li **(5)** the string will contain a copy of the characters of the
            null-terminated string `s`.

        @li **(6)** the string will contain a copy of the characters in the
            range `[s, s + count)`.

        @li **(7)** the string will contain a copy of the characters in the
            range `[first, last)`.

        @li **(8)**, **(9)** the string contains a copy of the characters of
            `other`.

        @li **(10)** the string acquires ownership of the contents of `other`.

        @li **(11)** equivalent to **(10)** if `*sp == *other.storage()`;
            otherwise equivalent to **(9)**.

        @li **(12)** the string is acquires ownership of the contents of
            `other` using pilfer semantics. This is more efficient than move
            construction, when it is known that the moved-from object
            will be immediately destroyed afterwards.

        With **(2)**--**(7)**, **(9)**, **(11)** the constructed string uses
        memory resource of `sp`. With **(8)**, **(10)**, and **(12)** it uses
        `other`'s memory resource. In either case the string will share the
        ownership of the memory resource. With **(1)** it uses the
        \<\<default_memory_resource, default memory resource\>\>.

        After **(10)** `other` behaves as if newly constructed with its
        current storage pointer.

        After **(12)** `other` is not in a usable state and may only be
        destroyed.

        @par Constraints
        `InputIt` satisfies {req_InputIterator}.

        @par Complexity
        @li **(1)**, **(2)**, **(10)**, **(12)** constant.
        @li **(3)** linear in `count`.
        @li **(4)** linear in `s.size()`.
        @li **(5)** linear in `std::strlen(s)`.
        @li **(6)** linear in `count`.
        @li **(7)** linear in `std::distance(first, last)`.
        @li **(8)**, **(9)** linear in `other.size()`.
        @li **(11)** constant if `*sp == *other.storage()`; otherwise linear in
            `other.size()`.

        @par Exception Safety
        @li **(1)**, **(2)**, **(10)**, **(12)** no-throw guarantee.
        @li **(3)**--**(6)**, **(8)**, **(9)**, **(11)**  strong guarantee.
        @li **(7)** strong guarantee if `InputIt` satisfies
        {req_ForwardIterator}, basic guarantee otherwise.

        Calls to `memory_resource::allocate` may throw.

        @throw boost::system::system_error The constructed string's size would
               have exceeded @ref max_size().

        @see @ref pilfer,
            [Valueless Variants Considered Harmful](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0308r0.html).

        @{
    */
    string() = default;

    /** Overload

        @param sp A pointer to the @ref boost::container::pmr::memory_resource
        to use. The container will acquire shared ownership of the memory
        resource.
    */
    explicit
    string(storage_ptr sp)
        : sp_(std::move(sp))
    {
    }

    /** Overload

        @param count The size of the resulting string.
        @param ch The value to initialize characters of the string with.
        @param sp
    */
    BOOST_JSON_DECL
    explicit
    string(
        std::size_t count,
        char ch,
        storage_ptr sp = {});

    /** Overload

        @param s The string to copy from.
        @param sp
    */
    BOOST_JSON_DECL
    string(
        string_view s,
        storage_ptr sp = {});

    /// Overload
    BOOST_JSON_DECL
    string(
        char const* s,
        storage_ptr sp = {});

    /// Overload
    BOOST_JSON_DECL
    explicit
    string(
        char const* s,
        std::size_t count,
        storage_ptr sp = {});

    /** Overload

        @tparam InputIt The type of the iterators.

        @param first An input iterator pointing to the first character to
               insert, or pointing to the end of the range.
        @param last An input iterator pointing to the end of the range.
        @param sp
    */
    template<class InputIt
    #ifndef BOOST_JSON_DOCS
        ,class = is_inputit<InputIt>
    #endif
    >
    explicit
    string(
        InputIt first,
        InputIt last,
        storage_ptr sp = {});

    /** Overload
        @param other The source string.
    */
    BOOST_JSON_DECL
    string(string const& other);

    /// Overload
    BOOST_JSON_DECL
    explicit
    string(
        string const& other,
        storage_ptr sp);

    /// Overload
    string(string&& other) noexcept
        : sp_(other.sp_)
        , impl_(other.impl_)
    {
        ::new(&other.impl_) string_impl();
    }

    /// Overload
    BOOST_JSON_DECL
    explicit
    string(
        string&& other,
        storage_ptr sp);

    /// Overload
    string(pilfered<string> other) noexcept
        : sp_(std::move(other.get().sp_))
        , impl_(other.get().impl_)
    {
        ::new(&other.get().impl_) string_impl();
    }
    /// @}

    //------------------------------------------------------
    //
    // Assignment
    //
    //------------------------------------------------------

    /** Assignment operators.

        @li **(1)**, **(4)** the contents are replaced with an element-wise
            copy of `other`.
        @li **(2)** takes ownership of `other`'s element storage if
            `*storage() == *other.storage()`; otherwise equivalent to **(1)**.
        @li **(3)** the contents are replaced with an element-wise copy of
            null-terminated string `s`.

        After **(2)**, the moved-from array behaves as if newly constructed
        with its current storage pointer.

        @par Complexity
        @li **(1)**, **(4)** linear in `other.size()`.
        @li **(2)** constant if `*storage() == *other.storage()`; otherwise
            linear in `other.size()`.
        @li **(3)** linear in `std::strlen(s)`.

        @par Exception Safety
        {sp} **(2)** provides strong guarantee if
        `*storage() != *other.storage()` and no-throw guarantee otherwise.
        Other overloads provide strong guarantee.
        Calls to `memory_resource::allocate` may throw.

        @param other The string to copy.

        @return `*this`

        @{
    */
    BOOST_JSON_DECL
    string&
    operator=(string const& other);

    BOOST_JSON_DECL
    string&
    operator=(string&& other);

    /** Overload

        @param s The null-terminated character string.

        @throw boost::system::system_error `std::strlen(s) >` @ref max_size().
    */
    BOOST_JSON_DECL
    string&
    operator=(char const* s);

    /** Overload

        @throw `boost::system::system_error` `other.size() >` @ref max_size().
    */
    BOOST_JSON_DECL
    string&
    operator=(string_view other);
    /// @}

    /** Assign characters to a string.

        @li **(1)** replaces the contents with `count` copies of character
        `ch`.

        @li **(2)** replaces the contents with copies of the characters in the
        range `[s, s + count)`. This range can contain null characters.

        @li **(3)** replaces the contents with those of the null terminated
        string `s`. The length of the string is determined by the first null
        character.

        @li **(4)** replaces the contents with copies of characters in the
        range `[first, last)`.

        @li **(5)** Replaces the contents with those of string view `s`. This
        view can contain null characters.

        @li **(6)** replaces the contents with a copy of the characters of
        `other`.

        @li **(7)** if `*storage() == *other.storage()` takes ownership of the
        element storage of `other`; otherwise equivalent to **(6)**.

        Self-assignment using **(7)** does nothing.

        After **(7)** `other` is left in valid but unspecified state.

        @par Constraints
        `InputIt` satisfies {req_InputIterator}.

        @par Complexity
        @li **(1)**, **(2)** linear in `count`.
        @li **(3)** linear in `std::strlen(s)`.
        @li **(4)** linear in `std::distance(first, last)`.
        @li **(5)** linear in `s.size()`.
        @li **(6)** linear in `other.size()`.
        @li **(7)** constant if `*storage() == *other.storage()`, otherwise
            linear in `other.size()`.

        @par Exception Safety
        {sp} **(7)** provides strong guarantee if
        `*storage() != *other.storage()` and no-throw guarantee otherwise.
        Other overloads provide strong guarantee. Calls to
        `memory_resource::allocate` may throw.

        @return `*this`.

        @param count The number of the characters to use.

        @param ch The character to fill the string with.

        @throw boost::system::system_error The size of the string after the
        operation would exceed @ref max_size().

        @{
    */
    BOOST_JSON_DECL
    string&
    assign(
        std::size_t count,
        char ch);

    /** Overload
        @param s A pointer to a character string used to copy from.
        @param count
    */
    BOOST_JSON_DECL
    string&
    assign(
        char const* s,
        std::size_t count);

    /** Overload
        @param s
    */
    BOOST_JSON_DECL
    string&
    assign(
        char const* s);

    /** Overload

        @tparam InputIt The type of the iterators.

        @param first An input iterator pointing to the first character to
        insert, or pointing to the end of the range.
        @param last An input iterator pointing to the end of the range.
    */
    template<class InputIt
    #ifndef BOOST_JSON_DOCS
        ,class = is_inputit<InputIt>
    #endif
    >
    string&
    assign(
        InputIt first,
        InputIt last);

    /** Overload
        @param s The string view to copy from.
    */
    string&
    assign(string_view s)
    {
        return assign(s.data(), s.size());
    }

    /** Overload
        @param other Another string.
    */
    BOOST_JSON_DECL
    string&
    assign(
        string const& other);

    /** Overload
        @param other
    */
    BOOST_JSON_DECL
    string&
    assign(string&& other);
    /// @}

    /** Return the associated memory resource.

        This function returns a smart pointer to the
        @ref boost::container::pmr::memory_resource used by the container.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.
    */
    storage_ptr const&
    storage() const noexcept
    {
        return sp_;
    }

    /** Return the associated allocator.

        This function returns an instance of @ref allocator_type constructed
        from the associated @ref boost::container::pmr::memory_resource.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.
    */
    allocator_type
    get_allocator() const noexcept
    {
        return sp_.get();
    }

    //------------------------------------------------------
    //
    // Element Access
    //
    //------------------------------------------------------

    /** Return a character with bounds checking.

        Returns @ref boost::system::result containing a reference to the
        character specified at location `pos`, if `pos` is within the range of
        the string. Otherwise the result contains an `error_code`.

        @par Exception Safety
        Strong guarantee.

        @param pos A zero-based index to access.

        @par Complexity
        Constant.

        @{
    */
    BOOST_JSON_DECL
    system::result<char&>
    try_at(std::size_t pos) noexcept;

    BOOST_JSON_DECL
    system::result<char const&>
    try_at(std::size_t pos) const noexcept;
    /// @}

    /** Return a character with bounds checking.

        Returns a reference to the character specified at location `pos`.

        @par Complexity
        Constant.

        @par Exception Safety
        Strong guarantee.

        @param pos A zero-based index to access.
        @param loc `source_location` to use in thrown exception; the source
               location of the call site by default.

        @throw boost::system::system_error `pos >=` @ref size().

        @{
    */
    inline
    char&
    at(
        std::size_t pos,
        source_location const& loc = BOOST_CURRENT_LOCATION);

    BOOST_JSON_DECL
    char const&
    at(
        std::size_t pos,
        source_location const& loc = BOOST_CURRENT_LOCATION) const;
    /// @}

    /** Return a character without bounds checking.

        Returns a reference to the character specified at location `pos`.

        @par Complexity
        Constant.

        @pre
        @code
        pos < size()
        @endcode

        @param pos A zero-based index to access.

        @{
    */
    char&
    operator[](std::size_t pos)
    {
        return impl_.data()[pos];
    }

    const char&
    operator[](std::size_t pos) const
    {
        return impl_.data()[pos];
    }
    /// @}

    /** Return the first character.

        Returns a reference to the first character.

        @pre
        @code
        ! empty()
        @endcode

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.

        @{
    */
    char&
    front()
    {
        return impl_.data()[0];
    }

    char const&
    front() const
    {
        return impl_.data()[0];
    }
    /// @}

    /** Return the last character.

        Returns a reference to the last character.

        @pre
        @code
        ! empty()
        @endcode

        @par Complexity
        Constant.

        @{
    */
    char&
    back()
    {
        return impl_.data()[impl_.size() - 1];
    }

    char const&
    back() const
    {
        return impl_.data()[impl_.size() - 1];
    }
    /// @}

    /** Return the underlying character array directly.

        Returns a pointer to the underlying array serving as storage. The value
        returned is such that the range `[data(), data() + size())` is always
        a valid range, even if the container is empty.

        @note The value returned from this function is never equal to
        `nullptr`.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.

        @{
    */
    char*
    data() noexcept
    {
        return impl_.data();
    }

    char const*
    data() const noexcept
    {
        return impl_.data();
    }
    /// @@}

    /** Return the underlying character array directly.

        Returns a pointer to the underlying array serving as storage. The value
        returned is such that the range `[c_str(), c_str() + size())` is always
        a valid range, even if the container is empty.

        @note The value returned from this function is never equal to
        `nullptr`.

        @par Complexity
        Constant.
    */
    char const*
    c_str() const noexcept
    {
        return impl_.data();
    }

    /** Convert to a @ref string_view referring to the string.

        Returns a string view to the
        underlying character string. The size of the view
        does not include the null terminator.

        @par Complexity

        Constant.
    */
    operator string_view() const noexcept
    {
        return {data(), size()};
    }

#if ! defined(BOOST_NO_CXX17_HDR_STRING_VIEW)
    /** Convert to @ref std::string_view referring to the string.

        Returns a string view to the underlying character string. The size of
        the view does not include the null terminator.

        This overload is not defined when `BOOST_NO_CXX17_HDR_STRING_VIEW` is
        defined.

        @par Complexity

        Constant.
    */
    operator std::string_view() const noexcept
    {
        return {data(), size()};
    }
#endif

    //------------------------------------------------------
    //
    // Iterators
    //
    //------------------------------------------------------

    /** Return an iterator to the beginning.

        If the container is empty, @ref end() is returned.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.

        @{
    */
    iterator
    begin() noexcept
    {
        return impl_.data();
    }

    const_iterator
    begin() const noexcept
    {
        return impl_.data();
    }
    /// @}

    /** Return a const iterator to the first element.

        If the container is empty, @ref cend() is returned.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.
    */
    const_iterator
    cbegin() const noexcept
    {
        return impl_.data();
    }

    /** Return an iterator to the end.

        The returned iterator only acts as a sentinel. Dereferencing it results
        in undefined behavior.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.

        @{
    */
    iterator
    end() noexcept
    {
        return impl_.end();
    }

    const_iterator
    end() const noexcept
    {
        return impl_.end();
    }
    /// @}

    /** Return a const iterator past the last element.

        The returned iterator only acts as a sentinel. Dereferencing it results
        in undefined behavior.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.
    */
    const_iterator
    cend() const noexcept
    {
        return impl_.end();
    }

    /** Return a reverse iterator to the first character of the reversed container.

        Returns the pointed-to character that corresponds to the last character
        of the non-reversed container. If the container is empty, @ref rend()
        is returned.

        @par Complexity
        Constant.

        @{
    */
    reverse_iterator
    rbegin() noexcept
    {
        return reverse_iterator(impl_.end());
    }

    const_reverse_iterator
    rbegin() const noexcept
    {
        return const_reverse_iterator(impl_.end());
    }
    /// @}

    /** Return a const reverse iterator to the first element of the reversed container.

        Returns the pointed-to character that corresponds to the last character
        of the non-reversed container. If the container is empty, @ref crend()
        is returned.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.
    */
    const_reverse_iterator
    crbegin() const noexcept
    {
        return const_reverse_iterator(impl_.end());
    }

    /** Return a reverse iterator to the character following the last character of the reversed container.

        The pointed-to element corresponds to the element preceding the first
        element of the non-reversed container. The returned iterator only acts
        as a sentinel. Dereferencing it results in undefined behavior.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.

        @{
    */
    reverse_iterator
    rend() noexcept
    {
        return reverse_iterator(begin());
    }

    const_reverse_iterator
    rend() const noexcept
    {
        return const_reverse_iterator(begin());
    }
    /// @}

    /** Return a const reverse iterator to the character following the last character of the reversed container.

        The pointed-to character corresponds to the character preceding the
        first character of the non-reversed container. The returned iterator
        only acts as a sentinel. Dereferencing it results in undefined
        behavior.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.
    */
    const_reverse_iterator
    crend() const noexcept
    {
        return const_reverse_iterator(begin());
    }

    //------------------------------------------------------
    //
    // Capacity
    //
    //------------------------------------------------------

    /** Check if the string has no characters.

        Returns `true` if there are no characters in the string, i.e. @ref
        size() returns 0.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.
    */
    bool
    empty() const noexcept
    {
        return impl_.size() == 0;
    }

    /** Return the number of characters in the string.

        The value returned does not include the null terminator, which is
        always present.

        @par Complexity
        Constant.
    */
    std::size_t
    size() const noexcept
    {
        return impl_.size();
    }

    /** Return the maximum number of characters any string can hold.

        The maximum is an implementation-defined number. This value is
        a theoretical limit; at runtime, the actual maximum size may be less
        due to resource limits.

        @par Complexity
        Constant.
    */
    static
    constexpr
    std::size_t
    max_size() noexcept
    {
        return string_impl::max_size();
    }

    /** Return the number of characters that can be held in currently allocated memory.

        Returns the number of characters that the container has currently
        allocated space for. This number is never smaller than the value
        returned by @ref size().

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.
    */
    std::size_t
    capacity() const noexcept
    {
        return impl_.capacity();
    }

    /** Increase the capacity to at least a certain amount.

        This increases the capacity of the array to a value that is greater
        than or equal to `new_capacity`. If `new_capacity > `@ref capacity(),
        new memory is allocated. Otherwise, the call has no effect. The number
        of elements and therefore the @ref size() of the container is not
        changed.

        If new memory is allocated, all iterators including any past-the-end
        iterators, and all references to the elements are invalidated.
        Otherwise, no iterators or references are invalidated.

        @par Complexity
        At most, linear in @ref size().

        @par Exception Safety
        Strong guarantee. Calls to `memory_resource::allocate` may throw.

        @param new_capacity The new capacity of the array.

        @throw boost::system::system_error `new_capacity > `@ref max_size().
    */
    void
    reserve(std::size_t new_capacity)
    {
        if(new_capacity <= capacity())
            return;
        reserve_impl(new_capacity);
    }

    /** Request the removal of unused capacity.

        This performs a non-binding request to reduce @ref capacity() to
        @ref size(). The request may or may not be fulfilled.

        @note If reallocation occurs, all iterators including  any past-the-end
        iterators, and all references to characters are invalidated. Otherwise,
        no iterators or references are invalidated.

        @par Complexity
        At most, linear in @ref size().
    */
    BOOST_JSON_DECL
    void
    shrink_to_fit();

    //------------------------------------------------------
    //
    // Operations
    //
    //------------------------------------------------------

    /** Clear the contents.

        Erases all characters from the string. After this call, @ref size()
        returns zero but @ref capacity() is unchanged. All references,
        pointers, or iterators referring to contained elements are invalidated.
        Any past-the-end iterators are also invalidated.

        @par Complexity
        Linear in @ref size().

        @par Exception Safety
        No-throw guarantee.
    */
    BOOST_JSON_DECL
    void
    clear() noexcept;

    /** Insert characters at the specified index.

        @li **(1)** inserts `sv`.
        @li **(2)** inserts `count` copies of `ch`.
        @li **(3)** inserts the character `ch`.
        @li **(4)** inserts characters from the range `[first, last)`.

        The first character is inserted at the index `pos`. All references,
        pointers, or iterators referring to contained elements are invalidated.
        Any past-the-end iterators are also invalidated.

        @par Constraints
        `InputIt` satisfies {req_InputIterator}.

        @pre
        `[first, last)` is a valid range.

        @par Exception Safety
        @li **(1)**--*(3)* strong guarantee.
        @li **(4)** strong guarantee if `InputIt` satisfies
            {req_ForwardIterator}, basic guarantee otherwise.

        @return `*this`

        @param pos The index to insert at.
        @param sv The `string_view` to insert.

        @throw boost::system::system_error The size of the string would exceed
               @ref max_size().

        @throw boost::system::system_error `pos > `@ref size().

        @{
    */
    BOOST_JSON_DECL
    string&
    insert(
        std::size_t pos,
        string_view sv);

    /** Overload
        @param count The number of characters to insert.
        @param ch The character to insert.
        @param pos
    */
    BOOST_JSON_DECL
    string&
    insert(
        std::size_t pos,
        std::size_t count,
        char ch);

    /** Overload
        @param pos
        @param ch
    */
    string&
    insert(
        size_type pos,
        char ch)
    {
        return insert(pos, 1, ch);
    }

    /** Overload

        @tparam InputIt The type of the iterators.

        @param first The beginning of the character range.
        @param last The end of the character range.
        @param pos
    */
    template<class InputIt
    #ifndef BOOST_JSON_DOCS
        ,class = is_inputit<InputIt>
    #endif
    >
    string&
    insert(
        size_type pos,
        InputIt first,
        InputIt last);
    /// @}

    /** Remove characters from the string.

        @li **(1)** removes at most `count` but not more than `size() - pos`
            characters starting at `index`.
        @li **(2)** removes the character at `pos`.
        @li **(3)** removes characters in the range `[first, last)`.

        All references, pointers, or iterators referring to contained elements
        are invalidated. Any past-the-end iterators are also invalidated.

        @pre
        `pos`, `first`, and `last` are iterators into this string. `first` and
        `last` form a valid range.

        @par Complexity
        @li **(1)** linear in `count`.
        @li **(2)** constant.
        @li **(3)** linear in `std::distance(first, last)`.

        @par Exception Safety
        Strong guarantee.

        @return
        @li **(1)** `*this`.

        @li **(2)** An iterator referring to the character immediately
        following the removed character, or @ref end() if one does not exist.

        @li **(3)** An iterator referring to the character `last` previously
        referred to, or @ref end() if one does not exist.

        @param index The index of the first character to remove.

        @param count The number of characters to remove. By default remove
        until the end of the string.

        @throw boost::system::system_error `pos >` @ref size().

        @{
    */
    BOOST_JSON_DECL
    string&
    erase(
        std::size_t index = 0,
        std::size_t count = npos);

    /** Overload
        @param pos An iterator referring to the character to erase.
    */
    BOOST_JSON_DECL
    iterator
    erase(const_iterator pos);

    /** Overload
        @param first An iterator representing the first character to erase.
        @param last An iterator one past the last character to erase.
    */
    BOOST_JSON_DECL
    iterator
    erase(
        const_iterator first,
        const_iterator last);
    /// @}

    //------------------------------------------------------

    /** Append a character.

        Appends a character to the end of the string.

        @par Exception Safety
        Strong guarantee.

        @param ch The character to append.

        @throw boost::system::system_error @ref size() `+ 1 > `@ref max_size().
    */
    BOOST_JSON_DECL
    void
    push_back(char ch);

    /** Remove the last character.

        Removes a character from the end of the string.

        @pre
        @code
        ! empty()
        @endcode
    */
    BOOST_JSON_DECL
    void
    pop_back();

    //------------------------------------------------------

    /** Append characters to the string.

        @li **(1)** appends `count` copies of `ch`.

        @li **(2)** appends copies of characters of `sv`, preserving order.

        @li **(3)** appends characters from the range `[first, last)`,
        preserving order.

        @pre
        `[first, last)` shall be a valid range.

        @par Constraints
        `InputIt` satisfies {req_InputIterator}.

        @par Exception Safety
        Strong guarantee.

        @return `*this`.

        @param count The number of characters to append.
        @param ch The character to append.

        @throw boost::system::system_error The size of the string after the
        operation would exceed @ref max_size().

        @{
    */
    BOOST_JSON_DECL
    string&
    append(
        std::size_t count,
        char ch);

    /** Overload
        @param sv The `string_view` to append.
    */
    BOOST_JSON_DECL
    string&
    append(string_view sv);

    /** Overload

        @tparam InputIt The type of the iterators.

        @param first An iterator representing the first character to append.
        @param last An iterator one past the last character to append.
    */
    template<class InputIt
    #ifndef BOOST_JSON_DOCS
        ,class = is_inputit<InputIt>
    #endif
    >
    string&
    append(InputIt first, InputIt last);
    /// @}

    /** Append characters to the string.

        @li **(1)** appends `[sv.begin(), sv.end())`.
        @li **(2)** appends `ch`.

        @par Exception Safety
        Strong guarantee.

        @return `*this`

        @param sv The `string_view` to append.

        @throw boost::system::system_error The size of the string after the
        operation would exceed @ref max_size().

        @{
    */
    string&
    operator+=(string_view sv)
    {
        return append(sv);
    }

    /** Overload
        @param ch The character to append.
    */
    string&
    operator+=(char ch)
    {
        push_back(ch);
        return *this;
    }
    /// @}

    //------------------------------------------------------

    /** Compare a string with the string.

        Let `comp` be `std::char_traits<char>::compare(data(), sv.data(),
        std::min(size(), sv.size())`. If `comp != 0`, then the result is
        `comp`. Otherwise, the result is `0` if `size() == sv.size()`, `-1` if
        `size() < sv.size()`, and `1` otherwise.

        @par Complexity
        Linear.

        @return The result of lexicographically comparing the characters of
        `sv` and the string.

        @param sv The `string_view` to compare.
    */
    int
    compare(string_view sv) const noexcept
    {
        return subview().compare(sv);
    }

    //------------------------------------------------------

    /** Return whether the string begins with another string.

        @li **(1)** checks if the string begins with `s`.
        @li **(2)** checks if the string begins with `ch`.

        @par Complexity
        @li **(1)** linear in `s.size()`.
        @li **(2)** constant.

        @param s The string to check for.

        @{
    */
    bool
    starts_with(string_view s) const noexcept
    {
        return subview(0, s.size()) == s;
    }

    /** Overload

        @param ch The character to check for.
    */
    bool
    starts_with(char ch) const noexcept
    {
        return ! empty() && front() == ch;
    }
    /// @}

    /** Check if the string ends with given suffix.

        @li **(1)** returns `true` if the string ends with `s`.
        @li **(2)** returns `true` if the string ends with the character `ch`.

        @par Complexity
        @li **(1)** linear in `s`.
        @li **(2)** constant.

        @par Exception Safety
        No-throw guarantee.

        @param s The string to check for.

        @{
    */
    bool
    ends_with(string_view s) const noexcept
    {
        return size() >= s.size() &&
            subview(size() - s.size()) == s;
    }

    /** Overload
        @param ch The character to check for.
    */
    bool
    ends_with(char ch) const noexcept
    {
        return ! empty() && back() == ch;
    }
    /// @}

    /** Replace a substring with another string.

        @li **(1)** replaces `std::min(count, size() - pos)` characters
            starting at index `pos` with those of `sv`.
        @li **(2)** replaces the characters in the range `[first, last)` with
            those of `sv`.
        @li **(3)** replaces the characters in the range `[first, last)` with
            those of `[first2, last2)`.
        @li **(4)** replaces `std::min(count, size() - pos)` characters
            starting at index `pos` with `count2` copies of `ch`.
        @li **(5)** replaces the characters in the range `[first, last)` with
            `count2` copies of `ch`.

        All references, pointers, or iterators referring to contained elements
        are invalidated. Any past-the-end iterators are also invalidated.

        @pre
        `[first, last)` is a valid range. `[first2, last2)` is a valid range.

        @par Constraints
        `InputIt` satisfies {req_InputIterator}.

        @par Exception Safety
        Strong guarantee.

        @return `*this`

        @param pos The index to replace at.

        @param count The number of characters to replace.

        @param sv The `string_view` to replace with.

        @throw boost::system::system_error The resulting string's size would
               have exceeded @ref max_size().

        @{
    */
    BOOST_JSON_DECL
    string&
    replace(
        std::size_t pos,
        std::size_t count,
        string_view sv);

    /** Overload

        @param first An iterator referring to the first character to replace.
        @param last An iterator one past the end of the last character to
               replace.
        @param sv
    */
    string&
    replace(
        const_iterator first,
        const_iterator last,
        string_view sv)
    {
        return replace(first - begin(), last - first, sv);
    }

    /** Overload

        @tparam InputIt The type of the iterators.

        @param first2 An iterator referring to the first character to replace
               with.
        @param last2 An iterator one past the end of the last character to
               replace with.
        @param first
        @param last
    */
    template<class InputIt
    #ifndef BOOST_JSON_DOCS
        ,class = is_inputit<InputIt>
    #endif
    >
    string&
    replace(
        const_iterator first,
        const_iterator last,
        InputIt first2,
        InputIt last2);

    /** Overload

        @param count2 The number of characters to replace with.
        @param ch The character to replace with.
        @param pos
        @param count
    */
    BOOST_JSON_DECL
    string&
    replace(
        std::size_t pos,
        std::size_t count,
        std::size_t count2,
        char ch);

    /** Overload

        @param first
        @param last
        @param count2
        @param ch
    */
    string&
    replace(
        const_iterator first,
        const_iterator last,
        std::size_t count2,
        char ch)
    {
        return replace(first - begin(), last - first, count2, ch);
    }
    /// @}

    //------------------------------------------------------

    /** Return a view.

        @li **(1)** equivalent to `subview().substr(pos, count)`.
        @li **(2)** equivalent to `string_view(data(), size())`.

        @par Exception Safety
        Strong guarantee.

        @param pos The index of the first character of the substring.
        @param count The length of the substring.

        @throw boost::system::system_error `pos > ` @ref size().
    */
    string_view
    subview(
        std::size_t pos,
        std::size_t count = npos) const
    {
        return subview().substr(pos, count);
    }

    /// Overload
    string_view
    subview() const noexcept
    {
        return string_view( data(), size() );
    }

    //------------------------------------------------------

    /** Copy a substring to another string.

        Copies `std::min(count, size() - pos)` characters starting at index
        `pos` to the string pointed to by `dest`.

        @attention This function doesn't put the null terminator after the
        copied characters.

        @return The number of characters copied.

        @param count The number of characters to copy.

        @param dest The string to copy to.

        @param pos The index to begin copying from.

        @throw boost::system::system_error `pos >` @ref max_size().
    */
    std::size_t
    copy(
        char* dest,
        std::size_t count,
        std::size_t pos = 0) const
    {
        return subview().copy(dest, count, pos);
    }

    //------------------------------------------------------

    /** Change the size of the string.

        Resizes the string to contain `count` characters. If
        `count > `@ref size(), **(2)** appends copies of `ch` and **(1)**
        appends ``'\0'``. Otherwise, `size()` is reduced to `count`.

        @param count The size to resize the string to.

        @throw boost::system::system_error `count > `@ref max_size().

        @{
    */
    void
    resize(std::size_t count)
    {
        resize(count, 0);
    }

    /** Overload

        @param count
        @param ch The characters to append if the size increases.
    */
    BOOST_JSON_DECL
    void
    resize(std::size_t count, char ch);
    /// @}

    /** Increase size without changing capacity.

        This increases the size of the string by `n` characters, adjusting the
        position of the terminating null character for the new size. The new
        characters remain uninitialized. This function may be used to append
        characters directly into the storage between @ref end() and @ref data()
        ` + ` @ref capacity().

        @pre
        @code
        count <= capacity() - size()
        @endcode

        @param n The amount to increase the size by.
    */
    void
    grow(std::size_t n) noexcept
    {
        BOOST_ASSERT(
            n <= impl_.capacity() - impl_.size());
        impl_.term(impl_.size() + n);
    }

    /** Swap the contents.

        Exchanges the contents of this string with another string. Ownership of
        the respective @ref boost::container::pmr::memory_resource objects is
        not transferred.

        @li If `&other == this`, do nothing. Otherwise,
        @li if `*other.storage() == *this->storage()`, ownership of the
            underlying memory is swapped in constant time, with no possibility
            of exceptions. All iterators and references remain valid.
            Otherwise,
        @li the contents are logically swapped by making copies, which can
            throw. In this case all iterators and references are invalidated.

        @par Complexity
        Constant or linear in @ref size() `+ other.size()`.

        @par Exception Safety
        Strong guarantee. Calls to `memory_resource::allocate` may throw.
    */
    BOOST_JSON_DECL
    void
    swap(string& other);

    /** Exchange the given values.

        Exchanges the contents of the string `lhs` with another string `rhs`.
        Ownership of the respective @ref boost::container::pmr::memory_resource
        objects is not transferred.

        @li If `&lhs == &rhs`, do nothing. Otherwise,
        @li if `*lhs.storage() == *rhs.storage()`, ownership of the underlying
            memory is swapped in constant time, with no possibility of
            exceptions. All iterators and references remain valid. Otherwise,
        @li the contents are logically swapped by making a copy, which can
            throw. In this case all iterators and references are invalidated.

        @par Effects
        @code
        lhs.swap( rhs );
        @endcode

        @par Complexity
        Constant or linear in `lhs.size() + rhs.size()`.

        @par Exception Safety
        Strong guarantee.
        Calls to `memory_resource::allocate` may throw.

        @param lhs The string to exchange.
        @param rhs The string to exchange.

        @see @ref string::swap
    */
    friend
    void
    swap(string& lhs, string& rhs)
    {
        lhs.swap(rhs);
    }
    //------------------------------------------------------
    //
    // Search
    //
    //------------------------------------------------------

    /** Find the first occurrence of characters within the string.

        Search from `pos` onward for the first substring that is equal to the
        first argument.

        @li **(1)** searches for the presense of the substring equal to `sv`.
        @li **(2)** searches for the presense of the substring consisting of
            the character `ch`.

        @par Complexity
        Linear in @ref size().

        @par Exception Safety
        No-throw guarantee.

        @return The index of the first character of the found substring, or
        @ref npos if none was found.

        @param sv The `string_view` to search for.
        @param pos The index to start searching at.

        @{
    */
    std::size_t
    find(
        string_view sv,
        std::size_t pos = 0) const noexcept
    {
        return subview().find(sv, pos);
    }

    /** Overload

        @param ch The character to search for.
        @param pos
    */
    std::size_t
    find(
        char ch,
        std::size_t pos = 0) const noexcept
    {
        return subview().find(ch, pos);
    }
    /// @}

    /** Find the last occurrence of a string within the string.

        @li **(1)** searches for the last substring equal to `sv`.
        @li **(2)** searches for the last occurrence of `ch`.

        Both functions search for substrings fully contained within `[begin(),
        begin() + pos)`.

        @par Complexity
        Linear.

        @return Index of the first character of the found substring or
                @ref npos if none was found.

        @param sv The string to search for.
        @param pos The index to start searching at. By default searches from
               the end of the string.

        @{
    */
    std::size_t
    rfind(
        string_view sv,
        std::size_t pos = npos) const noexcept
    {
        return subview().rfind(sv, pos);
    }

    /** Overload

        @param ch The character to search for.
        @param pos
    */
    std::size_t
    rfind(
        char ch,
        std::size_t pos = npos) const noexcept
    {
        return subview().rfind(ch, pos);
    }
    /// @}

    //------------------------------------------------------

    /** Find the first character present in the specified string.

        Search from `pos` onward for the first character in this string that is
        equal to any of the characters of `sv`.

        @par Complexity
        Linear in @ref size() `+ sv.size()`.

        @par Exception Safety
        No-throw guarantee.

        @return The index of the found character, or @ref npos if none exists.

        @param sv The characters to search for.
        @param pos The index to start searching at.
    */
    std::size_t
    find_first_of(
        string_view sv,
        std::size_t pos = 0) const noexcept
    {
        return subview().find_first_of(sv, pos);
    }

    /** Find the first character missing from the specified string.

        Search from `pos` onward for the first character in this string that is
        not equal to any of the characters in the string provided as the first
        argument.

        @li **(1)** compares with the characters in `sv`.
        @li **(2)** compares with the character `ch`.

        @par Complexity
        @li **(1)** linear in @ref size() `+ sv.size()`.
        @li **(2)** linear in @ref size().

        @par Exception Safety
        No-throw guarantee.

        @return The index of the found character, or @ref npos if none exists.

        @param sv The characters to compare with.
        @param pos The index to start searching at.

        @{
    */
    std::size_t
    find_first_not_of(
        string_view sv,
        std::size_t pos = 0) const noexcept
    {
        return subview().find_first_not_of(sv, pos);
    }

    /** Overload
        @param ch The character to compare with.
        @param pos
    */
    std::size_t
    find_first_not_of(
        char ch,
        std::size_t pos = 0) const noexcept
    {
        return subview().find_first_not_of(ch, pos);
    }
    /// @}

    /** Find the last character present in the specified string.

        Search from `pos` backwards for the first character in this string that
        is equal to any of the characters of `sv`. If `pos` is equal to @ref
        npos (the default), search from the last character.

        @par Complexity
        Linear in @ref size() `+ sv.size()`.

        @par Exception Safety
        No-throw guarantee.

        @return The index of the found character, or @ref npos if none exists.

        @param sv The characters to search for.
        @param pos The index to start searching at.
    */
    std::size_t
    find_last_of(
        string_view sv,
        std::size_t pos = npos) const noexcept
    {
        return subview().find_last_of(sv, pos);
    }

    /** Find the last character missing from the specified string.


        Search from `pos` backwards for the first character in this string that
        is not equal to any of the characters in the string provided as the
        first argument. If `pos` is equal to @ref npos (the default), search
        from the last character.

        @li **(1)** compares with the characters in `sv`.
        @li **(2)** compares with the character `ch`.

        @par Complexity
        @li **(1)** linear in @ref size() `+ sv.size()`.
        @li **(2)** linear in @ref size().

        @par Exception Safety
        No-throw guarantee.

        @return The index of the found character, or @ref npos if none exists.

        @param sv The characters to compare with.
        @param pos The index to start searching at.

        @{
    */
    std::size_t
    find_last_not_of(
        string_view sv,
        std::size_t pos = npos) const noexcept
    {
        return subview().find_last_not_of(sv, pos);
    }

    /** Overload
        @param ch The character to compare with.
        @param pos
    */
    std::size_t
    find_last_not_of(
        char ch,
        std::size_t pos = npos) const noexcept
    {
        return subview().find_last_not_of(ch, pos);
    }
    /// @}

    /** Serialize a @ref string to an output stream.

        This function serializes a `string` as JSON into the output stream.

        @return Reference to `os`.

        @par Complexity
        Linear in the `str.size()`.

        @par Exception Safety
        Strong guarantee. Calls to `memory_resource::allocate` may throw.

        @param os The output stream to serialize to.
        @param str The value to serialize.
    */
    BOOST_JSON_DECL
    friend
    std::ostream&
    operator<<(
        std::ostream& os,
        string const& str);

private:
    class undo;

    template<class It>
    using iter_cat = typename
        std::iterator_traits<It>::iterator_category;

    template<class InputIt>
    void
    assign(InputIt first, InputIt last,
        std::random_access_iterator_tag);

    template<class InputIt>
    void
    assign(InputIt first, InputIt last,
        std::input_iterator_tag);

    template<class InputIt>
    void
    append(InputIt first, InputIt last,
        std::random_access_iterator_tag);

    template<class InputIt>
    void
    append(InputIt first, InputIt last,
        std::input_iterator_tag);

    BOOST_JSON_DECL
    void
    reserve_impl(std::size_t new_capacity);
};

//----------------------------------------------------------

namespace detail
{

template <>
inline
string_view
to_string_view<string>(string const& s) noexcept
{
    return s.subview();
}

} // namespace detail


/** Checks if lhs equals rhs.

    @li **(1)** A lexicographical comparison is used.
    @li **(2)** equivalent to `lhs.get() == rhs.get()`.

    @par Complexity
    @li **(1)** linear in `lhs.size() + rhs.size()`.
    @li **(2)** constant.

    @par Exception Safety
    No-throw guarantee.
*/
#ifdef BOOST_JSON_DOCS
bool
operator==(string const& lhs, string const& rhs) noexcept
#else
template<class T, class U>
detail::string_comp_op_requirement<T, U>
operator==(T const& lhs, U const& rhs) noexcept
#endif
{
    return detail::to_string_view(lhs) == detail::to_string_view(rhs);
}

/** Checks if lhs does not equal rhs.

    @li **(1)** A lexicographical comparison is used.
    @li **(2)** equivalent to `lhs.get() != rhs.get()`.

    @par Complexity
    @li **(1)** linear in `lhs.size() + rhs.size()`.
    @li **(2)** constant.

    @par Exception Safety
    No-throw guarantee.
*/
#ifdef BOOST_JSON_DOCS
bool
operator!=(string const& lhs, string const& rhs) noexcept
#else
template<class T, class U>
detail::string_comp_op_requirement<T, U>
operator!=(T const& lhs, U const& rhs) noexcept
#endif
{
    return detail::to_string_view(lhs) != detail::to_string_view(rhs);
}

/** Check if lhs is less than rhs.

    A lexicographical comparison is used.

    @par Complexity
    Linear in `lhs.size() + rhs.size()`.

    @par Exception Safety
    No-throw guarantee.
*/
#ifdef BOOST_JSON_DOCS
bool
operator<(string const& lhs, string const& rhs) noexcept
#else
template<class T, class U>
detail::string_comp_op_requirement<T, U>
operator<(T const& lhs, U const& rhs) noexcept
#endif
{
    return detail::to_string_view(lhs) < detail::to_string_view(rhs);
}

/** Check if lhs is less than or equal to rhs.

    A lexicographical comparison is used.

    @par Complexity
    Linear in `lhs.size() + rhs.size()`.

    @par Exception Safety
    No-throw guarantee.
*/
#ifdef BOOST_JSON_DOCS
bool
operator<=(string const& lhs, string const& rhs) noexcept
#else
template<class T, class U>
detail::string_comp_op_requirement<T, U>
operator<=(T const& lhs, U const& rhs) noexcept
#endif
{
    return detail::to_string_view(lhs) <= detail::to_string_view(rhs);
}

/** Check if lhs is more than or equal to rhs.

    A lexicographical comparison is used.

    @par Complexity
    Linear in `lhs.size() + rhs.size()`.

    @par Exception Safety
    No-throw guarantee.
*/
#ifdef BOOST_JSON_DOCS
bool
operator>=(string const& lhs, string const& rhs) noexcept
#else
template<class T, class U>
detail::string_comp_op_requirement<T, U>
operator>=(T const& lhs, U const& rhs) noexcept
#endif
{
    return detail::to_string_view(lhs) >= detail::to_string_view(rhs);
}

/** Check if lhs is greater than rhs.

    A lexicographical comparison is used.

    @par Complexity
    Linear in `lhs.size() + rhs.size()`.

    @par Exception Safety
    No-throw guarantee.
*/
#ifdef BOOST_JSON_DOCS
bool
operator>(string const& lhs, string const& rhs) noexcept
#else
template<class T, class U>
detail::string_comp_op_requirement<T, U>
operator>(T const& lhs, U const& rhs) noexcept
#endif
{
    return detail::to_string_view(lhs) > detail::to_string_view(rhs);
}

} // namespace json
} // namespace boost

// std::hash specialization
#ifndef BOOST_JSON_DOCS
namespace std {
template<>
struct hash< ::boost::json::string >
{
    BOOST_JSON_DECL
    std::size_t
    operator()( ::boost::json::string const& js ) const noexcept;
};
} // std
#endif

#include <boost/json/impl/string.hpp>

#endif
