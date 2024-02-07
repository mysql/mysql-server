/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NdbLinHash_H
#define NdbLinHash_H

#include <ndb_types.h>

#define SEGMENTSIZE 64
#define SEGMENTLOGSIZE 6
#define DIRECTORYSIZE 64
#define DIRINDEX(adress) ((adress) >> SEGMENTLOGSIZE)
#define SEGINDEX(adress) ((adress) & (SEGMENTSIZE - 1))

#if !defined(MAXLOADFCTR)
#define MAXLOADFCTR 2
#endif
#if !defined(MINLOADFCTR)
#define MINLOADFCTR (MAXLOADFCTR / 2)
#endif

template <class C>
class NdbElement_t {
 public:
  NdbElement_t();
  ~NdbElement_t();

  Uint32 len;
  Uint32 hash;
  Uint32 localkey1;
  Uint32 *str;
  NdbElement_t<C> *next;
  C *theData;

 private:
  NdbElement_t &operator=(const NdbElement_t<C> &aElement_t) = delete;
};

template <class C>
class NdbLinHash {
 public:
  NdbLinHash() = default;
  ~NdbLinHash() = default;
  void createHashTable(void);
  void releaseHashTable(void);

  int insertKey(const char *str, Uint32 len, Uint32 lkey1, C *data);
  C *deleteKey(const char *str, Uint32 len);

  C *getData(const char *, Uint32);
  Uint32 *getKey(const char *, Uint32);

  void shrinkTable(void);
  void expandHashTable(void);

  Uint32 Hash(const char *str, Uint32 len);
  Uint32 Hash(Uint32 h);

  NdbElement_t<C> *getNext(NdbElement_t<C> *curr);

 private:
  void getBucket(Uint32 hash, int *dirindex, int *segindex);

  struct Segment_t {
    NdbElement_t<C> *elements[SEGMENTSIZE];
  };

  Uint32 p;    /*bucket to be split*/
  Uint32 max;  /*max is the upper bound*/
  Int32 slack; /*number of insertions before splits*/
  Segment_t *directory[DIRECTORYSIZE];

  NdbLinHash<C> &operator=(const NdbLinHash<C> &aLinHash) = delete;
};

template <class C>
inline Uint32 NdbLinHash<C>::Hash(const char *str, Uint32 len) {
  Uint32 h = 0;
  while (len >= 4) {
    h = (h << 5) + h + str[0];
    h = (h << 5) + h + str[1];
    h = (h << 5) + h + str[2];
    h = (h << 5) + h + str[3];
    len -= 4;
    str += 4;
  }

  while (len > 0) {
    h = (h << 5) + h + *str++;
    len--;
  }
  return h;
}

template <class C>
inline Uint32 NdbLinHash<C>::Hash(Uint32 h) {
  return h;
}

template <class C>
inline NdbElement_t<C>::NdbElement_t()
    : len(0),
      hash(0),
      localkey1(0),
      str(nullptr),
      next(nullptr),
      theData(nullptr) {}

template <class C>
inline NdbElement_t<C>::~NdbElement_t() {
  delete[] str;
}

/* Initialize the hashtable HASH_T  */
template <class C>
inline void NdbLinHash<C>::createHashTable() {
  p = 0;
  max = SEGMENTSIZE - 1;
  slack = SEGMENTSIZE * MAXLOADFCTR;
  directory[0] = new Segment_t();
  int i;

  /* The first segment cleared before used */
  for (i = 0; i < SEGMENTSIZE; i++) directory[0]->elements[i] = nullptr;

  /* clear the rest of the directory */
  for (i = 1; i < DIRECTORYSIZE; i++) directory[i] = nullptr;
}

template <class C>
inline void NdbLinHash<C>::getBucket(Uint32 hash, int *dir, int *seg) {
  Uint32 adress = hash & max;
  if (adress < p) adress = hash & (2 * max + 1);

  *dir = DIRINDEX(adress);
  *seg = SEGINDEX(adress);
}

template <class C>
inline Int32 NdbLinHash<C>::insertKey(const char *str, Uint32 len, Uint32 lkey1,
                                      C *data) {
  const Uint32 hash = Hash(str, len);
  int dir, seg;
  getBucket(hash, &dir, &seg);

  NdbElement_t<C> **chainp = &directory[dir]->elements[seg];

  /**
   * Check if the string already are in the hash table
   * chain=chainp will copy the contents of HASH_T into chain
   */
  NdbElement_t<C> *oldChain = nullptr;
  NdbElement_t<C> *chain;
  for (chain = *chainp; chain != nullptr; chain = chain->next) {
    if (chain->len == len && !memcmp(chain->str, str, len))
      return -1; /* Element already exists */
    else
      oldChain = chain;
  }

  /* New entry */
  chain = new NdbElement_t<C>();
  chain->len = len;
  chain->hash = hash;
  chain->localkey1 = lkey1;
  chain->next = nullptr;
  chain->theData = data;
  len++;  // Null terminated
  chain->str = new Uint32[((len + 3) >> 2)];
  memcpy(&chain->str[0], str, len);
  if (oldChain != nullptr)
    oldChain->next = chain;
  else
    *chainp = chain;

#if 0
  if(--(slack) < 0)
    expandHashTable();
#endif

  return chain->localkey1;
}

template <class C>
inline Uint32 *NdbLinHash<C>::getKey(const char *str, Uint32 len) {
  const Uint32 tHash = Hash(str, len);
  int dir, seg;
  getBucket(tHash, &dir, &seg);

  NdbElement_t<C> **keyp = &directory[dir]->elements[seg];

  /*Check if the string are in the hash table*/
  for (NdbElement_t<C> *key = *keyp; key != 0; key = key->next) {
    if (key->len == len && !memcmp(key->str, str, len)) {
      return &key->localkey1;
    }
  }
  return nullptr; /* The key was not found */
}

template <class C>
inline C *NdbLinHash<C>::getData(const char *str, Uint32 len) {
  const Uint32 tHash = Hash(str, len);
  int dir, seg;
  getBucket(tHash, &dir, &seg);

  NdbElement_t<C> **keyp = &directory[dir]->elements[seg];

  /*Check if the string are in the hash table*/
  for (NdbElement_t<C> *key = *keyp; key != nullptr; key = key->next) {
    if (key->len == len && !memcmp(key->str, str, len)) {
      return key->theData;
    }
  }
  return nullptr; /* The key was not found */
}

template <class C>
inline C *NdbLinHash<C>::deleteKey(const char *str, Uint32 len) {
  const Uint32 hash = Hash(str, len);
  int dir, seg;
  getBucket(hash, &dir, &seg);

  NdbElement_t<C> *oldChain = nullptr;
  NdbElement_t<C> **chainp = &directory[dir]->elements[seg];
  for (NdbElement_t<C> *chain = *chainp; chain != nullptr;
       chain = chain->next) {
    if (chain->len == len && !memcmp(chain->str, str, len)) {
      C *data = chain->theData;
      if (oldChain == nullptr) {
        *chainp = chain->next;
      } else {
        oldChain->next = chain->next;
      }
      delete chain;
      return data;
    } else {
      oldChain = chain;
    }
  }
  return nullptr; /* Element doesn't exist */
}

template <class C>
inline void NdbLinHash<C>::releaseHashTable(void) {
  NdbElement_t<C> *tNextElement;
  NdbElement_t<C> *tElement;

  // Traverse the whole directory structure
  for (int countd = 0; countd < DIRECTORYSIZE; countd++) {
    if (directory[countd] != nullptr) {
      // Traverse whole hashtable
      for (int counts = 0; counts < SEGMENTSIZE; counts++)
        if (directory[countd]->elements[counts] != nullptr) {
          tElement = directory[countd]->elements[counts];
          // Delete all elements even those who is linked
          do {
            tNextElement = tElement->next;
            delete tElement;
            tElement = tNextElement;
          } while (tNextElement != nullptr);
        }
      delete directory[countd];
    }
  }
}

template <class C>
inline void NdbLinHash<C>::shrinkTable(void) {
  Segment_t *lastseg;
  NdbElement_t<C> **chainp;
  Uint32 oldlast = p + max;

  if (oldlast == 0) return;

  // Adjust the state variables.
  if (p == 0) {
    max >>= 1;
    p = max;
  } else
    --(p);

  // Update slack after shrink.

  slack -= MAXLOADFCTR;

  // Insert the chain oldlast at the end of chain p.

  chainp = &directory[DIRINDEX(p)]->elements[SEGINDEX(p)];
  while (*chainp != 0) {
    chainp = &((*chainp)->next);
    lastseg = directory[DIRINDEX(oldlast)];
    *chainp = lastseg->elements[SEGINDEX(oldlast)];

    // If necessary free last segment.
    if (SEGINDEX(oldlast) == 0) delete lastseg;
  }
}

template <class C>
inline void NdbLinHash<C>::expandHashTable(void) {
  NdbElement_t<C> **oldbucketp, *chain, *headofold, *headofnew, *next;
  Uint32 maxp = max + 1;
  Uint32 newadress = maxp + p;

  // Still room in the address space?
  if (newadress >= DIRECTORYSIZE * SEGMENTSIZE) {
    return;
  }

  // If necessary, create a new segment.
  if (SEGINDEX(newadress) == 0)
    directory[DIRINDEX(newadress)] = new Segment_t();

  // Locate the old (to be split) bucket.
  oldbucketp = &directory[DIRINDEX(p)]->elements[SEGINDEX(p)];

  // Adjust the state variables.
  p++;
  if (p > max) {
    max = 2 * max + 1;
    p = 0;
  }

  // Update slack after expandation.
  slack += MAXLOADFCTR;

  // Relocate records to the new bucket.
  headofold = 0;
  headofnew = 0;

  for (chain = *oldbucketp; chain != 0; chain = next) {
    next = chain->next;
    if (chain->hash & maxp) {
      chain->next = headofnew;
      headofnew = chain;
    } else {
      chain->next = headofold;
      headofold = chain;
    }
  }
  *oldbucketp = headofold;
  directory[DIRINDEX(newadress)]->elements[SEGINDEX(newadress)] = headofnew;
}

template <class C>
inline NdbElement_t<C> *NdbLinHash<C>::getNext(NdbElement_t<C> *curr) {
  if (curr != nullptr && curr->next != nullptr) return curr->next;

  int dir = 0, seg = 0;
  int counts;
  if (curr != nullptr) {
    getBucket(curr->hash, &dir, &seg);
    counts = seg + 1;
  } else {
    counts = 0;
  }

  for (int countd = dir; countd < DIRECTORYSIZE; countd++) {
    if (directory[countd] != nullptr) {
      for (; counts < SEGMENTSIZE; counts++) {
        if (directory[countd]->elements[counts] != nullptr) {
          return directory[countd]->elements[counts];
        }
      }
    }
    counts = 0;
  }

  return nullptr;
}

#endif
