/* Copyright (C) 2006 MySQL AB & Alexey Botchkov & MySQL Finland AB
   & TCX DataKonsult AB

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

#include "maria_def.h"

#ifdef HAVE_RTREE_KEYS

#include "ma_rt_index.h"
#include "ma_rt_key.h"
#include "ma_rt_mbr.h"

typedef struct
{
  double square;
  int n_node;
  uchar *key;
  double *coords;
} SplitStruct;

inline static double *reserve_coords(double **d_buffer, int n_dim)
{
  double *coords = *d_buffer;
  (*d_buffer) += n_dim * 2;
  return coords;
}

static void mbr_join(double *a, const double *b, int n_dim)
{
  double *end = a + n_dim * 2;
  do
  {
    if (a[0] > b[0])
      a[0] = b[0];

    if (a[1] < b[1])
      a[1] = b[1];

    a += 2;
    b += 2;
  }while (a != end);
}

/*
Counts the square of mbr which is a join of a and b
*/
static double mbr_join_square(const double *a, const double *b, int n_dim)
{
  const double *end = a + n_dim * 2;
  double square = 1.0;
  do
  {
    square *=
      ((a[1] < b[1]) ? b[1] : a[1]) - ((a[0] > b[0]) ? b[0] : a[0]);

    a += 2;
    b += 2;
  }while (a != end);

  return square;
}

static double count_square(const double *a, int n_dim)
{
  const double *end = a + n_dim * 2;
  double square = 1.0;
  do
  {
    square *= a[1] - a[0];
    a += 2;
  }while (a != end);
  return square;
}

inline static void copy_coords(double *dst, const double *src, int n_dim)
{
  memcpy(dst, src, sizeof(double) * (n_dim * 2));
}

/*
Select two nodes to collect group upon
*/
static void pick_seeds(SplitStruct *node, int n_entries,
     SplitStruct **seed_a, SplitStruct **seed_b, int n_dim)
{
  SplitStruct *cur1;
  SplitStruct *lim1 = node + (n_entries - 1);
  SplitStruct *cur2;
  SplitStruct *lim2 = node + n_entries;

  double max_d = -DBL_MAX;
  double d;

  for (cur1 = node; cur1 < lim1; ++cur1)
  {
    for (cur2=cur1 + 1; cur2 < lim2; ++cur2)
    {

      d = mbr_join_square(cur1->coords, cur2->coords, n_dim) - cur1->square -
          cur2->square;
      if (d > max_d)
      {
        max_d = d;
        *seed_a = cur1;
        *seed_b = cur2;
      }
    }
  }
}

/*
Select next node and group where to add
*/
static void pick_next(SplitStruct *node, int n_entries, double *g1, double *g2,
    SplitStruct **choice, int *n_group, int n_dim)
{
  SplitStruct *cur = node;
  SplitStruct *end = node + n_entries;

  double max_diff = -DBL_MAX;

  for (; cur<end; ++cur)
  {
    double diff;
    double abs_diff;

    if (cur->n_node)
    {
      continue;
    }

    diff = mbr_join_square(g1, cur->coords, n_dim) -
      mbr_join_square(g2, cur->coords, n_dim);

    abs_diff = fabs(diff);
    if (abs_diff  > max_diff)
    {
      max_diff = abs_diff;
      *n_group = 1 + (diff > 0);
      *choice = cur;
    }
  }
}

/*
Mark not-in-group entries as n_group
*/
static void mark_all_entries(SplitStruct *node, int n_entries, int n_group)
{
  SplitStruct *cur = node;
  SplitStruct *end = node + n_entries;
  for (; cur<end; ++cur)
  {
    if (cur->n_node)
    {
      continue;
    }
    cur->n_node = n_group;
  }
}

static int split_maria_rtree_node(SplitStruct *node, int n_entries,
                   int all_size, /* Total key's size */
                   int key_size,
                   int min_size, /* Minimal group size */
                   int size1, int size2 /* initial group sizes */,
                   double **d_buffer, int n_dim)
{
  SplitStruct *cur;
  SplitStruct *a;
  SplitStruct *b;
  double *g1 = reserve_coords(d_buffer, n_dim);
  double *g2 = reserve_coords(d_buffer, n_dim);
  SplitStruct *next;
  int next_node;
  int i;
  SplitStruct *end = node + n_entries;

  if (all_size < min_size * 2)
  {
    return 1;
  }

  cur = node;
  for (; cur<end; ++cur)
  {
    cur->square = count_square(cur->coords, n_dim);
    cur->n_node = 0;
  }

  pick_seeds(node, n_entries, &a, &b, n_dim);
  a->n_node = 1;
  b->n_node = 2;


  copy_coords(g1, a->coords, n_dim);
  size1 += key_size;
  copy_coords(g2, b->coords, n_dim);
  size2 += key_size;


  for (i=n_entries - 2; i>0; --i)
  {
    if (all_size - (size2 + key_size) < min_size) /* Can't write into group 2 */
    {
      mark_all_entries(node, n_entries, 1);
      break;
    }

    if (all_size - (size1 + key_size) < min_size) /* Can't write into group 1 */
    {
      mark_all_entries(node, n_entries, 2);
      break;
    }

    pick_next(node, n_entries, g1, g2, &next, &next_node, n_dim);
    if (next_node == 1)
    {
      size1 += key_size;
      mbr_join(g1, next->coords, n_dim);
    }
    else
    {
      size2 += key_size;
      mbr_join(g2, next->coords, n_dim);
    }
    next->n_node = next_node;
  }

  return 0;
}

int maria_rtree_split_page(MARIA_HA *info, MARIA_KEYDEF *keyinfo, uchar *page, uchar *key,
                     uint key_length, my_off_t *new_page_offs)
{
  int n1, n2; /* Number of items in groups */

  SplitStruct *task;
  SplitStruct *cur;
  SplitStruct *stop;
  double *coord_buf;
  double *next_coord;
  double *old_coord;
  int n_dim;
  uchar *source_cur, *cur1, *cur2;
  uchar *new_page;
  int err_code= 0;
  uint nod_flag= _ma_test_if_nod(page);
  uint full_length= key_length + (nod_flag ? nod_flag :
                                  info->s->base.rec_reflength);
  int max_keys= (maria_getint(page)-2) / (full_length);

  n_dim = keyinfo->keysegs / 2;

  if (!(coord_buf= (double*) my_alloca(n_dim * 2 * sizeof(double) *
                                       (max_keys + 1 + 4) +
                                       sizeof(SplitStruct) * (max_keys + 1))))
    return -1;

  task= (SplitStruct *)(coord_buf + n_dim * 2 * (max_keys + 1 + 4));

  next_coord = coord_buf;

  stop = task + max_keys;
  source_cur = rt_PAGE_FIRST_KEY(page, nod_flag);

  for (cur = task; cur < stop; ++cur, source_cur = rt_PAGE_NEXT_KEY(source_cur,
       key_length, nod_flag))
  {
    cur->coords = reserve_coords(&next_coord, n_dim);
    cur->key = source_cur;
    maria_rtree_d_mbr(keyinfo->seg, source_cur, key_length, cur->coords);
  }

  cur->coords = reserve_coords(&next_coord, n_dim);
  maria_rtree_d_mbr(keyinfo->seg, key, key_length, cur->coords);
  cur->key = key;

  old_coord = next_coord;

  if (split_maria_rtree_node(task, max_keys + 1,
       maria_getint(page) + full_length + 2, full_length,
       rt_PAGE_MIN_SIZE(keyinfo->block_length),
       2, 2, &next_coord, n_dim))
  {
    err_code = 1;
    goto split_err;
  }

  if (!(new_page = (uchar*)my_alloca((uint)keyinfo->block_length)))
  {
    err_code= -1;
    goto split_err;
  }

  stop = task + (max_keys + 1);
  cur1 = rt_PAGE_FIRST_KEY(page, nod_flag);
  cur2 = rt_PAGE_FIRST_KEY(new_page, nod_flag);

  n1= n2 = 0;
  for (cur = task; cur < stop; ++cur)
  {
    uchar *to;
    if (cur->n_node == 1)
    {
      to = cur1;
      cur1 = rt_PAGE_NEXT_KEY(cur1, key_length, nod_flag);
      ++n1;
    }
    else
    {
      to = cur2;
      cur2 = rt_PAGE_NEXT_KEY(cur2, key_length, nod_flag);
      ++n2;
    }
    if (to != cur->key)
      memcpy(to - nod_flag, cur->key - nod_flag, full_length);
  }

  maria_putint(page, 2 + n1 * full_length, nod_flag);
  maria_putint(new_page, 2 + n2 * full_length, nod_flag);

  if ((*new_page_offs= _ma_new(info, keyinfo, DFLT_INIT_HITS)) ==
                                                               HA_OFFSET_ERROR)
    err_code= -1;
  else
    err_code= _ma_write_keypage(info, keyinfo, *new_page_offs,
                                DFLT_INIT_HITS, new_page);

  my_afree((byte*)new_page);

split_err:
  my_afree((byte*) coord_buf);
  return err_code;
}

#endif /*HAVE_RTREE_KEYS*/
