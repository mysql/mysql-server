/* Common base for all boost::bloom::filter instantiations.
 *
 * Copyright 2025 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/bloom for library home page.
 */

#ifndef BOOST_BLOOM_DETAIL_CORE_HPP
#define BOOST_BLOOM_DETAIL_CORE_HPP

#include <algorithm>
#include <boost/assert.hpp>
#include <boost/bloom/detail/mulx64.hpp>
#include <boost/bloom/detail/sse2.hpp>
#include <boost/config.hpp>
#include <boost/core/allocator_traits.hpp>
#include <boost/core/bit.hpp>
#include <boost/core/empty_value.hpp>
#include <boost/core/span.hpp>
#include <boost/throw_exception.hpp>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

#ifdef __has_builtin
#define BOOST_BLOOM_HAS_BUILTIN(x) __has_builtin(x)
#else
#define BOOST_BLOOM_HAS_BUILTIN(x) 0
#endif

#if !defined(NDEBUG)
#define BOOST_BLOOM_ASSUME(cond) BOOST_ASSERT(cond)
#elif BOOST_BLOOM_HAS_BUILTIN(__builtin_assume)
#define BOOST_BLOOM_ASSUME(cond) __builtin_assume(cond)
#elif defined(__GNUC__) || BOOST_BLOOM_HAS_BUILTIN(__builtin_unreachable)
#define BOOST_BLOOM_ASSUME(cond)    \
  do{                                   \
    if(!(cond))__builtin_unreachable(); \
  }while(0)
#elif defined(_MSC_VER)
#define BOOST_BLOOM_ASSUME(cond) __assume(cond)
#else
#define BOOST_BLOOM_ASSUME(cond)  \
  do{                                 \
    static_cast<void>(false&&(cond)); \
  }while(0)
#endif

 /* We use BOOST_BLOOM_PREFETCH[_WRITE] macros rather than proper
 * functions because of https://gcc.gnu.org/bugzilla/show_bug.cgi?id=109985
 */

#if defined(BOOST_GCC)||defined(BOOST_CLANG)
#define BOOST_BLOOM_PREFETCH(p) __builtin_prefetch((const char*)(p))
#define BOOST_BLOOM_PREFETCH_WRITE(p) __builtin_prefetch((const char*)(p),1)
#elif defined(BOOST_BLOOM_SSE2)
#define BOOST_BLOOM_PREFETCH(p) _mm_prefetch((const char*)(p),_MM_HINT_T0)
#if defined(_MM_HINT_ET0)
#define BOOST_BLOOM_PREFETCH_WRITE(p) \
_mm_prefetch((const char*)(p),_MM_HINT_ET0)
#else
#define BOOST_BLOOM_PREFETCH_WRITE(p) \
_mm_prefetch((const char*)(p),_MM_HINT_T0)
#endif
#else
#define BOOST_BLOOM_PREFETCH(p) ((void)(p))
#define BOOST_BLOOM_PREFETCH_WRITE(p) ((void)(p))
#endif

namespace boost{
namespace bloom{
namespace detail{

#if defined(BOOST_MSVC)
#pragma warning(push)
#pragma warning(disable:4714) /* marked as __forceinline not inlined */
#endif

/* fastrange_and_mcg produces (pos,hash') from hash as follows:
 *   - pos=high(mulx64(hash,range))
 *   - hash'=c*m
 * pos is uniformly distributed in [0,range) (see Lemire 2018
 * https://arxiv.org/pdf/1805.10941), whereas hash'<-hash is a multiplicative
 * congruential generator using well-behaved multipliers c from Steele and
 * Vigna 2021 https://arxiv.org/pdf/2001.05304 . To ensure the MCG generates
 * long cycles the initial value of hash is adjusted to be odd, which implies
 * that the least significant of hash' is always one. In general, the low bits
 * of MCG-produced values are of low quality and we don't use them downstream.
 */

struct fastrange_and_mcg
{
  constexpr fastrange_and_mcg(std::size_t m)noexcept:rng{m}{}

  /* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
  inline constexpr std::size_t range()const noexcept{return (std::size_t)rng;}

  /* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
  inline void prepare_hash(std::uint64_t& hash)const noexcept
  {
    hash|=1u;
  }

  /* NOLINTNEXTLINE(readability-redundant-inline-specifier) */
  inline std::size_t next_position(std::uint64_t& hash)const noexcept
  {
    boost::uint64_t hi;
    umul128(hash,rng,hi);

#if ((((SIZE_MAX>>16)>>16)>>16)>>15)!=0 /* 64-bit mode (or higher) */
    hash*=0xf1357aea2e62a9c5ull;
#else /* 32-bit mode */
    hash*=0xe817fb2d;
#endif
    return (std::size_t)hi;
  }

  std::uint64_t rng;
};

/* used_value_size<Subfilter>::value is Subfilter::used_value_size if it
 * exists, or sizeof(Subfilter::value_type) otherwise. This covers the
 * case where a subfilter only operates on the first bytes of its entire
 * value_type (e.g. fast_multiblock32<K> with K<8).
 */

template<typename Subfilter,typename=void>
struct used_value_size
{
  static constexpr std::size_t value=sizeof(typename Subfilter::value_type);
};

template<typename Subfilter>
struct used_value_size<
  Subfilter,
  typename std::enable_if<Subfilter::used_value_size!=0>::type
>
{
  static constexpr std::size_t value=Subfilter::used_value_size;
};

/* GCD with x,p > 1, p a power of two */

constexpr std::size_t gcd_pow2(std::size_t x,std::size_t p)
{
  /* x&-x: maximum power of two dividing x */
  return (x&(0-x))<p?(x&(0-x)):p;
}

/* std::ldexp is not constexpr in C++11 */

constexpr double constexpr_ldexp_1_positive(int exp)
{
  return exp==0?1.0:2.0*constexpr_ldexp_1_positive(exp-1);
}

inline unsigned int unchecked_countr_zero(std::uint64_t x)
{
#if defined(BOOST_MSVC)&&(defined(_M_X64)||defined(_M_ARM64))
  unsigned long r;
  _BitScanForward64(&r,x);
  return (unsigned int)r;
#elif defined(BOOST_GCC)||defined(BOOST_CLANG)
  return (unsigned int)__builtin_ctzll(x);
#else
  BOOST_BLOOM_ASSUME(x!=0);
  return (unsigned int)boost::core::countr_zero(x);
#endif
}

struct filter_array
{
  unsigned char* data;
  unsigned char* array; /* adjusted from data for proper alignment */
};

struct if_constexpr_void_else{void operator()()const{}};

template<bool B,typename F,typename G=if_constexpr_void_else>
void if_constexpr(F f,G g={})
{
  std::get<B?0:1>(std::forward_as_tuple(f,g))();
}

template<bool B,typename T,typename std::enable_if<B>::type* =nullptr>
void copy_assign_if(T& x,const T& y){x=y;}

template<bool B,typename T,typename std::enable_if<!B>::type* =nullptr>
void copy_assign_if(T&,const T&){}

template<bool B,typename T,typename std::enable_if<B>::type* =nullptr>
void move_assign_if(T& x,T& y){x=std::move(y);}

template<bool B,typename T,typename std::enable_if<!B>::type* =nullptr>
void move_assign_if(T&,T&){}

template<bool B,typename T,typename std::enable_if<B>::type* =nullptr>
void swap_if(T& x,T& y){using std::swap; swap(x,y);}

template<bool B,typename T,typename std::enable_if<!B>::type* =nullptr>
void swap_if(T&,T&){}

template<
  std::size_t K,typename Subfilter,std::size_t Stride,typename Allocator
>
class filter_core:empty_value<Allocator,0>
{
  static_assert(K>0,"K must be >= 1");
  static_assert(
    std::is_same<allocator_value_type_t<Allocator>,unsigned char>::value,
    "Allocator value_type must be unsigned char");

public:
  static constexpr std::size_t k=K;
  using subfilter=Subfilter;

private:
  static constexpr std::size_t kp=subfilter::k;
  static constexpr std::size_t k_total=k*kp;
  using block_type=typename subfilter::value_type;
  static constexpr std::size_t block_size=sizeof(block_type);
  static constexpr std::size_t used_value_size=
    detail::used_value_size<subfilter>::value;

public:
  static constexpr std::size_t stride=Stride?Stride:used_value_size;
  static_assert(
    stride<=used_value_size,"Stride can't exceed the block size");

private:
  static constexpr std::size_t tail_size=sizeof(block_type)-stride;
  static constexpr bool are_blocks_aligned=
    (stride%alignof(block_type)==0);
  static constexpr std::size_t cacheline=64; /* unknown at compile time */
  static constexpr std::size_t initial_alignment=
    are_blocks_aligned?
      alignof(block_type)>cacheline?alignof(block_type):cacheline:
      1;
  static constexpr std::size_t prefetched_cachelines=
    1+(block_size+cacheline-1-gcd_pow2(stride,cacheline))/cacheline;
  using hash_strategy=detail::fastrange_and_mcg;

public:
  using allocator_type=Allocator;
  using size_type=std::size_t;
  using difference_type=std::ptrdiff_t;
  using pointer=unsigned char*;
  using const_pointer=const unsigned char*;
  static constexpr std::size_t bulk_insert_size=
    (64+prefetched_cachelines-1)/prefetched_cachelines;
  static constexpr std::size_t bulk_may_contain_size=
    (64+prefetched_cachelines-1)/prefetched_cachelines;
  static_assert(
    bulk_may_contain_size<=64, /* see results in bulk_may_contain */
    "internal check, bulk_may_contain_size must be <= 64");

  explicit filter_core(std::size_t m=0):filter_core{m,allocator_type{}}{}

  filter_core(std::size_t m,const allocator_type& al_):
    allocator_base{empty_init,al_},
    hs{requested_range(m)},
    ar(new_array(al(),m?hs.range():0))
  {
    clear_bytes();
  }

  filter_core(std::size_t n,double fpr,const allocator_type& al_):
    filter_core(unadjusted_capacity_for(n,fpr),al_){}

  filter_core(const filter_core& x):
    filter_core{x,allocator_select_on_container_copy_construction(x.al())}{}

  filter_core(filter_core&& x)noexcept:
    filter_core{std::move(x),allocator_type(std::move(x.al()))}{}

  filter_core(const filter_core& x,const allocator_type& al_):
    allocator_base{empty_init,al_},
    hs{x.hs},
    ar(new_array(al(),x.range()))
  {
    copy_bytes(x);
  }

  filter_core(filter_core&& x,const allocator_type& al_):
    allocator_base{empty_init,al_},
    hs{x.hs}
  {
    auto empty_ar=new_array(x.al(),0); /* we're relying on this not throwing */
    if(al()==x.al()){
      ar=x.ar;
    }
    else{
      ar=new_array(al(),x.range());
      copy_bytes(x);
      x.delete_array();
    }
    x.hs=hash_strategy{0};
    x.ar=empty_ar;
  }

  ~filter_core()noexcept
  {
    delete_array();
  }

  filter_core& operator=(const filter_core& x)
  {
    static constexpr auto pocca=
      allocator_propagate_on_container_copy_assignment_t<allocator_type>::
        value;

    if(this!=&x){
      if_constexpr<pocca>([&,this]{
        if(al()!=x.al()||range()!=x.range()){
          auto x_al=x.al();
          auto new_ar=new_array(x_al,x.range());
          delete_array();
          hs=x.hs;
          ar=new_ar;
        }
        copy_assign_if<pocca>(al(),x.al());
      },
      [&,this]{ /* else */
        if(range()!=x.range()){
          auto new_ar=new_array(al(),x.range());
          delete_array();
          hs=x.hs;
          ar=new_ar;
        }
      });
      copy_bytes(x);
    }
    return *this;
  }

#if defined(BOOST_MSVC)
#pragma warning(push)
#pragma warning(disable:4127) /* conditional expression is constant */
#endif

  filter_core& operator=(filter_core&& x)noexcept(
    allocator_propagate_on_container_move_assignment_t<allocator_type>::value||
    allocator_is_always_equal_t<allocator_type>::value)
  {
    static constexpr auto pocma=
      allocator_propagate_on_container_move_assignment_t<allocator_type>::
        value;

    if(this!=&x){
      auto empty_ar=new_array(x.al(),0); /* relying on this not throwing */
      if(pocma||al()==x.al()){
        delete_array();
        move_assign_if<pocma>(al(),x.al());
        hs=x.hs;
        ar=x.ar;
      }
      else{
        if(range()!=x.range()){
          auto new_ar=new_array(al(),x.range());
          delete_array();
          hs=x.hs;
          ar=new_ar;
        }
        copy_bytes(x);
        x.delete_array();
      }
      x.hs=hash_strategy{0};
      x.ar=empty_ar;
    }
    return *this;
  }

#if defined(BOOST_MSVC)
#pragma warning(pop) /* C4127 */
#endif

  allocator_type get_allocator()const noexcept
  {
    return al();
  }

  std::size_t capacity()const noexcept
  {
    return used_array_size()*CHAR_BIT;
  }

  static std::size_t capacity_for(std::size_t n,double fpr)
  {
    auto m=unadjusted_capacity_for(n,fpr);
    if(m==0)return 0;
    auto rng=hash_strategy{requested_range(m)}.range();
    return used_array_size(rng)*CHAR_BIT;
  }

  static double fpr_for(std::size_t n,std::size_t m)
  {
    return m==0?1.0:n==0?0.0:fpr_for_c((double)m/n);
  }

  boost::span<unsigned char> array()noexcept
  {
    return {ar.data?ar.array:nullptr,capacity()/CHAR_BIT};
  }

  boost::span<const unsigned char> array()const noexcept
  {
    return {ar.data?ar.array:nullptr,capacity()/CHAR_BIT};
  }

  BOOST_FORCEINLINE void insert(std::uint64_t hash)
  {
    hs.prepare_hash(hash);
    for(auto n=k;n--;){
      auto p=next_element(hash); /* modifies h */
      /* We do the unhappy-path null check here rather than at the beginning
       * of the function because prefetch completion wait gives us free CPU
       * cycles to spare.
       */
      if(BOOST_UNLIKELY(n==k-1&&ar.data==nullptr))return;

      set(p,hash);
    }
  }

  template<typename HashStream>
  void bulk_insert(HashStream h,std::size_t n)
  {
    std::uint64_t  hashes[bulk_insert_size];
    unsigned char* positions[bulk_insert_size];

    if(n>=2*bulk_insert_size){
      for(auto i=bulk_insert_size;i--;){
        auto& hash=hashes[i]=h();
        auto& p=positions[i];
        hs.prepare_hash(hash);
        p=next_element(hash);
      }
      if(BOOST_UNLIKELY(ar.data==nullptr))return;
      do{
        for(auto j=k-1;j--;){
          for(auto i=bulk_insert_size;i--;){
            auto& hash=hashes[i];
            auto& p=positions[i];
            auto  hash0=hash;
            auto  p0=p;
            p=next_element(hash);
            set(p0,hash0);
          }
        }
        for(auto i=bulk_insert_size;i--;){
          auto& hash=hashes[i];
          auto& p=positions[i];
          auto  hash0=hash;
          auto  p0=p;
          hash=h();
          hs.prepare_hash(hash);
          p=next_element(hash);
          set(p0,hash0);
        }
        n-=bulk_insert_size;
      }while(n>=2*bulk_insert_size);
      for(auto j=k-1;j--;){
        for(auto i=bulk_insert_size;i--;){
          auto& hash=hashes[i];
          auto& p=positions[i];
          auto  hash0=hash;
          auto  p0=p;
          p=next_element(hash);
          set(p0,hash0);
        }
      }
      for(auto i=bulk_insert_size;i--;){
        auto& hash=hashes[i];
        auto& p=positions[i];
        set(p,hash);
      }
      n-=bulk_insert_size;
    }
    while(n--)insert(h());
  }

  void swap(filter_core& x)noexcept(
    allocator_propagate_on_container_swap_t<allocator_type>::value||
    allocator_is_always_equal_t<allocator_type>::value)
  {
    static constexpr auto pocs=
      allocator_propagate_on_container_swap_t<allocator_type>::value;

    if_constexpr<pocs>([&,this]{
      swap_if<pocs>(al(),x.al());
    },
    [&,this]{ /* else */
      BOOST_ASSERT(al()==x.al());
      (void)this; /* makes sure captured this is used */
    });
    std::swap(hs,x.hs);
    std::swap(ar,x.ar);
  }

  void clear()noexcept
  {
    clear_bytes();
  }

  void reset(std::size_t m=0)
  {
    hash_strategy new_hs{requested_range(m)};
    std::size_t   rng=m?new_hs.range():0;
    if(rng!=range()){
      auto new_ar=new_array(al(),rng);
      delete_array();
      hs=new_hs;
      ar=new_ar;
    }
    clear_bytes();
  }

  void reset(std::size_t n,double fpr)
  {
    reset(capacity_for(n,fpr));
  }

  filter_core& operator&=(const filter_core& x)
  {
    combine(x,[](unsigned char& a,unsigned char b){a&=b;});
    return *this;
  }

  filter_core& operator|=(const filter_core& x)
  {
    combine(x,[](unsigned char& a,unsigned char b){a|=b;});
    return *this;
  }

  BOOST_FORCEINLINE bool may_contain(std::uint64_t hash)const
  {
    hs.prepare_hash(hash);
#if 1
    auto p0=next_element(hash);
    for(std::size_t n=k-1;n--;){
      auto p=p0;
      auto hash0=hash;
      p0=next_element(hash);
      if(!get(p,hash0))return false;
    }
    if(!get(p0,hash))return false;
    return true;
#else
    for(auto n=k;n--;){
      auto p=next_element(hash); /* modifies hash */
      if(!get(p,hash))return false;
    }
    return true;
#endif
  }

  template<typename HashStream,typename F>
  void bulk_may_contain(HashStream h,std::size_t n,F f)const
  {
    if(k==1){
      std::uint64_t        hashes[bulk_may_contain_size];
      const unsigned char* positions[bulk_may_contain_size];

      if(n>=2*bulk_may_contain_size){
        for(auto i=bulk_may_contain_size;i--;){
          auto& hash=hashes[i]=h();
          auto& p=positions[i];
          hs.prepare_hash(hash);
          p=next_element(hash);
        }
        do{
          for(auto i=bulk_may_contain_size;i--;){
            auto& hash=hashes[i];
            auto& p=positions[i];
            auto  hash0=hash;
            auto  p0=p;
            hash=h();
            hs.prepare_hash(hash);
            p=next_element(hash);
            f(get(p0,hash0));
          }
          n-=bulk_may_contain_size;
        }while(n>=2*bulk_may_contain_size);

        for(auto i=bulk_may_contain_size;i--;){
          auto& hash=hashes[i];
          auto& p=positions[i];
          f(get(p,hash));
        }
        n-=bulk_may_contain_size;
      }

      while(n--)f(may_contain(h()));
    }
    else{
      static constexpr std::uint64_t initial_result_mask=
        ((std::uint64_t(1)<<(bulk_may_contain_size/2))<<
          ((bulk_may_contain_size+1)/2))-1;

      std::uint64_t        hashes[bulk_may_contain_size];
      const unsigned char* positions[bulk_may_contain_size];
      std::uint64_t        results=initial_result_mask;

      if(n>=2*bulk_may_contain_size){
        for(std::size_t i=0;i<bulk_may_contain_size;++i){
          auto& hash=hashes[i]=h();
          auto& p=positions[i];
          hs.prepare_hash(hash);
          p=next_element(hash);
        }
        do{
          for(auto j=k;j--;){
            auto mask=results;
            if(!mask)break;
            do{
              auto i=unchecked_countr_zero(mask);
              auto& hash=hashes[i];
              auto& p=positions[i];
              auto  b=get(p,hash);
              p=next_element(hash);
              results&=~(std::uint64_t(!b)<<i);
              mask&=mask-1;
            }while(mask);
          }
          for(std::size_t i=0;i<bulk_may_contain_size;++i){
            auto& hash=hashes[i];
            auto& p=positions[i];
            hash=h();
            hs.prepare_hash(hash);
            p=next_element(hash);
            f(results&1);
            results>>=1;
          }
          results=initial_result_mask;
          n-=bulk_may_contain_size;
        }while(n>=2*bulk_may_contain_size);

        for(auto j=k;j--;){
          auto mask=results;
          if(!mask)break;
          do{
            auto i=unchecked_countr_zero(mask);
            auto& hash=hashes[i];
            auto& p=positions[i];
            auto  b=get(p,hash);
            p=next_element(hash);
            results&=~(std::uint64_t(!b)<<i);
            mask&=mask-1;
          }while(mask);
        }
        for(std::size_t i=0;i<bulk_may_contain_size;++i){
          f(results&1);
          results>>=1;
        }
        n-=bulk_may_contain_size;
      }

      while(n--)f(may_contain(h()));
    }
  }

  friend bool operator==(const filter_core& x,const filter_core& y)
  {
    if(x.range()!=y.range())return false;
    else if(!x.ar.data)return true;
    else return std::memcmp(x.ar.array,y.ar.array,x.used_array_size())==0;
  }

private:
  using allocator_base=empty_value<Allocator,0>;

  const Allocator& al()const{return allocator_base::get();}
  Allocator& al(){return allocator_base::get();}

  static std::size_t requested_range(std::size_t m)
  {
    if(m>(used_value_size-stride)*CHAR_BIT){
      /* ensures filter_core{f.capacity()}.capacity()==f.capacity() */
      m-=(used_value_size-stride)*CHAR_BIT;
    }
    return
      (std::numeric_limits<std::size_t>::max)()-m>=stride*CHAR_BIT-1?
      (m+stride*CHAR_BIT-1)/(stride*CHAR_BIT):
      m/(stride*CHAR_BIT);
  }

  static filter_array new_array(allocator_type& al,std::size_t rng)
  {
    if(rng){
      auto p=allocator_allocate(al,space_for(rng));
      return {p,array_for(p)};
    }
    else{
      /* To avoid dynamic allocation for zero capacity or moved-from filters,
       * we point array to a statically allocated dummy array with all bits
       * set to one. This is good for read operations but not so for write
       * operations, where we need to resort to a null check on
       * filter_array::data.
       */

      static struct {unsigned char x=-1;}
      dummy[space_for(hash_strategy{0}.range())];

      return {nullptr,array_for(reinterpret_cast<unsigned char*>(&dummy))};
    }
  }

  void delete_array()noexcept
  {
    if(ar.data)allocator_deallocate(al(),ar.data,space_for(range()));
  }

  void clear_bytes()noexcept
  {
    std::memset(ar.array,0,used_array_size());
  }

  void copy_bytes(const filter_core& x)
  {
    BOOST_ASSERT(range()==x.range());
    std::memcpy(ar.array,x.ar.array,used_array_size());
  }

  std::size_t range()const noexcept
  {
    return ar.data?hs.range():0;
  }

  static constexpr std::size_t space_for(std::size_t rng)noexcept
  {
    return (initial_alignment-1)+rng*stride+tail_size;
  }

  static unsigned char* array_for(unsigned char* p)noexcept
  {
    return p+
      (std::uintptr_t(initial_alignment)-
       std::uintptr_t(p))%initial_alignment;
  }

  std::size_t used_array_size()const noexcept
  {
    return used_array_size(range());
  }

  static std::size_t used_array_size(std::size_t rng)noexcept
  {
    return rng?rng*stride+(used_value_size-stride):0;
  }

  static std::size_t unadjusted_capacity_for(std::size_t n,double fpr)
  {
    using size_t_limits=std::numeric_limits<std::size_t>;
    using double_limits=std::numeric_limits<double>;

    BOOST_ASSERT(fpr>=0.0&&fpr<=1.0);
    if(n==0)return fpr==1.0?0:1;

    constexpr double eps=1.0/(double)(size_t_limits::max)();
    constexpr double max_size_t_as_double=
      size_t_limits::digits<=double_limits::digits?
      (double)(size_t_limits::max)():
      (double)(size_t_limits::max)()
        /* ensure value is portably castable back to std::size_t */
        -constexpr_ldexp_1_positive(
          size_t_limits::digits-double_limits::digits);

    const double c_max=max_size_t_as_double/n;

    /* Capacity of a classical Bloom filter as a lower bound:
     * c = k / -log(1 - fpr^(1/k)).
     */
    
    double d=1.0-std::pow(fpr,1.0/k_total);
    if(std::fpclassify(d)==FP_ZERO)return 0; /* fpr ~ 1 */
    double l=std::log(d);
    if(std::fpclassify(l)==FP_ZERO)return (std::size_t)(c_max*n); /* fpr ~ 0 */
    double c0=(std::min)(k_total/-l,c_max);

    /* bracket target fpr between c0 and c1 */

    double c1=c0;
    if(fpr_for_c(c1)>fpr){ /* expected case */
      do{
        double cn=c1*1.5;
        if(cn>c_max)return (std::size_t)(c_max*n);
        c0=c1;
        c1=cn;
      }while(fpr_for_c(c1)>fpr);
    }
    else{ /* c0 shouldn't overshoot ever, just in case */
      do{
        double cn=c0/1.5;
        c1=c0;
        c0=cn;
      }while(fpr_for_c(c0)<fpr);
    }

    /* bisect */

    double cm;
    while((cm=c0+(c1-c0)/2)>c0 && cm<c1 && c1-c0>=eps){
      if(fpr_for_c(cm)>fpr)c0=cm;
      else                 c1=cm;
    }
    return (std::size_t)(cm*n);
  }

  static double fpr_for_c(double c)
  {
    constexpr std::size_t w=(2*used_value_size-stride)*CHAR_BIT;
    const double          lambda=w*k/c;
    const double          loglambda=std::log(lambda);
    double                res=0.0;
    double                deltap=0.0;
    for(int i=0;i<1000;++i){
      double poisson=std::exp(i*loglambda-lambda-std::lgamma(i+1));
      double delta=poisson*subfilter::fpr(i,w);
      double resn=res+delta;

      /* The terms of this summation are unimodal, so we check we're on the
       * descending slope before stopping.
       */

      if(delta<deltap&&resn==res)break;
      deltap=delta;
      res=resn;
    }

    /* For small values of c (high values of lambda), truncation errors,loop
     * exhaustion and the use of Poisson instead of binomial may result in a
     * calculated value less than the classical Bloom filter formula, which we
     * know is always the minimum attainable.
     */

    return (std::max)(
      std::pow((double)res,(double)k),
      std::pow(1.0-std::exp(-(double)k_total/c),(double)k_total));
  }

  BOOST_FORCEINLINE bool get(const unsigned char* p,std::uint64_t hash)const
  {
    return get(p,hash,std::integral_constant<bool,are_blocks_aligned>{});
  }

  BOOST_FORCEINLINE bool get(
    const unsigned char* p,std::uint64_t hash,
    std::true_type /* blocks aligned */)const
  {
    return subfilter::check(*reinterpret_cast<const block_type*>(p),hash);
  }

  BOOST_FORCEINLINE bool get(
    const unsigned char* p,std::uint64_t hash,
    std::false_type /* blocks not aligned */)const
  {
    block_type x;
    std::memcpy(&x,p,block_size);
    return subfilter::check(x,hash);
  }

  BOOST_FORCEINLINE void set(unsigned char* p,std::uint64_t hash)
  {
    return set(p,hash,std::integral_constant<bool,are_blocks_aligned>{});
  }

  BOOST_FORCEINLINE void set(
    unsigned char* p,std::uint64_t hash,
    std::true_type /* blocks aligned */)
  {
    subfilter::mark(*reinterpret_cast<block_type*>(p),hash);
  }

  BOOST_FORCEINLINE void set(
    unsigned char* p,std::uint64_t hash,
    std::false_type /* blocks not aligned */)
  {
    block_type x;
    std::memcpy(&x,p,block_size);
    subfilter::mark(x,hash);
    std::memcpy(p,&x,block_size);
  }

  BOOST_FORCEINLINE 
  unsigned char* next_element(std::uint64_t& h)noexcept
  {
    auto p=ar.array+hs.next_position(h)*stride;
    for(std::size_t i=0;i<prefetched_cachelines;++i){
      BOOST_BLOOM_PREFETCH_WRITE((unsigned char*)p+i*cacheline);
    }
    return p;
  }

  BOOST_FORCEINLINE
  const unsigned char* next_element(std::uint64_t& h)const noexcept
  {
    auto p=ar.array+hs.next_position(h)*stride;
    for(std::size_t i=0;i<prefetched_cachelines;++i){
      BOOST_BLOOM_PREFETCH((unsigned char*)p+i*cacheline);
    }
    return p;
  }

  template<typename F>
  void combine(const filter_core& x,F f)
  {
    if(range()!=x.range()){
      BOOST_THROW_EXCEPTION(std::invalid_argument("incompatible filters"));
    }
    auto first0=ar.array,
         last0=first0+used_array_size(),
         first1=x.ar.array;
    while(first0!=last0)f(*first0++,*first1++);
  }

  hash_strategy hs;
  filter_array  ar;
};

#if defined(BOOST_MSVC)
#pragma warning(pop) /* C4714 */
#endif

} /* namespace detail */
} /* namespace bloom */
} /* namespace boost */
#endif
