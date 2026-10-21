// Copyright (C) 2015-2024 Andrzej Krzemienski.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/optional for documentation.
//
// You are welcome to contact the author at:
//  akrzemi1@gmail.com

#ifndef BOOST_OPTIONAL_DETAIL_OPTIONAL_REFERENCE_SPEC_AJK_03OCT2015_HPP
#define BOOST_OPTIONAL_DETAIL_OPTIONAL_REFERENCE_SPEC_AJK_03OCT2015_HPP

#ifdef BOOST_OPTIONAL_CONFIG_NO_PROPER_ASSIGN_FROM_CONST_INT
#ifndef BOOST_OPTIONAL_USES_UNION_IMPLEMENTATION
#include <boost/type_traits/is_integral.hpp>
#include <boost/type_traits/is_const.hpp>
#endif
#endif

#ifdef BOOST_OPTIONAL_USES_UNION_IMPLEMENTATION
# define BOOST_OPTIONAL_TT_PREFIX ::std
#else
# define BOOST_OPTIONAL_TT_PREFIX boost
# define BOOST_OPTIONAL_REQUIRES(...) BOOST_DEDUCED_TYPENAME boost::enable_if_c<__VA_ARGS__::value, bool>::type = false
#endif

# define BOOST_OPTIONAL_TT_TYPE(...) BOOST_DEDUCED_TYPENAME BOOST_OPTIONAL_TT_PREFIX::__VA_ARGS__::type
# define BOOST_OPTIONAL_TT_PRED(...) BOOST_OPTIONAL_TT_PREFIX::__VA_ARGS__::value

# if 1

namespace boost {

namespace detail {

#ifndef BOOST_OPTIONAL_DETAIL_NO_RVALUE_REFERENCES

template <class From>
void prevent_binding_rvalue()
{
#ifndef BOOST_OPTIONAL_CONFIG_ALLOW_BINDING_TO_RVALUES
    static_assert(BOOST_OPTIONAL_TT_PRED(is_lvalue_reference<From>),
                  "binding rvalue references to optional lvalue references is disallowed");
#endif
}

template <class T>
BOOST_OPTIONAL_TT_TYPE(remove_reference<T>)& forward_reference(T&& r)
{
    static_assert(BOOST_OPTIONAL_TT_PRED(is_lvalue_reference<T>),
                  "binding rvalue references to optional lvalue references is disallowed");
    return optional_detail::forward_<T>(r);
}

#endif // BOOST_OPTIONAL_DETAIL_NO_RVALUE_REFERENCES


template <class T>
struct is_const_integral
{
  static const bool value = BOOST_OPTIONAL_TT_PRED(is_const<T>) && BOOST_OPTIONAL_TT_PRED(is_integral<T>);
};

template <class T>
struct is_const_integral_bad_for_conversion
{
#if (!defined BOOST_OPTIONAL_CONFIG_ALLOW_BINDING_TO_RVALUES) && (defined BOOST_OPTIONAL_CONFIG_NO_PROPER_CONVERT_FROM_CONST_INT)
  static const bool value = is_const_integral<T>::value;
#else
  static const bool value = false;
#endif
};

template <class From>
void prevent_assignment_from_false_const_integral()
{
#ifndef BOOST_OPTIONAL_CONFIG_ALLOW_BINDING_TO_RVALUES
#ifdef BOOST_OPTIONAL_CONFIG_NO_PROPER_ASSIGN_FROM_CONST_INT
    // MSVC compiler without rvalue references: we need to disable the assignment from
    // const integral lvalue reference, as it may be an invalid temporary
    static_assert(!is_const_integral<From>::value,
                  "binding const lvalue references to integral types is disabled in this compiler");
#endif
#endif
}


template <class T>
struct is_optional_
{
  static const bool value = false;
};

template <class U>
struct is_optional_< ::boost::optional<U> >
{
  static const bool value = true;
};

template <class T>
struct is_no_optional
{
  static const bool value = !is_optional_<BOOST_OPTIONAL_DECAY(T)>::value;
};


template <class T, class U>
  struct is_same_decayed
  {
    static const bool value = BOOST_OPTIONAL_TT_PRED(is_same<T, BOOST_OPTIONAL_TT_TYPE(remove_reference<U>)>)
                           || BOOST_OPTIONAL_TT_PRED(is_same<T, const BOOST_OPTIONAL_TT_TYPE(remove_reference<U>)>);
  };

template <class T, class U>
struct no_unboxing_cond
{
  static const bool value = is_no_optional<U>::value && !is_same_decayed<T, U>::value;
};


} // namespace detail

template <class T>
class optional<T&> : public optional_detail::optional_tag
{
    T* ptr_;

public:
    typedef T& value_type;
    typedef T& reference_type;
    typedef T& reference_const_type;
    typedef T& rval_reference_type;
    typedef T* pointer_type;
    typedef T* pointer_const_type;

    BOOST_CONSTEXPR optional() BOOST_NOEXCEPT : ptr_() {}
    BOOST_CONSTEXPR optional(none_t) BOOST_NOEXCEPT : ptr_() {}

    template <class U>
    BOOST_CONSTEXPR explicit optional(const optional<U&>& rhs) BOOST_NOEXCEPT : ptr_(rhs.get_ptr()) {}
    BOOST_CONSTEXPR optional(const optional& rhs) BOOST_NOEXCEPT : ptr_(rhs.get_ptr()) {}

    // the following two implement a 'conditionally explicit' constructor: condition is a hack for buggy compilers with screwed conversion construction from const int
    template <class U,
              BOOST_OPTIONAL_REQUIRES(detail::is_same_decayed<T, U>),
              BOOST_OPTIONAL_REQUIRES(detail::is_const_integral_bad_for_conversion<U>)>
    BOOST_CONSTEXPR explicit
    optional(U& rhs) BOOST_NOEXCEPT
    : ptr_(boost::addressof(rhs)) {}

    template <class U,
              BOOST_OPTIONAL_REQUIRES(detail::is_same_decayed<T, U>),
              BOOST_OPTIONAL_REQUIRES(!detail::is_const_integral_bad_for_conversion<U>)>
    BOOST_CONSTEXPR
    optional(U& rhs) BOOST_NOEXCEPT
    : ptr_(boost::addressof(rhs)) {}

    BOOST_CXX14_CONSTEXPR optional& operator=(const optional& rhs) BOOST_NOEXCEPT { ptr_ = rhs.get_ptr(); return *this; }
    template <class U>
        BOOST_CXX14_CONSTEXPR optional& operator=(const optional<U&>& rhs) BOOST_NOEXCEPT { ptr_ = rhs.get_ptr(); return *this; }
    BOOST_CXX14_CONSTEXPR optional& operator=(none_t) BOOST_NOEXCEPT { ptr_ = 0; return *this; }


    BOOST_CXX14_CONSTEXPR void swap(optional& rhs) BOOST_NOEXCEPT { ::std::swap(ptr_, rhs.ptr_); }
    constexpr T& get() const { return BOOST_OPTIONAL_ASSERTED_EXPRESSION(ptr_, *ptr_); }

    constexpr T* get_ptr() const BOOST_NOEXCEPT { return ptr_; }
    constexpr T* operator->() const { return BOOST_OPTIONAL_ASSERTED_EXPRESSION(ptr_, ptr_); }
    constexpr T& operator*() const { return BOOST_OPTIONAL_ASSERTED_EXPRESSION(ptr_, *ptr_); }

    constexpr T& value() const
    {
      return this->is_initialized() ?
             this->get() :
             (boost::throw_exception(boost::bad_optional_access()), this->get());
    }

    constexpr explicit operator bool() const BOOST_NOEXCEPT { return ptr_ != 0; }

    BOOST_CXX14_CONSTEXPR void reset() BOOST_NOEXCEPT { ptr_ = 0; }

    constexpr bool is_initialized() const BOOST_NOEXCEPT { return ptr_ != 0; }
    constexpr bool has_value() const BOOST_NOEXCEPT { return ptr_ != 0; }

    template <typename F>
    constexpr optional<typename optional_detail::result_of<F, reference_const_type>::type>
    map(F f) const
    {
      return this->has_value() ?
             f(get()) :
             optional<typename optional_detail::result_of<F, reference_const_type>::type>();
    }

    template <typename F>
    constexpr optional<typename optional_detail::result_value_type<F, reference_const_type>::type>
    flat_map(F f) const
      {
        return this->has_value() ?
               f(get()) :
               optional<typename optional_detail::result_value_type<F, reference_const_type>::type>();
      }

#ifndef BOOST_OPTIONAL_DETAIL_NO_RVALUE_REFERENCES

    optional(T&& /* rhs */) BOOST_NOEXCEPT { detail::prevent_binding_rvalue<T&&>(); }

    template <class R, BOOST_OPTIONAL_REQUIRES(detail::no_unboxing_cond<T, R>)>
        BOOST_CXX14_CONSTEXPR optional(R&& r) BOOST_NOEXCEPT
        : ptr_(boost::addressof(r)) { detail::prevent_binding_rvalue<R>(); }

    template <class R, BOOST_OPTIONAL_REQUIRES(detail::is_no_optional<R>)>
        BOOST_CXX14_CONSTEXPR optional(bool cond, R&& r) BOOST_NOEXCEPT
        : ptr_(cond ? boost::addressof(r) : 0) { detail::prevent_binding_rvalue<R>(); }

    template <class R, BOOST_OPTIONAL_REQUIRES(detail::is_no_optional<R>)>
        BOOST_CXX14_CONSTEXPR optional<T&>&
        operator=(R&& r) BOOST_NOEXCEPT { detail::prevent_binding_rvalue<R>(); ptr_ = boost::addressof(r); return *this; }

    template <class R, BOOST_OPTIONAL_REQUIRES(detail::is_no_optional<R>)>
        BOOST_CXX14_CONSTEXPR void emplace(R&& r) BOOST_NOEXCEPT
        { detail::prevent_binding_rvalue<R>(); ptr_ = boost::addressof(r); }

    template <class R, BOOST_OPTIONAL_REQUIRES(detail::is_no_optional<R>)>
      BOOST_CXX14_CONSTEXPR T& get_value_or(R&& r) const BOOST_NOEXCEPT
      { detail::prevent_binding_rvalue<R>(); return ptr_ ? *ptr_ : r; }

    template <class R, BOOST_OPTIONAL_REQUIRES(detail::is_no_optional<R>)>
        BOOST_CXX14_CONSTEXPR T& value_or(R&& r) const BOOST_NOEXCEPT
        { detail::prevent_binding_rvalue<R>(); return ptr_ ? *ptr_ : r; }

    template <class R, BOOST_OPTIONAL_REQUIRES(detail::is_no_optional<R>)>
      BOOST_CXX14_CONSTEXPR void reset(R&& r) BOOST_NOEXCEPT
      { detail::prevent_binding_rvalue<R>(); ptr_ = boost::addressof(r); }

    template <class F>
        BOOST_CXX14_CONSTEXPR T& value_or_eval(F f) const { return ptr_ ? *ptr_ : detail::forward_reference(f()); }

#else  // BOOST_OPTIONAL_DETAIL_NO_RVALUE_REFERENCES


    // the following two implement a 'conditionally explicit' constructor
    template <class U,
              BOOST_OPTIONAL_REQUIRES(detail::no_unboxing_cond<T, U>),
              BOOST_OPTIONAL_REQUIRES(detail::is_const_integral_bad_for_conversion<U>)>
      BOOST_CXX14_CONSTEXPR explicit optional(U& v) BOOST_NOEXCEPT
      : ptr_(boost::addressof(v)) { }

    template <class U,
              BOOST_OPTIONAL_REQUIRES(detail::no_unboxing_cond<T, U>),
              BOOST_OPTIONAL_REQUIRES(!detail::is_const_integral_bad_for_conversion<U>)>
      BOOST_CXX14_CONSTEXPR optional(U& v) BOOST_NOEXCEPT
      : ptr_(boost::addressof(v)) { }

    template <class U,
              BOOST_OPTIONAL_REQUIRES(detail::is_no_optional<U>)>
      BOOST_CXX14_CONSTEXPR optional(bool cond, U& v) BOOST_NOEXCEPT : ptr_(cond ? boost::addressof(v) : 0) {}

    template <class U,
              BOOST_OPTIONAL_REQUIRES(detail::is_no_optional<U>)>
      BOOST_CXX14_CONSTEXPR
      optional<T&>& operator=(U& v) BOOST_NOEXCEPT
      {
        detail::prevent_assignment_from_false_const_integral<U>();
        ptr_ = boost::addressof(v); return *this;
      }

    template <class U,
              BOOST_OPTIONAL_REQUIRES(detail::is_no_optional<U>)>
        BOOST_CXX14_CONSTEXPR void emplace(U& v) BOOST_NOEXCEPT
        { ptr_ = boost::addressof(v); }

    template <class U,
              BOOST_OPTIONAL_REQUIRES(detail::is_no_optional<U>)>
      BOOST_CXX14_CONSTEXPR T& get_value_or(U& v) const BOOST_NOEXCEPT
      { return ptr_ ? *ptr_ : v; }

    template <class U,
              BOOST_OPTIONAL_REQUIRES(detail::is_no_optional<U>)>
      BOOST_CXX14_CONSTEXPR T& value_or(U& v) const BOOST_NOEXCEPT
      { return ptr_ ? *ptr_ : v; }

    template <class U,
              BOOST_OPTIONAL_REQUIRES(detail::is_no_optional<U>)>
      BOOST_CXX14_CONSTEXPR void reset(U& v) BOOST_NOEXCEPT
      { ptr_ = boost::addressof(v); }

    template <class F>
      BOOST_CXX14_CONSTEXPR T& value_or_eval(F f) const { return ptr_ ? *ptr_ : f(); }

#endif // BOOST_OPTIONAL_DETAIL_NO_RVALUE_REFERENCES
};

template <class T>
  BOOST_CXX14_CONSTEXPR void swap ( optional<T&>& x, optional<T&>& y) BOOST_NOEXCEPT
{
  x.swap(y);
}

} // namespace boost

#endif // 1/0

#endif // header guard
