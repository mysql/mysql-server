/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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
#include "my_aligned_malloc.h"

#include "config.h"

#if defined(HAVE_POSIX_MEMALIGN)
#include <stdlib.h>
#elif defined(HAVE_MEMALIGN)
#include <memory.h>
#elif defined(HAVE_ALIGNED_MALLOC)
#include <malloc.h>
#include <cstdlib>
#else
#error "Missing implementation for posix_memalign, memalign or _aligned_malloc"
#endif

void *my_aligned_malloc(size_t size, size_t alignment) {
  void *ptr = nullptr;
#if defined(HAVE_POSIX_MEMALIGN)
  /* Linux */
  if (posix_memalign(&ptr, alignment, size)) {
    return nullptr;
  }
#elif defined(HAVE_MEMALIGN)
  /* Solaris */
  ptr = memalign(alignment, size);
  if (ptr == NULL) {
    return NULL;
  }
#elif defined(HAVE_ALIGNED_MALLOC)
  /* Windows */
  ptr = _aligned_malloc(size, alignment);
  if (ptr == NULL) {
    return NULL;
  }
#else
#error "Missing implementation for posix_memalign, memalign or _aligned_malloc"
#endif
  return ptr;
}

void my_aligned_free(void *ptr) {
  if (ptr == nullptr) {
    return;
  }
#if defined(HAVE_POSIX_MEMALIGN)
  /* Allocated with posix_memalign() */
  free(ptr);
#elif defined(HAVE_MEMALIGN)
  /* Allocated with memalign() */
  free(ptr);
#elif defined(HAVE_ALIGNED_MALLOC)
  /* Allocated with _aligned_malloc() */
  _aligned_free(ptr);
#else
  /* Allocated with malloc() */
  free(ptr);
#endif
}
