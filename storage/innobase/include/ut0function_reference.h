/* Copyright (c) 2023, 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#pragma once
#include <utility>
namespace ut {
template <typename>
class Function_reference;

/** A non-owning reference to a callable - a free function, like `sin`, or the
operator() of an object, together with pointer to that object, such a Less
functor or a lambda.
It is the users responsibility to ensure that the lifetime of the callable
referenced covers the lifetime of this Function_reference.
This is the price to be paid for the benefit that this object has fixed size
(just two pointers) and does not require dynamic allocation, which makes it
useful for callbacks and type erasure, in places where std::function would be to
expensive due to its need to copy the (variable size) object to dynamically
allocated space.*/
template <typename Return_type, typename... Args>
class Function_reference<Return_type(Args...)> {
 public:
  /* Most compilers allow storing pointer to function in pointer to obj, but
  this is not guaranteed by the C++17 standard, so we use a union. */
  union context_t {
    /* All function pointers are interchangeable, so lets use the simplest one.
    see https://stackoverflow.com/a/61617252/575699. The function pointer is
    first member of the union to assure one of the types will not cast to the
    first member's type in case someone uses {val} constructor, which is not
    able to init the second member of the union. Use C++20 designated
    initializers instead. */
    void (*fn_ptr)();
    void *obj_ptr;
  };
  using signature_t = Return_type(Args...);
  using ptr_t = Return_type (*)(const context_t &, Args...);

 private:
  /** A crucial context for making a call to the referenced function.
  In case of an object's method, that would be the pointer to the object.
  In case of a free function, this is just the free function itself. */
  context_t m_context;

  /** How to call the referenced function given the m_context.
  In case of an object's method, that would be a state-less non-capturing lambda
  which just calls the m_context->operator() with provided args.
  In case of a free function this will simply call m_context with provided args.
  It is a non-capturing lambda, i.e. lambda which does not require "this"
  pointer to be called because it has no state - for such a lambda one can take
  its address and store it (in m_call_with_context) and subsequently use it to
  call it as a regular function, passing m_context as the first argument.
  This is a crucial trick: instead of storing the pointer to operator() as a
  pointer-to-member of Callable, which would require the Function_reference type
  to depend on Callable, we hide any mention of Callable inside the lambda. */
  ptr_t m_call_with_context;

 public:
  /** Creates a reference to the callable->operator().
  @param[in]     callable
                     An object with operator() which you want to reference.
                     Note that the constness of callable impacts constness of
                     the operator() which will be picked if there are two.
                     For example a lambda.
                     This object must have life-time covering the life-time of
                     Function_reference.
  */
  template <typename Callable>
  Function_reference(Callable *callable)
      : m_context{.obj_ptr = (void *)callable},
        m_call_with_context([](const context_t &t, Args... args) {
          return (static_cast<Callable *>(t.obj_ptr)->operator())(
              std::forward<Args>(args)...);
        }) {}

  using rawfn_ptr_t = Return_type (*)(Args...);

  /** Creates a reference to a free function fn_ptr.
  @param[in]     fn_ptr
                     A pointer to a free function which you want to reference */
  Function_reference(rawfn_ptr_t fn_ptr)
      : m_context{.fn_ptr = fn_ptr},
        m_call_with_context([](const context_t &t, Args... args) {
          return (reinterpret_cast<Return_type (*)(Args...)>(t.fn_ptr))(
              std::forward<Args>(args)...);
        }) {}

  Return_type operator()(Args... args) const {
    return m_call_with_context(m_context, std::forward<Args>(args)...);
  }
};
}  // namespace ut
