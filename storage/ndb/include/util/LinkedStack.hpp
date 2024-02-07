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

#ifndef NDB_LINKEDSTACK_HPP
#define NDB_LINKEDSTACK_HPP

#include <ndb_global.h>

/**
 * LinkedStack
 *
 * A templated class for a stack of elements E.
 * Storage for the elements is allocated using the passed
 * allocator.
 * Push copies the supplied element into the stack
 * Pop overwrites the supplied element with the contents
 * of the top of the stack
 * Internally, the class allocates 'blocks' of elements of
 * the size passed, linking them together as necessary.
 * As the stack shrinks, the blocks are not released.
 * Blocks are returned to the allocator when release() is
 * called.
 * reset() empties the stack without releasing the allocated
 * storage.
 */
template <typename E, typename A>
class LinkedStack {
 private:
  struct BlockHeader {
    BlockHeader *next;
    BlockHeader *prev;
    E *elements;
  };

  BlockHeader *allocBlock() {
    /* Alloc blockheader and element array */
    BlockHeader *h =
        (BlockHeader *)A::alloc(allocatorContext, sizeof(BlockHeader));
    E *e = (E *)A::mem_calloc(allocatorContext, blockElements, sizeof(E));

    h->next = nullptr;
    h->prev = nullptr;
    h->elements = e;

    return h;
  }

  bool valid() {
    if (stackTop) {
      assert(firstBlock != nullptr);
      assert(currBlock != nullptr);
      /* Check that currBlock is positioned on correct
       * block, except for block boundary case
       */
      Uint32 blockNum = (stackTop - 1) / blockElements;
      BlockHeader *bh = firstBlock;
      while (blockNum--) {
        bh = bh->next;
      }
      assert(bh == currBlock);
    } else {
      assert(currBlock == nullptr);
    }
    return true;
  }

  /* Note that stackTop is 'next insertion point' whereas
   * currBlock points to block last inserted to.
   * On block boundaries, they refer to different blocks
   */
  void *allocatorContext;
  BlockHeader *firstBlock;
  BlockHeader *currBlock;
  Uint32 stackTop;
  Uint32 blockElements;

 public:
  LinkedStack(Uint32 _blockElements, void *_allocatorContext = nullptr)
      : allocatorContext(_allocatorContext),
        firstBlock(nullptr),
        currBlock(nullptr),
        stackTop(0),
        blockElements(_blockElements) {
    assert(blockElements > 0);
    assert(valid());
  }

  ~LinkedStack() {
    assert(valid());
    /* Release block storage if present */
    release();
  }

  bool push(E &elem) {
    assert(valid());
    Uint32 blockOffset = stackTop % blockElements;

    if (blockOffset == 0) {
      /* On block boundary */
      if (stackTop) {
        /* Some elements exist already */
        if (!currBlock->next) {
          /* End of block list, alloc another */
          BlockHeader *newBlock = allocBlock();
          if (!newBlock) return false;

          currBlock->next = newBlock;
          currBlock->next->prev = currBlock;
        }
        currBlock = currBlock->next;
      } else {
        /* First element */
        if (!firstBlock) {
          BlockHeader *newBlock = allocBlock();
          if (!newBlock) return false;

          firstBlock = currBlock = newBlock;
        }
        currBlock = firstBlock;
      }
    }

    currBlock->elements[blockOffset] = elem;
    stackTop++;

    assert(valid());
    return true;
  }

  bool pop(E &elem) {
    assert(valid());
    if (stackTop) {
      stackTop--;
      Uint32 blockOffset = stackTop % blockElements;
      elem = currBlock->elements[blockOffset];

      if (blockOffset == 0) {
        /* Block boundary, shift back to prev block. */
        if (stackTop) assert(currBlock->prev);

        currBlock = currBlock->prev;
      }

      assert(valid());
      return true;
    }
    return false;
  }

  Uint32 size() const { return stackTop; }

  void reset() {
    assert(valid());
    stackTop = 0;
    currBlock = nullptr;
    assert(valid());
  }

  void release() {
    assert(valid());
    BlockHeader *h = firstBlock;
    while (h) {
      BlockHeader *n = h->next;
      A::mem_free(allocatorContext, h->elements);
      A::mem_free(allocatorContext, h);
      h = n;
    };
    stackTop = 0;
    firstBlock = currBlock = nullptr;
    assert(valid());
  }
};

#endif
