// Copyright (C) 2026 Andrzej Krzemieński.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/optional for documentation.
//
// You are welcome to contact the author at:
//  akrzemi1@gmail.com
//
//
// This header provides definitions required by any specialization of
// optional<>.

#ifndef BOOST_OPTIONAL_DETAIL_UNION_OPTIONAL_01FEB2026_HPP
#define BOOST_OPTIONAL_DETAIL_UNION_OPTIONAL_01FEB2026_HPP

//#include <initializer_list>
#include <boost/assert.hpp>
#include <boost/core/invoke_swap.hpp>
#include <boost/throw_exception.hpp>
#include <boost/optional/bad_optional_access.hpp>



// This macro shall be put in the position of a template parameter.
// It emulates the requires clause.
# define BOOST_OPTIONAL_REQUIRES(...) typename ::std::enable_if<__VA_ARGS__::value, bool>::type = false


# ifdef __cpp_guaranteed_copy_elision
#   if __cpp_guaranteed_copy_elision
#     define BOOST_OPTIONAL_CONSTEXPR_COPY
#   endif
# endif


template <typename T, typename U>
struct fail_hard_on_nonconvertible
{
  static_assert(::std::is_convertible<U&&, T>::value, "The argument must be convertible to T");
  using type = bool;
};

// Missing C++17 type traits
namespace boost { namespace optional_detail {

template <class...>
struct conjunction : ::std::true_type {};

template <class B1>
struct conjunction<B1> : B1 {};

template <class B1, class... Bn>
struct conjunction<B1, Bn...>
    : ::std::conditional<bool(B1::value), conjunction<Bn...>, B1>::type {};

}}

namespace boost { namespace optional_detail {

// Tag to indicate a special-purpose constructor
BOOST_INLINE_VARIABLE constexpr struct trivial_init_t{} trivial_init{};


template <class T>
union constexpr_union_storage_t
{
    static_assert(::std::is_trivially_destructible<T>::value, "!!");

    unsigned char dummy_;
    T value_;

    constexpr constexpr_union_storage_t( trivial_init_t ) noexcept : dummy_() {};

#if defined(BOOST_GCC) && (__GNUC__ >= 7)
// false positive, see https://github.com/boostorg/variant2/issues/55,
//   https://github.com/boostorg/url/issues/979
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

    template <class... Args>
    constexpr constexpr_union_storage_t( Args&&... args ) : value_(forward_<Args>(args)...) {}

#if defined(BOOST_GCC) && (__GNUC__ >= 7)
# pragma GCC diagnostic pop
#endif

    //~constexpr_union_storage_t() = default; // No need to destroy a trivially-destructible type
};

template <class T>
union fallback_union_storage_t
{
  unsigned char dummy_;
  T value_;

  constexpr fallback_union_storage_t( trivial_init_t ) noexcept : dummy_() {};

#if defined(BOOST_GCC) && (__GNUC__ >= 7)
// false positive, see https://github.com/boostorg/variant2/issues/55,
//   https://github.com/boostorg/url/issues/979
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

  template <class... Args>
  constexpr fallback_union_storage_t( Args&&... args ) : value_(forward_<Args>(args)...) {}

#if defined(BOOST_GCC) && (__GNUC__ >= 7)
# pragma GCC diagnostic pop
#endif

  ~fallback_union_storage_t(){} // My owner will destroy the `T` if needed.
                                // Cannot default in a union with nontrivial `T`.
};


// `guarded_storage` is a union + a flag indicating if a `T` has been initialized.
// this way the destructor knows if it should destroy the `T`.
template <class T>
struct constexpr_guarded_storage
{
    static_assert(::std::is_trivially_destructible<T>::value, "!!");

    bool init_;
    constexpr_union_storage_t<T> storage_;

    constexpr constexpr_guarded_storage() noexcept : init_(false), storage_(trivial_init) {};

    explicit constexpr constexpr_guarded_storage(const T& v) : init_(true), storage_(v) {}

    explicit constexpr constexpr_guarded_storage(T&& v) : init_(true), storage_(move_(v)) {}

    template <class... Args> explicit constexpr constexpr_guarded_storage(optional_ns::in_place_init_t, Args&&... args)
      : init_(true), storage_(forward_<Args>(args)...) {}

    // template <class U, class... Args, BOOST_OPTIONAL_REQUIRES(::std::is_constructible<T, ::std::initializer_list<U>>)>
    // constexpr explicit constexpr_guarded_storage(optional_ns::in_place_init_t, ::std::initializer_list<U> il, Args&&... args)
    //   : init_(true), storage_(il, forward_<Args>(args)...) {}

    BOOST_CXX14_CONSTEXPR void reset () noexcept { init_ = false; }

    //~constexpr_guarded_storage() = default;

#if (defined(_MSC_VER) && 1910 <= _MSC_VER && _MSC_VER <= 1916)
  // Workaround for MSVC 14.1x bug where it eagerly tries to define the copy/move operations
  // these are declared but never defined
    constexpr_guarded_storage(const constexpr_guarded_storage&);
    constexpr_guarded_storage(constexpr_guarded_storage&&);
    constexpr_guarded_storage& operator=(const constexpr_guarded_storage&);
    constexpr_guarded_storage& operator=(constexpr_guarded_storage&&);
#endif
};


template <class T>
struct fallback_guarded_storage
{
    bool init_;
    fallback_union_storage_t<T> storage_;

    constexpr fallback_guarded_storage() noexcept : init_(false), storage_(trivial_init) {};

    explicit constexpr fallback_guarded_storage(const T& v) : init_(true), storage_(v) {}

    explicit constexpr fallback_guarded_storage(T&& v) : init_(true), storage_(move_(v)) {}

    template <class... Args> explicit constexpr fallback_guarded_storage(optional_ns::in_place_init_t, Args&&... args)
        : init_(true), storage_(forward_<Args>(args)...) {}

    // template <class U, class... Args, BOOST_OPTIONAL_REQUIRES(::std::is_constructible<T, ::std::initializer_list<U>>)>
    // explicit fallback_guarded_storage(optional_ns::in_place_init_t, ::std::initializer_list<U> il, Args&&... args)
    //     : init_(true), storage_(il, forward_<Args>(args)...) {}

    void reset() noexcept
    {
      if (init_)
      {
        storage_.value_.T::~T();
        init_ = false;
      }

    }

    ~fallback_guarded_storage() { if (init_) storage_.value_.T::~T(); }

#if (defined(_MSC_VER) && 1910 <= _MSC_VER && _MSC_VER <= 1916)
// Workaround for MSVC 14.1x bug where it eagerly tries to define the copy/move operations
// These are declared but never defined
  fallback_guarded_storage(const fallback_guarded_storage&);
  fallback_guarded_storage(fallback_guarded_storage&&);
  fallback_guarded_storage& operator=(const fallback_guarded_storage&);
  fallback_guarded_storage& operator=(fallback_guarded_storage&&);
#endif
};


template <class T>
using guarded_storage = typename ::std::conditional<
    ::std::is_trivially_destructible<T>::value,                       // if possible
    constexpr_guarded_storage<typename ::std::remove_const<T>::type>, // use storage with trivial destructor
    fallback_guarded_storage<typename ::std::remove_const<T>::type>
>::type;


}}


namespace boost {

  template <class T>
  class optional : public optional_detail::optional_tag
  {
    using storage_t = optional_detail::guarded_storage<T>;
    storage_t storage;
    static_assert( !::std::is_same<typename std::decay<T>::type, none_t>::value, "optional<none_t> is illegal" );
    static_assert( !::std::is_same<typename std::decay<T>::type, in_place_init_t>::value, "optional<in_place_init_t> is illegal" );
    static_assert( !::std::is_same<typename std::decay<T>::type, in_place_init_if_t>::value, "optional<in_place_init_if_t> is illegal" );

    BOOST_CXX14_CONSTEXPR typename ::std::remove_const<T>::type* dataptr() { return ::boost::addressof(storage.storage_.value_); }
    constexpr const T* dataptr() const { return ::boost::addressof(storage.storage_.value_); }

    constexpr const T& contained_val() const& { return storage.storage_.value_; }
    BOOST_CXX14_CONSTEXPR T&& contained_val() && { return optional_detail::move_(storage.storage_.value_); }
    BOOST_CXX14_CONSTEXPR T& contained_val() & { return storage.storage_.value_; }

    template <typename... Args>
    BOOST_OPTIONAL_CXX20_CONSTEXPR void initialize(Args&&... args)
    {
      BOOST_ASSERT(!storage.init_);
      ::new (static_cast<void*>(dataptr())) T(optional_detail::forward_<Args>(args)...);
      storage.init_ = true;
    }

  #ifdef BOOST_OPTIONAL_CONSTEXPR_COPY
    // The conditional initialization of storage needs to employ a factory
    // function, so that we can utilize the guaranteed copy elision on the
    // return type.
    // an alternative -- using a conditional operator in the initializer --
    // does not work in MSVC 14.2 and 14.3.
    template <typename... U>
    static constexpr storage_t conditional_storage_from_values(bool cond, U&&... v)
    {
      if (cond)
        return storage_t(in_place_init, optional_detail::forward_<U>(v)...);
      else
        return storage_t();
    }

    template <typename OU>
    static constexpr storage_t conditional_storage_from_optional(OU&& ou)
    {
      if (ou.has_value())
        return storage_t(in_place_init, *optional_detail::forward_<OU>(ou));
      else
        return storage_t();
    }
  #endif

  public:
    using value_type = T;
    using unqualified_value_type = typename ::std::remove_const<T>::type;

    using reference_type = T&;
    using reference_const_type = T const&;
    using argument_type = T const&;
    using rval_reference_type = T&&;
    using reference_type_of_temporary_wrapper = T&&;
    using pointer_type = T*;
    using pointer_const_type = T const*;


    constexpr bool is_initialized() const noexcept { return storage.init_; }

    constexpr optional() noexcept : storage()  {};
    constexpr optional(none_t) noexcept : storage() {};

    constexpr optional(const T& v) : storage(v) {}
    constexpr optional(T&& v) : storage(optional_detail::move_(v)) {}

#ifdef BOOST_OPTIONAL_CONSTEXPR_COPY
    constexpr optional(bool cond, const T& v)
    : storage(conditional_storage_from_values(cond, v))
    {}

    constexpr optional(bool cond, T&& v)
    : storage(conditional_storage_from_values(cond, optional_detail::move_(v)))
    {}

    constexpr optional(const optional& rhs)
    : storage(conditional_storage_from_optional(rhs))
    {}

    constexpr optional(optional&& rhs)
      noexcept(::std::is_nothrow_move_constructible<T>::value)
    : storage(conditional_storage_from_optional(optional_detail::move_(rhs)))
    {}

    template <typename U, BOOST_OPTIONAL_REQUIRES(::std::is_constructible<T, U const&>)>
    constexpr explicit optional(optional<U> const& rhs)
    : storage(conditional_storage_from_optional(rhs))
    {}

    template <typename U, BOOST_OPTIONAL_REQUIRES(::std::is_constructible<T, U&&>)>
    constexpr explicit optional(optional<U> && rhs)
    : storage(conditional_storage_from_optional(optional_detail::move_(rhs)))
    {}

    template <typename... Args>
    constexpr explicit optional( in_place_init_if_t, bool cond, Args&&... args )
    : storage(conditional_storage_from_values(cond, optional_detail::forward_<Args>(args)...))
    {}
#else
    optional(bool cond, const T& v)
    : storage()
    {
      if (cond)
        initialize(v);
    }

    optional(bool cond, T&& v)
    : storage()
    {
      if (cond)
        initialize(optional_detail::move_(v));
    }

    optional(const optional& rhs)
    : storage()
    {
      if (rhs.is_initialized())
        initialize(*rhs);
    }

    optional(optional&& rhs)
      noexcept(::std::is_nothrow_move_constructible<T>::value)
    : storage()
    {
      if (rhs.is_initialized())
        initialize(*optional_detail::move_(rhs));
    }

    template <typename U, BOOST_OPTIONAL_REQUIRES(::std::is_constructible<T, U const&>)>
    explicit optional(optional<U> const& rhs)
    : storage()
    {
      if (rhs.is_initialized())
        initialize(*rhs);
    }

    template <typename U, BOOST_OPTIONAL_REQUIRES(::std::is_constructible<T, U&&>)>
    explicit optional(optional<U> && rhs)
    : storage()
    {
      if (rhs.is_initialized())
        initialize(*optional_detail::move_(rhs));
    }

    template <typename... Args>
    explicit optional( in_place_init_if_t, bool cond, Args&&... args )
    : storage()
    {
      if (cond)
        initialize(optional_detail::forward_<Args>(args)...);
    }
#endif // BOOST_OPTIONAL_CONSTEXPR_COPY

    template <typename FT,
              BOOST_OPTIONAL_REQUIRES(optional_detail::is_typed_in_place_factory<FT>)>
    /*non-constexpr (deprecated)*/
    explicit optional (FT&& factory)
    : storage()
    {
      factory.apply(this->dataptr());
      storage.init_ = true ;
    }

    template <typename FT,
              BOOST_OPTIONAL_REQUIRES(optional_detail::is_in_place_factory<FT>)>
    /*non-constexpr (deprecated)*/
    explicit optional (FT&& factory)
    : storage()
    {
      boost_optional_detail::construct<value_type>(factory, this->dataptr());
      storage.init_ = true;
    }

    template <typename U,
              BOOST_OPTIONAL_REQUIRES(::std::is_constructible<T, U&&>),
              BOOST_OPTIONAL_REQUIRES(!optional_detail::is_typed_in_place_factory<U>),
              BOOST_OPTIONAL_REQUIRES(!optional_detail::is_in_place_factory<U>),
              BOOST_OPTIONAL_REQUIRES(!BOOST_OPTIONAL_IS_TAGGED(optional_detail::optional_tag, U))
              >
    constexpr explicit optional(U&& v)
    : storage(optional_ns::in_place_init, optional_detail::forward_<U>(v))
    {}

    template <typename... Args>
    constexpr explicit optional( in_place_init_t, Args&&... args )
    : storage(in_place_init, optional_detail::forward_<Args>(args)...)
    {}

    BOOST_CXX14_CONSTEXPR operator optional<T&>() & noexcept
    {
      return this->has_value() ? optional<T&>(**this) : optional<T&>();
    }

    BOOST_CONSTEXPR operator optional<const T&>() const& noexcept
    {
      return this->has_value() ? optional<const T&>(**this) : optional<const T&>();
    }

    BOOST_CXX14_CONSTEXPR operator optional<T&>() && noexcept = delete;
    BOOST_CONSTEXPR operator optional<const T&>() const&& noexcept = delete;


    BOOST_CXX14_CONSTEXPR void reset() noexcept
    {
      storage.reset();
    }

    BOOST_OPTIONAL_CXX20_CONSTEXPR void reset(const T& v)
    {
      *this = v;
    }

    template <typename... Args>
    BOOST_OPTIONAL_CXX20_CONSTEXPR void emplace(Args&&... args)
    {
      reset();
      // <-- now we are not containing a value
      initialize(optional_detail::forward_<Args>(args)...);
    }


    BOOST_CXX14_CONSTEXPR optional& operator=(none_t) noexcept
    {
      reset();
      return *this;
    }

    BOOST_OPTIONAL_CXX20_CONSTEXPR optional& operator=(const optional& rhs)
    {
      if (has_value())
      {
        if (rhs.has_value())
          **this = *rhs;
        else
          reset();
      }
      else
      {
        if (rhs.has_value())
          initialize(*rhs);
      }

      return *this;
    }

    BOOST_OPTIONAL_CXX20_CONSTEXPR optional& operator=(optional&& rhs)
      noexcept(::std::is_nothrow_move_assignable<T>::value && ::std::is_nothrow_move_constructible<T>::value)
    {
      if (has_value())
      {
        if (rhs.has_value())
          **this = *optional_detail::move_(rhs);
        else
          reset();
      }
      else
      {
        if (rhs.has_value())
          initialize(*optional_detail::move_(rhs) );
      }

      return *this;
    }

    template <typename U>
    BOOST_OPTIONAL_CXX20_CONSTEXPR optional& operator=(const optional<U>& rhs)
    {
      if (has_value())
      {
        if (rhs.has_value())
          **this = *rhs;
        else
          reset();
      }
      else
      {
        if (rhs.has_value())
          initialize(*rhs);
      }

      return *this;
    }

    template <typename U>
    BOOST_OPTIONAL_CXX20_CONSTEXPR optional& operator=(optional<U>&& rhs)
    {
      if (has_value())
      {
        if (rhs.has_value())
          **this = *optional_detail::move_(rhs);
        else
          reset();
      }
      else
      {
        if (rhs.has_value())
          initialize(*optional_detail::move_(rhs));
      }

      return *this;
    }

    template <class U = typename ::std::remove_cv<T>::type,
              BOOST_OPTIONAL_REQUIRES(!::std::is_same<typename ::std::decay<U>::type, optional>),
              BOOST_OPTIONAL_REQUIRES(!optional_detail::conjunction<::std::is_scalar<T>, ::std::is_same<T, BOOST_OPTIONAL_DECAY(U)>>),
              BOOST_OPTIONAL_REQUIRES(::std::is_constructible<T, U>),
              BOOST_OPTIONAL_REQUIRES(::std::is_assignable<T&, U>),
              BOOST_OPTIONAL_REQUIRES(!optional_detail::is_typed_in_place_factory<U>),
              BOOST_OPTIONAL_REQUIRES(!optional_detail::is_in_place_factory<U>)
             >
    BOOST_OPTIONAL_CXX20_CONSTEXPR optional& operator=(U&& v)
    {
      if (is_initialized())
        contained_val() = optional_detail::forward_<U>(v);
      else
        initialize(optional_detail::forward_<U>(v));
      return *this;
    }

    template <class F, BOOST_OPTIONAL_REQUIRES(optional_detail::is_in_place_factory<F>)>
    /*non-constexpr (deprecated)*/
    optional& operator=(F&& factory)
    {
      reset();
      boost_optional_detail::construct<value_type>(factory, this->dataptr());
      storage.init_ = true;
      return *this;
    }

    template <class F, BOOST_OPTIONAL_REQUIRES(optional_detail::is_typed_in_place_factory<F>)>
    /*non-constexpr (deprecated)*/
    optional& operator=(F&& factory)
    {
      reset();
      factory.apply(this->dataptr());
      storage.init_ = true;
      return *this;
    }

    BOOST_OPTIONAL_CXX20_CONSTEXPR
    void swap(optional& rhs)
      noexcept(::std::is_nothrow_move_constructible<T>::value && noexcept(boost::core::invoke_swap(*rhs, *rhs)))
    {
      if (is_initialized())
      {
        if (rhs.is_initialized())
          boost::core::invoke_swap(contained_val(), rhs.contained_val());
        else
          { rhs.initialize(optional_detail::move_(*this).contained_val()); reset(); }
      }
      else
      {
        if (rhs.is_initialized())
          { initialize(optional_detail::move_(rhs).contained_val()); rhs.reset(); }
      }
    }

    ~optional() = default; // The destructor in `storage`, based on the specialization
                           // will be trivial or not.

    constexpr bool has_value() const noexcept { return this->is_initialized(); }
    constexpr explicit operator bool() const noexcept { return this->is_initialized(); }

    constexpr             reference_const_type get() const { return BOOST_OPTIONAL_ASSERTED_EXPRESSION(this->is_initialized(), this->contained_val()); }
    BOOST_CXX14_CONSTEXPR reference_type       get()       { BOOST_ASSERT(this->is_initialized()) ; return this->contained_val(); }

    //BOOST_DEPRECATED("use `value_or(v)` instead")
    reference_const_type get_value_or (reference_const_type v) const { return this->is_initialized() ? this->contained_val() : v; }

    //BOOST_DEPRECATED("use `value_or(v)` instead")
    reference_type       get_value_or (reference_type       v)       { return this->is_initialized() ? this->contained_val() : v; }

    pointer_const_type get_ptr() const { return is_initialized() ? dataptr() : nullptr; }
    pointer_type       get_ptr()       { return is_initialized() ? dataptr() : nullptr; }

    constexpr             reference_const_type operator*() const& { return this->get(); }
    BOOST_CXX14_CONSTEXPR reference_type       operator*() &      { return this->get(); }
    BOOST_CXX14_CONSTEXPR reference_type_of_temporary_wrapper operator*() && { return optional_detail::move_(this->get()); }

    constexpr             pointer_const_type operator->() const { return BOOST_OPTIONAL_ASSERTED_EXPRESSION(this->is_initialized(), this->dataptr()); }
    BOOST_CXX14_CONSTEXPR pointer_type       operator->()       { BOOST_ASSERT(this->is_initialized()) ; return this->dataptr(); }

    constexpr reference_const_type value() const&
    {
      return this->is_initialized() ? this->get() : (boost::throw_exception(boost::bad_optional_access()), this->get());
    }

    BOOST_CXX14_CONSTEXPR reference_type value() &
    {
      if (this->is_initialized())
        return this->get();
      else
        boost::throw_exception(boost::bad_optional_access());
    }

    BOOST_CXX14_CONSTEXPR reference_type_of_temporary_wrapper value() &&
    {
      if (this->is_initialized())
        return optional_detail::move_(this->get());
      else
        boost::throw_exception(boost::bad_optional_access());
    }

    template <class U = typename ::std::remove_cv<T>::type,
              typename fail_hard_on_nonconvertible<T, U>::type = true>
    constexpr value_type value_or(U&& v) const&
    {
      return this->is_initialized() ? get() : T(optional_detail::forward_<U>(v));
    }

    template <class U = typename ::std::remove_cv<T>::type>
    BOOST_CXX14_CONSTEXPR value_type value_or(U&& v) &&
    {
      if (this->is_initialized())
        return optional_detail::move_(get());
      else
        return optional_detail::forward_<U>(v);
    }

    template <typename F,
              typename fail_hard_on_nonconvertible<T, decltype(optional_detail::declval_<F>()())>::type = true>
    constexpr value_type value_or_eval(F f) const&
    {
      return this->is_initialized() ? get() : value_type(f());
    }

    template <typename F>
    BOOST_CXX14_CONSTEXPR value_type value_or_eval ( F f ) &&
    {
      if (this->is_initialized())
        return optional_detail::move_(get());
      else
        return f();
    }

    template <typename F>
    BOOST_CXX14_CONSTEXPR optional<typename optional_detail::result_of<F, reference_type>::type>
    map(F f) &
    {
      if (this->has_value())
        return f(get());
      else
        return none;
    }

    template <typename F>
    constexpr optional<typename optional_detail::result_of<F, reference_const_type>::type>
    map(F f) const&
    {
      return this->has_value() ?
             f(get()) :
             optional<typename optional_detail::result_of<F, reference_const_type>::type>();
    }

    template <typename F>
    BOOST_CXX14_CONSTEXPR optional<typename optional_detail::result_of<F, reference_type_of_temporary_wrapper>::type>
    map(F f) &&
    {
      if (this->has_value())
        return f(optional_detail::move_(this->get()));
      else
        return none;
    }

    template <typename F>
    BOOST_CXX14_CONSTEXPR optional<typename optional_detail::result_value_type<F, reference_type>::type>
    flat_map(F f) &
    {
      if (this->has_value())
        return f(get());
      else
        return none;
    }

    template <typename F>
    constexpr optional<typename optional_detail::result_value_type<F, reference_const_type>::type>
    flat_map(F f) const&
    {
      return this->has_value() ?
             f(get()) :
             optional<typename optional_detail::result_value_type<F, reference_const_type>::type>();
    }

    template <typename F>
    BOOST_CXX14_CONSTEXPR optional<typename optional_detail::result_value_type<F, reference_type_of_temporary_wrapper>::type>
    flat_map(F f) &&
    {
      if (this->has_value())
        return f(optional_detail::move_(get()));
      else
        return none;
    }

  };

  template <typename T>
  BOOST_OPTIONAL_CXX20_CONSTEXPR
  void swap(optional<T>& lhs, optional<T>& rhs)
    noexcept(::std::is_nothrow_move_constructible<T>::value && noexcept(boost::core::invoke_swap(*lhs, *rhs)))
  {
    lhs.swap(rhs);
  }

}


#endif // BOOST_OPTIONAL_DETAIL_UNION_OPTIONAL_01FEB2026_HPP
