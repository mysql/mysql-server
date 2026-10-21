//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_SEGTOR_HPP
#define BOOST_CONTAINER_SEGTOR_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
#include <boost/container/detail/deque_impl.hpp>

namespace boost {
namespace container {

#ifdef BOOST_CONTAINER_DOXYGEN_INVOKED
//! A segtor (contraction of "segmented vector" is a sequence container that supports
//! random access to elements, constant time insertion and removal of elements at
//! the end, and linear time insertion and removal of elements in the middle.
//!
//! It's the single-ended version of boost::container::deque: it provides
//! push_back, pop_back, emplace_back, etc., but does not provide push_front,
//! pop_front, or emplace_front.
//!
//! \tparam T The type of object that is stored in the segtor
//! \tparam Allocator The allocator used for all internal memory management, use void
//!   for the default allocator
//! \tparam Options A type produced from \c boost::container::segtor_options.
template <class T, class Allocator = void, class Options = void>
#else
template <class T, class Allocator, class Options>
#endif
class segtor : public deque_impl<T, Allocator, true, Options>
{
#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED
   BOOST_COPYABLE_AND_MOVABLE(segtor)
   typedef deque_impl<T, Allocator, true, Options> base_type;
#endif

   public:
   //////////////////////////////////////////////
   //
   //                    types
   //
   //////////////////////////////////////////////

   typedef T                                                                  value_type;
   typedef typename base_type::allocator_type                                 allocator_type;
   typedef typename base_type::pointer                                        pointer;
   typedef typename base_type::const_pointer                                   const_pointer;
   typedef typename base_type::reference                                      reference;
   typedef typename base_type::const_reference                                const_reference;
   typedef typename base_type::size_type                                      size_type;
   typedef typename base_type::difference_type                                difference_type;
   typedef typename base_type::iterator                                      iterator;
   typedef typename base_type::const_iterator                                const_iterator;
   typedef typename base_type::reverse_iterator                              reverse_iterator;
   typedef typename base_type::const_reverse_iterator                        const_reverse_iterator;
   typedef typename base_type::stored_allocator_type                          stored_allocator_type;

   using base_type::get_block_size;
   #ifndef BOOST_CONTAINER_DOXYGEN_INVOKED
   using base_type::get_segment_size;
   #endif   //#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED
   static const std::size_t is_reservable = base_type::is_reservable;

   //////////////////////////////////////////////
   //
   //          construct/copy/destroy
   //
   //////////////////////////////////////////////

   //! <b>Effects</b>: Default constructs a segtor.
   //!
   //! <b>Throws</b>: If allocator_type's default constructor throws.
   //!
   //! <b>Complexity</b>: Constant.
   segtor()
      BOOST_NOEXCEPT_IF(dtl::is_nothrow_default_constructible<allocator_type>::value)
      : base_type()
   {}

   //! <b>Effects</b>: Constructs a segtor taking the allocator as parameter.
   //!
   //! <b>Throws</b>: Nothing
   //!
   //! <b>Complexity</b>: Constant.
   explicit segtor(const allocator_type& a) BOOST_NOEXCEPT_OR_NOTHROW
      : base_type(a)
   {}

   //! <b>Effects</b>: Constructs a segtor
   //!   and inserts n value initialized values.
   //!
   //! <b>Throws</b>: If allocator_type's default constructor
   //!   throws or T's value initialization throws.
   //!
   //! <b>Complexity</b>: Linear to n.
   explicit segtor(size_type n)
      : base_type(n)
   {}

   //! <b>Effects</b>: Constructs a segtor
   //!   and inserts n default initialized values.
   //!
   //! <b>Throws</b>: If allocator_type's default constructor
   //!   throws or T's default initialization or copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to n.
   //!
   //! <b>Note</b>: Non-standard extension
   segtor(size_type n, default_init_t)
      : base_type(n, default_init)
   {}

   //! <b>Effects</b>: Constructs a segtor that will use a copy of allocator a
   //!   and inserts n value initialized values.
   //!
   //! <b>Throws</b>: If allocator_type's default constructor
   //!   throws or T's value initialization throws.
   //!
   //! <b>Complexity</b>: Linear to n.
   explicit segtor(size_type n, const allocator_type& a)
      : base_type(n, a)
   {}

   //! <b>Effects</b>: Constructs a segtor that will use a copy of allocator a
   //!   and inserts n default initialized values.
   //!
   //! <b>Throws</b>: If allocator_type's default constructor
   //!   throws or T's default initialization or copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to n.
   //!
   //! <b>Note</b>: Non-standard extension
   segtor(size_type n, default_init_t, const allocator_type& a)
      : base_type(n, default_init, a)
   {}

   //! <b>Effects</b>: Constructs a segtor that will use a copy of allocator a
   //!   and inserts n copies of value.
   //!
   //! <b>Throws</b>: If allocator_type's default constructor
   //!   throws or T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to n.
   segtor(size_type n, const value_type& value)
      : base_type(n, value)
   {}

   //! <b>Effects</b>: Constructs a segtor that will use a copy of allocator a
   //!   and inserts n copies of value.
   //!
   //! <b>Throws</b>: If allocator_type's default constructor
   //!   throws or T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to n.
   segtor(size_type n, const value_type& value, const allocator_type& a)
      : base_type(n, value, a)
   {}

   //! <b>Effects</b>: Constructs a segtor that will use a copy of allocator a
   //!   and inserts a copy of the range [first, last).
   //!
   //! <b>Throws</b>: If allocator_type's default constructor
   //!   throws or T's constructor taking a dereferenced InIt throws.
   //!
   //! <b>Complexity</b>: Linear to the range [first, last).
   template <class InIt>
   segtor(InIt first, InIt last
      #if !defined(BOOST_CONTAINER_DOXYGEN_INVOKED)
      , typename dtl::disable_if_convertible<InIt, size_type>::type* = 0
      #endif
      )
      : base_type(first, last)
   {}

   //! <b>Effects</b>: Constructs a segtor that will use a copy of allocator a
   //!   and inserts a copy of the range [first, last).
   //!
   //! <b>Throws</b>: If allocator_type's default constructor
   //!   throws or T's constructor taking a dereferenced InIt throws.
   //!
   //! <b>Complexity</b>: Linear to the range [first, last).
   template <class InIt>
   segtor(InIt first, InIt last, const allocator_type& a
      #if !defined(BOOST_CONTAINER_DOXYGEN_INVOKED)
      , typename dtl::disable_if_convertible<InIt, size_type>::type* = 0
      #endif
      )
      : base_type(first, last, a)
   {}

#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
   //! <b>Effects</b>: Constructs a segtor that will use a copy of allocator a
   //!   and inserts a copy of the range [il.begin(), il.end()).
   //!
   //! <b>Throws</b>: If allocator_type's default constructor
   //!   throws or T's constructor taking a dereferenced std::initializer_list iterator throws.
   //!
   //! <b>Complexity</b>: Linear to the range [il.begin(), il.end()).
   segtor(std::initializer_list<value_type> il, const allocator_type& a = allocator_type())
      : base_type(il, a)
   {}
#endif

   //! <b>Effects</b>: Copy constructs a segtor.
   //!
   //! <b>Postcondition</b>: x == *this.
   //!
   //! <b>Complexity</b>: Linear to the elements x contains.
   segtor(const segtor& x)
      : base_type(x)
   {}

   //! <b>Effects</b>: Move constructor. Moves x's resources to *this.
   //!
   //! <b>Throws</b>: If allocator_type's copy constructor throws.
   //!
   //! <b>Complexity</b>: Constant.
   segtor(BOOST_RV_REF(segtor) x) BOOST_NOEXCEPT_OR_NOTHROW
      : base_type(boost::move(static_cast<base_type&>(x)))
   {}

   //! <b>Effects</b>: Copy constructs a segtor using the specified allocator.
   //!
   //! <b>Postcondition</b>: x == *this.
   //!
   //! <b>Throws</b>: If allocation
   //!   throws or T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to the elements x contains.
   segtor(const segtor& x, const BOOST_CONTAINER_DOC1ST(allocator_type, typename dtl::type_identity<allocator_type>::type)& a)
      : base_type(x, a)
   {}

   //! <b>Effects</b>: Move constructor using the specified allocator.
   //!                 Moves x's resources to *this if a == allocator_type().
   //!                 Otherwise copies values from x to *this.
   //!
   //! <b>Throws</b>: If allocation or T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Constant if a == x.get_allocator(), linear otherwise.
   segtor(BOOST_RV_REF(segtor) x, const BOOST_CONTAINER_DOC1ST(allocator_type, typename dtl::type_identity<allocator_type>::type)& a)
      : base_type(boost::move(static_cast<base_type&>(x)), a)
   {}

   //! <b>Effects</b>: Destroys the segtor. All stored values are destroyed
   //!   and used memory is deallocated.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements.
   ~segtor() BOOST_NOEXCEPT_OR_NOTHROW
   {}

   //! <b>Effects</b>: Makes *this contain the same elements as x.
   //!
   //! <b>Postcondition</b>: this->size() == x.size(). *this contains a copy
   //! of each of x's elements.
   //!
   //! <b>Throws</b>: If memory allocation throws or T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to the number of elements in x.
   segtor& operator=(BOOST_COPY_ASSIGN_REF(segtor) x)
   {
      base_type::operator=(static_cast<const base_type&>(x));
      return *this;
   }

   //! <b>Effects</b>: Move assignment. All x's values are transferred to *this.
   //!
   //! <b>Throws</b>: If allocator_traits_type::propagate_on_container_move_assignment
   //!   is false and (allocation throws or value_type's move constructor throws)
   //!
   //! <b>Complexity</b>: Constant if allocator_traits_type::
   //!   propagate_on_container_move_assignment is true or
   //!   this->get_allocator() == x.get_allocator(). Linear otherwise.
   segtor& operator=(BOOST_RV_REF(segtor) x)
      BOOST_NOEXCEPT_IF(allocator_traits<allocator_type>::propagate_on_container_move_assignment::value
                        || allocator_traits<allocator_type>::is_always_equal::value)
   {
      base_type::operator=(BOOST_MOVE_BASE(base_type, x));
      return *this;
   }

   #if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
   //! <b>Effects</b>: Makes *this contain the same elements as il.
   //!
   //! <b>Postcondition</b>: this->size() == il.size(). *this contains a copy
   //! of each of il's elements.
   //!
   //! <b>Throws</b>: If memory allocation throws or T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to the number of elements in il.
   segtor& operator=(std::initializer_list<value_type> il)
   {
      base_type::operator=(il);
      return *this;
   }
   #endif

   //////////////////////////////////////////////
   //
   //                capacity
   //
   //////////////////////////////////////////////

   //! <b>Requires</b>: The container must be "reservable" (is_reservable == true)
   //!
   //! <b>Effects</b>: If n is less than or equal to capacity() or the container is not reservable,
   //!   this call has no effect. Otherwise, it is a request for allocation of additional memory.
   //!   If the request is successful, then capacity() is greater than or equal to
   //!   n; otherwise, capacity() is unchanged. In either case, size() is unchanged.
   //!
   //! <b>Throws</b>: If memory allocation throws.
   //!
   //! <b>Complexity</b>: Linear to n.
   //!
   //! <b>Note</b>: Non-standard extension.
   void reserve(size_type n)
   {
      this->reserve_back(n);
   }

   //! <b>Effects</b>: Returns the number of elements that can be inserted
   //!   at the back without allocating additional memory.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Note</b>: Non-standard extension.
   BOOST_CONTAINER_NODISCARD size_type capacity() const BOOST_NOEXCEPT_OR_NOTHROW
   {
      return this->back_capacity();
   }

   #ifdef BOOST_CONTAINER_DOXYGEN_INVOKED

   //! <b>Effects</b>: Returns the number of continguous elements per segment/block.
   //! Same as get_block_size().
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   static size_type get_segment_size() BOOST_NOEXCEPT_OR_NOTHROW;

   //! <b>Effects</b>: Returns the number of continguous elements per segment/block.
   //! Same as get_segment_size().
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   static size_type get_block_size() BOOST_NOEXCEPT_OR_NOTHROW;

   //////////////////////////////////////////////
   //
   //                iterators
   //
   //////////////////////////////////////////////

   //! <b>Effects</b>: Returns an iterator to the first element contained in the container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_NODISCARD iterator begin() BOOST_NOEXCEPT_OR_NOTHROW;

   //! <b>Effects</b>: Returns a const_iterator to the first element contained in the container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_NODISCARD const_iterator begin() const BOOST_NOEXCEPT_OR_NOTHROW;

   //! <b>Effects</b>: Returns an iterator to the end of the container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_NODISCARD iterator end() BOOST_NOEXCEPT_OR_NOTHROW;

   //! <b>Effects</b>: Returns a const_iterator to the end of the container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_NODISCARD const_iterator end() const BOOST_NOEXCEPT_OR_NOTHROW;

   //! <b>Effects</b>: Returns a reverse_iterator pointing to the beginning
   //! of the reversed container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_NODISCARD reverse_iterator rbegin() BOOST_NOEXCEPT_OR_NOTHROW;

   //! <b>Effects</b>: Returns a const_reverse_iterator pointing to the beginning
   //! of the reversed container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_NODISCARD const_reverse_iterator rbegin() const BOOST_NOEXCEPT_OR_NOTHROW;

   //! <b>Effects</b>: Returns a reverse_iterator pointing to the end
   //! of the reversed container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_NODISCARD reverse_iterator rend() BOOST_NOEXCEPT_OR_NOTHROW;

   //! <b>Effects</b>: Returns a const_reverse_iterator pointing to the end
   //! of the reversed container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_NODISCARD const_reverse_iterator rend() const BOOST_NOEXCEPT_OR_NOTHROW;

   //! <b>Effects</b>: Returns a const_iterator to the first element contained in the container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_NODISCARD const_iterator cbegin() const BOOST_NOEXCEPT_OR_NOTHROW;

   //! <b>Effects</b>: Returns a const_iterator to the end of the container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_NODISCARD const_iterator cend() const BOOST_NOEXCEPT_OR_NOTHROW;

   //! <b>Effects</b>: Returns a const_reverse_iterator pointing to the beginning
   //! of the reversed container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_NODISCARD const_reverse_iterator crbegin() const BOOST_NOEXCEPT_OR_NOTHROW;

   //! <b>Effects</b>: Returns a const_reverse_iterator pointing to the end
   //! of the reversed container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_NODISCARD const_reverse_iterator crend() const BOOST_NOEXCEPT_OR_NOTHROW;

   //////////////////////////////////////////////
   //
   //               element access
   //
   //////////////////////////////////////////////

   //! <b>Effects</b>: Returns true if the container contains no elements.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_NODISCARD bool empty() const BOOST_NOEXCEPT_OR_NOTHROW;

   //! <b>Effects</b>: Returns the number of the elements contained in the container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_NODISCARD size_type size() const BOOST_NOEXCEPT_OR_NOTHROW;

   //! <b>Effects</b>: Returns the largest possible size of the container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_NODISCARD size_type max_size() const BOOST_NOEXCEPT_OR_NOTHROW;

   //! <b>Requires</b>: !empty()
   //!
   //! <b>Effects</b>: Returns a reference to the first element of the container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_NODISCARD reference front();

   //! <b>Requires</b>: !empty()
   //!
   //! <b>Effects</b>: Returns a const reference to the first element from the beginning of the container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_NODISCARD const_reference front() const;

   //! <b>Requires</b>: !empty()
   //!
   //! <b>Effects</b>: Returns a reference to the last element of the container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_NODISCARD reference back();

   //! <b>Requires</b>: !empty()
   //!
   //! <b>Effects</b>: Returns a const reference to the last element of the container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_NODISCARD const_reference back() const;

   //! <b>Requires</b>: size() > n.
   //!
   //! <b>Effects</b>: Returns a reference to the nth element from the beginning of the container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_NODISCARD reference operator[](size_type n);

   //! <b>Requires</b>: size() > n.
   //!
   //! <b>Effects</b>: Returns a const reference to the nth element from the beginning of the container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_NODISCARD const_reference operator[](size_type n) const;

   //! <b>Requires</b>: size() > n.
   //!
   //! <b>Effects</b>: Returns a reference to the nth element from the beginning of the container.
   //!
   //! <b>Throws</b>: range_error if n >= size()
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_NODISCARD reference at(size_type n);

   //! <b>Requires</b>: size() > n.
   //!
   //! <b>Effects</b>: Returns a const reference to the nth element from the beginning of the container.
   //!
   //! <b>Throws</b>: range_error if n >= size()
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_NODISCARD const_reference at(size_type n) const;

   //////////////////////////////////////////////
   //
   //                allocator
   //
   //////////////////////////////////////////////

   //! <b>Effects</b>: Returns a copy of the internal allocator.
   //!
   //! <b>Throws</b>: If allocator's copy constructor throws.
   //!
   //! <b>Complexity</b>: Constant.
   BOOST_CONTAINER_NODISCARD allocator_type get_allocator() const BOOST_NOEXCEPT_OR_NOTHROW;

   //! <b>Effects</b>: Returns a reference to the internal allocator.
   //!
   //! <b>Throws</b>: Nothing
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Note</b>: Non-standard extension.
   BOOST_CONTAINER_NODISCARD const stored_allocator_type& get_stored_allocator() const BOOST_NOEXCEPT_OR_NOTHROW;

   //! <b>Effects</b>: Returns a reference to the internal allocator.
   //!
   //! <b>Throws</b>: Nothing
   //!
   //! <b>Complexity</b>: Constant.
   //!
   //! <b>Note</b>: Non-standard extension.
   BOOST_CONTAINER_NODISCARD stored_allocator_type& get_stored_allocator() BOOST_NOEXCEPT_OR_NOTHROW;

   //////////////////////////////////////////////
   //
   //                modifiers
   //
   //////////////////////////////////////////////

   //! <b>Effects</b>: Assigns the n copies of val to *this.
   //!
   //! <b>Throws</b>: If memory allocation throws or T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to n.
   void assign(size_type n, const value_type& value);

   //! <b>Effects</b>: Assigns the range [first, last) to *this.
   //!
   //! <b>Throws</b>: If memory allocation throws or
   //!   T's constructor from dereferencing InIt throws.
   //!
   //! <b>Complexity</b>: Linear to the distance [first, last).
   template <class InIt>
   void assign(InIt first, InIt last);

   //! <b>Effects</b>: Assigns the range [il.begin(), il.end()) to *this.
   //!
   //! <b>Throws</b>: If memory allocation throws or
   //!   T's constructor from dereferencing std::initializer_list iterator throws.
   //!
   //! <b>Complexity</b>: Linear to il.size().
   void assign(std::initializer_list<value_type> il);

   //! <b>Effects</b>: Erases all the elements of the container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the number of elements in the container.
   void clear() BOOST_NOEXCEPT_OR_NOTHROW;

   //! <b>Requires</b>: p must be a valid iterator of *this.
   //!
   //! <b>Effects</b>: Inserts an object of type T constructed with
   //!   std::forward<Args>(args)... before p
   //!
   //! <b>Throws</b>: If memory allocation throws or the in-place constructor throws.
   //!
   //! <b>Complexity</b>: If p is end(), amortized constant time
   //!   Linear time otherwise.
   template <class... Args>
   iterator emplace(const_iterator p, BOOST_FWD_REF(Args)... args);

   //! <b>Effects</b>: Inserts an object of type T constructed with
   //!   std::forward<Args>(args)... at the end of the container.
   //!
   //! <b>Returns</b>: A reference to the created object.
   //!
   //! <b>Throws</b>: If memory allocation throws or the in-place constructor throws.
   //!
   //! <b>Complexity</b>: Amortized constant time
   template <class... Args>
   reference emplace_back(BOOST_FWD_REF(Args)... args);

   //! <b>Requires</b>: p must be a valid iterator of *this.
   //!
   //! <b>Effects</b>: Insert a copy of x before p.
   //!
   //! <b>Returns</b>: an iterator to the inserted element.
   //!
   //! <b>Throws</b>: If memory allocation throws or x's copy constructor throws.
   //!
   //! <b>Complexity</b>: If p is end(), amortized constant time
   //!   Linear time otherwise.
   iterator insert(const_iterator p, const value_type& x);

   //! <b>Requires</b>: p must be a valid iterator of *this.
   //!
   //! <b>Effects</b>: Insert a new element before p with x's resources.
   //!
   //! <b>Returns</b>: an iterator to the inserted element.
   //!
   //! <b>Throws</b>: If memory allocation throws.
   //!
   //! <b>Complexity</b>: If p is end(), amortized constant time
   //!   Linear time otherwise.
   iterator insert(const_iterator p, BOOST_RV_REF(value_type) x);

   //! <b>Requires</b>: pos must be a valid iterator of *this.
   //!
   //! <b>Effects</b>: Insert n copies of x before pos.
   //!
   //! <b>Returns</b>: an iterator to the first inserted element or pos if n is 0.
   //!
   //! <b>Throws</b>: If memory allocation throws or T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to n.
   iterator insert(const_iterator pos, size_type n, const value_type& x),

   //! <b>Requires</b>: pos must be a valid iterator of *this.
   //!
   //! <b>Effects</b>: Insert a copy of the [first, last) range before pos.
   //!
   //! <b>Returns</b>: an iterator to the first inserted element or pos if first == last.
   //!
   //! <b>Throws</b>: If memory allocation throws, T's constructor from a
   //!   dereferenced InIt throws or T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to distance [first, last).
   template <class InIt>
   iterator insert(const_iterator pos, InIt first, InIt last);

#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
   //! <b>Requires</b>: pos must be a valid iterator of *this.
   //!
   //! <b>Effects</b>: Insert a copy of the [il.begin(), il.end()) range before pos.
   //!
   //! <b>Returns</b>: an iterator to the first inserted element or pos if il.begin() == il.end().
   //!
   //! <b>Throws</b>: If memory allocation throws, T's constructor from a
   //!   dereferenced std::initializer_list throws or T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to distance [il.begin(), il.end()).
   iterator insert(const_iterator pos, std::initializer_list<value_type> il);
#endif

   //! <b>Effects</b>: Erases the element at p.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the elements between pos and the
   //!   last element (if pos is near the end) or the first element
   //!   if (pos is near the beginning).
   //!   Constant if pos is the first or the last element.
   iterator erase(const_iterator p) BOOST_NOEXCEPT_OR_NOTHROW;

   //! <b>Effects</b>: Erases the elements pointed by [first, last).
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Linear to the distance between first and
   //!   last plus the elements between pos and the
   //!   last element (if pos is near the end) or the first element
   //!   if (pos is near the beginning).
   iterator erase(const_iterator first, const_iterator last) BOOST_NOEXCEPT_OR_NOTHROW;

   //! <b>Effects</b>: Inserts a copy of x at the end of the container.
   //!
   //! <b>Throws</b>: If memory allocation throws or
   //!   T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Amortized constant time.
   void push_back(const value_type& x);

   //! <b>Effects</b>: Constructs a new element at the end of the container
   //!   and moves the resources of x to this new element.
   //!
   //! <b>Throws</b>: If memory allocation throws.
   //!
   //! <b>Complexity</b>: Amortized constant time.
   void push_back(BOOST_RV_REF(value_type) x);

   //! <b>Effects</b>: Removes the last element from the container.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant time.
   void pop_back() BOOST_NOEXCEPT_OR_NOTHROW;

   //! <b>Effects</b>: Inserts or erases elements at the end such that
   //!   the size becomes n. New elements are value initialized.
   //!
   //! <b>Throws</b>: If memory allocation throws, or T's constructor throws.
   //!
   //! <b>Complexity</b>: Linear to the difference between size() and new_size.
   void resize(size_type new_size);

   //! <b>Effects</b>: Inserts or erases elements at the end such that
   //!   the size becomes n. New elements are default initialized.
   //!
   //! <b>Throws</b>: If memory allocation throws, or T's constructor throws.
   //!
   //! <b>Complexity</b>: Linear to the difference between size() and new_size.
   //!
   //! <b>Note</b>: Non-standard extension
   void resize(size_type new_size, default_init_t);

   //! <b>Effects</b>: Inserts or erases elements at the end such that
   //!   the size becomes n. New elements are copy constructed from x.
   //!
   //! <b>Throws</b>: If memory allocation throws, or T's copy constructor throws.
   //!
   //! <b>Complexity</b>: Linear to the difference between size() and new_size.
   void resize(size_type new_size, const value_type& x);

   //! <b>Effects</b>: Swaps the contents of *this and x.
   //!
   //! <b>Throws</b>: Nothing.
   //!
   //! <b>Complexity</b>: Constant.
   void swap(segtor& x);
   #endif   //#ifdef BOOST_CONTAINER_DOXYGEN_INVOKED

   //! <b>Effects</b>: Returns true if x and y are equal
   //!
   //! <b>Complexity</b>: Linear to the number of elements in the container.
   BOOST_CONTAINER_NODISCARD inline
      friend bool operator==(const segtor& x, const segtor& y)
   {  return static_cast<const base_type&>(x) == static_cast<const base_type&>(y);  }

   //! <b>Effects</b>: Returns true if x and y are unequal
   //!
   //! <b>Complexity</b>: Linear to the number of elements in the container.
   BOOST_CONTAINER_NODISCARD inline
      friend bool operator!=(const segtor& x, const segtor& y)
   {  return static_cast<const base_type&>(x) != static_cast<const base_type&>(y); }

   //! <b>Effects</b>: Returns true if x is less than y
   //!
   //! <b>Complexity</b>: Linear to the number of elements in the container.
   BOOST_CONTAINER_NODISCARD inline
      friend bool operator<(const segtor& x, const segtor& y)
   {  return static_cast<const base_type&>(x) < static_cast<const base_type&>(y);  }

   //! <b>Effects</b>: Returns true if x is greater than y
   //!
   //! <b>Complexity</b>: Linear to the number of elements in the container.
   BOOST_CONTAINER_NODISCARD inline
      friend bool operator>(const segtor& x, const segtor& y)
   {  return static_cast<const base_type&>(x) > static_cast<const base_type&>(y);  }

   //! <b>Effects</b>: Returns true if x is equal or less than y
   //!
   //! <b>Complexity</b>: Linear to the number of elements in the container.
   BOOST_CONTAINER_NODISCARD inline
      friend bool operator<=(const segtor& x, const segtor& y)
   {  return static_cast<const base_type&>(x) <= static_cast<const base_type&>(y);  }

   //! <b>Effects</b>: Returns true if x is equal or greater than y
   //!
   //! <b>Complexity</b>: Linear to the number of elements in the container.
   BOOST_CONTAINER_NODISCARD inline
      friend bool operator>=(const segtor& x, const segtor& y)
   {  return static_cast<const base_type&>(x) >= static_cast<const base_type&>(y);  }

   //! <b>Effects</b>: x.swap(y)
   //!
   //! <b>Complexity</b>: Constant.
   inline friend void swap(segtor& x, segtor& y)
       BOOST_NOEXCEPT_IF(BOOST_NOEXCEPT(x.swap(y)))
   {  static_cast<base_type&>(x).swap(static_cast<base_type&>(y));  }

};


#ifndef BOOST_CONTAINER_NO_CXX17_CTAD

//! <b>Deduction guide</b>: allows a `segtor` to be constructed from the iterator
//! range <code>[first, last)</code>, deducing the element type from the value type
//! of `InputIterator` and using the default allocator.
template <typename InputIterator>
segtor(InputIterator, InputIterator) ->
   segtor<typename iter_value<InputIterator>::type>;

//! <b>Deduction guide</b>: allows a `segtor` to be constructed from the iterator
//! range <code>[first, last)</code>, deducing the element type from the value type
//! of `InputIterator` and taking the allocator type from the supplied allocator.
template <typename InputIterator, typename Allocator>
segtor(InputIterator, InputIterator, Allocator const&) ->
   segtor<typename iter_value<InputIterator>::type, Allocator>;

#endif

//! <b>Effects</b>: Erases all elements that compare equal to v from the container c.
//!
//! <b>Complexity</b>: Linear.
template <class T, class A, class O, class U>
inline typename segtor<T, A, O>::size_type erase(segtor<T, A, O>& c, const U& v)
{
   typename segtor<T, A, O>::size_type old_size = c.size();
   c.erase(boost::container::remove(c.begin(), c.end(), v), c.end());
   return old_size - c.size();
}

//! <b>Effects</b>: Erases all elements that satisfy the predicate pred from the container c.
//!
//! <b>Complexity</b>: Linear.
template <class T, class A, class O, class Pred>
inline typename segtor<T, A, O>::size_type erase_if(segtor<T, A, O>& c, Pred pred)
{
   typename segtor<T, A, O>::size_type old_size = c.size();
   c.erase(boost::container::remove_if(c.begin(), c.end(), pred), c.end());
   return old_size - c.size();
}

}  //namespace container {
}  //namespace boost {

#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

namespace boost {

template <class T, class Allocator, class Options>
struct has_trivial_destructor_after_move<boost::container::segtor<T, Allocator, Options> >
   : has_trivial_destructor_after_move<boost::container::deque_impl<T, Allocator, true, Options> >
{};

}  //namespace boost {

#endif   //#ifndef BOOST_CONTAINER_DOXYGEN_INVOKED

#include <boost/container/detail/config_end.hpp>

#endif   //#ifndef BOOST_CONTAINER_SEGTOR_HPP
