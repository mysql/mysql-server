/* Copyright (c) 2009, 2012, Oracle and/or its affiliates. All rights reserved.

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


#include <HashMap2.hpp>

#ifdef TEST_HASHMAP2
#include <NdbTap.hpp>

struct TestHeapAllocator
{
  static const int DEBUG_ALLOC=0;
  static void* alloc(void* ignore, size_t bytes)
  {
    void* p = ::malloc(bytes);
    if (DEBUG_ALLOC)
    {
      printf("--Allocating %u bytes at %p\n",
             (Uint32) bytes, p);
    }
    return p;
  }

  static void* mem_calloc(void* ignore, size_t nelem, size_t bytes)
  {
    void* p = ::calloc(nelem, bytes);
    if (DEBUG_ALLOC)
    {
      printf("--Allocating %u elements of %u bytes (%u total?) at %p\n",
             (Uint32)nelem, (Uint32) bytes, (Uint32) (nelem*bytes), p);
    }
    return p;
  }

  static void mem_free(void* ignore, void* mem)
  {
    if (DEBUG_ALLOC)
    {
      printf("--Freeing bytes at %p\n",mem);
    }
    ::free(mem);
  }
};

struct IntIntKVPod
{
  int a;
  int b;
  IntIntKVPod* next;
};

struct IntIntKVStaticMethods
{
  static Uint32 hashValue(const IntIntKVPod* obj)
  {
    return obj->a*31;
  }

  static bool equal(const IntIntKVPod* objA, const IntIntKVPod* objB)
  {
    return objA->a == objB->a;
  }

  static void setNext(IntIntKVPod* from, IntIntKVPod* to)
  {
    from->next = to;
  }

  static IntIntKVPod* getNext(const IntIntKVPod* from)
  {
    return from->next;
  }
};

struct IntIntKVObj
{
  int a;
  int b;
  IntIntKVObj* next;

  Uint32 hashValue() const
  {
    return a*31;
  }

  bool equal(const IntIntKVObj* other) const
  {
    return a == other->a;
  }

  void setNext(IntIntKVObj* _next)
  {
    next = _next;
  }

  IntIntKVObj* getNext() const
  {
    return next;
  }
};

TAPTEST(HashMap2)
{
  printf("int -> int (Static, unique) \n");
  for (int j = 1; j < 150; j ++)
  {
    HashMap2<IntIntKVPod, true, TestHeapAllocator, IntIntKVStaticMethods> hash1;

    hash1.setSize(j);

    OK(hash1.getElementCount() == 0);

    /* Create some values in a pool on the stack */
    IntIntKVPod pool[101];

    for (int i=0; i < 100; i++)
    {
      pool[i].a = i;
      pool[i].b = 3 * i;
      pool[i].next = NULL;
    }

    /* Add the pool elements to the hash table */
    for (int i=0; i < 100; i++)
    {
      OK(hash1.add(&pool[i]));
    }

    /* Now attempt to add a duplicate */
    pool[100].a = 0;
    pool[100].b = 999;
    pool[100].next = NULL;

    OK(hash1.getElementCount() == 100);

    OK(! hash1.add(&pool[100]));

    for (int i=1; i < 100; i++)
    {
      OK(hash1.get(&pool[i]) == &pool[i]);
    }

    OK(hash1.get(&pool[0]) == &pool[0]);

    /* Test iterator Api */
    HashMap2<IntIntKVPod, true, TestHeapAllocator, IntIntKVStaticMethods>::Iterator it(hash1);

    IntIntKVPod* k;
    for (int i=0; i < 2; i++)
    {
      int count = 0;
      while((k = it.next()))
      {
        OK( k->b == ((k->a * 3) - i) );
        k->b--;
        count++;
      }
      OK( count == 100 );
      it.reset();
    }

    hash1.reset();
    it.reset();
    OK( it.next() == NULL );
  }

  printf("int -> int (Static, !unique) \n");
  for (int j = 1; j < 150; j ++)
  {
    HashMap2<IntIntKVPod, false, TestHeapAllocator, IntIntKVStaticMethods> hash1;

    hash1.setSize(j);

    OK(hash1.getElementCount() == 0);

    /* Create some values in a pool on the stack */
    IntIntKVPod pool[101];

    for (int i=0; i < 100; i++)
    {
      pool[i].a = i;
      pool[i].b = 3 * i;
      pool[i].next = NULL;
    }

    /* Add the pool elements to the hash table */
    for (int i=0; i < 100; i++)
    {
      OK(hash1.add(&pool[i]));
    }

    /* Now attempt to add a duplicate */
    pool[100].a = 0;
    pool[100].b = 999;
    pool[100].next = NULL;

    OK(hash1.getElementCount() == 100);

    OK(  hash1.add(&pool[100]));

    for (int i=1; i < 100; i++)
    {
      OK(hash1.get(&pool[i]) == &pool[i]);
    }

    OK((hash1.get(&pool[0]) == &pool[0]) ||
       (hash1.get(&pool[0]) == &pool[100]));
  }

  printf("int -> int (!Static, defaults, (std alloc, unique)) \n");
  for (int j = 1; j < 150; j ++)
  {
    HashMap2<IntIntKVObj> hash1;

    hash1.setSize(j);

    OK(hash1.getElementCount() == 0);

    /* Create some values in a pool on the stack */
    IntIntKVObj pool[101];

    for (int i=0; i < 100; i++)
    {
      pool[i].a = i;
      pool[i].b = 3 * i;
      pool[i].next = NULL;
    }

    /* Add the pool elements to the hash table */
    for (int i=0; i < 100; i++)
    {
      OK(hash1.add(&pool[i]));
    }

    /* Now attempt to add a duplicate */
    pool[100].a = 0;
    pool[100].b = 999;
    pool[100].next = NULL;

    OK(hash1.getElementCount() == 100);

    OK(! hash1.add(&pool[100]));

    for (int i=1; i < 100; i++)
    {
      OK(hash1.get(&pool[i]) == &pool[i]);
    }

    OK(hash1.get(&pool[0]) == &pool[0]);
  }

  printf("int -> int (Static, unique, realloc) \n");
  {
    HashMap2<IntIntKVPod, true, TestHeapAllocator, IntIntKVStaticMethods> hash1;
    for (int j = 1; j < 150; j ++)
    {
      hash1.setSize(150 - j);

      OK(hash1.getElementCount() == 0);

      /* Create some values in a pool on the stack */
      IntIntKVPod pool[101];

      for (int i=0; i < 100; i++)
      {
        pool[i].a = i;
        pool[i].b = 3 * i;
        pool[i].next = NULL;
      }

      /* Add the pool elements to the hash table */
      for (int i=0; i < 100; i++)
      {
        OK(hash1.add(&pool[i]));
      }

      /* Now attempt to add a duplicate */
      pool[100].a = 0;
      pool[100].b = 999;
      pool[100].next = NULL;

      OK(hash1.getElementCount() == 100);

      OK(! hash1.add(&pool[100]));

      for (int i=1; i < 100; i++)
      {
        OK(hash1.get(&pool[i]) == &pool[i]);
      }

      OK(hash1.get(&pool[0]) == &pool[0]);

      OK(!hash1.setSize(j+1));

      hash1.reset();
    }
  }

  printf("int -> int (Static, unique, realloc, remove) \n");
  {
    HashMap2<IntIntKVPod, true, TestHeapAllocator, IntIntKVStaticMethods> hash1;
    for (int j = 1; j < 150; j ++)
    {
//      hash1.setSize(150 - j);
      hash1.setSize(j);

      OK(hash1.getElementCount() == 0);

      /* Create some values in a pool on the stack */
      IntIntKVPod pool[101];

      for (int i=0; i < 100; i++)
      {
        pool[i].a = i;
        pool[i].b = 3 * i;
        pool[i].next = NULL;
      }

      /* Add the pool elements to the hash table */
      for (int i=0; i < 100; i++)
      {
        OK(hash1.add(&pool[i]));
      }

      /* Now attempt to add a duplicate */
      pool[100].a = 0;
      pool[100].b = 999;
      pool[100].next = NULL;

      OK(hash1.getElementCount() == 100);

      OK(!hash1.add(&pool[100]));

      for (int i=1; i < 100; i++)
      {
        OK(hash1.get(&pool[i]) == &pool[i]);
      }

      OK((hash1.get(&pool[0]) == &pool[0]) ||
         (hash1.get(&pool[0]) == &pool[100]));

      OK(!hash1.setSize(j+1));

      /* Now replace elements with different ones */
      IntIntKVPod pool2[100];
      for (int i=0; i < 100; i++)
      {
        pool2[i].a = i;
        pool2[i].b = 4 * i;
        pool2[i].next = NULL;
      }

      for (int k=0; k < 4; k++)
      {
        for (int i=0; i< 100; i++)
        {
          if ((i % 4) == k)
          {
            OK(hash1.remove(&pool[i]) == &pool[i]);
          };
        };

        OK(hash1.getElementCount() == 75);

        for (int i=0; i< 100; i++)
        {
          if ((i % 4) == k)
          {
            OK(!hash1.get(&pool[i]));
          }
        };

        for (int i=0; i< 100; i++)
        {
          if ((i % 4) == k)
          {
            OK(hash1.add(&pool2[i]));
          }
        }
        OK(hash1.getElementCount() == 100);
      }

      for (int i=0; i< 100; i++)
      {
        OK(hash1.get(&pool2[i]) == &pool2[i]);
      };

      hash1.reset();
    }
  }

  return 1; // OK
}

#endif
