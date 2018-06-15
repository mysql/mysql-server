/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/** @file storage/temptable/include/temptable/storage.h
TempTable Storage. */

#ifndef TEMPTABLE_STORAGE_H
#define TEMPTABLE_STORAGE_H

#include <cstddef>
#include <utility>

#include "my_dbug.h"
#include "storage/temptable/include/temptable/allocator.h"
#include "storage/temptable/include/temptable/constants.h"

namespace temptable {

/** Storage container. This mimics std::vector and std::deque with the
 * difference that the size of the objects that are being stored in it can be
 * determined at runtime. Elements are stored next to each other in a
 * pre-allocated pages of fixed size `STORAGE_PAGE_SIZE`. */
class Storage {
 public:
  /** Type used for elements. We treat elements as black boxes. */
  typedef void Element;
  /** Type used for pages. */
  typedef void Page;

  /** Iterator over a Storage object. */
  class Iterator {
   public:
    /** Default constructor. This creates a hollow iterator object, that must be
     * assigned afterwards. */
    Iterator();

    /** Constructor where the Storage object to iterate over and the starting
     * position are provided. The iterator is fully initialized after this and
     * ready for iteration. */
    Iterator(
        /** [in] Storage object to iterate over. */
        const Storage *storage,
        /** [in] Initial position of the iterator, must point to an element
         * inside `storage`. */
        const Element *element);

    /** Copy-construct from another iterator. */
    Iterator(const Iterator &) = default;

    /** Dereference the iterator to the element it points to.
     * @return the element where the iterator is positioned */
    Element *operator*() const;

    /** Assign a new position within the same Storage object.
     * @return *this */
    Iterator &operator=(
        /** [in] New position for the iterator. */
        const Element *element);

    /** Copy-assign from another iterator. */
    Iterator &operator=(const Iterator &) = default;

    /** Compare with another iterator. For two iterators to be equal, they must
     * be positioned on the same element in the same storage.
     * @return true if equal */
    bool operator==(
        /** [in] Iterator to compare with. */
        const Iterator &rhs) const;

    /** Compare with another iterator. For two iterators to be equal, they must
     * be positioned on the same element in the same storage.
     * @return true if not equal */
    bool operator!=(
        /** [in] Iterator to compare with. */
        const Iterator &rhs) const;

    /** Advance the iterator one element forward. If the iterator points to
     * end() before this call, then the behavior is undefined.
     * @return *this */
    Iterator &operator++();

    /** Recede the iterator one element backwards. If the iterator points to
     * begin() before this call, then the behavior is undefined.
     * @return *this */
    Iterator &operator--();

   private:
    /** Storage over which the iterator operates. */
    Storage *m_storage;

    /** Current element. */
    Element *m_element;
  };

  /** Constructor. */
  explicit Storage(
      /** [in,out] Allocator to use for allocating pages. */
      Allocator<uint8_t> *allocator);

  /** Copy constructing is disabled, too expensive and not necessary. */
  Storage(const Storage &) = delete;

  /** Copy assignment is disabled, too expensive and not necessary. */
  Storage &operator=(const Storage &) = delete;

  /** Move constructor. */
  Storage(
      /** [in,out] Object whose state to grasp, after this call the state of
       * `other` is undefined. */
      Storage &&other);

  /** Move assignment. */
  Storage &operator=(
      /** [in,out] Object whose state to grasp, after this call the state of
       * `rhs` is undefined. */
      Storage &&rhs);

  /** Destructor. */
  ~Storage();

  /** Get an iterator, positioned on the first element.
   * @return iterator */
  Iterator begin() const;

  /** Get an iterator, positioned after the last element.
   * @return iterator */
  Iterator end() const;

  /** Set the element size. Only allowed if the storage is empty. */
  void element_size(
      /** [in] New element size to set, in bytes. */
      size_t element_size);

  /** Get the element size.
   * @return element size in bytes. */
  size_t element_size() const;

  /** Get the number of elements in the storage.
   * @return number of elements. */
  size_t size() const;

  /** Get the last element.
   * @return a pointer to the last element */
  Element *back();

  /** Allocate space for one more element at the end and return a pointer to it.
   * This will increase `size()` by one.
   * @return pointer to the newly created (uninitialized) element */
  Element *allocate_back();

  /** Destroy the last element. This will decrease `size()` by one. */
  void deallocate_back();

  /** Delete the element pointed to by `position`. Subsequent or previous
   * iterators are not invalidated. The memory occupied by the deleted element
   * is not returned to the underlying allocator.
   * @return an iterator to the next element (or end() if `position` points to
   * the last element before this call) */
  Iterator erase(
      /** [in] Delete element at this position. */
      const Iterator &position);

  /** Delete all elements in the storage. After this `size()` will be zero. */
  void clear();

 private:
  /** Align elements to this number of bytes. */
  static constexpr size_t ALIGN_TO = alignof(void *);

  /** Flag that denotes an element is the first element on a page. */
  static constexpr uint8_t ELEMENT_FIRST_ON_PAGE = 0x1;

  /** Flag that denotes an element is the last element on a page. */
  static constexpr uint8_t ELEMENT_LAST_ON_PAGE = 0x2;

  /** Flag that denotes an element is deleted. Deleted elements are skipped
   * during iteration. */
  static constexpr uint8_t ELEMENT_DELETED = 0x4;

  /** Extra bytes per element for element metadata. It must store all ELEMENT_*
   * bits. */
  static constexpr size_t META_BYTES_PER_ELEMENT = 1;

  /** Extra bytes per page for page metadata. This stores the previous and next
   * page pointers. */
  static constexpr size_t META_BYTES_PER_PAGE = 2 * sizeof(Page *);

  /** Calculate the size of a page. This is usually a little bit less than
   * `STORAGE_PAGE_SIZE`. For example if `STORAGE_PAGE_SIZE == 100` and our
   * element size is 10 and we need 4 extra bytes per page, then the calculated
   * page size will be 94: 9 elements (10 bytes each) and the extra 4 bytes.
   * @return page size in bytes */
  size_t page_size() const;

  /** Get a pointer to element's meta byte(s).
   * @return pointer to meta byte(s) */
  uint8_t *element_meta(
      /** [in] Element whose meta byte(s) to get a pointer to. */
      Element *element) const;

  /** Check if element is the first on its page. If the element is the first,
   * then the previous page pointer is stored right before that element (and its
   * meta bytes).
   * @return true if first */
  bool element_first_on_page(
      /** [in] Element to check. */
      Element *element) const;

  /** Set element's first-on-page flag. */
  void element_first_on_page(
      /** [in] Flag to set, true if first on page. */
      bool first_on_page,
      /** [in,out] Element to modify. */
      Element *element);

  /** Check if element is the last on its page. If the element is the last,
   * then the next page pointer is stored right after that element.
   * @return true if last */
  bool element_last_on_page(
      /** [in] Element to check. */
      Element *element) const;

  /** Set element's last-on-page flag. */
  void element_last_on_page(
      /** [in] Flag to set, true if last on page. */
      bool last_on_page,
      /** [in,out] Element to modify. */
      Element *element);

  /** Check if element is deleted.
   * @return true if deleted */
  bool element_deleted(
      /** [in] Element to check. */
      Element *element) const;

  /** Set element's deleted flag. */
  void element_deleted(
      /** [in] Flag to set, true if deleted. */
      bool deleted,
      /** [in,out] Element to modify. */
      Element *element);

  /** Get the previous page. Undefined if !element_first_on_page().
   * @return previous page or nullptr if this is the first page */
  Page **element_prev_page_ptr(
      /** [in] The first element on a page. */
      Element *first) const;

  /** Get the next page. Undefined if !element_last_on_page().
   * @return next page or nullptr if this is the last page */
  Page **element_next_page_ptr(
      /** [in] The last element on a page. */
      Element *last) const;

  /** Get the previous element of a given element on the same page. Undefined if
   * this is the first element on the page.
   * @return pointer to previous element, may be delete-marked */
  Element *prev_element(
      /** [in] Element whose sibling in the page to fetch. */
      Element *element) const;

  /** Get the next element of a given element on the same page. Undefined if
   * this is the last element on the page.
   * @return pointer to next element, may be delete-marked */
  Element *next_element(
      /** [in] Element whose sibling in the page to fetch. */
      Element *element) const;

  /** Get the first element of a page.
   * @return pointer to first element, may be delete-marked */
  Element *first_possible_element_on_page(
      /** [in] Page whose first element to fetch. */
      Page *page) const;

  /** Get the last possible element of a page (not the last occupied).
   * @return pointer where the last element would reside if the page is full,
   * may be delete-marked. */
  Element *last_possible_element_on_page(
      /** [in] Page whose last element to fetch. */
      Page *page) const;

  /** Get a pointer inside a page to the place denoting the previous page.
   * @return pointer to the pointer to the previous page */
  Page **page_prev_page_ptr(
      /** [in] Page whose previous to fetch. */
      Page *page) const;

  /** Get a pointer inside a page to the place denoting the next page.
   * @return pointer to the pointer to the next page */
  Page **page_next_page_ptr(
      /** [in] Page whose next to fetch. */
      Page *page) const;

  /** Allocator to use for allocating new pages. */
  Allocator<uint8_t> *m_allocator;

  /** Element size in bytes. */
  size_t m_element_size;

  /** Number of bytes used per element. This accounts for element size,
   * alignment and our element meta bytes. */
  size_t m_bytes_used_per_element;

  /** Maximum number of elements a page can store. */
  size_t m_number_of_elements_per_page;

  /** Number of elements in the container. Not counting deleted ones. */
  size_t m_number_of_elements;

  /** First page of the storage. */
  Page *m_first_page;

  /** Last page of the storage. */
  Page *m_last_page;

  /** Last used element in the last page of the storage. The last page may not
   * be fully occupied, so this may point somewhere in the middle of it. */
  Element *m_last_element;
};

/* Implementation of inlined methods. */

inline Storage::Iterator::Iterator() : m_storage(nullptr), m_element(nullptr) {}

inline Storage::Iterator::Iterator(const Storage *storage,
                                   const Element *element)
    : m_storage(const_cast<Storage *>(storage)),
      m_element(const_cast<Element *>(element)) {}

inline Storage::Element *Storage::Iterator::operator*() const {
  return m_element;
}

inline Storage::Iterator &Storage::Iterator::operator=(const Element *element) {
  DBUG_ASSERT(m_storage != nullptr || element == nullptr);
  m_element = const_cast<Element *>(element);
  return *this;
}

inline bool Storage::Iterator::operator==(const Iterator &rhs) const {
  return m_element == rhs.m_element;
}

inline bool Storage::Iterator::operator!=(const Iterator &rhs) const {
  return !(*this == rhs);
}

inline Storage::Iterator &Storage::Iterator::operator++() {
  DBUG_ASSERT(m_storage != nullptr);
  DBUG_ASSERT(*this != m_storage->end());

  do {
    if (m_storage->element_last_on_page(m_element)) {
      Page *next_page = *m_storage->element_next_page_ptr(m_element);
      if (next_page == nullptr) {
        /* Last element on last page, can't go further. */
        *this = m_storage->end();
        return *this;
      }
      m_element = m_storage->first_possible_element_on_page(next_page);
    } else {
      m_element = m_storage->next_element(m_element);
    }
  } while (m_storage->element_deleted(m_element));

  return *this;
}

inline Storage::Iterator &Storage::Iterator::operator--() {
  DBUG_ASSERT(m_storage != nullptr);
  DBUG_ASSERT(*this != m_storage->begin());

  /* Since *this != m_storage->begin() there is at least one non-deleted element
   * preceding our position (ie the one pointed to by begin()). */

  do {
    if (m_element == nullptr) {
      m_element = m_storage->back();
    } else if (m_storage->element_first_on_page(m_element)) {
      /* Go to the last element on the previous page. */
      Page *prev_page = *m_storage->element_prev_page_ptr(m_element);
      DBUG_ASSERT(prev_page != nullptr);
      m_element = m_storage->last_possible_element_on_page(prev_page);
      DBUG_ASSERT(m_storage->element_last_on_page(m_element));
    } else {
      m_element = m_storage->prev_element(m_element);
    }
  } while (m_storage->element_deleted(m_element));

  return *this;
}

inline Storage::Storage(Allocator<uint8_t> *allocator)
    : m_allocator(allocator),
      m_element_size(0),
      m_bytes_used_per_element(0),
      m_number_of_elements_per_page(0),
      m_number_of_elements(0),
      m_first_page(nullptr),
      m_last_page(nullptr),
      m_last_element(nullptr) {}

inline Storage::Storage(Storage &&other)
    : m_first_page(nullptr), m_last_page(nullptr), m_last_element(nullptr) {
  *this = std::move(other);
}

inline Storage &Storage::operator=(Storage &&rhs) {
  DBUG_ASSERT(m_first_page == nullptr);
  DBUG_ASSERT(m_last_page == nullptr);
  DBUG_ASSERT(m_last_element == nullptr);

  m_allocator = rhs.m_allocator;
  rhs.m_allocator = nullptr;

  m_element_size = rhs.m_element_size;
  rhs.m_element_size = 0;

  m_bytes_used_per_element = rhs.m_bytes_used_per_element;
  rhs.m_bytes_used_per_element = 0;

  m_number_of_elements_per_page = rhs.m_number_of_elements_per_page;
  rhs.m_number_of_elements_per_page = 0;

  m_number_of_elements = rhs.m_number_of_elements;
  rhs.m_number_of_elements = 0;

  m_first_page = rhs.m_first_page;
  rhs.m_first_page = nullptr;

  m_last_page = rhs.m_last_page;
  rhs.m_last_page = nullptr;

  m_last_element = rhs.m_last_element;
  rhs.m_last_element = nullptr;

  return *this;
}

inline Storage::~Storage() { clear(); }

inline Storage::Iterator Storage::begin() const {
  if (m_number_of_elements == 0) {
    return end();
  }

  DBUG_ASSERT(m_first_page != nullptr);

  Iterator it(this, first_possible_element_on_page(m_first_page));

  for (;;) {
    DBUG_ASSERT(it != end());
    if (!element_deleted(*it)) {
      break;
    }
    ++it;
  }

  return it;
}

inline Storage::Iterator Storage::end() const {
  return Storage::Iterator(this, nullptr);
}

inline void Storage::element_size(size_t element_size) {
  DBUG_ASSERT(m_number_of_elements == 0);

  m_element_size = element_size;

  const size_t element_size_plus_meta = element_size + META_BYTES_PER_ELEMENT;

  /* Confirm that ALIGN_TO is a power of 2 (or zero). */
  DBUG_ASSERT(ALIGN_TO == 0 || (ALIGN_TO & (ALIGN_TO - 1)) == 0);

  /* The next multiple of ALIGN_TO from element_size_plus_meta. */
  m_bytes_used_per_element =
      (element_size_plus_meta + ALIGN_TO - 1) & ~(ALIGN_TO - 1);

  m_number_of_elements_per_page =
      (STORAGE_PAGE_SIZE - META_BYTES_PER_PAGE) / m_bytes_used_per_element;
}

inline size_t Storage::element_size() const { return m_element_size; }

inline size_t Storage::size() const { return m_number_of_elements; }

inline Storage::Element *Storage::back() { return m_last_element; }

inline Storage::Element *Storage::allocate_back() {
  if (m_last_page == nullptr) {
    /* The storage is empty, create the first page. */
    DBUG_ASSERT(m_first_page == nullptr);

    m_first_page = m_allocator->allocate(page_size());

    *page_prev_page_ptr(m_first_page) = nullptr;
    *page_next_page_ptr(m_first_page) = nullptr;

    m_last_page = m_first_page;

    m_last_element = first_possible_element_on_page(m_first_page);

#ifndef DBUG_OFF
    *element_meta(m_last_element) = 0;
#endif /* DBUG_OFF */

    element_first_on_page(true, m_last_element);
  } else if (m_last_element == last_possible_element_on_page(m_last_page)) {
    /* Last page is full, create a new one. */
    Page *new_page = m_allocator->allocate(page_size());

    *page_next_page_ptr(m_last_page) = new_page;
    *page_prev_page_ptr(new_page) = m_last_page;
    *page_next_page_ptr(new_page) = nullptr;

    m_last_page = new_page;
    m_last_element = first_possible_element_on_page(new_page);

#ifndef DBUG_OFF
    *element_meta(m_last_element) = 0;
#endif /* DBUG_OFF */

    element_first_on_page(true, m_last_element);
  } else {
    element_last_on_page(false, m_last_element);

    m_last_element = next_element(m_last_element);

#ifndef DBUG_OFF
    *element_meta(m_last_element) = 0;
#endif /* DBUG_OFF */

    element_first_on_page(false, m_last_element);
  }

  ++m_number_of_elements;

  element_last_on_page(true, m_last_element);
  element_deleted(false, m_last_element);
  *element_next_page_ptr(m_last_element) = nullptr;

  return m_last_element;
}

inline void Storage::deallocate_back() {
  DBUG_ASSERT(m_number_of_elements > 0);

  --m_number_of_elements;

  do {
    if (m_last_element != first_possible_element_on_page(m_last_page)) {
      m_last_element = prev_element(m_last_element);
    } else if (m_first_page == m_last_page) {
      DBUG_ASSERT(m_number_of_elements == 0);
      m_allocator->deallocate(static_cast<uint8_t *>(m_first_page),
                              page_size());
      m_first_page = nullptr;
      m_last_page = nullptr;
      m_last_element = nullptr;
      return;
    } else {
      Page *page_to_free = m_last_page;

      m_last_page = *page_prev_page_ptr(m_last_page);
      *page_next_page_ptr(m_last_page) = nullptr;

      m_last_element = last_possible_element_on_page(m_last_page);

      m_allocator->deallocate(static_cast<uint8_t *>(page_to_free),
                              page_size());
    }
  } while (element_deleted(m_last_element));

  element_last_on_page(true, m_last_element);
  *element_next_page_ptr(m_last_element) = nullptr;
}

inline Storage::Iterator Storage::erase(const Iterator &position) {
  Iterator next_element_position = position;
  ++next_element_position;

  if (*position == m_last_element) {
    deallocate_back();
  } else {
    DBUG_ASSERT(m_number_of_elements > 0);
    --m_number_of_elements;

    element_deleted(true, *position);
  }

  return next_element_position;
}

inline void Storage::clear() {
  if (m_first_page == nullptr) {
    DBUG_ASSERT(m_number_of_elements == 0);
    return;
  }

  if (m_first_page == m_last_page) {
    DBUG_ASSERT(m_number_of_elements <= m_number_of_elements_per_page);
  } else {
    Page *p = m_first_page;
    do {
      p = *page_next_page_ptr(p);

      m_allocator->deallocate(static_cast<uint8_t *>(*page_prev_page_ptr(p)),
                              page_size());
    } while (p != m_last_page);
  }

  m_allocator->deallocate(static_cast<uint8_t *>(m_last_page), page_size());

  m_first_page = nullptr;
  m_last_page = nullptr;
  m_number_of_elements = 0;
}

inline size_t Storage::page_size() const {
  DBUG_ASSERT(m_bytes_used_per_element > 0);
  DBUG_ASSERT(m_number_of_elements_per_page > 0);

  return m_bytes_used_per_element * m_number_of_elements_per_page +
         META_BYTES_PER_PAGE;
}

inline uint8_t *Storage::element_meta(Element *element) const {
  return static_cast<uint8_t *>(element) + m_element_size;
}

inline bool Storage::element_first_on_page(Element *element) const {
  return *element_meta(element) & ELEMENT_FIRST_ON_PAGE;
}

inline void Storage::element_first_on_page(bool first_on_page,
                                           Element *element) {
  if (first_on_page) {
    *element_meta(element) |= ELEMENT_FIRST_ON_PAGE;
  } else {
    *element_meta(element) &= ~ELEMENT_FIRST_ON_PAGE;
  }
}

inline bool Storage::element_last_on_page(Element *element) const {
  return *element_meta(element) & ELEMENT_LAST_ON_PAGE;
}

inline void Storage::element_last_on_page(bool last_on_page, Element *element) {
  if (last_on_page) {
    *element_meta(element) |= ELEMENT_LAST_ON_PAGE;
  } else {
    *element_meta(element) &= ~ELEMENT_LAST_ON_PAGE;
  }
}

inline bool Storage::element_deleted(Element *element) const {
  return *element_meta(element) & ELEMENT_DELETED;
}

inline void Storage::element_deleted(bool deleted, Element *element) {
  if (deleted) {
    *element_meta(element) |= ELEMENT_DELETED;
  } else {
    *element_meta(element) &= ~ELEMENT_DELETED;
  }
}

inline Storage::Page **Storage::element_prev_page_ptr(Element *element) const {
  DBUG_ASSERT(element_first_on_page(element));
  return reinterpret_cast<Page **>(static_cast<uint8_t *>(element) -
                                   sizeof(Page *));
}

inline Storage::Page **Storage::element_next_page_ptr(Element *element) const {
  DBUG_ASSERT(element_last_on_page(element));
  return reinterpret_cast<Page **>(static_cast<uint8_t *>(element) +
                                   m_bytes_used_per_element);
}

inline Storage::Element *Storage::prev_element(Element *element) const {
  DBUG_ASSERT(element != nullptr);
  DBUG_ASSERT(!element_first_on_page(element));
  return static_cast<uint8_t *>(element) - m_bytes_used_per_element;
}

inline Storage::Element *Storage::next_element(Element *element) const {
  DBUG_ASSERT(element != nullptr);
  DBUG_ASSERT(!element_last_on_page(element));
  return static_cast<uint8_t *>(element) + m_bytes_used_per_element;
}

inline Storage::Element *Storage::first_possible_element_on_page(
    Page *page) const {
  DBUG_ASSERT(page != nullptr);
  return static_cast<uint8_t *>(page) + sizeof(Page *);
}

inline Storage::Element *Storage::last_possible_element_on_page(
    Page *page) const {
  DBUG_ASSERT(page != nullptr);
  return static_cast<uint8_t *>(page) + sizeof(Page *) +
         m_bytes_used_per_element * (m_number_of_elements_per_page - 1);
}

inline Storage::Page **Storage::page_prev_page_ptr(Page *page) const {
  DBUG_ASSERT(page != nullptr);
  return reinterpret_cast<Page **>(page);
}

inline Storage::Page **Storage::page_next_page_ptr(Page *page) const {
  DBUG_ASSERT(page != nullptr);
  /* The sizeof(Page*) bytes just after the last element (and its meta byte(s)
   * and padding). */
  return reinterpret_cast<Page **>(
      static_cast<uint8_t *>(page) + sizeof(Page *) +
      m_bytes_used_per_element * m_number_of_elements_per_page);
}

} /* namespace temptable */

#endif /* TEMPTABLE_STORAGE_H */
