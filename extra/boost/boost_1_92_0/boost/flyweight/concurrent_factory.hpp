/* Copyright 2024 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/flyweight for library home page.
 */

#ifndef BOOST_FLYWEIGHT_CONCURRENT_FACTORY_HPP
#define BOOST_FLYWEIGHT_CONCURRENT_FACTORY_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <atomic>
#include <boost/assert.hpp>
#include <boost/core/allocator_access.hpp> 
#include <boost/core/invoke_swap.hpp>
#include <boost/flyweight/concurrent_factory_fwd.hpp>
#include <boost/flyweight/factory_tag.hpp>
#include <boost/flyweight/detail/is_placeholder_expr.hpp>
#include <boost/unordered/concurrent_node_set.hpp>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>

/* flyweight factory based on boost::concurent_node_set */

namespace boost{

namespace flyweights{

namespace concurrent_factory_detail{

template<typename Entry>
struct refcounted_entry:Entry
{
  explicit refcounted_entry(Entry&& x):Entry{std::move(x)}{}
  refcounted_entry(refcounted_entry&& x):
    Entry{std::move(static_cast<Entry&>(x))}{}

  long count()const{return ref;}
  void add_ref()const{++ref;}
  void release()const{--ref;} 
  
private:  
  mutable std::atomic_long ref{0};
};

template<typename RefcountedEntry>
class refcounted_handle
{
public:
  refcounted_handle(const refcounted_handle& x):p{x.p}{p->add_ref();}
  ~refcounted_handle(){p->release();}

  refcounted_handle& operator=(refcounted_handle x)
  {
    boost::core::invoke_swap(p,x.p);
    return *this;
  }

  const RefcountedEntry& get()const{return *p;}

private:
  template<typename,typename,typename,typename,typename>
  friend class concurrent_factory_class_impl;

  refcounted_handle(const RefcountedEntry* p_):p{p_}
  {
    /* Doesn't add ref, refcount already incremented by
     * concurrent_factory_class_impl before calling this ctor.
     */
    BOOST_ASSERT(p!=nullptr);
    BOOST_ASSERT(p->count()>0);
  }

  const RefcountedEntry* p;
};

template<
  typename Entry,typename Key,
  typename Hash,typename Pred,typename Allocator
>
class concurrent_factory_class_impl:public factory_marker
{
  using entry_type=refcounted_entry<Entry>;
  using unrebound_allocator_type=typename std::conditional<
    mpl::is_na<Allocator>::value,
    std::allocator<entry_type>,
    Allocator
  >::type;
  using container_type=boost::concurrent_node_set<
    entry_type,
    typename std::conditional<
      mpl::is_na<Hash>::value,
      boost::hash<Key>,
      Hash
    >::type,
    typename std::conditional<
      mpl::is_na<Pred>::value,
      std::equal_to<Key>,
      Pred
    >::type,
    boost::allocator_rebind_t<unrebound_allocator_type,entry_type>
  >;

public:
  using handle_type=refcounted_handle<entry_type>;
  
  concurrent_factory_class_impl():gc{[this]{
    /* Garbage collector. Traverses the container every gc_time and lockedly
     * erases entries without any external reference.
     */

    constexpr auto gc_time=std::chrono::seconds(1);

    for(;;){
      {
        std::unique_lock<std::mutex> lk{m};
        if(cv.wait_for(lk,gc_time,[&]{return stop;}))return;
      }
      cont.erase_if([&](const entry_type& x){return !x.count();});
    }
  }}
  {}

  ~concurrent_factory_class_impl()
  {
    /* shut the garbage collector down */

    {
      std::unique_lock<std::mutex> lk{m};
      stop=true;
    }
    cv.notify_one();
    gc.join();
  }

  /* Instead of insert, concurrent_factory provides the undocumented extension
   * insert_and_visit, accessible through ADL (see global insert_and_visit
   * below). This ensures that visitation happens in a locked environment
   * even if no external locking is specified.
   */

  template<typename F>
  handle_type insert_and_visit(Entry&& x,F f)
  {
    const entry_type* p=nullptr;
    auto              g=[&p,&f](const entry_type& x){
      f(static_cast<const Entry&>(x));
      x.add_ref();
      p=std::addressof(x);
    };

    cont.insert_and_visit(entry_type{std::move(x)},g,g);
    return {p};
  }

  void erase(handle_type)
  {
    /* unused entries taken care of by garbage collector */
  }

  static const Entry& entry(handle_type h){return h.get();}

private:  
  container_type          cont;
  std::mutex              m;
  std::condition_variable cv;
  bool                    stop=false;
  std::thread             gc;
};

struct concurrent_factory_class_empty_base:factory_marker{};

template<
  typename Entry,typename Key,
  typename Hash,typename Pred,typename Allocator
>
struct check_concurrent_factory_class_params{};

} /* namespace concurrent_factory_detail */

template<
  typename Entry,typename Key,
  typename Hash,typename Pred,typename Allocator
>
class concurrent_factory_class:
  /* boost::mpl::apply may instantiate concurrent_factory_class<...> even when
   * the type is a lambda expression, so we need to guard against the
   * implementation defining nonsensical typedefs based on placeholder params.
   */

  public std::conditional<
    boost::flyweights::detail::is_placeholder_expression<
      concurrent_factory_detail::check_concurrent_factory_class_params<
        Entry,Key,Hash,Pred,Allocator
      >
    >::value,
    concurrent_factory_detail::concurrent_factory_class_empty_base,
    concurrent_factory_detail::concurrent_factory_class_impl<
      Entry,Key,Hash,Pred,Allocator
    >
  >::type
{};

template<
  typename Entry,typename Key,
  typename Hash,typename Pred,typename Allocator,
  typename F
>
typename concurrent_factory_class<Entry,Key,Hash,Pred,Allocator>::handle_type
insert_and_visit(
  concurrent_factory_class<Entry,Key,Hash,Pred,Allocator>& fac,Entry&& x,F f)
{
  return fac.insert_and_visit(std::move(x),f);
}

/* concurrent_factory_class specifier */

template<
  typename Hash,typename Pred,typename Allocator
  BOOST_FLYWEIGHT_NOT_A_PLACEHOLDER_EXPRESSION_DEF
>
struct concurrent_factory:factory_marker
{
  template<typename Entry,typename Key>
  struct apply:
    mpl::apply2<
      concurrent_factory_class<
        boost::mpl::_1,boost::mpl::_2,Hash,Pred,Allocator
      >,
      Entry,Key
    >
  {};
};

} /* namespace flyweights */

} /* namespace boost */

#endif
