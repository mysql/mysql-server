/* Copyright (C) 2000 MySQL AB

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

#include <my_pthread.h>

#define MY_BIT_NONE (~(uint) 0)

typedef struct st_bitmap
{
  uchar *bitmap;
  uint bitmap_size; /* number of bytes occupied by the above */
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
extern my_bool bitmap_cmp(const MY_BITMAP *map1, const MY_BITMAP *map2);
extern my_bool bitmap_init(MY_BITMAP *map, uchar *buf, uint bitmap_size, my_bool thread_safe);
extern my_bool bitmap_is_clear_all(const MY_BITMAP *map);
extern my_bool bitmap_is_prefix(const MY_BITMAP *map, uint prefix_size);
extern my_bool bitmap_is_set(const MY_BITMAP *map, uint bitmap_bit);
extern my_bool bitmap_is_set_all(const MY_BITMAP *map);
extern my_bool bitmap_is_subset(const MY_BITMAP *map1, const MY_BITMAP *map2);
extern my_bool bitmap_test_and_set(MY_BITMAP *map, uint bitmap_bit);
extern my_bool bitmap_fast_test_and_set(MY_BITMAP *map, uint bitmap_bit);
extern uint bitmap_set_next(MY_BITMAP *map);
extern uint bitmap_get_first(const MY_BITMAP *map);
extern uint bitmap_bits_set(const MY_BITMAP *map);
extern void bitmap_clear_all(MY_BITMAP *map);
extern void bitmap_clear_bit(MY_BITMAP *map, uint bitmap_bit);
extern void bitmap_free(MY_BITMAP *map);
extern void bitmap_intersect(MY_BITMAP *map, const MY_BITMAP *map2);
extern void bitmap_set_above(MY_BITMAP *map, uint from_byte, uint use_bit);
extern void bitmap_set_all(MY_BITMAP *map);
extern void bitmap_set_bit(MY_BITMAP *map, uint bitmap_bit);
extern void bitmap_set_prefix(MY_BITMAP *map, uint prefix_size);
extern void bitmap_subtract(MY_BITMAP *map, const MY_BITMAP *map2);
extern void bitmap_union(MY_BITMAP *map, const MY_BITMAP *map2);

/* Fast, not thread safe, bitmap functions */
#define bitmap_fast_set_bit(MAP, BIT) (MAP)->bitmap[(BIT) / 8] |= (1 << ((BIT) & 7))
#define bitmap_fast_clear_bit(MAP, BIT) (MAP)->bitmap[(BIT) / 8] &= ~ (1 << ((BIT) & 7))
#define bitmap_fast_is_set(MAP, BIT) (MAP)->bitmap[(BIT) / 8] & (1 << ((BIT) & 7))

#ifdef	__cplusplus
}
#endif

#endif /* _my_bitmap_h_ */
