//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2005-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_DETAIL_NODE_POOL_IMPL_HPP
#define BOOST_CONTAINER_DETAIL_NODE_POOL_IMPL_HPP

#ifndef BOOST_CONFIG_HPP
#  include <boost/config.hpp>
#endif

#if defined(BOOST_HAS_PRAGMA_ONCE)
#  pragma once
#endif

#include <boost/container/detail/config_begin.hpp>
#include <boost/container/detail/workaround.hpp>
#include <boost/container/container_fwd.hpp>

#include <boost/container/detail/math_functions.hpp>
#include <boost/container/detail/mpl.hpp>
#include <boost/container/detail/pool_common.hpp>
#include <boost/move/detail/to_raw_pointer.hpp>
#include <boost/move/detail/force_ptr.hpp>
#include <boost/container/detail/type_traits.hpp>

#include <boost/intrusive/pointer_traits.hpp>
#include <boost/intrusive/set.hpp>
#include <boost/intrusive/slist.hpp>

#include <boost/assert.hpp>
#include <cstddef>

namespace boost {
namespace container {
namespace dtl {


// Helper to generate masks (avoids shift overflow when N == bits_per_word)
template <typename SizeType, std::size_t N, std::size_t BitsPerWord>
struct packed_3n_mask_helper
{
   BOOST_STATIC_CONSTEXPR SizeType low_mask  = (SizeType(1) << N) - 1;
   BOOST_STATIC_CONSTEXPR SizeType high_mask = low_mask << (BitsPerWord - N);
};

template <typename SizeType, std::size_t N>
struct packed_3n_mask_helper<SizeType, N, N>
{
   BOOST_STATIC_CONSTEXPR SizeType low_mask  = ~SizeType(0);
   BOOST_STATIC_CONSTEXPR SizeType high_mask = ~SizeType(0);
};

// Reference-based packed bits view
// N = number of bits per segment (total storage = 3*N bits)
// SizeType must be unsigned
template <std::size_t N, typename SizeType = std::size_t>
class packed_3n_bits_ref
{
   public:
   typedef SizeType size_type;


   BOOST_STATIC_CONSTEXPR std::size_t bits_per_word    = sizeof(size_type) * CHAR_BIT;
   BOOST_STATIC_CONSTEXPR std::size_t bits_per_segment = N;
   BOOST_STATIC_CONSTEXPR std::size_t total_bits       = 3 * N;
   BOOST_STATIC_CONSTEXPR std::size_t top_shift        = bits_per_word - N;

   BOOST_CONTAINER_STATIC_ASSERT(sizeof(size_type)*CHAR_BIT >= total_bits);

private:
   typedef packed_3n_mask_helper<size_type, N, bits_per_word> masks;
   BOOST_CONTAINER_STATIC_ASSERT(N <= bits_per_word);

   size_type& m_low;
   size_type& m_high1;
   size_type& m_high2;

public:
   BOOST_STATIC_CONSTEXPR size_type low_mask  = masks::low_mask;
   BOOST_STATIC_CONSTEXPR size_type high_mask = masks::high_mask;

   BOOST_STATIC_CONSTEXPR std::size_t remaining_bits_low  = bits_per_word - N;
   BOOST_STATIC_CONSTEXPR std::size_t remaining_bits_high = bits_per_word - N;

   // Constructor takes references to three existing variables
   packed_3n_bits_ref(size_type& low, size_type& high1, size_type& high2)
      : m_low(low), m_high1(high1), m_high2(high2)
   {}

   void store(size_type value)
   {
      // Check that no bits are set outside the 3*N range
      BOOST_ASSERT((value & ~size_type((1u << 3*N) - 1u)) == 0);

      const size_type segment_mask = (size_type(1) << N) - 1;
      const size_type seg0 = static_cast<size_type>(value & segment_mask);
      const size_type seg1 = static_cast<size_type>((value >> N) & segment_mask);
      const size_type seg2 = static_cast<size_type>((value >> (2 * N)) & segment_mask);

      set_segment0(seg0);
      set_segment1(seg1);
      set_segment2(seg2);
   }

   size_type load() const
   {
      return static_cast<size_type>
            ( (static_cast<size_type>(get_segment0()) << (0u*N))
            | (static_cast<size_type>(get_segment1()) << (1u*N))
            | (static_cast<size_type>(get_segment2()) << (2u*N))
            );
   }

   void set_segment0(size_type value)
   {  m_low = (m_low & ~low_mask) | (value & low_mask);  }

   void set_segment1(size_type value)
   {  m_high1 = (m_high1 & ~high_mask) | ((value & low_mask) << top_shift);  }

   void set_segment2(size_type value)
   {  m_high2 = (m_high2 & ~high_mask) | ((value & low_mask) << top_shift);   }

   size_type get_segment0() const
   { return m_low & low_mask;   }
    
   size_type get_segment1() const
   { return (m_high1 & high_mask) >> top_shift;  }
    
   size_type get_segment2() const
   { return (m_high2 & high_mask) >> top_shift;  }

   size_type low_unpacked() const
   {  return m_low & ~low_mask;  }

   size_type high1_unpacked() const
   {  return m_high1 & ~high_mask;  }

   size_type high2_unpacked() const
   {  return m_high2 & ~high_mask;  }

   void set_low_unpacked(size_type value)
   {
      BOOST_ASSERT((value & low_mask) == 0);
      m_low = (m_low & low_mask) | value;
   }

   void set_high1_unpacked(size_type value)
   {
      BOOST_ASSERT((value & high_mask) == 0);
      m_high1 = (m_high1 & high_mask) | value;
   }

   void set_high2_unpacked(size_type value)
   {
      BOOST_ASSERT((value & high_mask) == 0);
      m_high2 = (m_high2 & high_mask) | value;
   }

   size_type& raw_low()        { return m_low; }
   size_type  raw_low()  const { return m_low; }

   size_type& raw_high1()       { return m_high1; }
   size_type  raw_high1() const { return m_high1; }

   size_type& raw_high2()       { return m_high2; }
   size_type  raw_high2() const { return m_high2; }

   packed_3n_bits_ref(const packed_3n_bits_ref& v)
      : m_low(v.m_low), m_high1(v.m_high1), m_high2(v.m_high2)
   {}
private:

   //packed_3n_bits_ref& operator=(const packed_3n_bits_ref&);
};

//This class uses packed_3n_bits_ref to store overalignment
//values maintatin the ABI, since ABI-sensitive libraries like Interprocess
//rely on this type and types without overalignment must continue working
//as before.
template<class SegmentManagerBase>
class private_node_pool_impl
{
   //Non-copyable
   private_node_pool_impl();
   private_node_pool_impl(const private_node_pool_impl &);
   private_node_pool_impl &operator=(const private_node_pool_impl &);

   //A node object will hold node_t when it's not allocated
   public:
   typedef typename SegmentManagerBase::void_pointer              void_pointer;
   typedef typename node_slist<void_pointer>::slist_hook_t        slist_hook_t;
   typedef typename node_slist<void_pointer>::node_t              list_node_t;
   typedef typename node_slist<void_pointer>::node_slist_t        free_nodes_t;
   typedef typename SegmentManagerBase::multiallocation_chain     multiallocation_chain;
   typedef typename SegmentManagerBase::size_type                 size_type;

   private:
   BOOST_STATIC_CONSTEXPR size_type list_node_align = alignment_of<list_node_t>::value;
   BOOST_STATIC_CONSTEXPR size_type list_node_align_bits
      = boost::container::dtl::log2_pow2<list_node_align>::value;

   //Make sure we can use at least the lowest 2 bits for other purposes
   BOOST_CONTAINER_STATIC_ASSERT(list_node_align_bits >= 2u);

   typedef typename bi::make_slist
      < list_node_t, bi::base_hook<slist_hook_t>
      , bi::linear<true>
      , bi::constant_time_size<false> >::type      blockslist_t;

   typedef packed_3n_bits_ref<list_node_align_bits, size_type> packed_3n_bits_ref_t;

   static size_type get_rounded_size(size_type orig_size, size_type round_to)
   {  return ((orig_size-1)/round_to+1)*round_to;  }

   public:

   //!Segment manager typedef
   typedef SegmentManagerBase segment_manager_base_type;

   //!Constructor from a segment manager. Never throws
   private_node_pool_impl(segment_manager_base_type *segment_mngr_base, size_type node_size, size_type nodes_per_block, size_type node_alignment)
      : m_nodes_per_block(0)
      , m_real_node_size(0)
      , mp_segment_mngr_base(segment_mngr_base)
      , m_blocklist()
      , m_freelist()
      , m_allocated()
   {
      BOOST_ASSERT(0 == (node_alignment & (node_alignment - 1u)));   //node_alignment must be power of two
      //node_size must be able to hold a list_node_t
      node_size = node_size < sizeof(list_node_t) ? sizeof(list_node_t) : node_size;

      //node_size must be multiple of list_node_t alignment because each node must be store it in that node address
      node_size = get_rounded_size(node_size, list_node_align);

      //node_size must be multiple of node_alignment
      if(node_alignment < list_node_align)
         node_alignment = list_node_align;
      else if(node_alignment > list_node_align)
         node_size = get_rounded_size(node_size, node_alignment);

      //We'll use log2(list_node_align) bits from m_real_node_size, m_nodes_per_block and m_allocated
      //to store the overaligned state
      //Thus, the maximum alignment that we support is list_node_align << 2*(list_node_align - 1)
      // - if list_node_align is 4, we can support up to 4 << 2^(3*2)-1 -> 4 << 63
      // - if list_node_align is 8, we can support up to 8 << 2^(3*3)-1 -> 8 << 255
      const size_type aligned_multiplier = node_alignment / list_node_align;
      const size_type aligned_shift = log2_ceil<size_type>(aligned_multiplier);
      BOOST_CONSTEXPR_OR_CONST size_type max_aligned_shift = std::size_t((1u << 3u*list_node_align_bits) - 1);
      if (aligned_shift > max_aligned_shift) {
         BOOST_ASSERT(aligned_shift <= max_aligned_shift);  //Unsupported alignment
         mp_segment_mngr_base = segment_mngr_base_ptr_t(); //Will crash on allocation
      }

      packed_3n_bits_ref_t packer(m_real_node_size, m_nodes_per_block, m_allocated);
      packer.store(aligned_shift);
      packer.set_low_unpacked(node_size);
      packer.set_high1_unpacked(nodes_per_block);
      packer.set_high2_unpacked(0);
   }

   //!Destructor. Deallocates all allocated blocks. Never throws
   inline ~private_node_pool_impl()
   {  this->purge_blocks();  }

   inline size_type get_real_num_node() const
   {
      packed_3n_bits_ref_t packer(m_real_node_size, m_nodes_per_block, m_allocated);
      return packer.high1_unpacked();
   }

   inline size_type get_real_node_size() const
   {
      packed_3n_bits_ref_t packer(m_real_node_size, m_nodes_per_block, m_allocated);
      return packer.low_unpacked();
   }

   //!Returns the segment manager. Never throws
   inline segment_manager_base_type* get_segment_manager_base()const
   {  return boost::movelib::to_raw_pointer(mp_segment_mngr_base);  }

   inline void *allocate_node()
   {  return this->priv_alloc_node();  }

   //!Deallocates an array pointed by ptr. Never throws
   inline void deallocate_node(void *ptr)
   {  this->priv_dealloc_node(ptr); }

   //!Allocates a singly linked list of n nodes ending in null pointer.
   void allocate_nodes(const size_type n, multiallocation_chain &chain)
   {
      packed_3n_bits_ref_t packer(m_real_node_size, m_nodes_per_block, m_allocated);
      size_type allocated        = packer.high2_unpacked();
      size_type nodes_per_block  = packer.high1_unpacked();

      //Preallocate all needed blocks to fulfill the request
      size_type cur_nodes = m_freelist.size();
      if(cur_nodes < n){
         this->priv_alloc_block(((n - cur_nodes) - 1)/nodes_per_block + 1);
      }

      //We just iterate the needed nodes to get the last we'll erase
      typedef typename free_nodes_t::iterator free_iterator;
      free_iterator before_last_new_it = m_freelist.before_begin();
      for(size_type j = 0; j != n; ++j){
         ++before_last_new_it;
      }

      //Cache the first node of the allocated range before erasing
      free_iterator first_node(m_freelist.begin());
      free_iterator last_node (before_last_new_it);

      //Erase the range. Since we already have the distance, this is O(1)
      m_freelist.erase_after( m_freelist.before_begin()
                            , ++free_iterator(before_last_new_it)
                            , n);

      //Now take the last erased node and just splice it in the end
      //of the intrusive list that will be traversed by the multialloc iterator.
      chain.incorporate_after(chain.before_begin(), &*first_node, &*last_node, n);
      allocated += n;
      packer.set_high2_unpacked(allocated);
   }

   void deallocate_nodes(multiallocation_chain &chain)
   {
      typedef typename multiallocation_chain::iterator iterator;
      iterator it(chain.begin()), itend(chain.end());
      while(it != itend){
         void *pElem = &*it;
         ++it;
         this->priv_dealloc_node(pElem);
      }
   }

   //!Deallocates all the free blocks of memory. Never throws
   void deallocate_free_blocks()
   {
      typedef typename free_nodes_t::iterator nodelist_iterator;
      typename blockslist_t::iterator bit(m_blocklist.before_begin()),
                                      it(m_blocklist.begin()),
                                      itend(m_blocklist.end());
      free_nodes_t backup_list;
      nodelist_iterator backup_list_last = backup_list.before_begin();

      //Execute the algorithm and get an iterator to the last value
      packed_3n_bits_ref_t packer(m_real_node_size, m_nodes_per_block, m_allocated);
      size_type real_node_size   = packer.low_unpacked();
      size_type nodes_per_block  = packer.high1_unpacked();

      size_type blocksize = real_node_size*nodes_per_block;

      while(it != itend){
         //Collect all the nodes from the block pointed by it
         //and push them in the list
         free_nodes_t free_nodes;
         nodelist_iterator last_it = free_nodes.before_begin();
         const void *addr = get_block_from_hook(&*it, blocksize);

         m_freelist.remove_and_dispose_if
            (is_between(addr, blocksize), push_in_list(free_nodes, last_it));

         //If the number of nodes is equal to nodes_per_block
         //this means that the block can be deallocated
         if(free_nodes.size() == nodes_per_block){
            //Unlink the nodes
            free_nodes.clear();
            it = m_blocklist.erase_after(bit);
            mp_segment_mngr_base->deallocate(const_cast<void*>(addr));
         }
         //Otherwise, insert them in the backup list, since the
         //next "remove_if" does not need to check them again.
         else{
            //Assign the iterator to the last value if necessary
            if(backup_list.empty() && !m_freelist.empty()){
               backup_list_last = last_it;
            }
            //Transfer nodes. This is constant time.
            backup_list.splice_after
               ( backup_list.before_begin()
               , free_nodes
               , free_nodes.before_begin()
               , last_it
               , free_nodes.size());
            bit = it;
            ++it;
         }
      }
      //We should have removed all the nodes from the free list
      BOOST_ASSERT(m_freelist.empty());

      //Now pass all the node to the free list again
      m_freelist.splice_after
         ( m_freelist.before_begin()
         , backup_list
         , backup_list.before_begin()
         , backup_list_last
         , backup_list.size());
   }

   inline size_type num_free_nodes()
   {  return m_freelist.size();  }

   //!Deallocates all used memory. Precondition: all nodes allocated from this pool should
   //!already be deallocated. Otherwise, undefined behaviour. Never throws
   void purge_blocks()
   {
      packed_3n_bits_ref_t packer(m_real_node_size, m_nodes_per_block, m_allocated);
      size_type real_node_size   = packer.low_unpacked();
      size_type nodes_per_block  = packer.high1_unpacked();
      size_type allocated        = packer.high2_unpacked();

      //check for memory leaks
      BOOST_ASSERT(allocated == 0); (void)allocated;
      const size_type blocksize = real_node_size * nodes_per_block;

      //We iterate though the NodeBlock list to free the memory
      while(!m_blocklist.empty()){
         void *addr = get_block_from_hook(&m_blocklist.front(), blocksize);
         m_blocklist.pop_front();
         mp_segment_mngr_base->deallocate(const_cast<void*>(addr));
      }
      //Just clear free node list
      m_freelist.clear();
   }

   void swap(private_node_pool_impl &other)
   {
      BOOST_ASSERT(this->get_real_node_size() == other.get_real_node_size());
      BOOST_ASSERT(this->get_real_num_node() == other.get_real_num_node());
      std::swap(mp_segment_mngr_base, other.mp_segment_mngr_base);
      m_blocklist.swap(other.m_blocklist);
      m_freelist.swap(other.m_freelist);
      std::swap(m_allocated, other.m_allocated);
   }

   private:

   struct push_in_list
   {
      push_in_list(free_nodes_t &l, typename free_nodes_t::iterator &it)
         :  slist_(l), last_it_(it)
      {}

      void operator()(typename free_nodes_t::pointer p) const
      {
         slist_.push_front(*p);
         if(slist_.size() == 1){ //Cache last element
            ++last_it_ = slist_.begin();
         }
      }

      private:
      free_nodes_t &slist_;
      typename free_nodes_t::iterator &last_it_;
   };

   struct is_between
   {
      typedef typename free_nodes_t::value_type argument_type;
      typedef bool                              result_type;

      is_between(const void *addr, std::size_t size)
         :  beg_(static_cast<const char *>(addr)), end_(beg_+size)
      {}

      bool operator()(typename free_nodes_t::const_reference v) const
      {
         return (beg_ <= reinterpret_cast<const char *>(&v) &&
                 end_ >  reinterpret_cast<const char *>(&v));
      }
      private:
      const char *      beg_;
      const char *      end_;
   };

   //!Allocates one node, using single segregated storage algorithm.
   //!Never throws
   list_node_t *priv_alloc_node()
   {
      //If there are no free nodes we allocate a new block
      if (m_freelist.empty())
         this->priv_alloc_block(1);
      //We take the first free node
      list_node_t *n = (list_node_t*)&m_freelist.front();
      m_freelist.pop_front();
      packed_3n_bits_ref_t packer(m_real_node_size, m_nodes_per_block, m_allocated);
      size_type allocated        = packer.high2_unpacked();

      ++allocated;
      packer.set_high2_unpacked(allocated);
      return n;
   }

   //!Deallocates one node, using single segregated storage algorithm.
   //!Never throws
   void priv_dealloc_node(void *pElem)
   {
      //We put the node at the beginning of the free node list
      list_node_t * to_deallocate = static_cast<list_node_t*>(pElem);
      m_freelist.push_front(*to_deallocate);
      packed_3n_bits_ref_t packer(m_real_node_size, m_nodes_per_block, m_allocated);
      size_type allocated        = packer.high2_unpacked();
      BOOST_ASSERT(allocated>0);
      --allocated;
      packer.set_high2_unpacked(allocated);
   }

   //!Allocates several blocks of nodes. Can throw
   void priv_alloc_block(size_type num_blocks)
   {
      BOOST_ASSERT(num_blocks > 0);

      packed_3n_bits_ref_t packer(m_real_node_size, m_nodes_per_block, m_allocated);
      size_type real_node_size   = packer.low_unpacked();
      size_type nodes_per_block  = packer.high1_unpacked();
      size_type mem_alignment    = list_node_align << packer.load();

      const size_type blocksize = real_node_size * nodes_per_block;

      BOOST_CONTAINER_TRY{
         for(size_type i = 0; i != num_blocks; ++i){
            //We allocate a new NodeBlock and put it as first
            //element in the free Node list
            char *pNode = reinterpret_cast<char*>
               (mp_segment_mngr_base->allocate_aligned(blocksize + sizeof(list_node_t), mem_alignment));
            char *pBlock = pNode;
            m_blocklist.push_front(get_block_hook(pBlock, blocksize));

            //We initialize all Nodes in Node Block to insert
            //them in the free Node list
            for(size_type j = 0; j < nodes_per_block; ++j, pNode += real_node_size){
               m_freelist.push_front(*new (pNode) list_node_t);
            }
         }
      }
      BOOST_CONTAINER_CATCH(...){
         //to-do: if possible, an efficient way to deallocate allocated blocks
         BOOST_CONTAINER_RETHROW
      }
      BOOST_CONTAINER_CATCH_END
   }

   //!Deprecated, use deallocate_free_blocks
   void deallocate_free_chunks()
   {  this->deallocate_free_blocks(); }

   //!Deprecated, use purge_blocks
   void purge_chunks()
   {  this->purge_blocks(); }

   private:
   //!Returns a reference to the block hook placed in the end of the block
   static list_node_t & get_block_hook (void *block, size_type blocksize)
   {
      return *move_detail::force_ptr<list_node_t*>(reinterpret_cast<char*>(block) + blocksize);
   }

   //!Returns the starting address of the block reference to the block hook placed in the end of the block
   void *get_block_from_hook (list_node_t *hook, size_type blocksize)
   {
      return (reinterpret_cast<char*>(hook) - blocksize);
   }

   private:
   typedef typename boost::intrusive::pointer_traits
      <void_pointer>::template rebind_pointer<segment_manager_base_type>::type   segment_mngr_base_ptr_t;

   mutable size_type m_nodes_per_block;
   mutable size_type m_real_node_size;
   segment_mngr_base_ptr_t mp_segment_mngr_base;   //Segment manager
   blockslist_t      m_blocklist;      //Intrusive container of blocks
   free_nodes_t      m_freelist;       //Intrusive container of free nods
   mutable size_type   m_allocated;      //Used nodes for debugging
};


}  //namespace dtl {
}  //namespace container {
}  //namespace boost {

#include <boost/container/detail/config_end.hpp>

#endif   //#ifndef BOOST_CONTAINER_DETAIL_ADAPTIVE_NODE_POOL_IMPL_HPP
