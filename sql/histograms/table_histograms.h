#ifndef HISTOGRAMS_TABLE_HISTOGRAMS_INCLUDED
#define HISTOGRAMS_TABLE_HISTOGRAMS_INCLUDED

/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include <array>
#include <cassert>

#include "map_helpers.h"  // mem_root_unordered_map
#include "my_alloc.h"     // MEM_ROOT

#include "sql/histograms/histogram.h"
#include "sql/psi_memory_key.h"

/*
The Table_histograms_collection holds a reference-counted collection of
Table_histograms objects.

Table_histograms_collection memory management
---------------------------------------------

Each TABLE_SHARE has a pointer to a Table_histograms_collection that is
allocated on the TABLE_SHARE MEM_ROOT when prepare_share() is called from
dd_table_share.cc:open_table_def().

The Table_histograms_collection is destroyed (its destructor is invoked) when
TABLE_SHARE::destroy() is called. The TABLE_SHARE MEM_ROOT also frees its memory
so the Table_histograms_collection object is also freed.

Table_histograms_collection and its managing of Table_histograms
----------------------------------------------------------------

The Table_histograms objects in the Table_histograms_collection each represent a
snapshot of the histograms on a table. The state of this collection is
manipulated through three methods:

insert(): Inserts a Table_histograms object and marks it current, removing the
current object if it has a reference count of zero.

acquire(): Returns a pointer to the current Table_histograms object and
increments its reference count.

release(): Releases a Table_histograms object back by decreasing its reference
count. Removes the object if it has a reference count of zero and is
non-current.

A Table_histograms object is inserted into the collection when the TABLE_SHARE
is first opened (not found in the table definition cache) in
sql_base.cc:get_table_share(). An updated Table_histograms object is inserted
following a successful ANALYZE TABLE UPDATE/DROP HISTOGRAM command. This happens
in sql_admin.cc:update_share_histograms().

TABLE objects acquire() a pointer to a Table_histograms object from the
Table_histograms_collection when a table is first opened through
table.cc:open_table_from_share(). This is the only place where acquire() is
called.

TABLE objects release() the pointer to the Table_histograms object back to the
Table_histograms_collection when the TABLE is destroyed and freed in
sql_base.cc:intern_close_table. If an error happens after a Table_histograms
object has been acquired during open_table_from_share() we also make sure to
release() it back. Finally histograms are released back in a few code paths that
perform ad-hoc opening of tables in connection with the REPAIR statement.

Table_histograms_collection concurrency
---------------------------------------

Because multiple threads can be attempting to insert/acquire/release
Table_histograms from the Table_histograms_collection on a single TABLE_SHARE we
require some concurrency control.

In order to protect the Table_histograms_collection from concurrent modification
we make sure to lock/unlock the LOCK_open mutex around certain operations. The
mutex protection is performed outside of the object (each object does not have
its own mutex), and must be seen in the context of the lifetime of the
TABLE_SHARE.

We do not use mutex protection when setting up or tearing down the TABLE_SHARE
object, because the appropriate protection should already be in place. For
example, for the insert() in sql_base.cc:get_table_share() we do not use mutex
protection since we are in the process of constructing the TABLE_SHARE.

-- insert() in histogram.cc:update_share_histograms(): protected by LOCK_open.

-- acquire() in table.cc:open_table_from_share(): protected by LOCK_open.

-- release() in sql_base.cc:intern_close_table(): protected by LOCK_open.

With respect to performance, for the insert() and release() operations we are
able to re-use existing lock/unlock pairs, but for the acquire() operation we
take out an additional lock. Since this lock is global and central to a lot of
server operations, we would have to benchmark to see if it is better to
introduce a new lock.

Table_histograms memory management
----------------------------------

Table_histograms objects are allocated on a MEM_ROOT that is a member of the
object itself. We create a Table_histograms object through the factory method
Table_histograms::create() which allocates a new Table_histograms object and
returns a pointer to it. It is the responsibility of the caller to ensure that
the destructor of this object is invoked which will free its memory.

When we want to insert() a new Table_histograms object in the
Table_histograms_collection on a TABLE_SHARE we first call
Table_histograms::create() to create an empty Table_histograms object. Next we
fill it with histograms by retrieving histograms from the data dictionary and
calling Table_histograms::insert_histogram() which copies the histogram to the
MEM_ROOT on the Table_histograms object. Finally we insert() the object into the
Table_histograms_collection which transfers ownership/lifetime responsibility
from the calling code to the collection.
*/

/**
  The Table_histograms class represents a snapshot of the collection of
  histograms associated with a table. Table_histograms contains a reference
  counter to keep track of the number of TABLE objects that point to it.

  Table_histogram objects are created using the static factory method create().
  The object itself and everything it points to (including its MEM_ROOT) is
  allocated on its own MEM_ROOT. Table_histogram objects are destroyed/freed by
  calling destroy() that clears the MEM_ROOT.
*/
class Table_histograms {
 public:
  /**
    Factory method to create Table_histogram objects. Allocates a
    Table_histogram object on its own MEM_ROOT and returns a pointer.
    Should be matched by a call to destroy().

    @param psi_key performance schema instrumentation memory key to track all
    memory used by the object.
    @return A pointer to a Table_histograms object if construction was
    successful, returns nullptr otherwise.
  */
  static Table_histograms *create(PSI_memory_key psi_key) noexcept;

 private:
  Table_histograms() = default;

 public:
  Table_histograms(Table_histograms &&) = delete;
  Table_histograms &operator=(Table_histograms &&) = delete;
  Table_histograms(const Table_histograms &) = delete;
  Table_histograms &operator=(const Table_histograms &) = delete;
  ~Table_histograms() = delete;

  /**
    Destroys the object and frees memory.
  */
  void destroy();

  /**
    Perform a lookup in the local collection of histograms for a histogram on
    a given field.

    @param field_index Index of the field to find a histogram for.

    @return Pointer to a histogram or nullptr if no histogram was found.
   */
  const histograms::Histogram *find_histogram(unsigned int field_index) const;

  /**
    Copies the given histogram onto the local MEM_ROOT and inserts the copy
    into the local collection of histograms.

    @param field_index Index of the field to insert a histogram for.
    @param histogram Pointer to the histogram to be copied and inserted.
    @return False if success, true on error.
   */
  bool insert_histogram(unsigned int field_index,
                        const histograms::Histogram *histogram);

  friend class Table_histograms_collection;

 private:
  MEM_ROOT m_mem_root;
  mem_root_unordered_map<unsigned int, const histograms::Histogram *>
      *m_histograms{nullptr};

  // The following members are only intended to be manipulated by the
  // Table_histograms_collection that the Table_histograms object is inserted
  // into.

  /// The number of TABLE objects referencing this object.
  int reference_count() const { return m_reference_counter; }
  void increment_reference_counter() { ++m_reference_counter; }
  void decrement_reference_counter() {
    assert(m_reference_counter > 0);
    --m_reference_counter;
  }

  /// The index of this object in the Table_histograms_collection.
  int get_index() const { return m_index; }
  void set_index(int index) { m_index = index; }

  int m_reference_counter{0};
  size_t m_index{0};
};

constexpr size_t kMaxNumberOfTableHistogramsInCollection = 16;
/**
  The Table_histograms_collection manages a collection of reference-counted
  snapshots of histogram statistics (Table_histograms objects) for a table. It
  is intended to live on the TABLE_SHARE and provide TABLE objects with
  reference-counted access to Table_histogram objects through the acquire() and
  release() methods. The motivation for this class is to decouple the lifetime
  of histogram statistics from the lifetime of the TABLE_SHARE, so that we
  avoid having to invalidate the TABLE_SHARE when updating/dropping histograms.

  Multiple threads can be opening/closing tables concurrently. Member functions
  on the Table_histograms_collection should be protected by holding LOCK_open.

  When the TABLE_SHARE is initialized and whenever the histograms associated
  with a table are updated, we create a new Table_histograms object, insert it
  into the collection, and mark it current.
*/
class Table_histograms_collection {
 public:
  Table_histograms_collection();
  Table_histograms_collection(Table_histograms_collection &&) = delete;
  Table_histograms_collection &operator=(Table_histograms_collection &&) =
      delete;
  Table_histograms_collection(const Table_histograms_collection &) = delete;
  Table_histograms_collection &operator=(const Table_histograms_collection &) =
      delete;
  ~Table_histograms_collection();

  /**
    Acquire a pointer to the most recently inserted Table_histograms object.
    Increments the reference counter on the returned Table_histograms object.

    @return Pointer to the current Table_histograms object or nullptr if none
    exists.
  */
  const Table_histograms *acquire();

  /**
    Release a previously acquired Table_histograms object, decreasing its
    reference count. If the reference count of a non-current Table_histograms
    object reaches zero we delete it. This frees up memory and makes room for a
    new Table_histograms object in the collection.

    @param histograms Pointer to a Table_histograms object to be released.
  */
  void release(const Table_histograms *histograms);

  /**
    Attempt to insert the supplied Table_histograms object into the collection.
    The insertion will fail if the collection is full. If the insertion succeeds
    we mark the object as current and take ownership. The previous current
    object is deleted if it has a reference count of zero

    @param histograms Pointer to the Table_histograms object to be inserted.

    @return False if the insertion took place, true otherwise.
  */
  bool insert(Table_histograms *histograms);

  /**
    Count the total number of TABLE objects referencing Table_histograms objects
    in the collection. Primarily used for testing.

    @return The sum of Table_histogram reference counters. Zero if the
    collection is empty.
  */
  int total_reference_count() const;

  /**
    Counts the number of Table_histograms objects in the collection. Primarily
    used for testing.

    @return The count of non-null pointers to Table_histograms objects in the
    collection.
   */
  size_t size() const;

 private:
  /**
    Frees a Table_histograms object from the collection and sets its pointer to
    nullptr.

    @param idx Index of the Table_histograms object to free.
   */
  void free_table_histograms(size_t idx) {
    m_table_histograms[idx]->destroy();
    m_table_histograms[idx] = nullptr;
  }

  std::array<Table_histograms *, kMaxNumberOfTableHistogramsInCollection>
      m_table_histograms;
  size_t m_current_index{0};
};

#endif
