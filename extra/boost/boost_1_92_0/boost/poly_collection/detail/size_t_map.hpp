/* Copyright 2024 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#ifndef BOOST_POLY_COLLECTION_DETAIL_SIZE_T_MAP_HPP
#define BOOST_POLY_COLLECTION_DETAIL_SIZE_T_MAP_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/assert.hpp>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace boost{

namespace poly_collection{

namespace detail{

/* Map-like wrapper over a vector of std::pair<std::size_t,T>. It is required
 * that the first member of the i-th element be i, which implies that
 * insert(i,x) must executed in increasing order of i.
 */

template<typename T,typename Allocator>
class size_t_map
{
  using vector_type=std::vector<
    std::pair<std::size_t,T>,
    typename std::allocator_traits<Allocator>::template
      rebind_alloc<std::pair<std::size_t,T>>
  >;

public:
  using key_type=std::size_t;
  using mapped_type=T;
  using value_type=typename vector_type::value_type;
  using allocator_type=typename vector_type::allocator_type;
  using iterator=typename vector_type::iterator;
  using const_iterator=typename vector_type::const_iterator;

  size_t_map()=default;
  size_t_map(const size_t_map& x)=default;
  size_t_map(size_t_map&& x)=default;
  size_t_map(const allocator_type& al):v{al}{}
  size_t_map(const size_t_map& x,const allocator_type& al):v{x.v,al}{}
  size_t_map(size_t_map&& x,const allocator_type& al):v{std::move(x.v),al}{}
  size_t_map& operator=(const size_t_map& x)=default;
  size_t_map& operator=(size_t_map&& x)=default;

  allocator_type get_allocator()const noexcept{return v.get_allocator();}

  iterator       begin()noexcept{return v.begin();}
  iterator       end()noexcept{return v.end();}
  const_iterator begin()const noexcept{return v.begin();}
  const_iterator end()const noexcept{return v.end();}
  const_iterator cbegin()const noexcept{return v.cbegin();}
  const_iterator cend()const noexcept{return v.cend();}

  const_iterator find(const key_type& key)const
  {
    if(key<v.size())return v.begin()+key;
    else            return v.end();
  }

  template<typename P>
  std::pair<iterator,bool> insert(const key_type& key,P&& x)
  {
    BOOST_ASSERT(key==v.size());
    v.emplace_back(key,std::forward<P>(x));
    return {v.end()-1,true};
  }

  void swap(size_t_map& x){v.swap(x.v);}

private:
  vector_type v;
};

template<typename T,typename Allocator>
void swap(size_t_map<T,Allocator>& x,size_t_map<T,Allocator>& y)
{
  x.swap(y);
}

} /* namespace poly_collection::detail */

} /* namespace poly_collection */

} /* namespace boost */

#endif
