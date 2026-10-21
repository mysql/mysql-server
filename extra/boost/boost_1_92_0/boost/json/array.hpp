//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

#ifndef BOOST_JSON_ARRAY_HPP
#define BOOST_JSON_ARRAY_HPP

#include <boost/json/detail/config.hpp>
#include <boost/json/detail/array.hpp>
#include <boost/json/kind.hpp>
#include <boost/json/pilfer.hpp>
#include <boost/json/storage_ptr.hpp>
#include <boost/system/result.hpp>
#include <cstdlib>
#include <initializer_list>
#include <iterator>

namespace boost {
namespace json {

#ifndef BOOST_JSON_DOCS
class value;
class value_ref;
#endif

/** A dynamically sized array of JSON values

    This is the type used to represent a JSON array as a modifiable container.
    The interface and performance characteristics are modeled after
    `std::vector<value>`.

    Elements are stored contiguously, which means that they can be accessed not
    only through iterators, but also using offsets to regular pointers to
    elements. A pointer to an element of an `array` may be passed to any
    function that expects a pointer to @ref value.

    The storage of the array is handled automatically, being expanded and
    contracted as needed. Arrays usually occupy more space than array language
    constructs, because more memory is allocated to handle future growth. This
    way an array does not need to reallocate each time an element is inserted,
    but only when the additional memory is used up. The total amount of
    allocated memory can be queried using the @ref capacity function. Extra
    memory can be relinquished by calling @ref shrink_to_fit.


    Reallocations are usually costly operations in terms of performance. The
    @ref reserve function can be used to eliminate reallocations if the number
    of elements is known beforehand.

    The complexity (efficiency) of common operations on arrays is as follows:

    @li Random access---constant *O(1)*.
    @li Insertion or removal of elements at the end - amortized
        constant *O(1)*.
    @li Insertion or removal of elements---linear in the distance to the end of
        the array *O(n)*.

    @par Allocators

    All elements stored in the container, and their children if any, will use
    the same memory resource that was used to construct the container.

    @par Thread Safety

    Non-const member functions may not be called concurrently with any other
    member functions.

    @par Satisfies
        [*ContiguousContainer*](https://en.cppreference.com/w/cpp/named_req/ContiguousContainer),
        [*ReversibleContainer*](https://en.cppreference.com/w/cpp/named_req/ReversibleContainer), and
        {req_SequenceContainer}.
*/
class array
{
    struct table;
    class revert_construct;
    class revert_insert;
    friend class value;

    storage_ptr sp_;        // must come first
    kind k_ = kind::array;  // must come second
    table* t_;

    BOOST_JSON_DECL
    static table empty_;

    inline
    static
    void
    relocate(
        value* dest,
        value* src,
        std::size_t n) noexcept;

    inline
    void
    destroy(
        value* first,
        value* last) noexcept;

    BOOST_JSON_DECL
    void
    destroy() noexcept;

    BOOST_JSON_DECL
    explicit
    array(detail::unchecked_array&& ua);

public:
    /// Associated [Allocator](https://en.cppreference.com/w/cpp/named_req/Allocator)
    using allocator_type = container::pmr::polymorphic_allocator<value>;

    /// The type used to represent unsigned integers
    using size_type = std::size_t;

    /// The type of each element
    using value_type = value;

    /// The type used to represent signed integers
    using difference_type = std::ptrdiff_t;

    /// A reference to an element
    using reference = value&;

    /// A const reference to an element
    using const_reference = value const&;

    /// A pointer to an element
    using pointer = value*;

    /// A const pointer to an element
    using const_pointer = value const*;

    /// A random access iterator to an element
    using iterator = value*;

    /// A random access const iterator to an element
    using const_iterator = value const*;

    /// A reverse random access iterator to an element
    using reverse_iterator =
        std::reverse_iterator<iterator>;

    /// A reverse random access const iterator to an element
    using const_reverse_iterator =
        std::reverse_iterator<const_iterator>;

    //------------------------------------------------------

    /** Destructor.

        The destructor for each element is called if needed, any used memory is
        deallocated, and shared ownership of the
        @ref boost::container::pmr::memory_resource is released.

        @par Complexity
        Linear in @ref size().

        @par Exception Safety
        No-throw guarantee.
    */
    BOOST_JSON_DECL
    ~array() noexcept;

    //------------------------------------------------------

    /** Constructors.

        Constructs an array.

        @li **(1)**, **(2)**  the array is empty and has zero capacity.

        @li **(3)** the array is filled with `count` copies of `jv`.

        @li **(4)** the array is filled with `count` null values.

        @li **(5)** the array is filled with values in the range
            `[first, last)`, preserving order.

        @li **(6)** the array is filled with copies of the values in `init`,
            preserving order.

        @li **(7)**, **(8)** the array is filled with copies of the elements of
            `other`, preserving order.

        @li **(9)** the array acquires ownership of the contents of `other`.

        @li **(10)** equivalent to **(9)** if `*sp == *other.storage()`;
            otherwise equivalent to **(8)**.

        @li **(11)** the array acquires ownership of the contents of `other`
            using pilfer semantics. This is more efficient than move
            construction, when it is known that the moved-from object will be
            immediately destroyed afterwards.

        With **(2)**, **(4)**, **(5)**, **(6)**, **(8)**, **(10)** the
        constructed array uses memory resource of `sp`. With **(7)**, **(9)**,
        and **(11)** it uses `other`'s memory resource. In either case the
        array will share the ownership of the memory resource. With **(1)**
        and **(3)** it uses the
        \<\<default_memory_resource, default memory resource\>\>.

        After **(9)** `other` behaves as if newly constructed with its
        current storage pointer.

        After **(11)** `other` is not in a usable state and may only be
        destroyed.

        @par Constraints

        @code
        std::is_constructible_v<value, std::iterator_traits<InputIt>::reference>
        @endcode

        @par Complexity
        @li **(1)**, **(2)**, **(9)**, **(11)** constant.
        @li **(3)**, **(4)** linear in `count`.
        @li **(5)** linear in `std::distance(first, last)`
        @li **(6)** linear in `init.size()`.
        @li **(7)**, **(8)** linear in `other.size()`.
        @li **(10)** constant if `*sp == *other.storage()`; otherwise linear in
            `other.size()`.

        @par Exception Safety
        @li **(1)**, **(2)**, **(9)**, **(11)** no-throw guarantee.
        @li **(3)**, **(4)**, **(6)**--**(8)**, **(10)** strong
            guarantee.
        @li **(5)** strong guarantee if `InputIt` satisfies
        {req_ForwardIterator}, basic guarantee otherwise.

        Calls to `memory_resource::allocate` may throw.

        @see @ref pilfer,
            [Valueless Variants Considered Harmful](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0308r0.html).
        @{
    */
    array() noexcept
        : t_(&empty_)
    {
    }

    /** Overload
        @param sp A pointer to the @ref boost::container::pmr::memory_resource
        to use. The container will acquire shared ownership of the memory
        resource.
    */
    explicit
    array(storage_ptr sp) noexcept
        : sp_(std::move(sp))
        , k_(kind::array)
        , t_(&empty_)
    {
    }

    /** Overload
        @param count The number of copies to insert.
        @param jv The value to be inserted.
        @param sp
    */
    BOOST_JSON_DECL
    array(
        std::size_t count,
        value const& jv,
        storage_ptr sp = {});

    /// Overload
    BOOST_JSON_DECL
    array(
        std::size_t count,
        storage_ptr sp = {});

    /** Overload

        @param first An input iterator pointing to the first element to insert,
               or pointing to the end of the range.
        @param last An input iterator pointing to the end of the range.
        @param sp

        @tparam InputIt a type satisfying the {req_InputIterator} requirement.
    */
    template<
        class InputIt
    #ifndef BOOST_JSON_DOCS
        ,class = typename std::enable_if<
            std::is_constructible<value,
                typename std::iterator_traits<
                    InputIt>::reference>::value>::type
    #endif
    >
    array(
        InputIt first, InputIt last,
        storage_ptr sp = {});

    /** Overload
        @param init The initializer list with elements to insert.
        @param sp
    */
    BOOST_JSON_DECL
    array(
        std::initializer_list<value_ref> init,
        storage_ptr sp = {});

    /** Overload
        @param other Another array.
    */
    BOOST_JSON_DECL
    array(array const& other);

    /// Overload
    BOOST_JSON_DECL
    array(
        array const& other,
        storage_ptr sp);

    /// Overload
    array(array&& other) noexcept
        : sp_(other.sp_)
        , t_(detail::exchange(
            other.t_, &empty_))
    {
    }

    /// Overload
    BOOST_JSON_DECL
    array(
        array&& other,
        storage_ptr sp);

    /// Overload
    array(pilfered<array> other) noexcept
        : sp_(std::move(other.get().sp_))
        , t_(detail::exchange(
            other.get().t_, &empty_))
    {
    }
    /// @}

    /** Assignment operators.

        Replaces the contents of the array.
        @li **(1)** the contents are replaced with an element-wise copy of
            `other`.
        @li **(2)** takes ownership of `other`'s element storage if
            `*storage() == *other.storage()`; otherwise equivalent to **(1)**.
        @li **(3)** the contents are replaced with a copy of the values in
            `init`.

        After **(2)**, the moved-from array behaves as if newly constructed
        with its current storage pointer.

        @par Complexity
        @li **(1)** linear in `size() + other.size()`.
        @li **(2)** constant if `*storage() == *other.storage()`; otherwise
            linear in `size() + other.size()`.
        @li **(1)** linear in `size() + init.size()`.

        @par Exception Safety
        {sp} **(2)** provides strong guarantee if
        `*storage() != *other.storage()` and no-throw guarantee otherwise.
        Other overloads provide strong guarantee.
        Calls to `memory_resource::allocate` may throw.

        @param other The array to copy.

        @return `*this`

        @{
    */
    BOOST_JSON_DECL
    array&
    operator=(array const& other);

    /** Overload
        @param other The array to move.
    */
    BOOST_JSON_DECL
    array&
    operator=(array&& other);

    /** Overload
        @param init The initializer list to copy.
    */
    BOOST_JSON_DECL
    array&
    operator=(
        std::initializer_list<value_ref> init);
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
    // Element access
    //
    //------------------------------------------------------

    /** Access an element, with bounds checking.

        Returns @ref boost::system::result containing a reference to the
        element specified at location `pos`, if `pos` is within the range of
        the container. Otherwise the result contains an `error_code`.

        @par Exception Safety
        No-throw guarantee.

        @param pos A zero-based index.

        @par Complexity
        Constant.

        @{
    */
    BOOST_JSON_DECL
    system::result<value&>
    try_at(std::size_t pos) noexcept;

    BOOST_JSON_DECL
    system::result<value const&>
    try_at(std::size_t pos) const noexcept;
    /// @}

    /** Access an element, with bounds checking.

        Returns a reference to the element specified at location `pos`, with
        bounds checking. If `pos` is not within the range of the container, an
        exception of type @ref boost::system::system_error is thrown.

        @par Complexity
        Constant.

        @param pos A zero-based index.

        @param loc `source_location` to use in thrown exception; the source
            location of the call site by default.

        @throw `boost::system::system_error` `pos >= size()`.

        @{
    */
    inline
    value&
    at(
        std::size_t pos,
        source_location const& loc = BOOST_CURRENT_LOCATION) &;

    inline
    value&&
    at(
        std::size_t pos,
        source_location const& loc = BOOST_CURRENT_LOCATION) &&;

    BOOST_JSON_DECL
    value const&
    at(
        std::size_t pos,
        source_location const& loc = BOOST_CURRENT_LOCATION) const&;
    /// @}

    /** Access an element.

        Returns a reference to the element specified at
        location `pos`. No bounds checking is performed.

        @pre `pos < size()`

        @par Complexity
        Constant.

        @param pos A zero-based index

        @{
    */
    inline
    value&
    operator[](std::size_t pos) & noexcept;

    inline
    value&&
    operator[](std::size_t pos) && noexcept;

    inline
    value const&
    operator[](std::size_t pos) const& noexcept;
    /// @}

    /** Access the first element.

        Returns a reference to the first element.

        @pre `! empty()`

        @par Complexity
        Constant.

        @{
    */
    inline
    value&
    front() & noexcept;

    inline
    value&&
    front() && noexcept;

    inline
    value const&
    front() const& noexcept;
    /// @}

    /** Access the last element.

        Returns a reference to the last element.

        @pre `!empty()`

        @par Complexity
        Constant.

        @{
    */
    inline
    value&
    back() & noexcept;

    inline
    value&&
    back() && noexcept;

    inline
    value const&
    back() const& noexcept;
    /// @}

    /** Access the underlying array directly.

        Returns a pointer to the underlying array serving as element storage.
        The value returned is such that the range `[data(), data() + size())`
        is always a valid range, even if the container is empty.

        @note
        If `size() == 0`, the function may or may not return
        a null pointer.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.

        @{
    */
    inline
    value*
    data() noexcept;

    inline
    value const*
    data() const noexcept;
    /// @}

    /** Return a pointer to an element if it exists.

        This function returns a pointer to the element at index `pos` when the
        index is less then the size of the container. Otherwise it returns
        null.

        @par Example
        @code
        if( auto p = arr.if_contains( 1 ) )
            std::cout << *p;
        @endcode

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.

        @param pos The index of the element to return.

        @{
    */
    inline
    value const*
    if_contains(std::size_t pos) const noexcept;

    inline
    value*
    if_contains(std::size_t pos) noexcept;
    /// @}

    //------------------------------------------------------
    //
    // Iterators
    //
    //------------------------------------------------------

    /** Return an iterator to the first element.

        If the container is empty, @ref end() is returned.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.

        @{
    */
    inline
    iterator
    begin() noexcept;

    inline
    const_iterator
    begin() const noexcept;
    /// @}

    /** Return a const iterator to the first element.

        If the container is empty, @ref cend() is returned.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.
    */
    inline
    const_iterator
    cbegin() const noexcept;

    /** Return a const iterator past the last element.

        The returned iterator only acts as a sentinel. Dereferencing it results
        in undefined behavior.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.

        @{
    */
    inline
    iterator
    end() noexcept;

    inline
    const_iterator
    end() const noexcept;
    /// @}

    /** Return a const iterator past the last element.

        The returned iterator only acts as a sentinel. Dereferencing it results
        in undefined behavior.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.
    */
    inline
    const_iterator
    cend() const noexcept;

    /** Return a reverse iterator to the first element of the reversed container.

        The pointed-to element corresponds to the
        last element of the non-reversed container.
        If the container is empty, @ref rend() is returned.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.

        @{
    */
    inline
    reverse_iterator
    rbegin() noexcept;

    inline
    const_reverse_iterator
    rbegin() const noexcept;
    /// @}

    /** Return a const reverse iterator to the first element of the reversed container.

        The pointed-to element corresponds to the
        last element of the non-reversed container.
        If the container is empty, @ref crend() is returned.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.
    */
    inline
    const_reverse_iterator
    crbegin() const noexcept;

    /** Return a reverse iterator to the element following the last element of the reversed container.

        The pointed-to element corresponds to the element
        preceding the first element of the non-reversed container.
        The returned iterator only acts as a sentinel. Dereferencing it results
        in undefined behavior.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.

        @{
    */
    inline
    reverse_iterator
    rend() noexcept;

    inline
    const_reverse_iterator
    rend() const noexcept;
    /// @}

    /** Return a const reverse iterator to the element following the last element of the reversed container.

        The pointed-to element corresponds to the element preceding the first
        element of the non-reversed container. The returned iterator only acts
        as a sentinel. Dereferencing it results in undefined behavior.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.
    */
    inline
    const_reverse_iterator
    crend() const noexcept;

    //------------------------------------------------------
    //
    // Capacity
    //
    //------------------------------------------------------

    /** Return the number of elements in the array.

        This returns the number of elements in the array.
        The value returned may be different from the number
        returned from @ref capacity.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.
    */
    inline
    std::size_t
    size() const noexcept;

    /** The maximum number of elements an array can hold.

        The maximum is an implementation-defined number. This value is
        a theoretical limit; at runtime, the actual maximum size may be less
        due to resource limits.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.
    */
    static
    inline
    constexpr
    std::size_t
    max_size() noexcept;

    /** Return the number of elements that can be held in currently allocated memory.

        Returns the number of elements that the container has currently
        allocated space for. This number is never smaller than the value
        returned by @ref size().

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.
    */
    inline
    std::size_t
    capacity() const noexcept;

    /** Check if the array has no elements.

        Returns `true` if there are no elements in the
        array, i.e. @ref size() returns 0.

        @par Complexity
        Constant.

        @par Exception Safety
        No-throw guarantee.
    */
    inline
    bool
    empty() const noexcept;

    /** Increase the capacity to at least a certain amount.

        This increases the @ref capacity() to a value that is greater than or
        equal to `new_capacity`. If `new_capacity > capacity()`, new memory is
        allocated. Otherwise, the call has no effect. The number of elements
        and therefore the @ref size() of the container is not changed.

        If new memory is allocated, all iterators including any past-the-end
        iterators, and all references to the elements are invalidated.
        Otherwise, no iterators or references are invalidated.

        @par Complexity
        At most, linear in @ref size().

        @par Exception Safety
        Strong guarantee.
        Calls to `memory_resource::allocate` may throw.

        @param new_capacity The new capacity of the array.

        @throw boost::system::system_error `new_capacity >` @ref max_size().
    */
    inline
    void
    reserve(std::size_t new_capacity);

    /** Request the removal of unused capacity.

        This performs a non-binding request to reduce the
        capacity to the current size. The request may or
        may not be fulfilled. If reallocation occurs, all
        iterators including any past-the-end iterators,
        and all references to the elements are invalidated.
        Otherwise, no iterators or references are
        invalidated.

        @par Complexity
        At most, linear in @ref size().

        @par Exception Safety
        No-throw guarantee.
    */
    BOOST_JSON_DECL
    void
    shrink_to_fit() noexcept;

    //------------------------------------------------------
    //
    // Modifiers
    //
    //------------------------------------------------------

    /** Clear the contents.

        Erases all elements from the container. After this call, @ref size()
        returns zero but @ref capacity() is unchanged. All references,
        pointers, and iterators are invalidated

        @par Complexity
        Linear in @ref size().

        @par Exception Safety
        No-throw guarantee.
    */
    BOOST_JSON_DECL
    void
    clear() noexcept;

    /** Insert elements before the specified location.

        @li **(1)** and **(2)** insert a single new element before
            `pos`. **(1)** copy-constructs and **(2)** move-constructs the new
            element from `jv`.
        @li **(3)** inserts `count` copies of `jv` before `pos`.
        @li **(4)** the elements in the range `[first, last)` are inserted in
            order.
        @li **(5)** the elements of the initializer list `init` are inserted in
            order.

        Inserted values will be constructed using the container's
        associated @ref boost::container::pmr::memory_resource.

        @note Overload **(2)** is equivalent to **(1)** if
        `*jv.storage() != *this->storage()`.

        If the size of the array after insertion would have exceeded
        @ref capacity(), a reallocation occurs first, and all iterators and
        references are invalidated. Otherwise, only the iterators and
        references from the insertion point forward are invalidated. All
        past-the-end iterators are also invalidated.

        @pre
        `first` and `last` are not iterators into `*this`.

        @par Constraints
        @code
        ! std::is_convertible_v<InputIt, value>
        std::is_constructible_v<value, std::iterator_traits<InputIt>::reference>
        @endcode

        @par Complexity
        @li **(1)**, **(2)** linear in `std::distance(pos, end())`.
        @li **(3)** linear in `count + std::distance(pos, end())`.
        @li **(4)** linear in `std::distance(first, last) +
            std::distance(pos, end())`.
        @li **(5)** linear in `init.size() + std::distance(pos, end())`.

        @par Exception Safety
        {sp}**(4)** provides strong guarantee if `InputIt` satisfies
        {req_ForwardIterator}, and basic guarantee otherwise. Other overloads
        provide strong guarantee.
        Calls to `memory_resource::allocate` may throw.

        @param pos Iterator before which the new elements will
        be inserted. This may be the @ref end() iterator.

        @param jv The value to insert. A copy will be made
        using container's associated
        @ref boost::container::pmr::memory_resource.

        @return An iterator to the first inserted value, or `pos` if no values
                were inserted.

        @{
    */
    BOOST_JSON_DECL
    iterator
    insert(
        const_iterator pos,
        value const& jv);

    // Overload
    BOOST_JSON_DECL
    iterator
    insert(
        const_iterator pos,
        value&& jv);


    /** Overload
        @param count The number of copies to insert.
        @param pos
        @param jv
    */
    BOOST_JSON_DECL
    iterator
    insert(
        const_iterator pos,
        std::size_t count,
        value const& jv);

    /** Overload

        @param first An input iterator pointing to the first element to insert,
               or pointing to the end of the range.
        @param last An input iterator pointing to the end of the range.
        @param pos

        @tparam InputIt a type satisfying the requirements
        of {req_InputIterator}.
    */
    template<
        class InputIt
    #ifndef BOOST_JSON_DOCS
        ,class = typename std::enable_if<
            std::is_constructible<value,
                typename std::iterator_traits<
                    InputIt>::reference>::value>::type
    #endif
    >
    iterator
    insert(
        const_iterator pos,
        InputIt first, InputIt last);

    /** Overload
        @param init The initializer list to insert
        @param pos
    */
    BOOST_JSON_DECL
    iterator
    insert(
        const_iterator pos,
        std::initializer_list<value_ref> init);
    /// @}

    /** Insert a constructed element in-place.

        Inserts a new element into the container directly before
        `pos`. The element is constructed using placement-new
        with the parameter `std::forward<Arg>(arg)`.
        If `capacity() < size() + 1`,
        a reallocation occurs first, and all iterators and
        references are invalidated.
        Otherwise, only the iterators and references from
        the insertion point forward are invalidated. All
        past-the-end iterators are also invalidated.

        @par Complexity
        Linear in `std::distance(pos, end())`.

        @par Exception Safety
        Strong guarantee.
        Calls to `memory_resource::allocate` may throw.

        @param pos Iterator before which the element will
        be inserted. This may be the @ref end() iterator.

        @param arg The argument to forward to the @ref value
        constructor.

        @return An iterator to the inserted element
    */
    template<class Arg>
    iterator
    emplace(
        const_iterator pos,
        Arg&& arg);

    /** Remove elements from the array.

        @li **(1)** the element at `pos` is removed.
        @li **(2)** the elements in the range `[first, last)` are removed.

        @par Complexity
        @li **(1)** linear in `std::distance(pos, end())`.
        @li **(2)** linear in `std::distance(first, end())`.

        @par Exception Safety
        No-throw guarantee.

        @param pos Iterator to the element to remove

        @return Iterator following the last removed element. If that was the
                last element of the array, the @ref end() iterator is returned.

        @{
    */
    BOOST_JSON_DECL
    iterator
    erase(const_iterator pos) noexcept;

    /** Overload
        @param first An iterator pointing to the first element to erase, or
               pointing to the end of the range.
        @param last An iterator pointing to one past the last element to erase,
                    or pointing to the end of the range.
    */
    BOOST_JSON_DECL
    iterator
    erase(
        const_iterator first,
        const_iterator last) noexcept;
    /// @}

    /** Add an element to the end.

        Insert a new element at the end of the container. **(1)**
        copy-constructs the new element from `jv`, **(2)** move-constructs from
        `jv`.

        If `capacity() < size() + 1`, a reallocation occurs first, and all
        iterators and references are invalidated. Any past-the-end iterators
        are always invalidated.

        The new element will be constructed using the container's associated
        @ref boost::container::pmr::memory_resource.

        @par Complexity
        Amortized constant.

        @par Exception Safety
        Strong guarantee.
        Calls to `memory_resource::allocate` may throw.

        @param jv The value to insert.

        @{
    */
    BOOST_JSON_DECL
    void
    push_back(value const& jv);

    BOOST_JSON_DECL
    void
    push_back(value&& jv);
    /// @}

    /** Append a constructed element in-place.

        Appends a new element to the end of the container's
        list of elements.
        The element is constructed using placement-new
        with the parameter `std::forward<Arg>(arg)`.
        If `capacity() < size() + 1`,
        a reallocation occurs first, and all iterators and
        references are invalidated.
        Otherwise, only the iterators and references from
        the insertion point forward are invalidated. All
        past-the-end iterators are also invalidated.

        @par Complexity
        Amortized constant.

        @par Exception Safety
        Strong guarantee.
        Calls to `memory_resource::allocate` may throw.

        @param arg The argument to forward to the @ref value
        constructor.

        @return A reference to the inserted element
    */
    template<class Arg>
    value&
    emplace_back(Arg&& arg);

    /** Remove the last element

        The last element of the container is erased.

        @pre
        `! empty()`

        @par Exception Safety
        No-throw guarantee.
    */
    BOOST_JSON_DECL
    void
    pop_back() noexcept;

    /** Change the number of elements stored.

        Resizes the container to contain `count` elements.

        @li If `size() > count`, the container is reduced to its first `count`
            elements.
        @li If `size() < count`, additional null values (**(1)**) or copies
            of `jv` (**(2)**) are appended.

        If `capacity() < count`, a reallocation occurs first, and all iterators
        and references are invalidated. Any past-the-end iterators are always
        invalidated.

        @par Complexity
        Linear in `size() + count`.

        @par Exception Safety
        Strong guarantee.
        Calls to `memory_resource::allocate` may throw.

        @param count The new size of the container.

        @{
    */
    BOOST_JSON_DECL
    void
    resize(std::size_t count);

    /** Overload
        @param jv The @ref value to copy into the new elements.
        @param count
    */
    BOOST_JSON_DECL
    void
    resize(
        std::size_t count,
        value const& jv);
    /// @}

    /** Swap two arrays.

        Exchanges the contents of this array with another array. Ownership of
        the respective @ref boost::container::pmr::memory_resource objects is
        not transferred. If `this == &other`, this function call has no effect.

        @li If `*storage() == *other.storage()` all iterators and references
        remain valid.

        @li Otherwise, the contents are logically swapped by making copies,
        which can throw. In this case all iterators and references are
        invalidated.

        @par Complexity
        If `*storage() == *other.storage()`, then constant; otherwise linear in
        `size() + other.size()`.

        @par Exception Safety
        No-throw guarantee if `*storage() == *other.storage()`. Otherwise
        strong guarantee. Calls to `memory_resource::allocate` may throw.

        @param other The value to swap with.
    */
    BOOST_JSON_DECL
    void
    swap(array& other);

    /** Swap two arrays.

        Exchanges the contents of the array `lhs` with another array `rhs`.
        Ownership of the respective @ref boost::container::pmr::memory_resource
        objects is not transferred. If `&lhs == &rhs`, this function call has
        no effect.

        @li If `*lhs.storage() == *rhs.storage()` all iterators and references
        remain valid.

        @li Otherwise, the contents are logically swapped by making copies,
        which can throw. In this case all iterators and references are
        invalidated.

        @par Complexity
        If `*lhs.storage() == *rhs.storage()`, then constant; otherwise linear
        in `lhs.size() + rhs.size()`.

        @par Exception Safety
        No-throw guarantee if `*lhs.storage() == *rhs.storage()`. Otherwise
        strong guarantee. Calls to `memory_resource::allocate` may throw.

        @param lhs The array to exchange.

        @param rhs The array to exchange.
        If `&lhs == &rhs`, this function call has no effect.

        @see @ref array::swap
    */
    friend
    void
    swap(array& lhs, array& rhs)
    {
        lhs.swap(rhs);
    }

    /** Compare two arrays for equality.

        Arrays are equal when their sizes are the same, and they are
        element-for-element equal in order.

        @par Complexity
        Linear in `lhs.size()`.

        @par Exception Safety
        No-throw guarantee.
    */
    // inline friend speeds up overload resolution
    friend
    bool
    operator==(
        array const& lhs,
        array const& rhs) noexcept
    {
        return lhs.equal(rhs);
    }

    /** Compare two arrays for inequality.

        Arrays are equal when their sizes are the same, and they are
        element-for-element equal in order.

        @par Complexity
        Linear in `lhs.size()`.

        @par Exception Safety
        No-throw guarantee.
    */
    // inline friend speeds up overload resolution
    friend
    bool
    operator!=(
        array const& lhs,
        array const& rhs) noexcept
    {
        return ! (lhs == rhs);
    }

    /** Serialize to an output stream.

        This function serializes an `array` as JSON into the output stream.

        @return Reference to `os`.

        @par Complexity
        Constant or linear in the size of `arr`.

        @par Exception Safety
        Strong guarantee.
        Calls to `memory_resource::allocate` may throw.

        @param os The output stream to serialize to.

        @param arr The value to serialize.
    */
    BOOST_JSON_DECL
    friend
    std::ostream&
    operator<<(
        std::ostream& os,
        array const& arr);

private:
    template<class It>
    using iter_cat = typename
        std::iterator_traits<It>::iterator_category;

    template<class InputIt>
    array(
        InputIt first, InputIt last,
        storage_ptr sp,
        std::input_iterator_tag);

    template<class InputIt>
    array(
        InputIt first, InputIt last,
        storage_ptr sp,
        std::forward_iterator_tag);

    inline
    std::size_t
    growth(std::size_t new_size) const;

    BOOST_JSON_DECL
    void
    reserve_impl(
        std::size_t new_capacity);

    BOOST_JSON_DECL
    value&
    push_back(
        pilfered<value> pv);

    BOOST_JSON_DECL
    iterator
    insert(
        const_iterator pos,
        pilfered<value> pv);

    template<class InputIt>
    iterator
    insert(
        const_iterator pos,
        InputIt first, InputIt last,
        std::input_iterator_tag);

    template<class InputIt>
    iterator
    insert(
        const_iterator pos,
        InputIt first, InputIt last,
        std::forward_iterator_tag);

    BOOST_JSON_DECL
    bool
    equal(array const& other) const noexcept;
};

} // namespace json
} // namespace boost

// std::hash specialization
#ifndef BOOST_JSON_DOCS
namespace std {
template <>
struct hash< ::boost::json::array > {
    BOOST_JSON_DECL
    std::size_t
    operator()(::boost::json::array const& ja) const noexcept;
};
} // std
#endif

// Must be included here for this file to stand alone
#include <boost/json/value.hpp>

// includes are at the bottom of <boost/json/value.hpp>

#endif
