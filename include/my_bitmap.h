/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef _my_bitmap_h_
#define _my_bitmap_h_

#include <my_pthread.h>

#define MY_BIT_NONE (~(uint) 0)


typedef struct st_bitmap
{
  uchar *bitmap;
  uint bitmap_size; /* number of bits occupied by the above */
  uint32 last_word_mask;
  uint32 *last_word_ptr;
  /*
     mutex will be acquired for the duration of each bitmap operation if
     thread_safe flag in bitmap_init was set.  Otherwise, we optimize by not
     acquiring the mutex
   */
#ifdef THREAD
  pthread_mutex_t *mutex;
#endif
} MY_BITMAP;

#ifdef	__cplusplus
extern "C" {
#endif
extern my_bool bitmap_init(MY_BITMAP *map, uchar *buf, uint bitmap_size, my_bool thread_safe);
extern my_bool bitmap_is_clear_all(const MY_BITMAP *map);
extern my_bool bitmap_is_prefix(const MY_BITMAP *map, uint prefix_size);
extern my_bool bitmap_is_set_all(const MY_BITMAP *map);
extern my_bool bitmap_is_subset(const MY_BITMAP *map1, const MY_BITMAP *map2);
extern uint bitmap_set_next(MY_BITMAP *map);
extern uint bitmap_get_first(const MY_BITMAP *map);
extern uint bitmap_get_first_set(const MY_BITMAP *map);
extern uint bitmap_bits_set(const MY_BITMAP *map);
extern void bitmap_free(MY_BITMAP *map);
extern void bitmap_set_prefix(MY_BITMAP *map, uint prefix_size);
extern void bitmap_intersect(MY_BITMAP *map, const MY_BITMAP *map2);
extern void bitmap_subtract(MY_BITMAP *map, const MY_BITMAP *map2);
extern void bitmap_union(MY_BITMAP *map, const MY_BITMAP *map2);

extern uint bitmap_lock_set_next(MY_BITMAP *map);
extern void bitmap_lock_clear_bit(MY_BITMAP *map, uint bitmap_bit);
#ifdef NOT_USED
extern uint bitmap_lock_bits_set(const MY_BITMAP *map);
extern my_bool bitmap_lock_is_set_all(const MY_BITMAP *map);
extern uint bitmap_lock_get_first(const MY_BITMAP *map);
extern uint bitmap_lock_get_first_set(const MY_BITMAP *map);
extern my_bool bitmap_lock_is_subset(const MY_BITMAP *map1,
                                     const MY_BITMAP *map2);
extern my_bool bitmap_lock_is_prefix(const MY_BITMAP *map, uint prefix_size);
extern my_bool bitmap_lock_is_set(const MY_BITMAP *map, uint bitmap_bit);
extern my_bool bitmap_lock_is_clear_all(const MY_BITMAP *map);
extern my_bool bitmap_lock_cmp(const MY_BITMAP *map1, const MY_BITMAP *map2);
extern void bitmap_lock_set_all(MY_BITMAP *map);
extern void bitmap_lock_clear_all(MY_BITMAP *map);
extern void bitmap_lock_set_bit(MY_BITMAP *map, uint bitmap_bit);
extern void bitmap_lock_flip_bit(MY_BITMAP *map, uint bitmap_bit);
extern void bitmap_lock_set_prefix(MY_BITMAP *map, uint prefix_size);
extern void bitmap_lock_intersect(MY_BITMAP *map, const MY_BITMAP *map2);
extern void bitmap_lock_subtract(MY_BITMAP *map, const MY_BITMAP *map2);
extern void bitmap_lock_union(MY_BITMAP *map, const MY_BITMAP *map2);
extern void bitmap_lock_xor(MY_BITMAP *map, const MY_BITMAP *map2);
extern void bitmap_lock_invert(MY_BITMAP *map);
#endif
/* Fast, not thread safe, bitmap functions */
#define no_bytes_in_map(map) ((map->bitmap_size + 7)/8)
#define no_words_in_map(map) ((map->bitmap_size + 31)/32)
#define bytes_word_aligned(bytes) (4*((bytes + 3)/4))
#define bitmap_set_bit(MAP, BIT) ((MAP)->bitmap[(BIT) / 8] |= (1 << ((BIT) & 7)))
#define bitmap_flip_bit(MAP, BIT) ((MAP)->bitmap[(BIT) / 8] ^= (1 << ((BIT) & 7)))
#define bitmap_clear_bit(MAP, BIT) ((MAP)->bitmap[(BIT) / 8] &= ~ (1 << ((BIT) & 7)))
#define bitmap_is_set(MAP, BIT) ((MAP)->bitmap[(BIT) / 8] & (1 << ((BIT) & 7)))
#define bitmap_cmp(MAP1, MAP2) \
  (memcmp((MAP1)->bitmap, (MAP2)->bitmap, 4*no_words_in_map((MAP1)))==0)
#define bitmap_clear_all(MAP) \
  memset((MAP)->bitmap, 0, 4*no_words_in_map((MAP))); \
  *(MAP)->last_word_ptr|= (MAP)->last_word_mask
#define bitmap_set_all(MAP) \
  (memset((MAP)->bitmap, 0xFF, 4*no_words_in_map((MAP))))

#ifdef	__cplusplus
}
#endif

#endif /* _my_bitmap_h_ */
