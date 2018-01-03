/* Copyright (c) 2009, 2017, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


#ifndef NDB_HASHMAP_HPP
#define NDB_HASHMAP_HPP

#include <ndb_global.h>

#include <memory>
#include <unordered_map>

/*
  Default implementation for extracting key_ptr and
  key_length from type K. Used when the entire type
  K should be used as key.

  NOTE! Optimized inside HashMap so that it's never
  actually called
*/

inline const void* HashMap__get_key(const void* key_ptr, size_t* key_length)
{
  (void)key_length;
  return key_ptr;
}


/*
  Hash container for storing key value pairs
*/

template<typename K, typename T,
         const void* G(const void*, size_t*) = HashMap__get_key >
class HashMap {
  static inline std::string get_key_string(const K& key)
  {
    if (G == HashMap__get_key)
      return std::string(pointer_cast<const char *>(&key), sizeof(K));

    size_t key_length = sizeof(K);
    const char* key_ptr = pointer_cast<const char *>(G(&key, &key_length));
    return std::string(key_ptr, key_length);
  }

  struct HashMap__hash {
    size_t operator() (const K& key) const {
       return hasher(get_key_string(key));
    }
    std::hash<std::string> hasher;
  };

  struct HashMap__equal_to {
    bool operator() (const K& key1, const K& key2) const {
      return get_key_string(key1) == get_key_string(key2);
    }
  };

  typedef
    std::unordered_map<K, std::unique_ptr<T>,
                       HashMap__hash, HashMap__equal_to>
    InternalHash;

  InternalHash m_hash;

public:
  HashMap(ulong initial_size = 1024)
    : m_hash(initial_size, HashMap__hash(), HashMap__equal_to())
  {
  }

  bool insert(const K& k, const T& v, bool replace = false) {
    // Note: This can be written simpler with try_emplace once we get to C++17.
    std::unique_ptr<T> v_ptr(new T(v));
    if (replace) {
      auto it = m_hash.find(k);
      if (it != m_hash.end()) {
        it->second = std::move(v_ptr);
        return true;
      }
      // Did not already exist, fall through to below.
    }
    return m_hash.emplace(k, std::move(v_ptr)).second;
  }

  bool search(const K& k, T& v) const {
    const auto it = m_hash.find(k);
    if (it == m_hash.end())
      return false;
    v = *it->second.get();
    return true;
  }

  bool search(const K& k, const T** v) const {
    auto it = m_hash.find(k);
    if (it == m_hash.end())
      return false;

    *v = it->second.get();
    return true;
  }

  bool search(const K& k, T** v) {
    auto it = m_hash.find(k);
    if (it == m_hash.end())
      return false;

    *v = it->second.get();
    return true;
  }

  bool remove(const K& k) {
    return m_hash.erase(k) != 0;
  }

  size_t entries(void) const {
    return m_hash.size();
  }

  // Forwarders to the underlying map.

  typename InternalHash::iterator begin() { return m_hash.begin(); }
  typename InternalHash::iterator end() { return m_hash.end(); }
  typename InternalHash::iterator
  erase(typename InternalHash::const_iterator pos) {
    return m_hash.erase(pos);
  }

};

#endif
