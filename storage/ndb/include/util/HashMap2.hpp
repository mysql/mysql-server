/*
   Copyright (c) 2009, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_HASHMAP2_HPP
#define NDB_HASHMAP2_HPP

#include <ndb_global.h>


/* Basic HashTable implementation
 * The HashTable stores elements of type KV.
 * The storage for the elements is managed outside the
 * HashTable implementation.
 * The HashTable uses element chaining in each bucket to
 * deal with collisions.
 * The HashTable can optionally enforce uniqueness
 * The HashTable can be resized when it is empty.
 */

/**
 * KVOPStaticAdapter template
 *
 * Used with HashMap2
 * Creates a class with static methods calling members of
 * an object passed to them, for the case where the HashMap
 * contains types ptrs with the relevant methods defined
 * (objects).
 *
 * Implements the KVOP Api, and requires the following API
 * from the KV type :
 *
 * Useful for supplying OP type.
 * Required KV Api:
 *   Uint32 hashValue() const;
 *   bool equal(const KV* other) const;
 *   void setNext(KV* next);
 *   KV* getNext() const;
 */
template<typename KV>
class KVOPStaticAdapter
{
public:
  static Uint32 hashValue(const KV* obj)
  {
    return obj->hashValue();
  }

  static bool equal(const KV* objA, const KV* objB)
  {
    return objA->equal(objB);
  }

  static void setNext(KV* from, KV* to)
  {
    return from->setNext(to);
  }

  static KV* getNext(const KV* from)
  {
    return from->getNext();
  }
};

// TODO :
//    Pass allocator context rather than allocator
//    Support re-alloc?
//    Use calloc?

/**
 * StandardAllocator - used in HashMap2 when no allocator supplied
 * Uses standard malloc/free.
 */
struct StandardAllocator
{
  static void* alloc(void*, size_t bytes)
  {
    return ::malloc(bytes);
  }

  static void* mem_calloc(void*, size_t nelem, size_t bytes)
  {
    return ::calloc(nelem, bytes);
  }

  static void mem_free(void*, void* mem)
  {
    ::free(mem);
  }
};

/**
 * Template parameters
 *
 * Classes with static methods are used to avoid the necessity
 * of using OO wrapper objects for C data.
 * Objects can be used by defining static methods which call
 * normal methods.
 * A default StaticWrapper class exists to 'automate' this if
 * necessary.
 *
 * class KV - Key Value pair.
 *   The HashTable stores pointers to these.  No interface is
 *   assumed - they are manipulated via KVOP below, so can be
 *   chunks of memory or C structs etc.
 *
 * bool unique
 *   True if all keys in a hash table instance must be
 *   unique.
 *   False otherwise.
 *
 * class A - Allocator
 *   Used for hash bucket allocation on setSize() call.
 *   NOT used for element allocation, which is the responsibility
 *   of the user.
 *
 *   Must support static methods :
 *   - static void* calloc(void* context, size_t nelem, size_t bytes)
 *   - static void free(void* context, void*)
 *
 * class KVOP - Operations on Key Value pair
 *   KV instances are stored based on the hash returned
 *   by the KVOP::hashValue() method, with identity based on the
 *   KVOP::equal() method.
 *   KV instances must be linkable using KVOP::getNext() and
 *   KVOP::setNext() methods.
 *
 *   KVOP allows the static methods on the KV pair to be separate
 *   from the data itself.  If they are in the same class, use
 *   KVOP=KV.  If the methods are not static, and are on the KV class,
 *   use KVOP=KVOPStaticAdapter<KV>, or equivalent.
 *
 *   KVOP must support the following static methods :
 *   - static bool equal(const class KV* a, const class KV* b);
 *     Return true if two elements are equal.
 *
 *   - static Uint32 hashValue(const class KV*) const;
 *     Return a 32-bit stable hashvalue for the KV.
 *     equal(a,b) implies hashValue(a) == hashValue(b)
 *
 *   - static void setNext(KV* from, KV* to)
 *
 *   - static KV* getNext(const KV* from) const
 *
 *
 * TODO :
 *   - collision count?
 *   - release option?
 */
template<typename KV,
         bool unique = true,
         typename A = StandardAllocator,
         typename KVOP = KVOPStaticAdapter<KV> >
class HashMap2
{
public:
  /**
   * HashMap2 constructor
   * Pass an Allocator pointer if the templated allocator
   * requires some context info.
   * setSize() must be called before the HashMap is used.
   */
  HashMap2(void* _allocatorContext = nullptr)
    : tableSize(0),
      elementCount(0),
      allocatorContext(_allocatorContext),
      table(nullptr)
  {
  }

  ~HashMap2()
  {
    if (table)
      A::mem_free(allocatorContext, table);
  }

  /**
   * setSize
   *
   * Set the number of buckets.
   *  Can only be set when the hash table is empty.
   *  The Allocator is used to allocate/release bucket
   *  storage.
   */
  bool
  setSize(Uint32 hashBuckets)
  {
    if (elementCount)
    {
      /* Can't set size while we have contents */
      return false;
    }

    if (hashBuckets == 0)
    {
      return false;
    }

    if (table)
    {
      A::mem_free(allocatorContext, table);
      table = nullptr;
    }

    /* TODO : Consider using only power-of-2 + bitmask instead of mod */
    tableSize = hashBuckets;

    table = (KV**) A::mem_calloc(allocatorContext, hashBuckets, sizeof(KV*));

    if (!table)
    {
      return false;
    }

    for (Uint32 i=0; i < tableSize; i++)
      table[i] = nullptr;

    return true;
  }

  /**
   * add
   *
   * Add a KV element to the hash table
   * The next value must be null
   * If the hash table requires uniqueness, and the
   * element is not unique, false will be returned
   */
  bool
  add(KV* keyVal)
  {
    assert(table);

    Uint32 hashVal = rehash(KVOP::hashValue(keyVal));
    Uint32 bucketIdx = hashVal % tableSize;

    KV* bucket = table[bucketIdx];

    if (bucket != nullptr)
    {
      if (unique)
      {
        /* Need to check element is not already there, in this
         * chain
         */
        const KV* chainElement = bucket;
        while (chainElement)
        {
          if (KVOP::equal(keyVal, chainElement))
          {
            /* Found duplicate */
            return false;
          }
          chainElement= KVOP::getNext(chainElement);
        }
      }

      /* Can insert at head of list, as either no uniqueness
       * guarantee, or uniqueness checked.
       */
      assert(KVOP::getNext(keyVal) == nullptr);
      KVOP::setNext(keyVal, bucket);
      table[bucketIdx] = keyVal;
    }
    else
    {
      /* First element in bucket */
      assert(KVOP::getNext(keyVal) == nullptr);
      KVOP::setNext(keyVal, nullptr);
      table[bucketIdx] = keyVal;
    }

    elementCount++;
    return true;
  }

  KV*
  remove(KV* key)
  {
    assert(table);

    Uint32 hashVal = rehash(KVOP::hashValue(key));
    Uint32 bucketIdx = hashVal % tableSize;

    KV* bucket = table[bucketIdx];

    if (bucket != nullptr)
    {
      KV* chainElement = bucket;
      KV* prev = nullptr;
      while (chainElement)
      {
        if (KVOP::equal(key, chainElement))
        {
          /* Found, repair bucket chain
           * Get next, might be NULL
           */
          KV* n = KVOP::getNext(chainElement);
          if (prev)
          {
            /* Link prev to next */
            KVOP::setNext(prev, n);
          }
          else
          {
            /* Put next as first in bucket */
            table[bucketIdx] = n;
          }

          KVOP::setNext(chainElement, nullptr);
          elementCount--;

          return chainElement;
        }
        prev = chainElement;
        chainElement = KVOP::getNext(chainElement);
      }
    }

    return nullptr;
  }

  /**
   * get
   *
   * Get a ptr to a KV element in the hash table
   * with the same key as the element passed.
   * Returns NULL if none exists.
   *
   * For non unique hash tables, a ptr to the
   * first element with the given key is returned.
   * Further elements must be found by iteration
   * (and key comparison), until NULL is returned.
   */
  KV*
  get(const KV* key) const
  {
    assert(table);

    Uint32 hashVal = rehash(KVOP::hashValue(key));
    Uint32 bucketIdx = hashVal % tableSize;

    KV* chainElement = table[bucketIdx];

    while(chainElement)
    {
      if (KVOP::equal(key, chainElement))
      {
        break;
      }
      chainElement = KVOP::getNext(chainElement);
    }

    return chainElement;
  }

  /**
   * reset
   *
   * Resets the hash table to have no entries.
   * KV elements added are not released in any
   * way.  This must be handled outside the
   * HashTable implementation, perhaps by
   * iterating and removing the elements?
   * Storage for the hash table itself is
   * preserved
   */
  void
  reset()
  {
    /* Zap the hashtable ptrs, without freeing the 'elements'
     * Keep the storage allocated for the ptrs
     */
    if (elementCount)
    {
      assert(table);
      for (Uint32 i=0; i < tableSize; i++)
      {
        table[i] = nullptr;
      }

      elementCount = 0;
    }
  }

  /**
   * getElementCount
   *
   * Returns the number of elements currently
   * stored in this hash table.
   */
  Uint32
  getElementCount() const
  {
      return elementCount;
  }

  /**
   * getTableSize
   *
   * Returns the number of hashBuckets in this hash table
   */
  Uint32
  getTableSize() const
  {
    return tableSize;
  }

  class Iterator
  {
  public:
    Iterator(HashMap2& hashMap)
      : m_hash(&hashMap),
        curr_element(nullptr),
        curr_bucket(0)
    {}

    /**
      Return the current element and reposition the iterator to the next
      element.
    */
    KV* next()
    {
      while (curr_bucket < m_hash->tableSize)
      {
        if (curr_element == nullptr)
        {
          /* First this bucket */
          curr_element = m_hash->table[ curr_bucket ];
        }
        else
        {
          /* Next this bucket */
          curr_element = KVOP::getNext(curr_element);
        }

        if (curr_element)
        {
          return curr_element;
        }
        curr_bucket++;
      }

      return nullptr;
    }
    void reset()
    {
      curr_element = nullptr;
      curr_bucket = 0;
    }
  private:
    HashMap2* m_hash;
    KV* curr_element;
    Uint32 curr_bucket;
  };

private:
  static Uint32 rehash(Uint32 userHash)
  {
    /* We rehash the supplied hash value in case
     * it's low quality, mixing some higher order
     * bits in with the lower bits
     */
    userHash ^= ((userHash >> 20) ^ (userHash >> 12));
    return userHash ^ (userHash >> 7) ^ (userHash >> 4);
  }

  Uint32 tableSize;
  Uint32 elementCount;
  void* allocatorContext;

  KV** table;

}; // class HashMap2()

#endif
