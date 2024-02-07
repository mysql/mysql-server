/* Copyright (c) 2009, 2024, Oracle and/or its affiliates.

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

#ifdef TEST_LINKEDSTACK
#include <LinkedStack.hpp>
#include <NdbTap.hpp>

struct TestHeapAllocator {
  static const int DEBUG_ALLOC = 0;
  static void *alloc(void *, size_t bytes) {
    void *p = ::malloc(bytes);
    if (DEBUG_ALLOC) {
      printf("--Allocating %u bytes at %p\n", (Uint32)bytes, p);
    }
    return p;
  }

  static void *mem_calloc(void *, size_t nelem, size_t bytes) {
    void *p = ::calloc(nelem, bytes);
    if (DEBUG_ALLOC) {
      printf("--Allocating %u elements of %u bytes (%u bytes?) at %p\n",
             (Uint32)nelem, (Uint32)bytes, (Uint32)(nelem * bytes), p);
    }
    return p;
  }

  static void mem_free(void *, void *mem) {
    if (DEBUG_ALLOC) {
      printf("--Freeing bytes at %p\n", mem);
    }
    ::free(mem);
  }
};

TAPTEST(LinkedStack) {
  Uint32 popped;
  Uint32 blockSize = 1;

  for (Uint32 b = 0; b < 10; b++) {
    LinkedStack<Uint32, TestHeapAllocator> testStack(blockSize);

    for (Uint32 p = 0; p < 4; p++) {
      /* Pass 0 == alloc, Pass 1 == re-use, Pass 3 = Reset, Pass 4 = Release */
      printf("LinkedBlockStack size %u, pass %u\n", blockSize, p);
      Uint32 stackSize = 2033 * (p + 1);

      OK(testStack.size() == 0);
      printf("  Pushing %u elements\n", stackSize);
      for (Uint32 i = 0; i < stackSize; i++) {
        /* Push items onto the stack */
        OK(testStack.push(i) == true);
        OK(testStack.size() == i + 1);
        OK(testStack.pop(popped) == true);
        OK(popped == i);
        OK(testStack.size() == i);
        OK(testStack.push(i) == true);
      };

      switch (p) {
        case 0:
        case 1: {
          printf("  Popping %u elements\n", stackSize);
          for (Uint32 i = 0; i < stackSize; i++) {
            /* Pop items off the stack */
            OK(testStack.size() == stackSize - i);
            OK(testStack.pop(popped) == true);
            OK(popped == stackSize - (i + 1));
          }
          break;
        }
        case 2: {
          printf("  Releasing stack\n");
          testStack.release();
          break;
        }
        case 3: {
          printf("  Resetting stack\n");
          testStack.reset();
          break;
        }
      }

      OK(testStack.size() == 0);
      OK(testStack.pop(popped) == false);
    }
    printf("  Destructing stack\n");
    blockSize = (blockSize * 2) + 1;
  }

  return 1;  // OK
}

#endif
