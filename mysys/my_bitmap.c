/*
   Copyright (c) 2001, 2011, Oracle and/or its affiliates.
   Copyright (C) 2009- 2011 Monty Program Ab

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */

/*
  Handling of uchar arrays as large bitmaps.

  API limitations (or, rather asserted safety assumptions,
  to encourage correct programming)

    * the internal size is a set of 32 bit words
    * the number of bits specified in creation can be any number > 0

  TODO:
  Make assembler thread safe versions of these using test-and-set instructions

  Original version created by Sergei Golubchik 2001 - 2004.
  New version written and test program added and some changes to the interface
  was made by Mikael Ronstrom 2005, with assistance of Tomas Ulin and Mats
  Kindahl.
*/

#include "mysys_priv.h"
#include <my_bitmap.h>
#include <m_string.h>
#include <my_bit.h>


/* Create a mask of the significant bits for the last byte (1,3,7,..255) */

static inline uchar last_byte_mask(uint bits)
{
  /* Get the number of used bits-1 (0..7) in the last byte */
  unsigned int const used= (bits - 1U) & 7U;
  /* Return bitmask for the significant bits */
  return ((2U << used) - 1);
}

/*
  Create a mask with the upper 'unused' bits set and the lower 'used'
  bits clear. The bits within each byte is stored in big-endian order.
*/

static inline uchar invers_last_byte_mask(uint bits)
{
  return last_byte_mask(bits) ^ 255;
}


void create_last_word_mask(MY_BITMAP *map)
{
  unsigned char const mask= invers_last_byte_mask(map->n_bits);

  /*
    The first bytes are to be set to zero since they represent real  bits
    in the bitvector. The last bytes are set to 0xFF since they  represent
    bytes not used by the bitvector. Finally the last byte contains  bits
    as set by the mask above.
  */
  unsigned char *ptr= (unsigned char*)&map->last_word_mask;

  map->last_word_ptr= map->bitmap + no_words_in_map(map)-1;
  switch (no_bytes_in_map(map) & 3) {
  case 1:
    map->last_word_mask= ~0U;
    ptr[0]= mask;
    return;
  case 2:
    map->last_word_mask= ~0U;
    ptr[0]= 0;
    ptr[1]= mask;
    return;
  case 3:
    map->last_word_mask= 0U;
    ptr[2]= mask;
    ptr[3]= 0xFFU;
    return;
  case 0:
    map->last_word_mask= 0U;
    ptr[3]= mask;
    return;
  }
}


static inline my_bitmap_map last_word_mask(uint bit)
{
  my_bitmap_map last_word_mask;
  uint n_bits= bit + 1;
  unsigned char const mask= invers_last_byte_mask(n_bits);

  /*
    The first bytes are to be set to zero since they represent real  bits
    in the bitvector. The last bytes are set to 0xFF since they  represent
    bytes not used by the bitvector. Finally the last byte contains  bits
    as set by the mask above.
  */
  unsigned char *ptr= (unsigned char*)&last_word_mask;

  switch ((n_bits + 7)/8 & 3) {
  case 1:
    last_word_mask= ~0U;
    ptr[0]= mask;
    break;
  case 2:
    last_word_mask= ~0U;
    ptr[0]= 0;
    ptr[1]= mask;
    break;
  case 3:
    last_word_mask= 0U;
    ptr[2]= mask;
    ptr[3]= 0xFFU;
    break;
  case 0:
    last_word_mask= 0U;
    ptr[3]= mask;
    break;
  }
  return last_word_mask;
}


static inline void bitmap_lock(MY_BITMAP *map __attribute__((unused)))
{
  if (map->mutex)
    mysql_mutex_lock(map->mutex);
}

static inline void bitmap_unlock(MY_BITMAP *map __attribute__((unused)))
{
  if (map->mutex)
    mysql_mutex_unlock(map->mutex);
}


my_bool bitmap_init(MY_BITMAP *map, my_bitmap_map *buf, uint n_bits,
		    my_bool thread_safe __attribute__((unused)))
{
  DBUG_ENTER("bitmap_init");
  if (!buf)
  {
    uint size_in_bytes= bitmap_buffer_size(n_bits);
    uint extra= 0;
    if (thread_safe)
    {
      size_in_bytes= ALIGN_SIZE(size_in_bytes);
      extra= sizeof(mysql_mutex_t);
    }
    map->mutex= 0;
    if (!(buf= (my_bitmap_map*) my_malloc(size_in_bytes+extra, MYF(MY_WME))))
      DBUG_RETURN(1);
    if (thread_safe)
    {
      map->mutex= (mysql_mutex_t *) ((char*) buf + size_in_bytes);
      mysql_mutex_init(key_BITMAP_mutex, map->mutex, MY_MUTEX_INIT_FAST);
    }
  }
  else
  {
    DBUG_ASSERT(thread_safe == 0);
  }

  map->bitmap= buf;
  map->n_bits= n_bits;
  create_last_word_mask(map);
  bitmap_clear_all(map);
  DBUG_RETURN(0);
}


void bitmap_free(MY_BITMAP *map)
{
  DBUG_ENTER("bitmap_free");
  if (map->bitmap)
  {
    if (map->mutex)
      mysql_mutex_destroy(map->mutex);
    my_free(map->bitmap);
    map->bitmap=0;
  }
  DBUG_VOID_RETURN;
}


/*
  test if bit already set and set it if it was not (thread unsafe method)

  SYNOPSIS
    bitmap_fast_test_and_set()
    MAP   bit map struct
    BIT   bit number

  RETURN
    0    bit was not set
    !=0  bit was set
*/

my_bool bitmap_fast_test_and_set(MY_BITMAP *map, uint bitmap_bit)
{
  uchar *value= ((uchar*) map->bitmap) + (bitmap_bit / 8);
  uchar bit= 1 << ((bitmap_bit) & 7);
  uchar res= (*value) & bit;
  *value|= bit;
  return res;
}


/*
  test if bit already set and set it if it was not (thread safe method)

  SYNOPSIS
    bitmap_fast_test_and_set()
    map          bit map struct
    bitmap_bit   bit number

  RETURN
    0    bit was not set
    !=0  bit was set
*/

my_bool bitmap_test_and_set(MY_BITMAP *map, uint bitmap_bit)
{
  my_bool res;
  DBUG_ASSERT(map->bitmap && bitmap_bit < map->n_bits);
  bitmap_lock(map);
  res= bitmap_fast_test_and_set(map, bitmap_bit);
  bitmap_unlock(map);
  return res;
}

/*
  test if bit already set and clear it if it was set(thread unsafe method)

  SYNOPSIS
    bitmap_fast_test_and_set()
    MAP   bit map struct
    BIT   bit number

  RETURN
    0    bit was not set
    !=0  bit was set
*/

my_bool bitmap_fast_test_and_clear(MY_BITMAP *map, uint bitmap_bit)
{
  uchar *byte= (uchar*) map->bitmap + (bitmap_bit / 8);
  uchar bit= 1 << ((bitmap_bit) & 7);
  uchar res= (*byte) & bit;
  *byte&= ~bit;
  return res;
}


my_bool bitmap_test_and_clear(MY_BITMAP *map, uint bitmap_bit)
{
  my_bool res;
  DBUG_ASSERT(map->bitmap && bitmap_bit < map->n_bits);
  bitmap_lock(map);
  res= bitmap_fast_test_and_clear(map, bitmap_bit);
  bitmap_unlock(map);
  return res;
}


uint bitmap_set_next(MY_BITMAP *map)
{
  uint bit_found;
  DBUG_ASSERT(map->bitmap);
  if ((bit_found= bitmap_get_first(map)) != MY_BIT_NONE)
    bitmap_set_bit(map, bit_found);
  return bit_found;
}


void bitmap_set_prefix(MY_BITMAP *map, uint prefix_size)
{
  uint prefix_bytes, prefix_bits, d;
  uchar *m= (uchar *)map->bitmap;

  DBUG_ASSERT(map->bitmap &&
	      (prefix_size <= map->n_bits || prefix_size == (uint) ~0));
  set_if_smaller(prefix_size, map->n_bits);
  if ((prefix_bytes= prefix_size / 8))
    memset(m, 0xff, prefix_bytes);
  m+= prefix_bytes;
  if ((prefix_bits= prefix_size & 7))
  {
    *m++= (1 << prefix_bits)-1;
    prefix_bytes++;
  }
  if ((d= no_bytes_in_map(map)-prefix_bytes))
    bzero(m, d);
}


my_bool bitmap_is_prefix(const MY_BITMAP *map, uint prefix_size)
{
  uint prefix_mask= last_byte_mask(prefix_size);
  uchar *m= (uchar*) map->bitmap;
  uchar *end_prefix= m+(prefix_size-1)/8;
  uchar *end;
  DBUG_ASSERT(m && prefix_size <= map->n_bits);

  /* Empty prefix is always true */
  if (!prefix_size)
    return 1;

  while (m < end_prefix)
    if (*m++ != 0xff)
      return 0;

  end= ((uchar*) map->bitmap) + no_bytes_in_map(map) - 1;
  if (m == end)
    return ((*m & last_byte_mask(map->n_bits)) == prefix_mask);

  if (*m != prefix_mask)
    return 0;

  while (++m < end)
    if (*m != 0)
      return 0;
  return ((*m & last_byte_mask(map->n_bits)) == 0);
}


my_bool bitmap_is_set_all(const MY_BITMAP *map)
{
  my_bitmap_map *data_ptr= map->bitmap;
  my_bitmap_map *end= map->last_word_ptr;
  for (; data_ptr < end; data_ptr++)
    if (*data_ptr != 0xFFFFFFFF)
      return FALSE;
  return (*data_ptr | map->last_word_mask) == 0xFFFFFFFF;
}


my_bool bitmap_is_clear_all(const MY_BITMAP *map)
{
  my_bitmap_map *data_ptr= map->bitmap;
  my_bitmap_map *end= map->last_word_ptr;

  for (; data_ptr < end; data_ptr++)
    if (*data_ptr)
      return FALSE;
  return (*data_ptr & ~map->last_word_mask) == 0;
}

/* Return TRUE if map1 is a subset of map2 */

my_bool bitmap_is_subset(const MY_BITMAP *map1, const MY_BITMAP *map2)
{
  my_bitmap_map *m1= map1->bitmap, *m2= map2->bitmap, *end;

  DBUG_ASSERT(map1->bitmap && map2->bitmap &&
              map1->n_bits==map2->n_bits);

  end= map1->last_word_ptr;
  while (m1 < end)
  {
    if ((*m1++) & ~(*m2++))
      return 0;
  }
  /* here both maps have the same number of bits - see assert above */
  return ((*m1 & ~*m2 & ~map1->last_word_mask) ? 0 : 1);
}

/* True if bitmaps has any common bits */

my_bool bitmap_is_overlapping(const MY_BITMAP *map1, const MY_BITMAP *map2)
{
  my_bitmap_map *m1= map1->bitmap, *m2= map2->bitmap, *end;

  DBUG_ASSERT(map1->bitmap && map2->bitmap &&
              map1->n_bits==map2->n_bits);

  end= map1->last_word_ptr;
  while (m1 < end)
  {
    if ((*m1++) & (*m2++))
      return 1;
  }
  /* here both maps have the same number of bits - see assert above */
  return ((*m1 & *m2 & ~map1->last_word_mask) ? 1 : 0);
}


void bitmap_intersect(MY_BITMAP *map, const MY_BITMAP *map2)
{
  my_bitmap_map *to= map->bitmap, *from= map2->bitmap, *end;
  uint len= no_words_in_map(map), len2 = no_words_in_map(map2);

  DBUG_ASSERT(map->bitmap && map2->bitmap);

  end= to+min(len,len2);
  while (to < end)
    *to++ &= *from++;

  if (len2 <= len)
  {
    to[-1]&= ~map2->last_word_mask; /* Clear last not relevant bits */
    end+= len-len2;
    while (to < end)
      *to++= 0;
  }
}


/*
  Check if there is some bit index between start_bit and end_bit, such that
  this is bit is set for all bitmaps in bitmap_list.

  SYNOPSIS
    bitmap_exists_intersection()
    bitmpap_array [in]  a set of MY_BITMAPs
    bitmap_count  [in]  number of elements in bitmpap_array
    start_bit     [in]  beginning (inclusive) of the range of bits to search
    end_bit       [in]  end (inclusive) of the range of bits to search, must be
                        no bigger than the bits of the shortest bitmap.

  NOTES
  This function assumes that for at least one of the bitmaps in bitmap_array all
  bits outside the range [start_bit, end_bit] are 0. As a result is not
  necessary to take care of the bits outside the range [start_bit, end_bit].

  RETURN
    TRUE  if an intersecion exists
    FALSE no intersection
*/

my_bool bitmap_exists_intersection(const MY_BITMAP **bitmap_array,
                                   uint bitmap_count,
                                   uint start_bit, uint end_bit)
{
  uint i, j, start_idx, end_idx;
  my_bitmap_map cur_res;

  DBUG_ASSERT(bitmap_count && end_bit >= start_bit);
  for (j= 0; j < bitmap_count; j++)
    DBUG_ASSERT(end_bit < bitmap_array[j]->n_bits);

  start_idx= start_bit/8/sizeof(my_bitmap_map);
  end_idx= end_bit/8/sizeof(my_bitmap_map);

  for (i= start_idx; i < end_idx; i++)
  {
    cur_res= ~0;
    for (j= 0; cur_res && j < bitmap_count; j++)
      cur_res &= bitmap_array[j]->bitmap[i];
    if (cur_res)
      return TRUE;
  }
  cur_res= ~last_word_mask(end_bit);
  for (j= 0; cur_res && j < bitmap_count; j++)
    cur_res &= bitmap_array[j]->bitmap[end_idx];
  return cur_res != 0;
}


/* True if union of bitmaps have all bits set */

my_bool bitmap_union_is_set_all(const MY_BITMAP *map1, const MY_BITMAP *map2)
{
  my_bitmap_map *m1= map1->bitmap, *m2= map2->bitmap, *end;

  DBUG_ASSERT(map1->bitmap && map2->bitmap &&
              map1->n_bits==map2->n_bits);
  end= map1->last_word_ptr;
  while ( m1 < end)
    if ((*m1++ | *m2++) != 0xFFFFFFFF)
      return FALSE;
  /* here both maps have the same number of bits - see assert above */
  return ((*m1 | *m2 | map1->last_word_mask) != 0xFFFFFFFF);
}



/*
  Set/clear all bits above a bit.

  SYNOPSIS
    bitmap_set_above()
    map                  RETURN The bitmap to change.
    from_byte                   The bitmap buffer byte offset to start with.
    use_bit                     The bit value (1/0) to use for all upper bits.

  NOTE
    You can only set/clear full bytes.
    The function is meant for the situation that you copy a smaller bitmap
    to a bigger bitmap. Bitmap lengths are always multiple of eigth (the
    size of a byte). Using 'from_byte' saves multiplication and division
    by eight during parameter passing.

  RETURN
    void
*/

void bitmap_set_above(MY_BITMAP *map, uint from_byte, uint use_bit)
{
  uchar use_byte= use_bit ? 0xff : 0;
  uchar *to= (uchar *)map->bitmap + from_byte;
  uchar *end= (uchar *)map->bitmap + (map->n_bits+7)/8;

  while (to < end)
    *to++= use_byte;
}


void bitmap_subtract(MY_BITMAP *map, const MY_BITMAP *map2)
{
  my_bitmap_map *to= map->bitmap, *from= map2->bitmap, *end;
  DBUG_ASSERT(map->bitmap && map2->bitmap &&
              map->n_bits==map2->n_bits);

  end= map->last_word_ptr;

  while (to <= end)
    *to++ &= ~(*from++);
}


void bitmap_union(MY_BITMAP *map, const MY_BITMAP *map2)
{
  my_bitmap_map *to= map->bitmap, *from= map2->bitmap, *end;

  DBUG_ASSERT(map->bitmap && map2->bitmap &&
              map->n_bits==map2->n_bits);
  end= map->last_word_ptr;

  while (to <= end)
    *to++ |= *from++;
}


void bitmap_xor(MY_BITMAP *map, const MY_BITMAP *map2)
{
  my_bitmap_map *to= map->bitmap, *from= map2->bitmap, *end= map->last_word_ptr;
  DBUG_ASSERT(map->bitmap && map2->bitmap &&
              map->n_bits==map2->n_bits);
  while (to <= end)
    *to++ ^= *from++;
}


void bitmap_invert(MY_BITMAP *map)
{
  my_bitmap_map *to= map->bitmap, *end;

  DBUG_ASSERT(map->bitmap);
  end= map->last_word_ptr;

  while (to <= end)
    *to++ ^= 0xFFFFFFFF;
}


uint bitmap_bits_set(const MY_BITMAP *map)
{
  my_bitmap_map *data_ptr= map->bitmap;
  my_bitmap_map *end= map->last_word_ptr;
  uint res= 0;
  DBUG_ASSERT(map->bitmap);

  for (; data_ptr < end; data_ptr++)
    res+= my_count_bits_uint32(*data_ptr);

  /*Reset last bits to zero*/
  res+= my_count_bits_uint32(*map->last_word_ptr & ~map->last_word_mask);
  return res;
}

void bitmap_copy(MY_BITMAP *map, const MY_BITMAP *map2)
{
  my_bitmap_map *to= map->bitmap, *from= map2->bitmap, *end;

  DBUG_ASSERT(map->bitmap && map2->bitmap &&
              map->n_bits==map2->n_bits);
  end= map->last_word_ptr;

  while (to <= end)
    *to++ = *from++;
}


uint bitmap_get_first_set(const MY_BITMAP *map)
{
  uchar *byte_ptr;
  uint i,j,k;
  my_bitmap_map *data_ptr, *end= map->last_word_ptr;

  DBUG_ASSERT(map->bitmap);
  data_ptr= map->bitmap;

  for (i=0; data_ptr < end; data_ptr++, i++)
    if (*data_ptr)
      goto found;
  if (!(*data_ptr & ~map->last_word_mask))
    return MY_BIT_NONE;

found:
  {
    byte_ptr= (uchar*)data_ptr;
    for (j=0; ; j++, byte_ptr++)
    {
      if (*byte_ptr)
      {
        for (k=0; ; k++)
        {
          if (*byte_ptr & (1 << k))
            return (i*32) + (j*8) + k;
        }
      }
    }
  }
  DBUG_ASSERT(0);
  return MY_BIT_NONE;                           /* Impossible */
}


uint bitmap_get_first(const MY_BITMAP *map)
{
  uchar *byte_ptr;
  uint i,j,k;
  my_bitmap_map *data_ptr, *end= map->last_word_ptr;

  DBUG_ASSERT(map->bitmap);
  data_ptr= map->bitmap;
  *map->last_word_ptr|= map->last_word_mask;

  for (i=0; data_ptr < end; data_ptr++, i++)
    if (*data_ptr != 0xFFFFFFFF)
      goto found;
  if ((*data_ptr | map->last_word_mask) == 0xFFFFFFFF)
    return MY_BIT_NONE;

found:
  {
    byte_ptr= (uchar*)data_ptr;
    for (j=0; ; j++, byte_ptr++)
    {
      if (*byte_ptr != 0xFF)
      {
        for (k=0; ; k++)
        {
          if (!(*byte_ptr & (1 << k)))
            return (i*32) + (j*8) + k;
        }
      }
    }
  }
  DBUG_ASSERT(0);
  return MY_BIT_NONE;                           /* Impossible */
}


uint bitmap_lock_set_next(MY_BITMAP *map)
{
  uint bit_found;
  bitmap_lock(map);
  bit_found= bitmap_set_next(map);
  bitmap_unlock(map);
  return bit_found;
}


void bitmap_lock_clear_bit(MY_BITMAP *map, uint bitmap_bit)
{
  bitmap_lock(map);
  DBUG_ASSERT(map->bitmap && bitmap_bit < map->n_bits);
  bitmap_clear_bit(map, bitmap_bit);
  bitmap_unlock(map);
}

