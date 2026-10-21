/* Copyright 2006-2024 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/flyweight for library home page.
 */

#ifndef BOOST_FLYWEIGHT_KEY_VALUE_HPP
#define BOOST_FLYWEIGHT_KEY_VALUE_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <boost/assert.hpp>
#include <boost/config/workaround.hpp>
#include <boost/flyweight/detail/perfect_fwd.hpp>
#include <boost/flyweight/detail/value_tag.hpp>
#include <boost/flyweight/key_value_fwd.hpp>
#include <boost/mpl/assert.hpp>
#include <boost/type_traits/aligned_storage.hpp>
#include <boost/type_traits/alignment_of.hpp> 
#include <boost/type_traits/declval.hpp>
#include <boost/type_traits/is_same.hpp>
#include <new>

/* key-value policy: flywewight lookup is based on Key, which also serves
 * to construct Value only when needed (new factory entry). key_value is
 * used to avoid the construction of temporary values when such construction
 * is expensive.
 * Optionally, KeyFromValue extracts the key from a value, which
 * is needed in expressions like this:
 *
 *  typedef flyweight<key_value<Key,Value> > fw_t;
 *  fw_t  fw;
 *  Value v;
 *  fw=v; // no key explicitly given
 *
 * If no KeyFromValue is provided, this latter expression fails to compile.
 */

namespace boost{

namespace flyweights{

namespace detail{

template<typename Key,typename Value,typename KeyFromValue>
struct variant_key_value:value_marker
{
  typedef Key   key_type;
  typedef Value value_type;

  class rep_type
  {
  public:
    /* template ctors */

#define BOOST_FLYWEIGHT_PERFECT_FWD_CTR_BODY(args)       \
  :value_cted(false)                                     \
{                                                        \
  new(key_ptr())key_type(BOOST_FLYWEIGHT_FORWARD(args)); \
}

  BOOST_FLYWEIGHT_PERFECT_FWD(
    explicit rep_type,
    BOOST_FLYWEIGHT_PERFECT_FWD_CTR_BODY)

#undef BOOST_FLYWEIGHT_PERFECT_FWD_CTR_BODY

    rep_type(const rep_type& x):value_cted(false)
    {
      if(!x.value_cted)new(key_ptr())key_type(*x.key_ptr());
      else            new(key_ptr())key_type(key_from_value(*x.value_ptr()));
    }

    rep_type(const value_type& x):value_cted(false)
    {
      new(key_ptr())key_type(key_from_value(x));
    }

#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
    rep_type(rep_type&& x):value_cted(false)
    {
      if(!x.value_cted)new(key_ptr())key_type(std::move(*x.key_ptr()));
      else             new(key_ptr())key_type(key_from_value(*x.value_ptr()));
    }

    rep_type(value_type&& x):value_cted(false)
    {
      new(key_ptr())key_type(key_from_value(x));
    }
#endif

    ~rep_type()
    {
      if(value_cted)value_ptr()->~value_type();
      else          key_ptr()->~key_type();
    }

    operator const key_type&()const
    BOOST_NOEXCEPT_IF(noexcept(
      boost::declval<KeyFromValue>()(boost::declval<const value_type&>())))
    {
      if(value_cted)return key_from_value(*value_ptr());
      else          return *key_ptr();
    }

    operator const value_type&()const
    {
      BOOST_ASSERT(value_cted);
      return *value_ptr();
    }

  private:
    friend struct variant_key_value;

    key_type* key_ptr()const
    {
      return static_cast<key_type*>(static_cast<void*>(&key_spc));
    }

    value_type* value_ptr()const
    {
      return static_cast<value_type*>(static_cast<void*>(&value_spc));
    }

    static const key_type& key_from_value(const value_type& x)
    {
      KeyFromValue k;
      return k(x);
    }

    void key_construct_value()const
    {
      if(!value_cted){
        new(value_ptr())value_type(*key_ptr());
        key_ptr()->~key_type();
        value_cted=true;
      }
    }

    void copy_construct_value(const value_type& x)const
    {
      if(!value_cted){
        new(value_ptr())value_type(x);
        key_ptr()->~key_type();
        value_cted=true;
      }
    }

#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
    void move_construct_value(value_type&& x)const
    {
      if(!value_cted){
        new(value_ptr())value_type(std::move(x));
        key_ptr()->~key_type();
        value_cted=true;
      }
    }
#endif

    mutable typename boost::aligned_storage<
      sizeof(key_type),
      boost::alignment_of<key_type>::value
    >::type                                    key_spc;
    mutable typename boost::aligned_storage<
      sizeof(value_type),
      boost::alignment_of<value_type>::value
    >::type                                    value_spc;
    mutable bool                               value_cted;
  };

  static void key_construct_value(const rep_type& r)
  {
    r.key_construct_value();
  }

  static void copy_construct_value(const rep_type& r,const value_type& x)
  {
    r.copy_construct_value(x);
  }

#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
  static void move_construct_value(const rep_type& r,value_type&& x)
  {
    r.move_construct_value(std::move(x));
  }
#endif
};

template<typename Key,typename Value>
struct product_key_value:value_marker
{
  typedef Key   key_type;
  typedef Value value_type;

  class rep_type
  {
  public:
    /* template ctors */

#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)&&\
    !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)&&\
    BOOST_WORKAROUND(__GNUC__,<=4)&&(__GNUC__<4||__GNUC_MINOR__<=4)

/* GCC 4.4.2 (and probably prior) bug: the default ctor generated by the
 * variadic temmplate ctor below fails to value-initialize key.
 */

    rep_type():key(),value_cted(false){}
#endif

#define BOOST_FLYWEIGHT_PERFECT_FWD_CTR_BODY(args) \
  :key(BOOST_FLYWEIGHT_FORWARD(args)),value_cted(false){}

  BOOST_FLYWEIGHT_PERFECT_FWD(
    explicit rep_type,
    BOOST_FLYWEIGHT_PERFECT_FWD_CTR_BODY)

#undef BOOST_FLYWEIGHT_PERFECT_FWD_CTR_BODY

    rep_type(const rep_type& x):key(x.key),value_cted(false){}
    rep_type(const value_type&):key(no_key_from_value_failure()){}

#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
    rep_type(rep_type&& x):key(std::move(x.key)),value_cted(false){}
    rep_type(value_type&&):key(no_key_from_value_failure()){}
#endif

    ~rep_type()
    {
      if(value_cted)value_ptr()->~value_type();
    }

    operator const key_type&()const BOOST_NOEXCEPT{return key;}

    operator const value_type&()const
    {
      BOOST_ASSERT(value_cted);
      return *value_ptr();
    }

  private:
    friend struct product_key_value;

    value_type* value_ptr()const
    {
      return static_cast<value_type*>(static_cast<void*>(&value_spc));
    }

    struct no_key_from_value_failure
    {
      BOOST_MPL_ASSERT_MSG(
        false,
        NO_KEY_FROM_VALUE_CONVERSION_PROVIDED,
        (key_type,value_type));

      operator const key_type&()const;
    };

    void key_construct_value()const
    {
      if(!value_cted){
        new(value_ptr())value_type(key);
        value_cted=true;
      }
    }

    key_type                                 key;
    mutable typename boost::aligned_storage<
      sizeof(value_type),
      boost::alignment_of<value_type>::value
    >::type                                  value_spc;
    mutable bool                             value_cted;
  };

  static void key_construct_value(const rep_type& r)
  {
    r.key_construct_value();
  }

  /* [copy|move]_construct_value() can't really ever be called, provided to
   * avoid compile errors (it is the no_key_from_value_failure compile error
   * we want to appear in these cases).
   */

  static void copy_construct_value(const rep_type&,const value_type&){}

#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
  static void move_construct_value(const rep_type&,value_type&&){}
#endif
};

} /* namespace flyweights::detail */

template<typename Key,typename Value,typename KeyFromValue>
struct key_value:
  mpl::if_<
    is_same<KeyFromValue,no_key_from_value>,
    detail::product_key_value<Key,Value>,
    detail::variant_key_value<Key,Value,KeyFromValue>
  >::type
{};

} /* namespace flyweights */

} /* namespace boost */

#endif
