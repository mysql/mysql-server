/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#include "mysql/components/library_mysys/my_memory.h"
#include <assert.h>
#include <stdlib.h>
#include "mysql/components/services/psi_memory.h"

#ifdef HAVE_VALGRIND
#include <valgrind/valgrind.h>

#define MEM_MALLOCLIKE_BLOCK(p1, p2, p3, p4) \
  VALGRIND_MALLOCLIKE_BLOCK(p1, p2, p3, p4)
#define MEM_FREELIKE_BLOCK(p1, p2) VALGRIND_FREELIKE_BLOCK(p1, p2)
#else /* HAVE_VALGRIND */
#define MEM_MALLOCLIKE_BLOCK(p1, p2, p3, p4) \
  do {                                       \
  } while (0)
#define MEM_FREELIKE_BLOCK(p1, p2) \
  do {                             \
  } while (0)
#endif /* HAVE_VALGRIND */

#define HEADER_SIZE 32
#define MAGIC 1234
#define USER_TO_HEADER(P) ((my_memory_header *)(((char *)P) - HEADER_SIZE))
#define HEADER_TO_USER(P) (((char *)P) + HEADER_SIZE)

struct my_memory_header {
  PSI_memory_key m_key;
  unsigned int m_magic;
  size_t m_size;
  PSI_thread *m_owner;
};
typedef struct my_memory_header my_memory_header;

extern "C" void *my_malloc(PSI_memory_key key, size_t size, int flags) {
  my_memory_header *mh;
  size_t raw_size;
  static_assert(sizeof(my_memory_header) <= HEADER_SIZE,
                "We must reserve enough memory to hold the header.");

  raw_size = HEADER_SIZE + size;
  if (flags & MY_ZEROFILL)
    mh = (my_memory_header *)calloc(raw_size, 1);
  else
    mh = (my_memory_header *)malloc(raw_size);

  if (mh != nullptr) {
    void *user_ptr;
    mh->m_magic = MAGIC;
    mh->m_size = size;
    mh->m_key = PSI_MEMORY_CALL(memory_alloc)(key, size, &mh->m_owner);
    user_ptr = HEADER_TO_USER(mh);
    MEM_MALLOCLIKE_BLOCK(user_ptr, size, 0, (flags & MY_ZEROFILL));
    return user_ptr;
  }
  return nullptr;
}

extern "C" void my_free(void *ptr) {
  my_memory_header *mh;

  if (ptr == nullptr) return;

  mh = USER_TO_HEADER(ptr);
  assert(mh->m_magic == MAGIC);
  PSI_MEMORY_CALL(memory_free)(mh->m_key, mh->m_size, mh->m_owner);
  /* Catch double free */
  mh->m_magic = 0xDEAD;
  MEM_FREELIKE_BLOCK(ptr, 0);
  free(mh);
}
