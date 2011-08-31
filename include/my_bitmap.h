/*
   Copyright (c) 2001, 2011, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _my_bitmap_h_
#define _my_bitmap_h_

#define MY_BIT_NONE (~(uint) 0)

#include <m_string.h>

typedef uint32 my_bitmap_map;

typedef struct st_bitmap
{
  my_bitmap_map *bitmap;
  uint n_bits; /* number of bits occupied by the above */
  my_bitmap_map last_word_mask;
  my_bitmap_map *last_word_ptr;
  /*
     mutex will be acquired for the duration of each bitmap operation if
     thread_safe flag in bitmap_init was set.  Otherwise, we optimize by not
     acquiring the mutex
   */
  mysql_mutex_t *mutex;
} MY_BITMAP;

#ifdef	__cplusplus
extern "C" {
#endif
extern void create_last_word_mask(MY_BITMAP *map);
extern my_bool bitmap_init(MY_BITMAP *map, my_bitmap_map *buf, uint n_bits,
                           my_bool thread_safe);
extern my_bool bitmap_is_clear_all(const MY_BITMAP *map);
extern my_bool bitmap_is_prefix(const MY_BITMAP *map, uint prefix_size);
extern my_bool bitmap_is_set_all(const MY_BITMAP *map);
extern my_bool bitmap_is_subset(const MY_BITMAP *map1, const MY_BITMAP *map2);
extern my_bool bitmap_is_overlapping(const MY_BITMAP *map1,
                                     const MY_BITMAP *map2);
extern my_bool bitmap_test_and_set(MY_BITMAP *map, uint bitmap_bit);
extern my_bool bitmap_test_and_clear(MY_BITMAP *map, uint bitmap_bit);
extern my_bool bitmap_fast_test_and_set(MY_BITMAP *map, uint bitmap_bit);
extern uint bitmap_set_next(MY_BITMAP *map);
extern uint bitmap_get_first(const MY_BITMAP *map);
extern uint bitmap_get_first_set(const MY_BITMAP *map);
extern uint bitmap_bits_set(const MY_BITMAP *map);
extern void bitmap_free(MY_BITMAP *map);
extern void bitmap_set_above(MY_BITMAP *map, uint from_byte, uint use_bit);
extern void bitmap_set_prefix(MY_BITMAP *map, uint prefix_size);
extern void bitmap_intersect(MY_BITMAP *map, const MY_BITMAP *map2);
extern void bitmap_subtract(MY_BITMAP *map, const MY_BITMAP *map2);
extern void bitmap_union(MY_BITMAP *map, const MY_BITMAP *map2);
extern void bitmap_xor(MY_BITMAP *map, const MY_BITMAP *map2);
extern void bitmap_invert(MY_BITMAP *map);
extern void bitmap_copy(MY_BITMAP *map, const MY_BITMAP *map2);

extern uint bitmap_lock_set_next(MY_BITMAP *map);
extern void bitmap_lock_clear_bit(MY_BITMAP *map, uint bitmap_bit);
/* Fast, not thread safe, bitmap functions */
#define bitmap_buffer_size(bits) (((bits)+31)/32)*4
#define no_bytes_in_map(map) (((map)->n_bits + 7)/8)
#define no_words_in_map(map) (((map)->n_bits + 31)/32)
#define bytes_word_aligned(bytes) (4*((bytes + 3)/4))
#define _bitmap_set_bit(MAP, BIT) (((uchar*)(MAP)->bitmap)[(BIT) / 8] \
                                  |= (1 << ((BIT) & 7)))
#define _bitmap_flip_bit(MAP, BIT) (((uchar*)(MAP)->bitmap)[(BIT) / 8] \
                                  ^= (1 << ((BIT) & 7)))
#define _bitmap_clear_bit(MAP, BIT) (((uchar*)(MAP)->bitmap)[(BIT) / 8] \
                                  &= ~ (1 << ((BIT) & 7)))
#define _bitmap_is_set(MAP, BIT) (uint) (((uchar*)(MAP)->bitmap)[(BIT) / 8] \
                                         & (1 << ((BIT) & 7)))
/*
  WARNING!

  The below symbols are inline functions in DEBUG builds and macros in
  non-DEBUG builds. The latter evaluate their 'bit' argument twice.

  NEVER use an increment/decrement operator with the 'bit' argument.
  It would work with DEBUG builds, but fails later in production builds!

  FORBIDDEN: bitmap_set_bit($my_bitmap, (field++)->field_index);
*/
#ifndef DBUG_OFF
static inline void
bitmap_set_bit(MY_BITMAP *map,uint bit)
{
  DBUG_ASSERT(bit < (map)->n_bits);
  _bitmap_set_bit(map,bit);
}
static inline void
bitmap_flip_bit(MY_BITMAP *map,uint bit)
{
  DBUG_ASSERT(bit < (map)->n_bits);
  _bitmap_flip_bit(map,bit);
}
static inline void
bitmap_clear_bit(MY_BITMAP *map,uint bit)
{
  DBUG_ASSERT(bit < (map)->n_bits);
  _bitmap_clear_bit(map,bit);
}
static inline uint
bitmap_is_set(const MY_BITMAP *map,uint bit)
{
  DBUG_ASSERT(bit < (map)->n_bits);
  return _bitmap_is_set(map,bit);
}
#else
#define bitmap_set_bit(MAP, BIT) _bitmap_set_bit(MAP, BIT)
#define bitmap_flip_bit(MAP, BIT) _bitmap_flip_bit(MAP, BIT)
#define bitmap_clear_bit(MAP, BIT) _bitmap_clear_bit(MAP, BIT)
#define bitmap_is_set(MAP, BIT) _bitmap_is_set(MAP, BIT)
#endif

static inline my_bool bitmap_cmp(const MY_BITMAP *map1, const MY_BITMAP *map2)
{
  if (memcmp(map1->bitmap, map2->bitmap, 4*(no_words_in_map(map1)-1)) != 0)
    return FALSE;
  return ((*map1->last_word_ptr | map1->last_word_mask) ==
          (*map2->last_word_ptr | map2->last_word_mask));
}

#define bitmap_clear_all(MAP) \
  { memset((MAP)->bitmap, 0, 4*no_words_in_map((MAP))); }
#define bitmap_set_all(MAP) \
  (memset((MAP)->bitmap, 0xFF, 4*no_words_in_map((MAP))))

#ifdef	__cplusplus
}
#endif

#endif /* _my_bitmap_h_ */
