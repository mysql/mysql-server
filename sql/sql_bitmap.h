/* Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  Implementation of a bitmap type.
  The idea with this is to be able to handle any constant number of bits but
  also be able to use 32 or 64 bits bitmaps very efficiently
*/

#ifndef SQL_BITMAP_INCLUDED
#define SQL_BITMAP_INCLUDED

#include <my_sys.h>
#include <my_bitmap.h>

template <uint default_width> class Bitmap
{
  MY_BITMAP map;
  uint32 buffer[(default_width+31)/32];
public:
  Bitmap() { init(); }
  Bitmap(const Bitmap& from) { *this=from; }
  explicit Bitmap(uint prefix_to_set) { init(prefix_to_set); }
  void init() { bitmap_init(&map, buffer, default_width, 0); }
  void init(uint prefix_to_set) { init(); set_prefix(prefix_to_set); }
  uint length() const { return default_width; }
  Bitmap& operator=(const Bitmap& map2)
  {
    init();
    memcpy(buffer, map2.buffer, sizeof(buffer));
    return *this;
  }
  void set_bit(uint n) { bitmap_set_bit(&map, n); }
  void clear_bit(uint n) { bitmap_clear_bit(&map, n); }
  void set_prefix(uint n) { bitmap_set_prefix(&map, n); }
  void set_all() { bitmap_set_all(&map); }
  void clear_all() { bitmap_clear_all(&map); }
  void intersect(Bitmap& map2) { bitmap_intersect(&map, &map2.map); }
  void intersect(ulonglong map2buff)
  {
    MY_BITMAP map2;
    bitmap_init(&map2, (uint32 *)&map2buff, sizeof(ulonglong)*8, 0);
    bitmap_intersect(&map, &map2);
  }
  /* Use highest bit for all bits above sizeof(ulonglong)*8. */
  void intersect_extended(ulonglong map2buff)
  {
    intersect(map2buff);
    if (map.n_bits > sizeof(ulonglong) * 8)
      bitmap_set_above(&map, sizeof(ulonglong),
                       test(map2buff & (LL(1) << (sizeof(ulonglong) * 8 - 1))));
  }
  void subtract(Bitmap& map2) { bitmap_subtract(&map, &map2.map); }
  void merge(Bitmap& map2) { bitmap_union(&map, &map2.map); }
  bool is_set(uint n) const { return bitmap_is_set(&map, n); }
  bool is_prefix(uint n) const { return bitmap_is_prefix(&map, n); }
  bool is_clear_all() const { return bitmap_is_clear_all(&map); }
  bool is_set_all() const { return bitmap_is_set_all(&map); }
  bool is_subset(const Bitmap& map2) const { return bitmap_is_subset(&map, &map2.map); }
  bool is_overlapping(const Bitmap& map2) const { return bitmap_is_overlapping(&map, &map2.map); }
  bool operator==(const Bitmap& map2) const { return bitmap_cmp(&map, &map2.map); }
  bool operator!=(const Bitmap& map2) const { return !(*this == map2); }
  char *print(char *buf) const
  {
    char *s=buf;
    const uchar *e=(uchar *)buffer, *b=e+sizeof(buffer)-1;
    while (!*b && b>e)
      b--;
    if ((*s=_dig_vec_upper[*b >> 4]) != '0')
        s++;
    *s++=_dig_vec_upper[*b & 15];
    while (--b>=e)
    {
      *s++=_dig_vec_upper[*b >> 4];
      *s++=_dig_vec_upper[*b & 15];
    }
    *s=0;
    return buf;
  }
  ulonglong to_ulonglong() const
  {
    if (sizeof(buffer) >= 8)
      return uint8korr(buffer);
    DBUG_ASSERT(sizeof(buffer) >= 4);
    return (ulonglong) uint4korr(buffer);
  }
  uint bits_set()
  {
    return bitmap_bits_set(&map);
  }
};

/* An iterator to quickly walk over bits in unlonglong bitmap. */
class Table_map_iterator
{
  ulonglong bmp;
  uint no;
public:
  Table_map_iterator(ulonglong t) : bmp(t), no(0) {}
  int next_bit()
  {
    static const char last_bit[16]= {32, 0, 1, 0, 
                                      2, 0, 1, 0, 
                                      3, 0, 1, 0,
                                      2, 0, 1, 0};
    uint bit;
    while ((bit= last_bit[bmp & 0xF]) == 32)
    {
      no += 4;
      bmp= bmp >> 4;
      if (!bmp)
        return BITMAP_END;
    }
    bmp &= ~(1LL << bit);
    return no + bit;
  }
  int operator++(int) { return next_bit(); }
  enum { BITMAP_END= 64 };
};

template <> class Bitmap<64>
{
  ulonglong map;
public:
  Bitmap<64>() { }
  explicit Bitmap<64>(uint prefix_to_set) { set_prefix(prefix_to_set); }
  void init() { }
  void init(uint prefix_to_set) { set_prefix(prefix_to_set); }
  uint length() const { return 64; }
  void set_bit(uint n) { map|= ((ulonglong)1) << n; }
  void clear_bit(uint n) { map&= ~(((ulonglong)1) << n); }
  void set_prefix(uint n)
  {
    if (n >= length())
      set_all();
    else
      map= (((ulonglong)1) << n)-1;
  }
  void set_all() { map=~(ulonglong)0; }
  void clear_all() { map=(ulonglong)0; }
  void intersect(Bitmap<64>& map2) { map&= map2.map; }
  void intersect(ulonglong map2) { map&= map2; }
  void intersect_extended(ulonglong map2) { map&= map2; }
  void subtract(Bitmap<64>& map2) { map&= ~map2.map; }
  void merge(Bitmap<64>& map2) { map|= map2.map; }
  bool is_set(uint n) const { return test(map & (((ulonglong)1) << n)); }
  bool is_prefix(uint n) const { return map == (((ulonglong)1) << n)-1; }
  bool is_clear_all() const { return map == (ulonglong)0; }
  bool is_set_all() const { return map == ~(ulonglong)0; }
  bool is_subset(const Bitmap<64>& map2) const { return !(map & ~map2.map); }
  bool is_overlapping(const Bitmap<64>& map2) const { return (map & map2.map)!= 0; }
  bool operator==(const Bitmap<64>& map2) const { return map == map2.map; }
  char *print(char *buf) const { longlong2str(map,buf,16); return buf; }
  ulonglong to_ulonglong() const { return map; }
  class Iterator : public Table_map_iterator
  {
  public:
    Iterator(Bitmap<64> &bmp) : Table_map_iterator(bmp.map) {}
  };
  uint bits_set()
  {
    //TODO: use my_count_bits()
    uint res= 0, i= 0;
    for (; i < 64 ; i++)
    {
      if (map & ((ulonglong)1<<i))
        res++;
    }
    return res;
  }
};


#endif /* SQL_BITMAP_INCLUDED */
