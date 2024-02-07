/* Copyright (c) 2019, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MEM_ROOT_DEQUE_H
#define MEM_ROOT_DEQUE_H

#include <algorithm>
#include <memory>
#include <type_traits>
#include <utility>

#include <assert.h>
#include <stdint.h>

#include "my_alloc.h"

template <class Element_type>
static constexpr size_t FindElementsPerBlock() {
  // Aim for 1 kB.
  size_t base_number_elems =
      1024 / sizeof(Element_type);  // NOLINT(bugprone-sizeof-expression)

  // Find the next power of two, rounded up. We should have at least 16 elements
  // per block to avoid allocating way too often (although the code itself
  // should work fine with 1, for debugging purposes).
  for (size_t block_size = 16; block_size < 1024; ++block_size) {
    if (block_size >= base_number_elems) {
      return block_size;
    }
  }
  return 1024;
}

/**
  A (partial) implementation of std::deque allocating its blocks on a MEM_ROOT.

  This class works pretty much like an std::deque with a Mem_root_allocator,
  and used to be a forwarder to it. However, libstdc++ has a very complicated
  implementation of std::deque, leading to code blowup (e.g., operator[] is
  23 instructions on x86-64, including two branches), and we cannot easily use
  libc++ on all platforms. This version is instead:

   - Optimized for small, straight-through machine code (few and simple
     instructions, few branches).
   - Optimized for few elements; in particular, zero elements is an important
     special case, much more so than 10,000.

  It gives mostly the same guarantees as std::deque; elements can be
  inserted efficiently on both front and back [1]. {push,pop}_{front,back}
  guarantees reference stability except for to removed elements (obviously),
  and invalidates iterators. (Iterators are forcefully invalidated using
  asserts.) erase() does the same. Note that unlike std::deque, insert()
  at begin() will invalidate references. Some functionality, like several
  constructors, resize(), shrink_to_fit(), swap(), etc. is missing.

  The implementation is the same as classic std::deque: Elements are held in
  blocks of about 1 kB each. Once an element is in a block, it never moves
  (giving the aforementioned pointer stability). The blocks are held in an
  array, which can be reallocated when more blocks are needed. The
  implementation keeps track of the used items in terms of “physical indexes”;
  element 0 starts at the first byte of the first block, element 1 starts
  immediately after 0, and so on. So you can have something like (assuming very
  small blocks of only 4 elements each for the sake of drawing):

         block 0   block 1   block 2
         ↓         ↓         ↓
        [x x 2 3] [4 5 6 7] [8 9 x x]
             ↑                   ↑
             begin_idx = 2       end_idx = 10

  end_idx counts as is customary one-past-the-end, so in this case, the elements
  [2,9) would be valid, and e.g. (*this)[4] would give physical index 6, which
  points to the third element (index 2) in the middle block (block 1). Inserting
  a new element at the front is as easy as putting it in physical index 1 and
  adjusting begin_idx to the left. This means a lookup by index requires some
  shifting, masking and a double indirect load.

  Iterators keep track of which deque they belong to, and what physical index
  they point to. (So lookup by iterator also requires a double indirect load,
  but the alternative would be caching the block pointer and having an extra
  branch when advancing the iterator.) Inserting a new block at the beginning
  would move around all the physical indexes (which is why iterators get
  invalidated; we could probably get around this by having an “offset to first
  block”, but it's likely not worth it.)

  [1] Actually, it's O(n), since there's no exponential growth of the blocks
  array. But the blocks are reallocated very rarely, so it is generally
  efficient nevertheless.
 */
template <class Element_type>
class mem_root_deque {
 public:
  /// Used to conform to STL algorithm demands.
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using value_type = Element_type;
  using pointer = Element_type *;
  using reference = Element_type &;
  using const_pointer = const Element_type *;
  using const_reference = const Element_type &;

  /// Constructor. Leaves the array in an empty, valid state.
  explicit mem_root_deque(MEM_ROOT *mem_root) : m_root(mem_root) {}

  // Copy constructor and assignment. We could probably be a bit smarter
  // about these for large arrays, if the source array has e.g. empty blocks.
  mem_root_deque(const mem_root_deque &other)
      : m_begin_idx(other.m_begin_idx),
        m_end_idx(other.m_end_idx),
        m_capacity(other.m_capacity),
        m_root(other.m_root) {
    m_blocks = m_root->ArrayAlloc<Block>(num_blocks());
    for (size_t block_idx = 0; block_idx < num_blocks(); ++block_idx) {
      m_blocks[block_idx].init(m_root);
    }
    for (size_t idx = m_begin_idx; idx != m_end_idx; ++idx) {
      new (&get(idx)) Element_type(other.get(idx));
    }
  }
  mem_root_deque &operator=(const mem_root_deque &other) {
    if (this != &other) {
      clear();
      for (const Element_type &elem : other) {
        push_back(elem);
      }
    }
    return *this;
  }

  // Move constructor and assignment.
  mem_root_deque(mem_root_deque &&other)
      : m_blocks(other.m_blocks),
        m_begin_idx(other.m_begin_idx),
        m_end_idx(other.m_end_idx),
        m_root(other.m_root) {
    other.m_blocks = nullptr;
    other.m_begin_idx = other.m_end_idx = other.m_capacity = 0;
    other.invalidate_iterators();
  }
  mem_root_deque &operator=(mem_root_deque &&other) {
    if (this != &other) {
      this->~mem_root_deque();
      new (this) mem_root_deque(std::move(other));
    }
    return *this;
  }

  ~mem_root_deque() { clear(); }

  Element_type &operator[](size_t idx) const { return get(idx + m_begin_idx); }

  /**
    Adds the given element to the end of the deque.
    The element is a copy of the given one.

    @returns true on OOM (no change is done if so)
   */
  bool push_back(const Element_type &element) {
    if (m_end_idx == m_capacity) {
      if (add_block_back()) {
        return true;
      }
    }
    new (&get(m_end_idx++)) Element_type(element);
    invalidate_iterators();
    return false;
  }

  /**
    Adds the given element to the end of the deque.
    The element is moved into place.

    @returns true on OOM (no change is done if so)
   */
  bool push_back(Element_type &&element) {
    if (m_end_idx == m_capacity) {
      if (add_block_back()) {
        return true;
      }
    }
    new (&get(m_end_idx++)) Element_type(std::move(element));
    invalidate_iterators();
    return false;
  }

  /**
    Adds the given element to the beginning of the deque.
    The element is a copy of the given one.

    @returns true on OOM (no change is done if so)
   */
  bool push_front(const Element_type &element) {
    if (m_begin_idx == 0) {
      if (add_block_front()) {
        return true;
      }
      assert(m_begin_idx != 0);
    }
    new (&get(--m_begin_idx)) Element_type(element);
    invalidate_iterators();
    return false;
  }

  /**
    Adds the given element to the end of the deque.
    The element is moved into place.
   */
  bool push_front(Element_type &&element) {
    if (m_begin_idx == 0) {
      if (add_block_front()) {
        return true;
      }
      assert(m_begin_idx != 0);
    }
    new (&get(--m_begin_idx)) Element_type(std::move(element));
    invalidate_iterators();
    return false;
  }

  /// Removes the last element from the deque.
  void pop_back() {
    assert(!empty());
    ::destroy_at(&get(--m_end_idx));
    invalidate_iterators();
  }

  /// Removes the first element from the deque.
  void pop_front() {
    assert(!empty());
    ::destroy_at(&get(m_begin_idx++));
    invalidate_iterators();
  }

  /// Returns the first element in the deque.
  Element_type &front() {
    assert(!empty());
    return get(m_begin_idx);
  }

  const Element_type &front() const {
    assert(!empty());
    return get(m_begin_idx);
  }

  /// Returns the last element in the deque.
  Element_type &back() {
    assert(!empty());
    return get(m_end_idx - 1);
  }

  const Element_type &back() const {
    assert(!empty());
    return get(m_end_idx - 1);
  }

  /// Removes all elements from the deque. Destructors are called,
  /// but since the elements themselves are allocated on the MEM_ROOT,
  /// their memory cannot be freed.
  void clear() {
    for (size_t idx = m_begin_idx; idx != m_end_idx; ++idx) {
      ::destroy_at(&get(idx));
    }
    m_begin_idx = m_end_idx = m_capacity / 2;
    invalidate_iterators();
  }

  template <class Iterator_element_type>
  class Iterator {
   public:
    using difference_type = ptrdiff_t;
    using value_type = Iterator_element_type;
    using pointer = Iterator_element_type *;
    using reference = Iterator_element_type &;
    using iterator_category = std::random_access_iterator_tag;

    // DefaultConstructible (required for ForwardIterator).
    Iterator() = default;

    Iterator(const mem_root_deque *deque, size_t physical_idx)
        : m_deque(deque), m_physical_idx(physical_idx) {
#ifndef NDEBUG
      m_generation = m_deque->generation();
#endif
    }

    /// For const_iterator: Implicit conversion from iterator.
    /// This is written in a somewhat cumbersome fashion to avoid
    /// declaring an explicit copy constructor for iterator,
    /// which causes compiler warnings other places for some compilers.
    // NOLINTNEXTLINE(google-explicit-constructor): Intentional.
    template <
        class T,
        typename = std::enable_if_t<
            std::is_const<Iterator_element_type>::value &&
            std::is_same<typename T::value_type,
                         std::remove_const_t<Iterator_element_type>>::value>>
    Iterator(const T &other)
        : m_deque(other.m_deque), m_physical_idx(other.m_physical_idx) {
#ifndef NDEBUG
      m_generation = other.m_generation;
#endif
    }

    // Iterator (required for InputIterator).
    Iterator_element_type &operator*() const {
      assert_not_invalidated();
      return m_deque->get(m_physical_idx);
    }
    Iterator &operator++() {
      assert_not_invalidated();
      ++m_physical_idx;
      return *this;
    }

    // EqualityComparable (required for InputIterator).
    bool operator==(const Iterator &other) const {
      assert_not_invalidated();
      assert(m_deque == other.m_deque);
      return m_physical_idx == other.m_physical_idx;
    }

    // InputIterator (required for ForwardIterator).
    bool operator!=(const Iterator &other) const { return !(*this == other); }

    Iterator_element_type *operator->() const {
      assert_not_invalidated();
      return &m_deque->get(m_physical_idx);
    }

    // ForwardIterator (required for RandomAccessIterator).
    Iterator operator++(int) {
      assert_not_invalidated();
      Iterator ret = *this;
      ++m_physical_idx;
      return ret;
    }

    // BidirectionalIterator (required for RandomAccessIterator).
    Iterator &operator--() {
      assert_not_invalidated();
      --m_physical_idx;
      return *this;
    }
    Iterator operator--(int) {
      assert_not_invalidated();
      Iterator ret = *this;
      --m_physical_idx;
      return ret;
    }

    // RandomAccessIterator.
    Iterator &operator+=(difference_type diff) {
      assert_not_invalidated();
      m_physical_idx += diff;
      return *this;
    }

    Iterator &operator-=(difference_type diff) {
      assert_not_invalidated();
      m_physical_idx -= diff;
      return *this;
    }

    Iterator operator+(difference_type offset) {
      assert_not_invalidated();
      return Iterator{m_deque, m_physical_idx + offset};
    }

    Iterator operator-(difference_type offset) {
      assert_not_invalidated();
      return Iterator{m_deque, m_physical_idx - offset};
    }

    difference_type operator-(const Iterator &other) const {
      assert_not_invalidated();
      assert(m_deque == other.m_deque);
      return m_physical_idx - other.m_physical_idx;
    }

    Iterator_element_type &operator[](size_t idx) const {
      return *(*this + idx);
    }

    bool operator<(const Iterator &other) const {
      assert_not_invalidated();
      assert(m_deque == other.m_deque);
      return m_physical_idx < other.m_physical_idx;
    }

    bool operator<=(const Iterator &other) const { return !(*this > other); }

    bool operator>(const Iterator &other) const {
      assert_not_invalidated();
      assert(m_deque == other.m_deque);
      return m_physical_idx > other.m_physical_idx;
    }

    bool operator>=(const Iterator &other) const { return !(*this < other); }

   private:
    const mem_root_deque *m_deque = nullptr;
    size_t m_physical_idx = 0;
#ifndef NDEBUG
    size_t m_generation = 0;
#endif

    void assert_not_invalidated() const {
      assert(m_generation == m_deque->generation());
    }

    friend class mem_root_deque;
  };

  using iterator = Iterator<Element_type>;
  using const_iterator = Iterator<const Element_type>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using reverse_const_iterator = std::reverse_iterator<const_iterator>;

  iterator begin() { return iterator{this, m_begin_idx}; }
  iterator end() { return iterator{this, m_end_idx}; }
  reverse_iterator rbegin() { return std::make_reverse_iterator(end()); }
  reverse_iterator rend() { return std::make_reverse_iterator(begin()); }
  const_iterator cbegin() { return const_iterator{this, m_begin_idx}; }
  const_iterator cend() { return const_iterator{this, m_end_idx}; }
  const_iterator begin() const { return const_iterator{this, m_begin_idx}; }
  const_iterator end() const { return const_iterator{this, m_end_idx}; }
  reverse_const_iterator crbegin() const {
    return std::make_reverse_iterator(end());
  }
  reverse_const_iterator crend() const {
    return std::make_reverse_iterator(begin());
  }
  reverse_const_iterator rbegin() const {
    return std::make_reverse_iterator(end());
  }
  reverse_const_iterator rend() const {
    return std::make_reverse_iterator(begin());
  }

  size_t size() const { return m_end_idx - m_begin_idx; }
  bool empty() const { return size() == 0; }

  /**
    Erase all the elements in the specified range.

    @param first  iterator that points to the first element to remove
    @param last   iterator that points to the element after the
                  last one to remove
    @return an iterator to the first element after the removed range
  */
  iterator erase(const_iterator first, const_iterator last) {
    iterator pos = begin() + (first - cbegin());
    if (first != last) {
      iterator new_end = std::move(last, cend(), pos);
      for (size_t idx = new_end.m_physical_idx; idx != m_end_idx; ++idx) {
        ::destroy_at(&get(idx));
      }
      m_end_idx = new_end.m_physical_idx;
    }
    invalidate_iterators();
#ifndef NDEBUG
    pos.m_generation = m_generation;  // Re-validate.
#endif
    return pos;
  }

  /**
    Removes a single element from the array.

    @param position  iterator that points to the element to remove

    @return an iterator to the first element after the removed range
  */
  iterator erase(const_iterator position) {
    return erase(position, std::next(position));
  }

  /**
    Insert an element at a given position.
    The element is a copy of the given one.

    @param pos    the new element is inserted before the element
                  at this position
    @param value  the value of the new element
    @return an iterator that points to the inserted element
  */
  iterator insert(const_iterator pos, const Element_type &value) {
    difference_type idx = pos - cbegin();
    push_back(value);
    std::rotate(begin() + idx, end() - 1, end());
    invalidate_iterators();
    return begin() + idx;
  }

  /**
    Insert an element at a given position.
    The element is moved into place.

    @param pos    the new element is inserted before the element
                  at this position
    @param value  the value of the new element
    @return an iterator that points to the inserted element
  */
  iterator insert(const_iterator pos, Element_type &&value) {
    difference_type idx = pos - cbegin();
    push_back(std::move(value));
    std::rotate(begin() + idx, end() - 1, end());
    invalidate_iterators();
    return begin() + idx;
  }

  template <class ForwardIt>
  iterator insert(const_iterator pos, ForwardIt first, ForwardIt last) {
    difference_type idx = pos - cbegin();
    for (ForwardIt it = first; it != last; ++it) {
      push_back(*it);
    }
    std::rotate(begin() + idx, end() - (last - first), end());
    invalidate_iterators();
    return begin() + idx;
  }

 private:
  /// Number of elements in each block.
  static constexpr size_t block_elements = FindElementsPerBlock<Element_type>();

  // A block capable of storing <block_elements> elements. Deliberately
  // has no constructor, since it wouldn't help any of the code that actually
  // allocates any blocks (all it would do would be to hinder using
  // ArrayAlloc when allocating new blocks).
  struct Block {
    Element_type *elements;

    bool init(MEM_ROOT *mem_root) {
      // Use Alloc instead of ArrayAlloc, so that no constructors are called.
      elements = static_cast<Element_type *>(mem_root->Alloc(
          block_elements *
          sizeof(Element_type)));  // NOLINT(bugprone-sizeof-expression)
      return elements == nullptr;
    }
  };

  /// Pointer to the first block. Can be nullptr if there are no elements
  /// (this makes the constructor cheaper). Stored on the MEM_ROOT,
  /// and needs no destruction, so just a raw pointer.
  Block *m_blocks = nullptr;

  /// Physical index to the first valid element.
  size_t m_begin_idx = 0;

  /// Physical index one past the last valid element. If begin == end,
  /// the array is empty (and then it doesn't matter what the values are).
  size_t m_end_idx = 0;

  /// Number of blocks, multiplied by block_elements. (Storing this instead
  /// of the number of blocks itself makes testing in push_back cheaper.)
  size_t m_capacity = 0;

  /// Pointer to the MEM_ROOT that we store our blocks and elements on.
  MEM_ROOT *m_root;

#ifndef NDEBUG
  /// Incremented each time we make an operation that would invalidate
  /// iterators. Asserts use this value in debug mode to be able to
  /// verify that they have not been invalidated. (In optimized mode,
  /// using an invalidated iterator incurs undefined behavior.)
  size_t m_generation = 0;
  void invalidate_iterators() { ++m_generation; }
#else
  void invalidate_iterators() {}
#endif

  /// Adds the first block of elements.
  bool add_initial_block() {
    m_blocks = m_root->ArrayAlloc<Block>(1);
    if (m_blocks == nullptr) {
      return true;
    }
    if (m_blocks[0].init(m_root)) {
      return true;
    }
    m_begin_idx = m_end_idx = block_elements / 2;
    m_capacity = block_elements;
    return false;
  }

  // Not inlined, to get them off of the hot path.
  bool add_block_back();
  bool add_block_front();

  size_t num_blocks() const { return m_capacity / block_elements; }

  /// Gets a reference to the memory used to store an element with the given
  /// physical index, starting from zero. Note that block_elements is always
  /// a power of two, so the division and modulus operations are cheap.
  Element_type &get(size_t physical_idx) const {
    return m_blocks[physical_idx / block_elements]
        .elements[physical_idx % block_elements];
  }

#ifndef NDEBUG
  size_t generation() const { return m_generation; }
#endif
};

// TODO(sgunders): Consider storing spare blocks at either end to have
// exponential growth and get true O(1) allocation.

template <class Element_type>
bool mem_root_deque<Element_type>::add_block_back() {
  if (m_blocks == nullptr) {
    return add_initial_block();
  }
  Block *new_blocks = m_root->ArrayAlloc<Block>(num_blocks() + 1);
  if (new_blocks == nullptr) {
    return true;
  }
  memcpy(new_blocks, m_blocks, sizeof(Block) * num_blocks());
  if (new_blocks[num_blocks()].init(m_root)) {
    return true;
  }

  m_blocks = new_blocks;
  m_capacity += block_elements;
  return false;
}

template <class Element_type>
bool mem_root_deque<Element_type>::add_block_front() {
  if (m_blocks == nullptr) {
    if (add_initial_block()) {
      return true;
    }
    if (m_begin_idx == 0) {
      // Only relevant for very small values of block_elements.
      m_begin_idx = m_end_idx = 1;
    }
    return false;
  }
  Block *new_blocks = m_root->ArrayAlloc<Block>(num_blocks() + 1);
  memcpy(new_blocks + 1, m_blocks, sizeof(Block) * num_blocks());
  if (new_blocks[0].init(m_root)) {
    return true;
  }

  m_blocks = new_blocks;
  m_begin_idx += block_elements;
  m_end_idx += block_elements;
  m_capacity += block_elements;
  return false;
}

#endif  // MEM_ROOT_DEQUE_H
