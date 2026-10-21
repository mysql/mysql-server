/* Copyright 2003-2026 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#ifndef BOOST_MULTI_INDEX_DETAIL_SEQ_INDEX_NODE_HPP
#define BOOST_MULTI_INDEX_DETAIL_SEQ_INDEX_NODE_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <boost/core/allocator_access.hpp>
#include <algorithm>
#include <boost/multi_index/detail/raw_ptr.hpp>

namespace boost{

namespace multi_index{

namespace detail{

/* doubly-linked node for use by sequenced_index */

template<typename Allocator>
struct sequenced_index_node_impl
{
  typedef allocator_rebind_t<
    Allocator,sequenced_index_node_impl
  >                                              node_allocator;
  typedef allocator_pointer_t<node_allocator>    pointer;
  typedef allocator_const_pointer_t<
    node_allocator>                              const_pointer;
  typedef allocator_difference_type_t<
    node_allocator>                              difference_type;

  pointer& prior(){return prior_;}
  pointer  prior()const{return prior_;}
  pointer& next(){return next_;}
  pointer  next()const{return next_;}

  /* interoperability with bidir_node_iterator */

  static void increment(pointer& x){x=x->next();}
  static void decrement(pointer& x){x=x->prior();}

  /* algorithmic stuff */

  static void link(pointer x,pointer header)
  {
    x->prior()=header->prior();
    x->next()=header;
    x->prior()->next()=x->next()->prior()=x;
  }

  static void unlink(pointer x)
  {
    x->prior()->next()=x->next();
    x->next()->prior()=x->prior();
  }

  static void relink(pointer position,pointer x)
  {
    unlink(x);
    x->prior()=position->prior();
    x->next()=position;
    x->prior()->next()=x->next()->prior()=x;
  }

  static void relink(pointer position,pointer x,pointer y)
  {
    /* position is assumed not to be in [x,y) */

    if(x!=y){
      pointer z=y->prior();
      x->prior()->next()=y;
      y->prior()=x->prior();
      x->prior()=position->prior();
      z->next()=position;
      x->prior()->next()=x;
      z->next()->prior()=z;
    }
  }

  static void reverse(pointer header)
  {
    pointer x=header;
    do{
      pointer y=x->next();
      std::swap(x->prior(),x->next());
      x=y;
    }while(x!=header);
  }

  static void swap(pointer x,pointer y)
  {
    /* This swap function does not exchange the header nodes,
     * but rather their pointers. This is *not* used for implementing
     * sequenced_index::swap.
     */

    if(x->next()!=x){
      if(y->next()!=y){
        std::swap(x->next(),y->next());
        std::swap(x->prior(),y->prior());
        x->next()->prior()=x->prior()->next()=x;
        y->next()->prior()=y->prior()->next()=y;
      }
      else{
        y->next()=x->next();
        y->prior()=x->prior();
        x->next()=x->prior()=x;
        y->next()->prior()=y->prior()->next()=y;
      }
    }
    else if(y->next()!=y){
      x->next()=y->next();
      x->prior()=y->prior();
      y->next()=y->prior()=y;
      x->next()->prior()=x->prior()->next()=x;
    }
  }

private:
  pointer prior_;
  pointer next_;
};

template<typename Super>
struct sequenced_index_node_trampoline:
  sequenced_index_node_impl<
    allocator_rebind_t<
      typename Super::allocator_type,
      char
    >
  >
{
  typedef sequenced_index_node_impl<
    allocator_rebind_t<
      typename Super::allocator_type,
      char
    >
  > impl_type;
};

template<typename Super>
struct sequenced_index_node:Super,sequenced_index_node_trampoline<Super>
{
private:
  typedef sequenced_index_node_trampoline<Super> trampoline;

public:
  typedef typename trampoline::impl_type       impl_type;
  typedef typename trampoline::pointer         impl_pointer;
  typedef typename trampoline::const_pointer   const_impl_pointer;
  typedef typename trampoline::difference_type difference_type;
  typedef allocator_rebind_t<
    typename trampoline::node_allocator,
    sequenced_index_node>                      final_allocator_type;
  typedef allocator_pointer_t<
    final_allocator_type>                      pointer;
  typedef allocator_const_pointer_t<
    final_allocator_type>                      const_pointer;

  impl_pointer& prior(){return trampoline::prior();}
  impl_pointer  prior()const{return trampoline::prior();}
  impl_pointer& next(){return trampoline::next();}
  impl_pointer  next()const{return trampoline::next();}

  impl_pointer impl()
  {
    return static_cast<impl_pointer>(
      static_cast<impl_type*>(static_cast<trampoline*>(this)));
  }

  const_impl_pointer impl()const
  {
    return static_cast<const_impl_pointer>(
      static_cast<const impl_type*>(static_cast<const trampoline*>(this)));
  }

  static sequenced_index_node* from_impl(impl_pointer x)
  {
    return
      static_cast<sequenced_index_node*>(
        static_cast<trampoline*>(
          raw_ptr<impl_type*>(x)));
  }

  static const sequenced_index_node* from_impl(const_impl_pointer x)
  {
    return
      static_cast<const sequenced_index_node*>(
        static_cast<const trampoline*>(
          raw_ptr<const impl_type*>(x)));
  }

  /* Interoperability with bidir_node_iterator and index impl.
   * Templated for raw-pointer/non-raw-pointer versions.
   */

  template<typename NodePtr>
  static void increment(NodePtr& x)
  {
    impl_pointer xi=x->impl();
    trampoline::increment(xi);
    x=NodePtr(from_impl(xi));
  }

  template<typename NodePtr>
  static void decrement(NodePtr& x)
  {
    impl_pointer xi=x->impl();
    trampoline::decrement(xi);
    x=NodePtr(from_impl(xi));
  }
};

} /* namespace multi_index::detail */

} /* namespace multi_index */

} /* namespace boost */

#endif
