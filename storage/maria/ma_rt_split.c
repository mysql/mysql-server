/* Copyright (C) 2006 MySQL AB & Alexey Botchkov & MySQL Finland AB
   & TCX DataKonsult AB

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

#include "maria_def.h"
#include "trnman.h"
#include "ma_key_recover.h"

#ifdef HAVE_RTREE_KEYS

#include "ma_rt_index.h"
#include "ma_rt_key.h"
#include "ma_rt_mbr.h"

typedef struct
{
  double square;
  int n_node;
  const uchar *key;
  double *coords;
} SplitStruct;

inline static double *reserve_coords(double **d_buffer, int n_dim)
{
  double *coords= *d_buffer;
  (*d_buffer)+= n_dim * 2;
  return coords;
}

static void mbr_join(double *a, const double *b, int n_dim)
{
  double *end= a + n_dim * 2;
  do
  {
    if (a[0] > b[0])
      a[0]= b[0];

    if (a[1] < b[1])
      a[1]= b[1];

    a+= 2;
    b+= 2;
  } while (a != end);
}

/*
Counts the square of mbr which is a join of a and b
*/
static double mbr_join_square(const double *a, const double *b, int n_dim)
{
  const double *end= a + n_dim * 2;
  double square= 1.0;
  do
  {
    square *=
      ((a[1] < b[1]) ? b[1] : a[1]) - ((a[0] > b[0]) ? b[0] : a[0]);

    a+= 2;
    b+= 2;
  } while (a != end);

  return square;
}

static double count_square(const double *a, int n_dim)
{
  const double *end= a + n_dim * 2;
  double square= 1.0;
  do
  {
    square *= a[1] - a[0];
    a+= 2;
  } while (a != end);
  return square;
}

inline static void copy_coords(double *dst, const double *src, int n_dim)
{
  memcpy(dst, src, sizeof(double) * (n_dim * 2));
}

/**
  Select two nodes to collect group upon.

  Note that such function uses 'double' arithmetic so may behave differently
  on different platforms/builds. There are others in this file.
*/
static void pick_seeds(SplitStruct *node, int n_entries,
     SplitStruct **seed_a, SplitStruct **seed_b, int n_dim)
{
  SplitStruct *cur1;
  SplitStruct *lim1= node + (n_entries - 1);
  SplitStruct *cur2;
  SplitStruct *lim2= node + n_entries;

  double max_d= -DBL_MAX;
  double d;

  for (cur1= node; cur1 < lim1; cur1++)
  {
    for (cur2=cur1 + 1; cur2 < lim2; cur2++)
    {

      d= mbr_join_square(cur1->coords, cur2->coords, n_dim) - cur1->square -
          cur2->square;
      if (d > max_d)
      {
        max_d= d;
        *seed_a= cur1;
        *seed_b= cur2;
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
  SplitStruct *cur= node;
  SplitStruct *end= node + n_entries;

  double max_diff= -DBL_MAX;

  for (; cur < end; cur++)
  {
    double diff;
    double abs_diff;

    if (cur->n_node)
    {
      continue;
    }

    diff= mbr_join_square(g1, cur->coords, n_dim) -
      mbr_join_square(g2, cur->coords, n_dim);

    abs_diff= fabs(diff);
    if (abs_diff  > max_diff)
    {
      max_diff= abs_diff;
      *n_group= 1 + (diff > 0);
      *choice= cur;
    }
  }
}

/*
Mark not-in-group entries as n_group
*/
static void mark_all_entries(SplitStruct *node, int n_entries, int n_group)
{
  SplitStruct *cur= node;
  SplitStruct *end= node + n_entries;

  for (; cur < end; cur++)
  {
    if (cur->n_node)
    {
      continue;
    }
    cur->n_node= n_group;
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
  double *g1= reserve_coords(d_buffer, n_dim);
  double *g2= reserve_coords(d_buffer, n_dim);
  SplitStruct *next;
  int next_node;
  int i;
  SplitStruct *end= node + n_entries;
  LINT_INIT(a);
  LINT_INIT(b);
  LINT_INIT(next);
  LINT_INIT(next_node);

  if (all_size < min_size * 2)
  {
    return 1;
  }

  cur= node;
  for (; cur < end; cur++)
  {
    cur->square= count_square(cur->coords, n_dim);
    cur->n_node= 0;
  }

  pick_seeds(node, n_entries, &a, &b, n_dim);
  a->n_node= 1;
  b->n_node= 2;


  copy_coords(g1, a->coords, n_dim);
  size1+= key_size;
  copy_coords(g2, b->coords, n_dim);
  size2+= key_size;


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
      size1+= key_size;
      mbr_join(g1, next->coords, n_dim);
    }
    else
    {
      size2+= key_size;
      mbr_join(g2, next->coords, n_dim);
    }
    next->n_node= next_node;
  }

  return 0;
}


/**
  Logs key reorganization done in a split page (new page is logged elsewhere).

  The effect of a split on the split page is three changes:
  - some piece of the page move to different places inside this page (we are
  not interested here in the pieces which move to the new page)
  - the key is inserted into the page or not (could be in the new page)
  - page is shrunk
  All this is uniquely determined by a few parameters:
  - the key (starting at 'key-nod_flag', for 'full_length' bytes
  (maria_rtree_split_page() seems to depend on its parameters key&key_length
  but in fact it reads more (to the left: nod_flag, and to the right:
  full_length)
  - the binary content of the page
  - some variables in the share
  - double arithmetic, which is unpredictable from machine to machine and
  from build to build (see pick_seeds() above: it has a comparison between
  double-s 'if (d > max_d)' so the comparison can go differently from machine
  to machine or build to build, it has happened in real life).
  If one day we use precision-math instead of double-math, in GIS, then the
  last parameter would become constant accross machines and builds and we
  could some cheap logging: just log the few parameters above.
  Until then, we log the list of memcpy() operations (fortunately, we often do
  not have to log the source bytes, as they can be found in the page before
  applying the REDO; the only source bytes to log are the key), the key if it
  was inserted into this page, and the shrinking.

  @param  info             table
  @param  page             page's offset in the file
  @param  buff             content of the page (post-split)
  @param  key_with_nod_flag pointer to key-nod_flag
  @param  full_length      length of (key + (nod_flag (if node) or rowid (if
                           leaf)))
  @param  log_internal_copy encoded list of mempcy() operations done on
                           split page, having their source in the page
  @param  log_internal_copy_length length of above list, in bytes
  @param  log_key_copy     operation describing the key's copy, or NULL if the
                           inserted key was not put into the page (was put in
                           new page, so does not have to be logged here)
  @param  length_diff      by how much the page has shrunk during split
*/

static my_bool _ma_log_rt_split(MARIA_PAGE *page,
                                const uchar *key_with_nod_flag,
                                uint full_length,
                                const uchar *log_internal_copy,
                                uint log_internal_copy_length,
                                const uchar *log_key_copy,
                                uint length_diff)
{
  MARIA_HA    *info=  page->info;
  MARIA_SHARE *share= info->s;
  LSN lsn;
  uchar log_data[FILEID_STORE_SIZE + PAGE_STORE_SIZE + 1 + 2 + 1 + 2 + 2 + 7],
    *log_pos;
  LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 6];
  uint translog_parts, extra_length= 0;
  my_off_t page_pos;
  DBUG_ENTER("_ma_log_rt_split");
  DBUG_PRINT("enter", ("page: %lu", (ulong) page));

  DBUG_ASSERT(share->now_transactional);
  page_pos= page->pos / share->block_size;
  page_store(log_data + FILEID_STORE_SIZE, page_pos);
  log_pos= log_data+ FILEID_STORE_SIZE + PAGE_STORE_SIZE;
  log_pos[0]= KEY_OP_DEL_SUFFIX;
  log_pos++;
  DBUG_ASSERT((int)length_diff > 0);
  int2store(log_pos, length_diff);
  log_pos+= 2;
  log_pos[0]= KEY_OP_MULTI_COPY;
  log_pos++;
  int2store(log_pos, full_length);
  log_pos+= 2;
  int2store(log_pos, log_internal_copy_length);
  log_pos+= 2;
  log_array[TRANSLOG_INTERNAL_PARTS + 0].str=    log_data;
  log_array[TRANSLOG_INTERNAL_PARTS + 0].length= sizeof(log_data) - 7;
  log_array[TRANSLOG_INTERNAL_PARTS + 1].str=    log_internal_copy;
  log_array[TRANSLOG_INTERNAL_PARTS + 1].length= log_internal_copy_length;
  translog_parts= 2;
  if (log_key_copy != NULL) /* need to store key into record */
  {
    log_array[TRANSLOG_INTERNAL_PARTS + 2].str=    log_key_copy;
    log_array[TRANSLOG_INTERNAL_PARTS + 2].length= 1 + 2 + 1 + 2;
    log_array[TRANSLOG_INTERNAL_PARTS + 3].str=    key_with_nod_flag;
    log_array[TRANSLOG_INTERNAL_PARTS + 3].length= full_length;
    extra_length= 1 + 2 + 1 + 2 + full_length;
    translog_parts+= 2;
  }

  _ma_log_key_changes(page,
                      log_array + TRANSLOG_INTERNAL_PARTS + translog_parts,
                      log_pos, &extra_length, &translog_parts);
  /* Remember new page length for future log entires for same page */
  page->org_size= page->size;

  if (translog_write_record(&lsn, LOGREC_REDO_INDEX,
                            info->trn, info,
                            (translog_size_t) ((log_pos - log_data) +
                                               log_internal_copy_length +
                                               extra_length),
                            TRANSLOG_INTERNAL_PARTS + translog_parts,
                            log_array, log_data, NULL))
    DBUG_RETURN(1);
  DBUG_RETURN(0);
}

/**
   0 ok; the created page is put into page cache; the shortened one is not (up
   to the caller to do it)
   1 or -1: error.
   If new_page_offs==NULL, won't create new page (for redo phase).
*/

int maria_rtree_split_page(const MARIA_KEY *key, MARIA_PAGE *page,
                           my_off_t *new_page_offs)
{
  MARIA_HA   *info= page->info;
  MARIA_SHARE *share= info->s;
  const my_bool transactional= share->now_transactional;
  int n1, n2; /* Number of items in groups */
  SplitStruct *task;
  SplitStruct *cur;
  SplitStruct *stop;
  double *coord_buf;
  double *next_coord;
  double *old_coord;
  int n_dim;
  uchar *source_cur, *cur1, *cur2;
  uchar *new_page_buff, *log_internal_copy, *log_internal_copy_ptr,
    *log_key_copy= NULL;
  int err_code= 0;
  uint new_page_length;
  uint nod_flag= page->node;
  uint org_length= page->size;
  uint full_length= key->data_length + (nod_flag ? nod_flag :
                                        key->ref_length);
  uint key_data_length= key->data_length;
  int max_keys= ((org_length - share->keypage_header) / (full_length));
  MARIA_PINNED_PAGE tmp_page_link, *page_link= &tmp_page_link;
  MARIA_KEYDEF *keyinfo= key->keyinfo;
  DBUG_ENTER("maria_rtree_split_page");
  DBUG_PRINT("rtree", ("splitting block"));

  n_dim= keyinfo->keysegs / 2;

  if (!(coord_buf= (double*) my_alloca(n_dim * 2 * sizeof(double) *
                                       (max_keys + 1 + 4) +
                                       sizeof(SplitStruct) * (max_keys + 1))))
    DBUG_RETURN(-1); /* purecov: inspected */

  task= (SplitStruct *)(coord_buf + n_dim * 2 * (max_keys + 1 + 4));

  next_coord= coord_buf;

  stop= task + max_keys;
  source_cur= rt_PAGE_FIRST_KEY(share, page->buff, nod_flag);

  for (cur= task;
       cur < stop;
       cur++, source_cur= rt_PAGE_NEXT_KEY(share, source_cur, key_data_length,
                                           nod_flag))
  {
    cur->coords= reserve_coords(&next_coord, n_dim);
    cur->key= source_cur;
    maria_rtree_d_mbr(keyinfo->seg, source_cur, key_data_length, cur->coords);
  }

  cur->coords= reserve_coords(&next_coord, n_dim);
  maria_rtree_d_mbr(keyinfo->seg, key->data, key_data_length, cur->coords);
  cur->key= key->data;

  old_coord= next_coord;

  if (split_maria_rtree_node(task, max_keys + 1,
                             page->size + full_length + 2,
                             full_length,
       rt_PAGE_MIN_SIZE(keyinfo->block_length),
       2, 2, &next_coord, n_dim))
  {
    err_code= 1;
    goto split_err;
  }

  /* Allocate buffer for new page and piece of log record */
  if (!(new_page_buff= (uchar*) my_alloca((uint)keyinfo->block_length +
                                          (transactional ?
                                           (max_keys * (2 + 2) +
                                            1 + 2 + 1 + 2) : 0))))
  {
    err_code= -1;
    goto split_err;
  }
  log_internal_copy= log_internal_copy_ptr= new_page_buff +
    keyinfo->block_length;
  bzero(new_page_buff, share->block_size);

  stop= task + (max_keys + 1);
  cur1= rt_PAGE_FIRST_KEY(share, page->buff, nod_flag);
  cur2= rt_PAGE_FIRST_KEY(share, new_page_buff, nod_flag);

  n1= n2= 0;
  for (cur= task; cur < stop; cur++)
  {
    uchar *to;
    const uchar *cur_key= cur->key;
    my_bool log_this_change;
    DBUG_ASSERT(log_key_copy == NULL);
    if (cur->n_node == 1)
    {
      to= cur1;
      cur1= rt_PAGE_NEXT_KEY(share, cur1, key_data_length, nod_flag);
      n1++;
      log_this_change= transactional;
    }
    else
    {
      to= cur2;
      cur2= rt_PAGE_NEXT_KEY(share, cur2, key_data_length, nod_flag);
      n2++;
      log_this_change= FALSE;
    }
    if (to != cur_key)
    {
      uchar *to_with_nod_flag= to - nod_flag;
      const uchar *cur_key_with_nod_flag= cur_key - nod_flag;
      memcpy(to_with_nod_flag, cur_key_with_nod_flag, full_length);
      if (log_this_change)
      {
        uint to_with_nod_flag_offs= to_with_nod_flag - page->buff;
        if (likely(cur_key != key->data))
        {
          /* this memcpy() is internal to the page (source in the page) */
          uint cur_key_with_nod_flag_offs= cur_key_with_nod_flag - page->buff;
          int2store(log_internal_copy_ptr, to_with_nod_flag_offs);
          log_internal_copy_ptr+= 2;
          int2store(log_internal_copy_ptr, cur_key_with_nod_flag_offs);
          log_internal_copy_ptr+= 2;
        }
        else
        {
          /* last iteration, and this involves *key: source is external */
          log_key_copy= log_internal_copy_ptr;
          log_key_copy[0]= KEY_OP_OFFSET;
          int2store(log_key_copy + 1, to_with_nod_flag_offs);
          log_key_copy[3]= KEY_OP_CHANGE;
          int2store(log_key_copy + 4, full_length);
          /* _ma_log_rt_split() will store *key, right after */
        }
      }
    }
  }
  { /* verify that above loop didn't touch header bytes */
    uint i;
    for (i= 0; i < share->keypage_header; i++)
      DBUG_ASSERT(new_page_buff[i]==0);
  }

  if (nod_flag)
    _ma_store_keypage_flag(share, new_page_buff, KEYPAGE_FLAG_ISNOD);
  _ma_store_keynr(share, new_page_buff, keyinfo->key_nr);
  new_page_length= share->keypage_header + n2 * full_length;
  _ma_store_page_used(share, new_page_buff, new_page_length);
  page->size= share->keypage_header + n1 * full_length;
  page_store_size(share, page);

  if ((*new_page_offs= _ma_new(info, DFLT_INIT_HITS, &page_link)) ==
      HA_OFFSET_ERROR)
    err_code= -1;
  else
  {
    MARIA_PAGE new_page;
    _ma_page_setup(&new_page, info, keyinfo, *new_page_offs, new_page_buff);

    if (transactional &&
        ( /* log change to split page */
         _ma_log_rt_split(page, key->data - nod_flag,
                          full_length, log_internal_copy,
                          log_internal_copy_ptr - log_internal_copy,
                          log_key_copy, org_length - page->size) ||
         /* and to new page */
         _ma_log_new(&new_page, 0)))
      err_code= -1;

    if (_ma_write_keypage(&new_page, page_link->write_lock,
                          DFLT_INIT_HITS))
      err_code= -1;
  }
  DBUG_PRINT("rtree", ("split new block: %lu", (ulong) *new_page_offs));

  my_afree(new_page_buff);
split_err:
  my_afree(coord_buf);
  DBUG_RETURN(err_code);
}

#endif /*HAVE_RTREE_KEYS*/
