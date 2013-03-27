/* Copyright (c) 2009, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */


#ifndef NDB_HASHMAP_HPP
#define NDB_HASHMAP_HPP

#include <ndb_global.h>
#include <my_sys.h>
#include <hash.h>


/*
  Default implementation for extracting key_ptr and
  key_length from type K. Used when the entire type
  K should be used as key.

  NOTE! Optimized inside HashMap so that it's never
  actually called
*/

inline const void* HashMap__get_key(const void* key_ptr, size_t* key_length)
{
  key_length = key_length;
  return key_ptr;
}


/*
  Hash container for storing key value pairs
*/

template<typename K, typename T,
         const void* G(const void*, size_t*) = HashMap__get_key >
class HashMap {
  class Entry {
  public:
    K m_key;
    T m_value;
    Entry(const K& k, const T& v) : m_key(k), m_value(v) {};
  };

  HASH m_hash;

  static void free_element(void * ptr) {
    Entry* entry = (Entry*)ptr;
    delete entry;
  }

  /*
    Callback function which is installed into 'my_hash'
    and thus called once for each key in the hash that need to
    be compared. Should return a pointer to where the key
    start and the key's length.
  */
  static uchar* _get_key(const uchar* ptr,
                         size_t* key_length, my_bool first) {
    const Entry * entry = reinterpret_cast<const Entry*>(ptr);
    const void* key_ptr = G(&entry->m_key, key_length);
    return (uchar*)key_ptr;
  }

  const void* get_key_ptr(const K* key, size_t *key_length) const {
    if (G == HashMap__get_key)
      return key;
    return _get_key((const uchar*)key, key_length, false);
  }

public:
  HashMap(ulong initial_size = 1024, uint grow_size = 256) {

    assert(my_init_done);

    if (my_hash_init2(&m_hash,
                      grow_size,
                      &my_charset_bin, // charset
                      initial_size,    // default_array_elements
                      0,               // key_offset
                      sizeof(K),       // key_length
                      G == HashMap__get_key ? NULL : _get_key, // get_key,
                      free_element,    // free_element
                      HASH_UNIQUE      // flags
                      ))
      abort();
  }

  ~HashMap() {
    my_hash_free(&m_hash);
  }

  bool insert(const K& k, const T& v, bool replace = false) {
    Entry* entry = new Entry(k, v);
    if (my_hash_insert(&m_hash, (const uchar*)entry) != 0) {
      // An entry already existed

      delete entry;

      T* p;
      if (replace && search(k, &p)) {
        *p = v;
        return true;
      }
      return false;
    }
    return true;
  }

  bool search(const K& k, T& v) const {
    T* p;
    if (!search(k, &p))
      return false;
    v = *p;
    return true;
  }

  bool search(const K& k, T** v) const {
    size_t key_length = sizeof(K);
    const void *key = get_key_ptr(&k, &key_length);
    Entry* entry= (Entry*)my_hash_search(&m_hash,
                                         (const uchar*)key, key_length);
    if (entry == NULL)
      return false;

    *v = &(entry->m_value);
    return true;
  }

  bool remove(const K& k) {
    size_t key_length = sizeof(K);
    const void *key = get_key_ptr(&k, &key_length);
    Entry* entry= (Entry*)my_hash_search(&m_hash,
                                         (const uchar*)key, key_length);
    if (entry == NULL)
      return false;

    if (my_hash_delete(&m_hash, (uchar*)entry))
      return false;
    return true;
  }

  bool remove(size_t i) {
    Entry* entry = (Entry*)my_hash_element(&m_hash, (ulong)i);
    if (entry == NULL)
      return false;

    if (my_hash_delete(&m_hash, (uchar*)entry))
      return false;
    return true;
  }

  size_t entries(void) const {
    return m_hash.records;
  }

  T* value(size_t i) const {
    Entry* entry = (Entry*)my_hash_element((HASH*)&m_hash, (ulong)i);
    if (entry == NULL)
      return NULL;
    return &(entry->m_value);
  }

};

#endif
