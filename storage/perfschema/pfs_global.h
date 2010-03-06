/* Copyright (C) 2008-2009 Sun Microsystems, Inc

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

#ifndef PFS_GLOBAL_H
#define PFS_GLOBAL_H

/**
  @file storage/perfschema/pfs_global.h
  Miscellaneous global dependencies (declarations).
*/

extern bool pfs_initialized;

void *pfs_malloc(size_t size, myf flags);
#define PFS_MALLOC_ARRAY(n, T, f) \
  reinterpret_cast<T*> (pfs_malloc((n) * sizeof(T), (f)))
void pfs_free(void *ptr);

inline uint randomized_index(const void *ptr, uint max_size)
{
  if (unlikely(max_size == 0))
    return 0;

  /*
    ptr is typically an aligned structure,
    so the last bits are not really random, but this has no effect.
    Apply a factor A*x to spread
    close values of ptr further apart (which helps with arrays),
    and to spread values way beyond a typical max_size.
    Then, apply a modulo to end within [0, max_size - 1].
    A is big prime numbers, to avoid resonating with max_size,
    to have a uniform distribution in [0, max_size - 1].
    The value of A is chosen so that index(ptr) and index(ptr + N) (for arrays)
    are likely to be not similar for typical values of max_size
    (50, 100, 1000, etc).
    In other words, (sizeof(T)*A % max_size) should not be a small number,
    to avoid that with 'T array[max_size]', index(array[i])
    and index(array[i + 1]) end up pointing in the same area in [0, max_size - 1].
  */
  return static_cast<uint>
    (((reinterpret_cast<intptr> (ptr)) * 2166179) % max_size);
}

void pfs_print_error(const char *format, ...);

#endif

