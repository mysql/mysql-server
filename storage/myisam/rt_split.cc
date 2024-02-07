/* Copyright (c) 2002, 2024, Oracle and/or its affiliates.

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

#include <float.h>
#include <string.h>
#include <sys/types.h>
#include <cmath>

#include "my_base.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "myisam.h"
#include "storage/myisam/myisamdef.h"
#include "storage/myisam/rt_index.h"
#include "storage/myisam/rt_mbr.h"

typedef struct {
  double square;
  int n_node;
  uchar *key;
  double *coords;
} SplitStruct;

inline static double *reserve_coords(double **d_buffer, int n_dim) {
  double *coords = *d_buffer;
  (*d_buffer) += n_dim * 2;
  return coords;
}

static void mbr_join(double *a, const double *b, int n_dim) {
  double *end = a + n_dim * 2;
  do {
    if (a[0] > b[0]) a[0] = b[0];

    if (a[1] < b[1]) a[1] = b[1];

    a += 2;
    b += 2;
  } while (a != end);
}

/*
Counts the square of mbr which is a join of a and b
*/
static double mbr_join_square(const double *a, const double *b, int n_dim) {
  const double *end = a + n_dim * 2;
  double square = 1.0;
  do {
    square *= ((a[1] < b[1]) ? b[1] : a[1]) - ((a[0] > b[0]) ? b[0] : a[0]);

    a += 2;
    b += 2;
  } while (a != end);

  /* Check for infinity or NaN */
  if (!std::isfinite(square)) square = DBL_MAX;

  return square;
}

static double count_square(const double *a, int n_dim) {
  const double *end = a + n_dim * 2;
  double square = 1.0;
  do {
    square *= a[1] - a[0];
    a += 2;
  } while (a != end);
  return square;
}

inline static void copy_coords(double *dst, const double *src, int n_dim) {
  memcpy(dst, src, sizeof(double) * (n_dim * 2));
}

/*
Select two nodes to collect group upon
*/
static void pick_seeds(SplitStruct *node, int n_entries, SplitStruct **seed_a,
                       SplitStruct **seed_b, int n_dim) {
  SplitStruct *cur1;
  SplitStruct *lim1 = node + (n_entries - 1);
  SplitStruct *cur2;
  SplitStruct *lim2 = node + n_entries;

  double max_d = -DBL_MAX;
  double d;

  *seed_a = node;
  *seed_b = node + 1;

  for (cur1 = node; cur1 < lim1; ++cur1) {
    for (cur2 = cur1 + 1; cur2 < lim2; ++cur2) {
      d = mbr_join_square(cur1->coords, cur2->coords, n_dim) - cur1->square -
          cur2->square;
      if (d > max_d) {
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
                      SplitStruct **choice, int *n_group, int n_dim) {
  SplitStruct *cur = node;
  SplitStruct *end = node + n_entries;

  double max_diff = -DBL_MAX;

  for (; cur < end; ++cur) {
    double diff;
    double abs_diff;

    if (cur->n_node) {
      continue;
    }

    diff = mbr_join_square(g1, cur->coords, n_dim) -
           mbr_join_square(g2, cur->coords, n_dim);

    abs_diff = std::fabs(diff);
    if (abs_diff > max_diff) {
      max_diff = abs_diff;
      *n_group = 1 + (diff > 0);
      *choice = cur;
    }
  }
}

/*
Mark not-in-group entries as n_group
*/
static void mark_all_entries(SplitStruct *node, int n_entries, int n_group) {
  SplitStruct *cur = node;
  SplitStruct *end = node + n_entries;
  for (; cur < end; ++cur) {
    if (cur->n_node) {
      continue;
    }
    cur->n_node = n_group;
  }
}

static int split_rtree_node(SplitStruct *node, int n_entries,
                            int all_size,               /* Total key's size */
                            int key_size, int min_size, /* Minimal group size */
                            int size1, int size2 /* initial group sizes */,
                            double **d_buffer, int n_dim) {
  SplitStruct *cur;
  SplitStruct *a = nullptr, *b = nullptr;
  double *g1 = reserve_coords(d_buffer, n_dim);
  double *g2 = reserve_coords(d_buffer, n_dim);
  SplitStruct *next = nullptr;
  int next_node = 0;
  int i;
  SplitStruct *end = node + n_entries;

  if (all_size < min_size * 2) {
    return 1;
  }

  cur = node;
  for (; cur < end; ++cur) {
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

  for (i = n_entries - 2; i > 0; --i) {
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
    if (next_node == 1) {
      size1 += key_size;
      mbr_join(g1, next->coords, n_dim);
    } else {
      size2 += key_size;
      mbr_join(g2, next->coords, n_dim);
    }
    next->n_node = next_node;
  }

  return 0;
}

int rtree_split_page(MI_INFO *info, MI_KEYDEF *keyinfo, uchar *page, uchar *key,
                     uint key_length, my_off_t *new_page_offs) {
  int n1, n2; /* Number of items in groups */

  SplitStruct *task;
  SplitStruct *cur;
  SplitStruct *stop;
  double *coord_buf;
  double *next_coord;
  int n_dim;
  uchar *source_cur, *cur1, *cur2;
  uchar *new_page = info->buff;
  int err_code = 0;
  uint nod_flag = mi_test_if_nod(page);
  uint full_length =
      key_length + (nod_flag ? nod_flag : info->s->base.rec_reflength);
  int max_keys = (mi_getint(page) - 2) / (full_length);
  DBUG_TRACE;
  DBUG_PRINT("rtree", ("splitting block"));

  n_dim = keyinfo->keysegs / 2;

  if (!(coord_buf = (double *)my_alloca(n_dim * 2 * sizeof(double) *
                                            (max_keys + 1 + 4) +
                                        sizeof(SplitStruct) * (max_keys + 1))))
    return -1; /* purecov: inspected */

  task = (SplitStruct *)(coord_buf + n_dim * 2 * (max_keys + 1 + 4));

  next_coord = coord_buf;

  stop = task + max_keys;
  source_cur = rt_PAGE_FIRST_KEY(page, nod_flag);

  for (cur = task; cur < stop;
       ++cur, source_cur = rt_PAGE_NEXT_KEY(source_cur, key_length, nod_flag)) {
    cur->coords = reserve_coords(&next_coord, n_dim);
    cur->key = source_cur;
    rtree_d_mbr(keyinfo->seg, source_cur, key_length, cur->coords);
  }

  cur->coords = reserve_coords(&next_coord, n_dim);
  rtree_d_mbr(keyinfo->seg, key, key_length, cur->coords);
  cur->key = key;

  if (split_rtree_node(task, max_keys + 1, mi_getint(page) + full_length + 2,
                       full_length, rt_PAGE_MIN_SIZE(keyinfo->block_length), 2,
                       2, &next_coord, n_dim)) {
    err_code = 1;
    goto split_err;
  }

  info->buff_used = true;
  stop = task + (max_keys + 1);
  cur1 = rt_PAGE_FIRST_KEY(page, nod_flag);
  cur2 = rt_PAGE_FIRST_KEY(new_page, nod_flag);

  n1 = n2 = 0;
  for (cur = task; cur < stop; ++cur) {
    uchar *to;
    if (cur->n_node == 1) {
      to = cur1;
      cur1 = rt_PAGE_NEXT_KEY(cur1, key_length, nod_flag);
      ++n1;
    } else {
      to = cur2;
      cur2 = rt_PAGE_NEXT_KEY(cur2, key_length, nod_flag);
      ++n2;
    }
    if (to != cur->key) memcpy(to - nod_flag, cur->key - nod_flag, full_length);
  }

  mi_putint(page, 2 + n1 * full_length, nod_flag);
  mi_putint(new_page, 2 + n2 * full_length, nod_flag);

  if ((*new_page_offs = _mi_new(info, keyinfo, DFLT_INIT_HITS)) ==
      HA_OFFSET_ERROR)
    err_code = -1;
  else
    err_code = _mi_write_keypage(info, keyinfo, *new_page_offs, DFLT_INIT_HITS,
                                 new_page);
  DBUG_PRINT("rtree", ("split new block: %lu", (ulong)*new_page_offs));

split_err:
  return err_code;
}
