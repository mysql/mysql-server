/* Configurable Bloom filter.
 * 
 * Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#ifndef BOOST_BLOOM_FILTER_HPP
#define BOOST_BLOOM_FILTER_HPP

#include <boost/bloom/block.hpp>
#include <boost/bloom/detail/bloom_printers.hpp>
#include <boost/bloom/detail/core.hpp>
#include <boost/bloom/detail/mulx64.hpp>
#include <boost/bloom/detail/type_traits.hpp>
#include <boost/config.hpp>
#include <boost/container_hash/hash.hpp>
#include <boost/container_hash/hash_is_avalanching.hpp>
#include <boost/core/allocator_traits.hpp>
#include <boost/core/empty_value.hpp>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>

namespace boost{
namespace bloom{
namespace detail{

/* Mixing policies: no_mix_policy is the identity function, and
 * mulx64_mix_policy uses the mulx64 function from
 * <boost/bloom/detail/mulx64.hpp>.
 *
 * filter mixes hash results with mulx64 if the hash is not marked as
 * avalanching, i.e. it's not of good quality (see
 * <boost/unordered/hash_traits.hpp>), or if std::size_t is less than 64 bits
 * (mixing policies promote to std::uint64_t).
 */

struct no_mix_policy
{
  template<typename Hash,typename T>
  /* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
  static inline std::uint64_t mix(const Hash& h,const T& x)
  {
    return (std::uint64_t)h(x);
  }
};

struct mulx64_mix_policy
{
  template<typename Hash,typename T>
  /* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
  static inline std::uint64_t mix(const Hash& h,const T& x)
  {
    return mulx64((std::uint64_t)h(x));
  }
};

} /* namespace detail */

#if defined(BOOST_MSVC)
#pragma warning(push)
#pragma warning(disable:4714) /* marked as __forceinline not inlined */
#endif

template<
  typename T,std::size_t K,
  typename Subfilter=block<unsigned char,1>,std::size_t Stride=0,
  typename Hash=boost::hash<T>,typename Allocator=std::allocator<unsigned char>
>
class

#if defined(_MSC_VER)&&_MSC_FULL_VER>=190023918
__declspec(empty_bases) /* activate EBO with multiple inheritance */
#endif

filter:
  detail::filter_core<
    K,Subfilter,Stride,allocator_rebind_t<Allocator,unsigned char>
  >,
  empty_value<Hash,0>
{
  BOOST_BLOOM_STATIC_ASSERT_IS_CV_UNQUALIFIED_OBJECT(T);
  static_assert(
    std::is_same<unsigned char,allocator_value_type_t<Allocator>>::value,
    "Allocator's value_type must be unsigned char");
  using super=detail::filter_core<K,Subfilter,Stride,Allocator>;
  using mix_policy=typename std::conditional<
    boost::hash_is_avalanching<Hash>::value&&
    sizeof(std::size_t)>=sizeof(std::uint64_t),
    detail::no_mix_policy,
    detail::mulx64_mix_policy
  >::type;

public:
  using value_type=T;
  using super::k;
  using subfilter=typename super::subfilter;
  using super::stride;
  using hasher=Hash;
  using allocator_type=Allocator;
  using size_type=typename super::size_type;
  using difference_type=typename super::difference_type;
  using reference=value_type&;
  using const_reference=const value_type&;
  using pointer=value_type*;
  using const_pointer=const value_type*;
  static constexpr std::size_t bulk_insert_size=super::bulk_insert_size;
  static constexpr std::size_t bulk_may_contain_size=
    super::bulk_may_contain_size;

  filter()=default;

  explicit filter(
    std::size_t m,const hasher& h=hasher(),
    const allocator_type& al=allocator_type()):
    super{m,al},hash_base{empty_init,h}{}

  filter(
    std::size_t n,double fpr,const hasher& h=hasher(),
    const allocator_type& al=allocator_type()):
    super{n,fpr,al},hash_base{empty_init,h}{}

  template<typename InputIterator>
  filter(
    InputIterator first,InputIterator last,
    std::size_t m,const hasher& h=hasher(),
    const allocator_type& al=allocator_type()):
    filter{m,h,al}
  {
    insert(first,last);
  }

  template<typename InputIterator>
  filter(
    InputIterator first,InputIterator last,
    std::size_t n,double fpr,const hasher& h=hasher(),
    const allocator_type& al=allocator_type()):
    filter{n,fpr,h,al}
  {
    insert(first,last);
  }

  filter(const filter&)=default;
  filter(filter&&)=default;

  template<typename InputIterator>
  filter(
    InputIterator first,InputIterator last,
    std::size_t m,const allocator_type& al):
    filter{first,last,m,hasher(),al}{}

  template<typename InputIterator>
  filter(
    InputIterator first,InputIterator last,
    std::size_t n,double fpr,const allocator_type& al):
    filter{first,last,n,fpr,hasher(),al}{}

  explicit filter(const allocator_type& al):filter{0,al}{}

  filter(const filter& x,const allocator_type& al):
    super{x,al},hash_base{empty_init,x.h()}{}

  filter(filter&& x,const allocator_type& al):
    super{std::move(x),al},hash_base{empty_init,std::move(x.h())}{}

  filter(
    std::initializer_list<value_type> il,
    std::size_t m,const hasher& h=hasher(),
    const allocator_type& al=allocator_type()):
    filter{il.begin(),il.end(),m,h,al}{}

  filter(
    std::initializer_list<value_type> il,
    std::size_t n,double fpr,const hasher& h=hasher(),
    const allocator_type& al=allocator_type()):
    filter{il.begin(),il.end(),n,fpr,h,al}{}

  filter(std::size_t m,const allocator_type& al):
    filter{m,hasher(),al}{}

  filter(std::size_t n,double fpr,const allocator_type& al):
    filter{n,fpr,hasher(),al}{}

  filter(
    std::initializer_list<value_type> il,
    std::size_t m,const allocator_type& al):
    filter{il.begin(),il.end(),m,hasher(),al}{}

  filter(
    std::initializer_list<value_type> il,
    std::size_t n,double fpr,const allocator_type& al):
    filter{il.begin(),il.end(),n,fpr,hasher(),al}{}

  filter& operator=(const filter& x)
  {
    BOOST_BLOOM_STATIC_ASSERT_IS_NOTHROW_SWAPPABLE(Hash);
    using std::swap;

    auto x_h=x.h();
    super::operator=(x);
    swap(h(),x_h);
    return *this;
  }

  filter& operator=(filter&& x)
    noexcept(noexcept(std::declval<super&>()=(std::declval<super&&>())))
  {
    BOOST_BLOOM_STATIC_ASSERT_IS_NOTHROW_SWAPPABLE(Hash);
    using std::swap;

    super::operator=(std::move(x));
    swap(h(),x.h());
    return *this;
  }

  filter& operator=(std::initializer_list<value_type> il)
  {
    clear();
    insert(il);
    return *this;
  }

  using super::get_allocator;
  using super::capacity;
  using super::capacity_for;
  using super::fpr_for;
  using super::array;

  BOOST_FORCEINLINE void insert(const T& x)
  {
    super::insert(hash_for(x));
  }

  template<
    typename U,
    typename H=hasher,detail::enable_if_transparent_t<H>* =nullptr
  >
  BOOST_FORCEINLINE void insert(const U& x)
  {
    super::insert(hash_for(x));
  }

  template<typename InputIterator>
  void insert(InputIterator first,InputIterator last)
  {
    insert_impl(
      first,last,
      std::integral_constant<
        bool,detail::is_forward_iterator<InputIterator>::value>{});
  }

  void insert(std::initializer_list<value_type> il)
  {
    insert(il.begin(),il.end());
  }

  void swap(filter& x)
    noexcept(noexcept(std::declval<super&>().swap(std::declval<super&>())))
  {
    BOOST_BLOOM_STATIC_ASSERT_IS_NOTHROW_SWAPPABLE(Hash);
    using std::swap;

    super::swap(x);
    swap(h(),x.h());
  }

  using super::clear;
  using super::reset;

  filter& operator&=(const filter& x)
  {
    super::operator&=(x);
    return *this;
  }

  filter& operator|=(const filter& x)
  {
    super::operator|=(x);
    return *this;
  }

  hasher hash_function()const
  {
    return h();
  }

  BOOST_FORCEINLINE bool may_contain(const T& x)const
  {
    return super::may_contain(hash_for(x));
  }

  template<
    typename U,
    typename H=hasher,detail::enable_if_transparent_t<H>* =nullptr
  >
  BOOST_FORCEINLINE bool may_contain(const U& x)const
  {
    return super::may_contain(hash_for(x));
  }

  template<typename ForwardIterator,typename F>
  void may_contain(
    ForwardIterator first,ForwardIterator last,F f)const
  {
    BOOST_BLOOM_STATIC_ASSERT_IS_FORWARD_ITERATOR(ForwardIterator);

    super::bulk_may_contain(
      [this,first]()mutable{return promoting_hash_for(*first++);},
      static_cast<std::size_t>(std::distance(first,last)),
      [&f,first](bool res)mutable{f(*first++,res);});
  }

private:
  template<
    typename T1,std::size_t K1,typename SF,std::size_t S,typename H,typename A
  >
  bool friend operator==(
    const filter<T1,K1,SF,S,H,A>& x,const filter<T1,K1,SF,S,H,A>& y);

  using hash_base=empty_value<Hash,0>;

  const Hash& h()const{return hash_base::get();}
  Hash& h(){return hash_base::get();}

  template<typename U>
  /* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
  inline std::uint64_t hash_for(const U& x)const
  {
    return mix_policy::mix(h(),x);
  }

  /* promoting_hash_for forces conversion to value_type unless Hash
   * is transparent.
   */

  /* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
  inline std::uint64_t promoting_hash_for(const T& x)const
  {
    return hash_for(x);
  }

  template<
    typename U,
    typename H=hasher,detail::enable_if_transparent_t<H>* =nullptr
  >
  /* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
  inline std::uint64_t promoting_hash_for(const U& x)const
  {
    return hash_for(x);
  }

  template<typename Iterator>
  void insert_impl(
    Iterator first,Iterator last,std::false_type /* input iterator */)
  {
    while(first!=last)insert(*first++);
  }

  template<typename Iterator>
  void insert_impl(
    Iterator first,Iterator last,std::true_type /* forward iterator */)
  {
    super::bulk_insert(
      [this,first]()mutable{return promoting_hash_for(*first++);},
      static_cast<std::size_t>(std::distance(first,last)));
  }
};

template<
  typename T,std::size_t K,typename SF,std::size_t S,typename H,typename A
>
bool operator==(const filter<T,K,SF,S,H,A>& x,const filter<T,K,SF,S,H,A>& y)
{
  using super=typename filter<T,K,SF,S,H,A>::super;
  return static_cast<const super&>(x)==static_cast<const super&>(y);
}

template<
  typename T,std::size_t K,typename SF,std::size_t S,typename H,typename A
>
bool operator!=(const filter<T,K,SF,S,H,A>& x,const filter<T,K,SF,S,H,A>& y)
{
  return !(x==y);
}

template<
  typename T,std::size_t K,typename SF,std::size_t S,typename H,typename A
>
void swap(filter<T,K,SF,S,H,A>& x,filter<T,K,SF,S,H,A>& y)
  noexcept(noexcept(x.swap(y)))
{
  x.swap(y);
}

#if defined(BOOST_MSVC)
#pragma warning(pop) /* C4714 */
#endif

} /* namespace bloom */
} /* namespace boost */
#endif
