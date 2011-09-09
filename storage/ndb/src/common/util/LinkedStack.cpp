/* Copyright (C) 2009 Sun Microsystems Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include <LinkedStack.hpp>

#ifdef TEST_LINKEDSTACK
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

  static void* calloc(void* ignore, size_t nelem, size_t bytes)
  {
    void* p = ::calloc(nelem, bytes);
    if (DEBUG_ALLOC)
    {
      printf("--Allocating %u elements of %u bytes (%u bytes?) at %p\n",
             (Uint32) nelem, (Uint32) bytes, (Uint32) (nelem * bytes), p);
    }
    return p;
  }

  static void free(void* ignore, void* mem)
  {
    if (DEBUG_ALLOC)
    {
      printf("--Freeing bytes at %p\n",mem);
    }
    ::free(mem);
  }
};

TAPTEST(LinkedStack)
{
  Uint32 popped;
  Uint32 blockSize = 1;

  for (Uint32 b=0; b < 10; b++)
  {
    LinkedStack<Uint32, TestHeapAllocator> testStack(blockSize);

    for (Uint32 p=0; p<4; p++)
    {
      /* Pass 0 == alloc, Pass 1 == re-use, Pass 3 = Reset, Pass 4 = Release */
      printf("LinkedBlockStack size %u, pass %u\n", blockSize, p);
      Uint32 stackSize = 2033 * (p+1);

      OK(testStack.size() == 0);
      printf("  Pushing %u elements\n", stackSize);
      for (Uint32 i=0; i < stackSize; i++)
      {
        /* Push items onto the stack */
        OK(testStack.push(i) == true);
        OK(testStack.size() == i+1);
        OK(testStack.pop(popped) == true);
        OK(popped == i);
        OK(testStack.size() == i);
        OK(testStack.push(i) == true);
      };

      switch(p)
      {
      case 0:
      case 1:
      {
        printf("  Popping %u elements\n", stackSize);
        for (Uint32 i=0; i < stackSize; i++)
        {
          /* Pop items off the stack */
          OK(testStack.size() == stackSize - i);
          OK(testStack.pop(popped) == true);
          OK(popped == stackSize - (i+1));
        }
        break;
      }
      case 2:
      {
        printf("  Releasing stack\n");
        testStack.release();
        break;
      }
      case 3:
      {
        printf("  Resetting stack\n");
        testStack.reset();
        break;
      }
      }

      OK(testStack.size() == 0);
      OK(testStack.pop(popped) == false);
    }
    printf("  Destructing stack\n");
    blockSize = (blockSize * 2)+1;
  }

  return 1; // OK
}

#endif
