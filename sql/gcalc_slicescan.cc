/* Copyright (c) 2000, 2010 Oracle and/or its affiliates. All rights reserved.
   Copyright (C) 2011 Monty Program Ab.

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


#include <my_global.h>
#include <my_sys.h>
#include <m_string.h>

#ifdef HAVE_SPATIAL

#include "gcalc_slicescan.h"


#define PH_DATA_OFFSET 8
#define coord_to_float(d) ((double) d)
#define coord_eq(a, b) (a == b)

typedef int (*sc_compare_func)(const void*, const void*);

#define LS_LIST_ITEM Gcalc_dyn_list::Item
#define LS_COMPARE_FUNC_DECL sc_compare_func compare,
#define LS_COMPARE_FUNC_CALL(list_el1, list_el2) (*compare)(list_el1, list_el2)
#define LS_NEXT(A) (A)->next
#define LS_SET_NEXT(A,val) (A)->next= val
#define LS_P_NEXT(A) &(A)->next
#define LS_NAME sort_list
#define LS_SCOPE static
#define LS_STRUCT_NAME sort_list_stack_struct
#include "plistsort.c"


#define GCALC_COORD_MINUS     0x80000000
#define FIRST_DIGIT(d) ((d) & 0x7FFFFFFF)
#define GCALC_SIGN(d)  ((d) & 0x80000000)

static Gcalc_scan_iterator::point *eq_sp(const Gcalc_heap::Info *pi)
{
  GCALC_DBUG_ASSERT(pi->type == Gcalc_heap::nt_eq_node);
  return (Gcalc_scan_iterator::point *) pi->eq_data;
}


static Gcalc_scan_iterator::intersection_info *i_data(const Gcalc_heap::Info *pi)
{
  GCALC_DBUG_ASSERT(pi->type == Gcalc_heap::nt_intersection);
  return (Gcalc_scan_iterator::intersection_info *) pi->intersection_data;
}


#ifndef GCALC_DBUG_OFF

int gcalc_step_counter= 0;

void GCALC_DBUG_CHECK_COUNTER()
{
  if (++gcalc_step_counter == 0)
    GCALC_DBUG_PRINT(("step_counter_0"));
  else
    GCALC_DBUG_PRINT(("%d step_counter", gcalc_step_counter));
}


const char *gcalc_ev_name(int ev)
{
  switch (ev)
  {
    case scev_none:
      return "n";
    case scev_thread:
      return "t";
    case scev_two_threads:
      return "tt";
    case scev_end:
      return "e";
    case scev_two_ends:
      return "ee";
    case scev_intersection:
      return "i";
    case scev_point:
      return "p";
    case scev_single_point:
      return "sp";
    default:;
  };
  GCALC_DBUG_ASSERT(0);
  return "unk";
}


static int gcalc_pi_str(char *str, const Gcalc_heap::Info *pi, const char *postfix)
{
  return sprintf(str, "%s %d %d | %s %d %d%s",
                     GCALC_SIGN(pi->ix[0]) ? "-":"", FIRST_DIGIT(pi->ix[0]),pi->ix[1],
                     GCALC_SIGN(pi->iy[0]) ? "-":"", FIRST_DIGIT(pi->iy[0]),pi->iy[1],
                     postfix);

}


static void GCALC_DBUG_PRINT_PI(const Gcalc_heap::Info *pi)
{
  char buf[128];
  int n_buf;
  if (pi->type == Gcalc_heap::nt_intersection)
  {
    const Gcalc_scan_iterator::intersection_info *ic= i_data(pi);

    GCALC_DBUG_PRINT(("intersection point %d %d",
                      ic->edge_a->thread, ic->edge_b->thread));
    return;
  }
  if (pi->type == Gcalc_heap::nt_eq_node)
  {
    const Gcalc_scan_iterator::point *e= eq_sp(pi);
    GCALC_DBUG_PRINT(("eq point %d", e->thread));
    return;
  }
  n_buf= gcalc_pi_str(buf, pi, "");
  buf[n_buf]= 0;
  GCALC_DBUG_PRINT(("%s", buf));
}


static void GCALC_DBUG_PRINT_SLICE(const char *header,
                                   const Gcalc_scan_iterator::point *slice)
{
  int nbuf;
  char buf[1024];
  nbuf= strlen(header);
  strcpy(buf, header);
  for (; slice; slice= slice->get_next())
  {
    int lnbuf= nbuf;
    lnbuf+= sprintf(buf + lnbuf, "%d\t", slice->thread);
    lnbuf+= sprintf(buf + lnbuf, "%s\t", gcalc_ev_name(slice->event));

    lnbuf+= gcalc_pi_str(buf + lnbuf, slice->pi, "\t");
    if (slice->is_bottom())
      lnbuf+= sprintf(buf+lnbuf, "bt\t");
    else
      lnbuf+= gcalc_pi_str(buf+lnbuf, slice->next_pi, "\t");
    buf[lnbuf]= 0;
    GCALC_DBUG_PRINT(("%s", buf));
  }
}


#else
#define GCALC_DBUG_CHECK_COUNTER()              do { } while(0)
#define GCALC_DBUG_PRINT_PI(pi)                 do { } while(0)
#define GCALC_DBUG_PRINT_SLICE(a, b)            do { } while(0)
#define GCALC_DBUG_PRINT_INTERSECTIONS(a)       do { } while(0)
#define GCALC_DBUG_PRINT_STATE(a)               do { } while(0)
#endif /*GCALC_DBUG_OFF*/


Gcalc_dyn_list::Gcalc_dyn_list(size_t blk_size, size_t sizeof_item):
  m_blk_size(blk_size - ALLOC_ROOT_MIN_BLOCK_SIZE),
  m_sizeof_item(ALIGN_SIZE(sizeof_item)),
  m_points_per_blk((m_blk_size - PH_DATA_OFFSET) / m_sizeof_item),
  m_blk_hook(&m_first_blk),
  m_free(NULL),
  m_keep(NULL)
{}


void Gcalc_dyn_list::format_blk(void* block)
{
  Item *pi_end, *cur_pi, *first_pi;
  GCALC_DBUG_ASSERT(m_free == NULL);
  first_pi= cur_pi= (Item *)(((char *)block) + PH_DATA_OFFSET);
  pi_end= ptr_add(first_pi, m_points_per_blk - 1);
  do {
    cur_pi= cur_pi->next= ptr_add(cur_pi, 1);
  } while (cur_pi<pi_end);
  cur_pi->next= m_free;
  m_free= first_pi;
}


Gcalc_dyn_list::Item *Gcalc_dyn_list::alloc_new_blk()
{
  void *new_block= my_malloc(m_blk_size, MYF(MY_WME));
  if (!new_block)
    return NULL;
  *m_blk_hook= new_block;
  m_blk_hook= (void**)new_block;
  format_blk(new_block);
  return new_item();
}


static void free_blk_list(void *list)
{
  void *next_blk;
  while (list)
  {
    next_blk= *((void **)list);
    my_free(list);
    list= next_blk;
  }
}


void Gcalc_dyn_list::cleanup()
{
  *m_blk_hook= NULL;
  free_blk_list(m_first_blk);
  m_first_blk= NULL;
  m_blk_hook= &m_first_blk;
  m_free= NULL;
}


Gcalc_dyn_list::~Gcalc_dyn_list()
{
  cleanup();
}


void Gcalc_dyn_list::reset()
{
  *m_blk_hook= NULL;
  if (m_first_blk)
  {
    free_blk_list(*((void **)m_first_blk));
    m_blk_hook= (void**)m_first_blk;
    m_free= NULL;
    format_blk(m_first_blk);
  }
}


/* Internal coordinate operations implementations */

void gcalc_set_zero(Gcalc_internal_coord *d, int d_len)
{
  do
  {
    d[--d_len]= 0;
  } while (d_len); 
}


int gcalc_is_zero(const Gcalc_internal_coord *d, int d_len)
{
  do
  {
    if (d[--d_len] != 0)
      return 0;
  } while (d_len); 
  return 1;
}


#ifdef GCALC_CHECK_WITH_FLOAT
static double *gcalc_coord_extent= NULL;

long double gcalc_get_double(const Gcalc_internal_coord *d, int d_len)
{
  int n= 1;
  long double res= (long double) FIRST_DIGIT(d[0]);
  do
  {
    res*= (long double) GCALC_DIG_BASE;
    res+= (long double) d[n];
  } while(++n < d_len);

  n= 0;
  do
  {
    if ((n & 1) && gcalc_coord_extent)
      res/= *gcalc_coord_extent;
  } while(++n < d_len);

  if (GCALC_SIGN(d[0]))
    res*= -1.0;
  return res;
}
#endif /*GCALC_CHECK_WITH_FLOAT*/


static void do_add(Gcalc_internal_coord *result, int result_len,
                   const Gcalc_internal_coord *a,
                   const Gcalc_internal_coord *b)
{
  int n_digit= result_len-1;
  gcalc_digit_t carry= 0;

  do
  {
    if ((result[n_digit]=
          a[n_digit] + b[n_digit] + carry) >= GCALC_DIG_BASE)
    {
      carry= 1;
      result[n_digit]-= GCALC_DIG_BASE;
    }
    else
      carry= 0;
  } while (--n_digit);

  result[0]= (a[0] + FIRST_DIGIT(b[0]) + carry);

  GCALC_DBUG_ASSERT(FIRST_DIGIT(result[0]) < GCALC_DIG_BASE);
}


static void do_sub(Gcalc_internal_coord *result, int result_len,
                   const Gcalc_internal_coord *a,
                   const Gcalc_internal_coord *b)
{
  int n_digit= result_len-1;
  gcalc_digit_t carry= 0;
  gcalc_digit_t cur_b, cur_a;

  do
  {
    cur_b= b[n_digit] + carry;
    cur_a= a[n_digit];
    if (cur_a < cur_b)
    {
      carry= 1;
      result[n_digit]= (GCALC_DIG_BASE - cur_b) + cur_a;
    }
    else
    {
      carry= 0;
      result[n_digit]= cur_a - cur_b;
    }
  } while (--n_digit);


  result[0]= a[0] - FIRST_DIGIT(b[0]) - carry;

  GCALC_DBUG_ASSERT(FIRST_DIGIT(a[0]) >= FIRST_DIGIT(b[0]) + carry);
  GCALC_DBUG_ASSERT(!gcalc_is_zero(result, result_len));
}
/*
static void do_sub(Gcalc_internal_coord *result, int result_len,
                   const Gcalc_internal_coord *a,
                   const Gcalc_internal_coord *b)
{
  int n_digit= result_len-1;
  gcalc_digit_t carry= 0;

  do
  {
    if ((result[n_digit]= a[n_digit] - b[n_digit] - carry) < 0)
    {
      carry= 1;
      result[n_digit]+= GCALC_DIG_BASE;
    }
    else
      carry= 0;
  } while (--n_digit);


  result[0]= a[0] - FIRST_DIGIT(b[0]) - carry;

  GCALC_DBUG_ASSERT(FIRST_DIGIT(a[0]) - FIRST_DIGIT(b[0]) - carry >= 0);
  GCALC_DBUG_ASSERT(!gcalc_is_zero(result, result_len));
}
*/

static int do_cmp(const Gcalc_internal_coord *a,
                  const Gcalc_internal_coord *b, int len)
{
  int n_digit= 1;

  if ((FIRST_DIGIT(a[0]) != FIRST_DIGIT(b[0])))
    return FIRST_DIGIT(a[0]) > FIRST_DIGIT(b[0]) ? 1 : -1;

  do
  {
    if ((a[n_digit] != b[n_digit]))
      return a[n_digit] > b[n_digit] ? 1 : -1;
  } while (++n_digit < len);

  return 0;
}


#ifdef GCALC_CHECK_WITH_FLOAT
static int de_weak_check(long double a, long double b, long double ex)
{
  long double d= a - b;
  if (d < ex && d > -ex)
    return 1;

  d/= fabsl(a) + fabsl(b);
  if (d < ex && d > -ex)
    return 1;
  return 0;
}

static int de_check(long double a, long double b)
{
  return de_weak_check(a, b, (long double) 1e-9);
}
#endif /*GCALC_CHECK_WITH_FLOAT*/


void gcalc_mul_coord(Gcalc_internal_coord *result, int result_len,
                     const Gcalc_internal_coord *a, int a_len,
                     const Gcalc_internal_coord *b, int b_len)
{
  GCALC_DBUG_ASSERT(result_len == a_len + b_len);
  GCALC_DBUG_ASSERT(a_len >= b_len);
  int n_a, n_b, n_res;
  gcalc_digit_t carry= 0;

  gcalc_set_zero(result, result_len);

  n_a= a_len - 1;
  do
  {
    gcalc_coord2 cur_a= n_a ? a[n_a] : FIRST_DIGIT(a[0]);
    n_b= b_len - 1;
    do
    {
      gcalc_coord2 cur_b= n_b ? b[n_b] : FIRST_DIGIT(b[0]);
      gcalc_coord2 mul= cur_a * cur_b + carry + result[n_a + n_b + 1];
      result[n_a + n_b + 1]= mul % GCALC_DIG_BASE;
      carry= (gcalc_digit_t) (mul / (gcalc_coord2) GCALC_DIG_BASE);
    } while (n_b--);
    if (carry)
    {
      for (n_res= n_a; (result[n_res]+= carry) >= GCALC_DIG_BASE;
           n_res--)
      {
        result[n_res]-= GCALC_DIG_BASE;
        carry= 1;
      }
      carry= 0;
    }
  } while (n_a--);
  if (!gcalc_is_zero(result, result_len))
    result[0]|= GCALC_SIGN(a[0] ^ b[0]);
#ifdef GCALC_CHECK_WITH_FLOAT
  GCALC_DBUG_ASSERT(de_check(gcalc_get_double(a, a_len) *
                               gcalc_get_double(b, b_len),
                       gcalc_get_double(result, result_len)));
#endif /*GCALC_CHECK_WITH_FLOAT*/
}


inline void gcalc_mul_coord1(Gcalc_coord1 result,
                             const Gcalc_coord1 a, const Gcalc_coord1 b)
{
  return gcalc_mul_coord(result, GCALC_COORD_BASE2,
                         a, GCALC_COORD_BASE, b, GCALC_COORD_BASE);
}


void gcalc_add_coord(Gcalc_internal_coord *result, int result_len,
                     const Gcalc_internal_coord *a,
                     const Gcalc_internal_coord *b)
{
  if (GCALC_SIGN(a[0]) == GCALC_SIGN(b[0]))
    do_add(result, result_len, a, b);
  else
  {
    int cmp_res= do_cmp(a, b, result_len);
    if (cmp_res == 0)
      gcalc_set_zero(result, result_len);
    else if (cmp_res > 0)
      do_sub(result, result_len, a, b);
    else
      do_sub(result, result_len, b, a);
  }
#ifdef GCALC_CHECK_WITH_FLOAT
  GCALC_DBUG_ASSERT(de_check(gcalc_get_double(a, result_len) +
                               gcalc_get_double(b, result_len),
                       gcalc_get_double(result, result_len)));
#endif /*GCALC_CHECK_WITH_FLOAT*/
}


void gcalc_sub_coord(Gcalc_internal_coord *result, int result_len,
                     const Gcalc_internal_coord *a,
                     const Gcalc_internal_coord *b)
{
  if (GCALC_SIGN(a[0] ^ b[0]))
    do_add(result, result_len, a, b);
  else
  {
    int cmp_res= do_cmp(a, b, result_len);
    if (cmp_res == 0)
      gcalc_set_zero(result, result_len);
    else if (cmp_res > 0)
      do_sub(result, result_len, a, b);
    else
    {
      do_sub(result, result_len, b, a);
      result[0]^= GCALC_COORD_MINUS;
    }
  }
#ifdef GCALC_CHECK_WITH_FLOAT
  GCALC_DBUG_ASSERT(de_check(gcalc_get_double(a, result_len) -
                               gcalc_get_double(b, result_len),
                       gcalc_get_double(result, result_len)));
#endif /*GCALC_CHECK_WITH_FLOAT*/
}


inline void gcalc_sub_coord1(Gcalc_coord1 result,
                             const Gcalc_coord1 a, const Gcalc_coord1 b)
{
  return gcalc_sub_coord(result, GCALC_COORD_BASE, a, b);
}


int gcalc_cmp_coord(const Gcalc_internal_coord *a,
                    const Gcalc_internal_coord *b, int len)
{
  int n_digit= 0;
  int result= 0;

  do
  {
    if (a[n_digit] == b[n_digit])
    {
      n_digit++;
      continue;
    }
    if (a[n_digit] > b[n_digit])
      result= GCALC_SIGN(a[0]) ? -1 : 1;
    else
      result= GCALC_SIGN(b[0]) ? 1 : -1;
    break;

  } while (n_digit < len);

#ifdef GCALC_CHECK_WITH_FLOAT
  if (result == 0)
    GCALC_DBUG_ASSERT(de_check(gcalc_get_double(a, len),
                                 gcalc_get_double(b, len)));
  else if (result == 1)
    GCALC_DBUG_ASSERT(de_check(gcalc_get_double(a, len),
                                 gcalc_get_double(b, len)) ||
                gcalc_get_double(a, len) > gcalc_get_double(b, len));
  else
    GCALC_DBUG_ASSERT(de_check(gcalc_get_double(a, len),
                                 gcalc_get_double(b, len)) ||
                gcalc_get_double(a, len) < gcalc_get_double(b, len));
#endif /*GCALC_CHECK_WITH_FLOAT*/
  return result;
}


#define gcalc_cmp_coord1(a, b) gcalc_cmp_coord(a, b, GCALC_COORD_BASE)

int gcalc_set_double(Gcalc_internal_coord *c, double d, double ext)
{
  int sign;
  double ds= d * ext;
  if ((sign= ds < 0))
    ds= -ds;
  c[0]= (gcalc_digit_t) (ds / (double) GCALC_DIG_BASE);
  c[1]= (gcalc_digit_t) (ds - ((double) c[0]) * (double) GCALC_DIG_BASE);
  if (c[1] >= GCALC_DIG_BASE)
  {
    c[1]= 0;
    c[0]++;
  }
  if (sign && (c[0] | c[1]))
    c[0]|= GCALC_COORD_MINUS;
#ifdef GCALC_CHECK_WITH_FLOAT
  GCALC_DBUG_ASSERT(de_check(d, gcalc_get_double(c, 2)));
#endif /*GCALC_CHECK_WITH_FLOAT*/
  return 0;
} 


typedef gcalc_digit_t Gcalc_coord4[GCALC_COORD_BASE*4];
typedef gcalc_digit_t Gcalc_coord5[GCALC_COORD_BASE*5];


void Gcalc_scan_iterator::intersection_info::do_calc_t()
{
  Gcalc_coord1 a2_a1x, a2_a1y;
  Gcalc_coord2 x1y2, x2y1;

  gcalc_sub_coord1(a2_a1x, edge_b->pi->ix, edge_a->pi->ix);
  gcalc_sub_coord1(a2_a1y, edge_b->pi->iy, edge_a->pi->iy);

  GCALC_DBUG_ASSERT(!gcalc_is_zero(edge_a->dy, GCALC_COORD_BASE) ||
                    !gcalc_is_zero(edge_b->dy, GCALC_COORD_BASE));

  gcalc_mul_coord1(x1y2, edge_a->dx, edge_b->dy);
  gcalc_mul_coord1(x2y1, edge_a->dy, edge_b->dx);
  gcalc_sub_coord(t_b, GCALC_COORD_BASE2, x1y2, x2y1);


  gcalc_mul_coord1(x1y2, a2_a1x, edge_b->dy);
  gcalc_mul_coord1(x2y1, a2_a1y, edge_b->dx);
  gcalc_sub_coord(t_a, GCALC_COORD_BASE2, x1y2, x2y1);
  t_calculated= 1;
}


void Gcalc_scan_iterator::intersection_info::do_calc_y()
{
  GCALC_DBUG_ASSERT(t_calculated);

  Gcalc_coord3 a_tb, b_ta;

  gcalc_mul_coord(a_tb, GCALC_COORD_BASE3,
                  t_b, GCALC_COORD_BASE2, edge_a->pi->iy, GCALC_COORD_BASE);
  gcalc_mul_coord(b_ta, GCALC_COORD_BASE3,
                  t_a, GCALC_COORD_BASE2, edge_a->dy, GCALC_COORD_BASE);

  gcalc_add_coord(y_exp, GCALC_COORD_BASE3, a_tb, b_ta);
  y_calculated= 1;
}


void Gcalc_scan_iterator::intersection_info::do_calc_x()
{
  GCALC_DBUG_ASSERT(t_calculated);

  Gcalc_coord3 a_tb, b_ta;

  gcalc_mul_coord(a_tb, GCALC_COORD_BASE3,
                  t_b, GCALC_COORD_BASE2, edge_a->pi->ix, GCALC_COORD_BASE);
  gcalc_mul_coord(b_ta, GCALC_COORD_BASE3,
                  t_a, GCALC_COORD_BASE2, edge_a->dx, GCALC_COORD_BASE);

  gcalc_add_coord(x_exp, GCALC_COORD_BASE3, a_tb, b_ta);
  x_calculated= 1;
}


static int cmp_node_isc(const Gcalc_heap::Info *node,
                        const Gcalc_heap::Info *isc)
{
  GCALC_DBUG_ASSERT(node->type == Gcalc_heap::nt_shape_node);
  Gcalc_scan_iterator::intersection_info *inf= i_data(isc);
  Gcalc_coord3 exp;
  int result;

  inf->calc_t();
  inf->calc_y_exp();

  gcalc_mul_coord(exp, GCALC_COORD_BASE3,
                  inf->t_b, GCALC_COORD_BASE2, node->iy, GCALC_COORD_BASE);

  result= gcalc_cmp_coord(exp, inf->y_exp, GCALC_COORD_BASE3);
#ifdef GCALC_CHECK_WITH_FLOAT
  long double int_x, int_y;
  isc->calc_xy_ld(&int_x, &int_y);
  if (result < 0)
  {
    if (!de_check(int_y, node->y) && node->y > int_y)
      GCALC_DBUG_PRINT(("floatcheck cmp_nod_iscy %g < %LG", node->y, int_y));
  }
  else if (result > 0)
  {
    if (!de_check(int_y, node->y) && node->y < int_y)
      GCALC_DBUG_PRINT(("floatcheck cmp_nod_iscy %g > %LG", node->y, int_y));
  }
  else
  {
    if (!de_check(int_y, node->y))
      GCALC_DBUG_PRINT(("floatcheck cmp_nod_iscy %g == %LG", node->y, int_y));
  }
#endif /*GCALC_CHECK_WITH_FLOAT*/
  if (result)
    goto exit;


  inf->calc_x_exp();
  gcalc_mul_coord(exp, GCALC_COORD_BASE3,
                  inf->t_b, GCALC_COORD_BASE2, node->ix, GCALC_COORD_BASE);

  result= gcalc_cmp_coord(exp, inf->x_exp, GCALC_COORD_BASE3);
#ifdef GCALC_CHECK_WITH_FLOAT
  if (result < 0)
  {
    if (!de_check(int_x, node->x) && node->x > int_x)
      GCALC_DBUG_PRINT(("floatcheck cmp_nod_iscx failed %g < %LG",
                         node->x, int_x));
  }
  else if (result > 0)
  {
    if (!de_check(int_x, node->x) && node->x < int_x)
      GCALC_DBUG_PRINT(("floatcheck cmp_nod_iscx failed %g > %LG",
                        node->x, int_x));
  }
  else
  {
    if (!de_check(int_x, node->x))
      GCALC_DBUG_PRINT(("floatcheck cmp_nod_iscx failed %g == %LG",
                        node->x, int_x));
  }
#endif /*GCALC_CHECK_WITH_FLOAT*/
exit:
  return result;
}


static int cmp_intersections(const Gcalc_heap::Info *i1,
                             const Gcalc_heap::Info *i2)
{
  Gcalc_scan_iterator::intersection_info *inf1= i_data(i1);
  Gcalc_scan_iterator::intersection_info *inf2= i_data(i2);
  Gcalc_coord5 exp_a, exp_b;
  int result;

  inf1->calc_t();
  inf2->calc_t();

  inf1->calc_y_exp();
  inf2->calc_y_exp();

  gcalc_mul_coord(exp_a, GCALC_COORD_BASE5,
                  inf1->y_exp, GCALC_COORD_BASE3, inf2->t_b, GCALC_COORD_BASE2);
  gcalc_mul_coord(exp_b, GCALC_COORD_BASE5,
                  inf2->y_exp, GCALC_COORD_BASE3, inf1->t_b, GCALC_COORD_BASE2);

  result= gcalc_cmp_coord(exp_a, exp_b, GCALC_COORD_BASE5);
#ifdef GCALC_CHECK_WITH_FLOAT
  long double x1, y1, x2, y2;
  i1->calc_xy_ld(&x1, &y1);
  i2->calc_xy_ld(&x2, &y2);

  if (result < 0)
  {
    if (!de_check(y1, y2) && y2 > y1)
      GCALC_DBUG_PRINT(("floatcheck cmp_intersections_y failed %LG < %LG",
                        y2, y1));
  }
  else if (result > 0)
  {
    if (!de_check(y1, y2) && y2 < y1)
      GCALC_DBUG_PRINT(("floatcheck cmp_intersections_y failed %LG > %LG",
                        y2, y1));
  }
  else
  {
    if (!de_check(y1, y2))
      GCALC_DBUG_PRINT(("floatcheck cmp_intersections_y failed %LG == %LG",
                        y2, y1));
  }
#endif /*GCALC_CHECK_WITH_FLOAT*/

  if (result != 0)
    return result;


  inf1->calc_x_exp();
  inf2->calc_x_exp();
  gcalc_mul_coord(exp_a, GCALC_COORD_BASE5,
                  inf1->x_exp, GCALC_COORD_BASE3, inf2->t_b, GCALC_COORD_BASE2);
  gcalc_mul_coord(exp_b, GCALC_COORD_BASE5,
                  inf2->x_exp, GCALC_COORD_BASE3, inf1->t_b, GCALC_COORD_BASE2);

  result= gcalc_cmp_coord(exp_a, exp_b, GCALC_COORD_BASE5);
#ifdef GCALC_CHECK_WITH_FLOAT
  if (result < 0)
  {
    if (!de_check(x1, x2) && x2 > x1)
      GCALC_DBUG_PRINT(("floatcheck cmp_intersectionsx failed %LG < %LG",
                        x2, x1));
  }
  else if (result > 0)
  {
    if (!de_check(x1, x2) && x2 < x1)
      GCALC_DBUG_PRINT(("floatcheck cmp_intersectionsx failed %LG > %LG",
                        x2, x1));
  }
  else
  {
    if (!de_check(x1, x2))
      GCALC_DBUG_PRINT(("floatcheck cmp_intersectionsx failed %LG == %LG",
                        x2, x1));
  }
#endif /*GCALC_CHECK_WITH_FLOAT*/
  return result;
}
/* Internal coordinates implementation end */


#define GCALC_SCALE_1 1e18

static double find_scale(double extent)
{
  double scale= 1e-2;
  while (scale < extent)
    scale*= (double ) 10;
  return GCALC_SCALE_1 / scale / 10;
}


void Gcalc_heap::set_extent(double xmin, double xmax, double ymin, double ymax)
{
  xmin= fabs(xmin);
  xmax= fabs(xmax);
  ymin= fabs(ymin);
  ymax= fabs(ymax);

  if (xmax < xmin)
    xmax= xmin;
  if (ymax < ymin)
    ymax= ymin;

  coord_extent= xmax > ymax ? xmax : ymax;
  coord_extent= find_scale(coord_extent);
#ifdef GCALC_CHECK_WITH_FLOAT
  gcalc_coord_extent= &coord_extent;
#endif /*GCALC_CHECK_WITH_FLOAT*/
}


void Gcalc_heap::free_point_info(Gcalc_heap::Info *i,
                                 Gcalc_dyn_list::Item **i_hook)
{
  if (m_hook == &i->next)
    m_hook= i_hook;
  *i_hook= i->next;
  free_item(i);
  m_n_points--;
}


Gcalc_heap::Info *Gcalc_heap::new_point_info(double x, double y,
                                             gcalc_shape_info shape)
{
  Info *result= (Info *)new_item();
  if (!result)
    return NULL;
  *m_hook= result;
  m_hook= &result->next;
  result->x= x;
  result->y= y;
  result->shape= shape;
  result->top_node= 1;
  result->type= nt_shape_node;
  gcalc_set_double(result->ix, x, coord_extent);
  gcalc_set_double(result->iy, y, coord_extent);

  m_n_points++;
  return result;
}


static Gcalc_heap::Info *new_intersection(
    Gcalc_heap *heap, Gcalc_scan_iterator::intersection_info *ii)
{
  Gcalc_heap::Info *isc= (Gcalc_heap::Info *)heap->new_item();
  if (!isc)
    return 0;
  isc->type= Gcalc_heap::nt_intersection;
  isc->p1= ii->edge_a->pi;
  isc->p2= ii->edge_a->next_pi;
  isc->p3= ii->edge_b->pi;
  isc->p4= ii->edge_b->next_pi;
  isc->intersection_data= ii;
  return isc;
}


static Gcalc_heap::Info *new_eq_point(
    Gcalc_heap *heap, const Gcalc_heap::Info *p,
    Gcalc_scan_iterator::point *edge)
{
  Gcalc_heap::Info *eqp= (Gcalc_heap::Info *)heap->new_item();
  if (!eqp)
    return 0;
  eqp->type= Gcalc_heap::nt_eq_node;
  eqp->node= p;
  eqp->eq_data= edge;
  return eqp;
}


void Gcalc_heap::Info::calc_xy(double *x, double *y) const
{
  double b0_x= p2->x - p1->x;
  double b0_y= p2->y - p1->y;
  double b1_x= p4->x - p3->x;
  double b1_y= p4->y - p3->y;
  double b0xb1= b0_x * b1_y - b0_y * b1_x;
  double t= (p3->x - p1->x) * b1_y - (p3->y - p1->y) * b1_x;

  t/= b0xb1;

  *x= p1->x + b0_x * t;
  *y= p1->y + b0_y * t;
}


#ifdef GCALC_CHECK_WITH_FLOAT
void Gcalc_heap::Info::calc_xy_ld(long double *x, long double *y) const
{
  long double b0_x= ((long double) p2->x) - p1->x;
  long double b0_y= ((long double) p2->y) - p1->y;
  long double b1_x= ((long double) p4->x) - p3->x;
  long double b1_y= ((long double) p4->y) - p3->y;
  long double b0xb1= b0_x * b1_y - b0_y * b1_x;
  long double ax=   ((long double) p3->x) - p1->x;
  long double ay=   ((long double) p3->y) - p1->y;
  long double t_a= ax * b1_y - ay * b1_x;
  long double hx= (b0xb1 * (long double) p1->x + b0_x * t_a);
  long double hy= (b0xb1 * (long double) p1->y + b0_y * t_a);

  if (fabs(b0xb1) < 1e-15)
  {
    *x= p1->x;
    *y= p1->y;
    return;
  }

  *x= hx/b0xb1;
  *y= hy/b0xb1;
}
#endif /*GCALC_CHECK_WITH_FLOAT*/


static int cmp_point_info(const Gcalc_heap::Info *i0,
                          const Gcalc_heap::Info *i1)
{
  int cmp_y= gcalc_cmp_coord1(i0->iy, i1->iy);
  if (cmp_y)
    return cmp_y;
  return gcalc_cmp_coord1(i0->ix, i1->ix);
}


static inline void trim_node(Gcalc_heap::Info *node, Gcalc_heap::Info *prev_node)
{
  if (!node)
    return;
  node->top_node= 0;
  GCALC_DBUG_ASSERT((node->left == prev_node) || (node->right == prev_node));
  if (node->left == prev_node)
    node->left= node->right;
  node->right= NULL;
  GCALC_DBUG_ASSERT(cmp_point_info(node, prev_node));
}


static int compare_point_info(const void *e0, const void *e1)
{
  const Gcalc_heap::Info *i0= (const Gcalc_heap::Info *)e0;
  const Gcalc_heap::Info *i1= (const Gcalc_heap::Info *)e1;
  return cmp_point_info(i0, i1) > 0;
}


void Gcalc_heap::prepare_operation()
{
  Info *cur;
  GCALC_DBUG_ASSERT(m_hook);
  *m_hook= NULL;
  m_hook= NULL; /* just to check it's not called twice */
  m_first= sort_list(compare_point_info, m_first, m_n_points);

  /* TODO - move this to the 'normal_scan' loop */
  for (cur= get_first(); cur; cur= cur->get_next())
  {
    trim_node(cur->left, cur);
    trim_node(cur->right, cur);
  }
}


void Gcalc_heap::reset()
{
  if (m_n_points)
  {
    free_list(m_first);
    m_n_points= 0;
  }
  m_hook= &m_first;
}


int Gcalc_shape_transporter::int_single_point(gcalc_shape_info Info,
                                              double x, double y)
{
  Gcalc_heap::Info *point= m_heap->new_point_info(x, y, Info);
  if (!point)
    return 1;
  point->left= point->right= 0;
  return 0;
}


int Gcalc_shape_transporter::int_add_point(gcalc_shape_info Info,
                                           double x, double y)
{
  Gcalc_heap::Info *point;
  Gcalc_dyn_list::Item **hook;

  hook= m_heap->get_cur_hook();

  if (!(point= m_heap->new_point_info(x, y, Info)))
    return 1;
  if (m_first)
  {
    if (cmp_point_info(m_prev, point) == 0)
    {
      /* Coinciding points, do nothing */
      m_heap->free_point_info(point, hook);
      return 0;
    }
    GCALC_DBUG_ASSERT(!m_prev || m_prev->x != x || m_prev->y != y);
    m_prev->left= point;
    point->right= m_prev;
  }
  else
    m_first= point;
  m_prev= point;
  m_prev_hook= hook;
  return 0;
}


void Gcalc_shape_transporter::int_complete()
{
  GCALC_DBUG_ASSERT(m_shape_started == 1 || m_shape_started == 3);

  if (!m_first)
    return;

  /* simple point */
  if (m_first == m_prev)
  {
    m_first->right= m_first->left= NULL;
    return;
  }

  /* line */
  if (m_shape_started == 1)
  {
    m_first->right= NULL;
    m_prev->left= m_prev->right;
    m_prev->right= NULL;
    return;
  }

  /* polygon */
  if (cmp_point_info(m_first, m_prev) == 0)
  {
    /* Coinciding points, remove the last one from the list */
    m_prev->right->left= m_first;
    m_first->right= m_prev->right;
    m_heap->free_point_info(m_prev, m_prev_hook);
  }
  else
  {
    GCALC_DBUG_ASSERT(m_prev->x != m_first->x || m_prev->y != m_first->y);
    m_first->right= m_prev;
    m_prev->left= m_first;
  }
}


inline void calc_dx_dy(Gcalc_scan_iterator::point *p)
{
  gcalc_sub_coord1(p->dx, p->next_pi->ix, p->pi->ix);
  gcalc_sub_coord1(p->dy, p->next_pi->iy, p->pi->iy);
  if (GCALC_SIGN(p->dx[0]))
  {
    p->l_border= &p->next_pi->ix;
    p->r_border= &p->pi->ix;
  }
  else
  {
    p->r_border= &p->next_pi->ix;
    p->l_border= &p->pi->ix;
  }
}


Gcalc_scan_iterator::Gcalc_scan_iterator(size_t blk_size) :
  Gcalc_dyn_list(blk_size, sizeof(point) > sizeof(intersection_info) ?
                             sizeof(point) :
                             sizeof(intersection_info))
{
  state.slice= NULL;
  m_bottom_points= NULL;
  m_bottom_hook= &m_bottom_points;
}
		  

void Gcalc_scan_iterator::init(Gcalc_heap *points)
{
  GCALC_DBUG_ASSERT(points->ready());
  GCALC_DBUG_ASSERT(!state.slice);

  if (!(m_cur_pi= points->get_first()))
    return;
  m_heap= points;
  state.event_position_hook= &state.slice;
  state.event_end= NULL;
#ifndef GCALC_DBUG_OFF
  m_cur_thread= 0;
#endif /*GCALC_DBUG_OFF*/
  GCALC_SET_TERMINATED(killed, 0);
}

void Gcalc_scan_iterator::reset()
{
  state.slice= NULL;
  m_bottom_points= NULL;
  m_bottom_hook= &m_bottom_points;
  Gcalc_dyn_list::reset();
}


int Gcalc_scan_iterator::point::cmp_dx_dy(const Gcalc_coord1 dx_a,
                                          const Gcalc_coord1 dy_a,
                                          const Gcalc_coord1 dx_b,
                                          const Gcalc_coord1 dy_b)
{
  Gcalc_coord2 dx_a_dy_b;
  Gcalc_coord2 dy_a_dx_b;
  gcalc_mul_coord1(dx_a_dy_b, dx_a, dy_b);
  gcalc_mul_coord1(dy_a_dx_b, dy_a, dx_b);

  return gcalc_cmp_coord(dx_a_dy_b, dy_a_dx_b, GCALC_COORD_BASE2);
}


int Gcalc_scan_iterator::point::cmp_dx_dy(const Gcalc_heap::Info *p1,
                                          const Gcalc_heap::Info *p2,
                                          const Gcalc_heap::Info *p3,
                                          const Gcalc_heap::Info *p4)
{
  Gcalc_coord1 dx_a, dy_a, dx_b, dy_b;
  gcalc_sub_coord1(dx_a, p2->ix, p1->ix);
  gcalc_sub_coord1(dy_a, p2->iy, p1->iy);
  gcalc_sub_coord1(dx_b, p4->ix, p3->ix);
  gcalc_sub_coord1(dy_b, p4->iy, p3->iy);
  return cmp_dx_dy(dx_a, dy_a, dx_b, dy_b);
}


int Gcalc_scan_iterator::point::cmp_dx_dy(const point *p) const
{
  GCALC_DBUG_ASSERT(!is_bottom());
  return cmp_dx_dy(dx, dy, p->dx, p->dy);
}


#ifdef GCALC_CHECK_WITH_FLOAT
void Gcalc_scan_iterator::point::calc_x(long double *x, long double y,
                                        long double ix) const
{
  long double ddy= gcalc_get_double(dy, GCALC_COORD_BASE);
  if (fabsl(ddy) < (long double) 1e-20)
  {
    *x= ix;
  }
  else
    *x= (ddy * (long double) pi->x + gcalc_get_double(dx, GCALC_COORD_BASE) *
          (y - pi->y)) / ddy;
}
#endif /*GCALC_CHECK_WITH_FLOAT*/


static int compare_events(const void *e0, const void *e1)
{
  const Gcalc_scan_iterator::point *p0= (const Gcalc_scan_iterator::point *)e0;
  const Gcalc_scan_iterator::point *p1= (const Gcalc_scan_iterator::point *)e1;
  return p0->cmp_dx_dy(p1) > 0;
}


int Gcalc_scan_iterator::arrange_event(int do_sorting, int n_intersections)
{
  int ev_counter;
  point *sp;
  point **sp_hook;

  ev_counter= 0;

  *m_bottom_hook= NULL;
  for (sp= m_bottom_points; sp; sp= sp->get_next())
    sp->ev_next= sp->get_next();

  for (sp= state.slice, sp_hook= &state.slice;
       sp; sp_hook= sp->next_ptr(), sp= sp->get_next())
  {
    if (sp->event)
    {
      state.event_position_hook= sp_hook;
      break;
    }
  }

  for (sp= *(sp_hook= state.event_position_hook);
       sp && sp->event; sp_hook= sp->next_ptr(), sp= sp->get_next())
  {
    ev_counter++;
    if (sp->get_next() && sp->get_next()->event)
      sp->ev_next= sp->get_next();
    else
      sp->ev_next= m_bottom_points;
  }

#ifndef GCALC_DBUG_OFF
  {
    point *cur_p= sp;
    for (; cur_p; cur_p= cur_p->get_next())
      GCALC_DBUG_ASSERT(!cur_p->event);
  }
#endif /*GCALC_DBUG_OFF*/

  state.event_end= sp;

  if (ev_counter == 2 && n_intersections == 1)
  {
    /* If we had only intersection, just swap the two points. */
    sp= *state.event_position_hook;
    *state.event_position_hook= sp->get_next();
    sp->next= (*state.event_position_hook)->next;
    (*state.event_position_hook)->next= sp;

    /* The list of the events should be restored. */
    (*state.event_position_hook)->ev_next= sp;
    sp->ev_next= m_bottom_points;
  }
  else if (ev_counter == 2 && get_events()->event == scev_two_threads)
  {
    /* Do nothing. */
  }
  else if (ev_counter > 1 && do_sorting)
  {
    point *cur_p;
    *sp_hook= NULL;
    sp= (point *) sort_list(compare_events, *state.event_position_hook,
                            ev_counter);
    /* Find last item in the list, it's changed after the sorting. */
    for (cur_p= sp->get_next(); cur_p->get_next();
        cur_p= cur_p->get_next())
    {}
    cur_p->next= state.event_end;
    *state.event_position_hook= sp;
    /* The list of the events should be restored. */
    for (; sp && sp->event; sp= sp->get_next())
    {
      if (sp->get_next() && sp->get_next()->event)
        sp->ev_next= sp->get_next();
      else
        sp->ev_next= m_bottom_points;
    }
  }

#ifndef GCALC_DBUG_OFF
  {
    const event_point *ev= get_events();
    for (; ev && ev->get_next(); ev= ev->get_next())
    {
      if (ev->is_bottom() || ev->get_next()->is_bottom())
        break;
      GCALC_DBUG_ASSERT(ev->cmp_dx_dy(ev->get_next()) <= 0);
    }
  }
#endif /*GCALC_DBUG_OFF*/
  return 0;
}


int Gcalc_heap::Info::equal_pi(const Info *pi) const
{
  if (type == nt_intersection)
    return equal_intersection;
  if (pi->type == nt_eq_node)
    return 1;
  if (type == nt_eq_node || pi->type == nt_intersection)
    return 0;
  return cmp_point_info(this, pi) == 0;
}

int Gcalc_scan_iterator::step()
{
  int result= 0;
  int do_sorting= 0;
  int n_intersections= 0;
  point *sp;
  GCALC_DBUG_ENTER("Gcalc_scan_iterator::step");
  GCALC_DBUG_ASSERT(more_points());

  if (GCALC_TERMINATED(killed))
    GCALC_DBUG_RETURN(0xFFFF);

  /* Clear the old event marks. */
  if (m_bottom_points)
  {
    free_list((Gcalc_dyn_list::Item **) &m_bottom_points,
              (Gcalc_dyn_list::Item **) m_bottom_hook);
    m_bottom_points= NULL;
    m_bottom_hook= &m_bottom_points;
  }
  for (sp= *state.event_position_hook;
       sp != state.event_end; sp= sp->get_next())
    sp->event= scev_none;

//#ifndef GCALC_DBUG_OFF
  state.event_position_hook= NULL;
  state.pi= NULL;
//#endif /*GCALC_DBUG_OFF*/

  do
  {
#ifndef GCALC_DBUG_OFF
    if (m_cur_pi->type == Gcalc_heap::nt_intersection &&
        m_cur_pi->get_next()->type == Gcalc_heap::nt_intersection &&
        m_cur_pi->equal_intersection)
      GCALC_DBUG_ASSERT(cmp_intersections(m_cur_pi, m_cur_pi->get_next()) == 0);
#endif /*GCALC_DBUG_OFF*/
    GCALC_DBUG_CHECK_COUNTER();
    GCALC_DBUG_PRINT_SLICE("step:", state.slice);
    GCALC_DBUG_PRINT_PI(m_cur_pi);
    if (m_cur_pi->type == Gcalc_heap::nt_shape_node)
    {
      if (m_cur_pi->is_top())
      {
        result= insert_top_node();
        if (!m_cur_pi->is_bottom())
          do_sorting++;
      }
      else if (m_cur_pi->is_bottom())
        remove_bottom_node();
      else
      {
        do_sorting++;
        result= node_scan();
      }
      if (result)
        GCALC_DBUG_RETURN(result);
      state.pi= m_cur_pi;
    }
    else if (m_cur_pi->type == Gcalc_heap::nt_eq_node)
    {
      do_sorting++;
      eq_scan();
    }
    else
    {
      /* nt_intersection */
      do_sorting++;
      n_intersections++;
      intersection_scan();
      if (!state.pi || state.pi->type == Gcalc_heap::nt_intersection)
        state.pi= m_cur_pi;
    }

    m_cur_pi= m_cur_pi->get_next();
  } while (m_cur_pi && state.pi->equal_pi(m_cur_pi));

  GCALC_DBUG_RETURN(arrange_event(do_sorting, n_intersections));
}


static int node_on_right(const Gcalc_heap::Info *node, 
    const Gcalc_heap::Info *edge_a, const Gcalc_heap::Info *edge_b)
{
  Gcalc_coord1 a_x, a_y;
  Gcalc_coord1 b_x, b_y;
  Gcalc_coord2 ax_by, ay_bx;
  int result;

  gcalc_sub_coord1(a_x, node->ix, edge_a->ix);
  gcalc_sub_coord1(a_y, node->iy, edge_a->iy);
  gcalc_sub_coord1(b_x, edge_b->ix, edge_a->ix);
  gcalc_sub_coord1(b_y, edge_b->iy, edge_a->iy);
  gcalc_mul_coord1(ax_by, a_x, b_y);
  gcalc_mul_coord1(ay_bx, a_y, b_x);
  result= gcalc_cmp_coord(ax_by, ay_bx, GCALC_COORD_BASE2);
#ifdef GCALC_CHECK_WITH_FLOAT
  {
    long double dx= gcalc_get_double(edge_b->ix, GCALC_COORD_BASE) -
                      gcalc_get_double(edge_a->ix, GCALC_COORD_BASE);
    long double dy= gcalc_get_double(edge_b->iy, GCALC_COORD_BASE) -
                      gcalc_get_double(edge_a->iy, GCALC_COORD_BASE);
    long double ax= gcalc_get_double(node->ix, GCALC_COORD_BASE) -
                      gcalc_get_double(edge_a->ix, GCALC_COORD_BASE);
    long double ay= gcalc_get_double(node->iy, GCALC_COORD_BASE) -
                      gcalc_get_double(edge_a->iy, GCALC_COORD_BASE);
    long double d= ax * dy - ay * dx;
    if (result == 0)
      GCALC_DBUG_ASSERT(de_check(d, 0.0));
    else if (result < 0)
      GCALC_DBUG_ASSERT(de_check(d, 0.0) || d < 0);
    else
      GCALC_DBUG_ASSERT(de_check(d, 0.0) || d > 0);
  }
#endif /*GCALC_CHECK_WITH_FLOAT*/
  return result;
}


static int cmp_tops(const Gcalc_heap::Info *top_node, 
    const Gcalc_heap::Info *edge_a, const Gcalc_heap::Info *edge_b)
{
  int cmp_res_a, cmp_res_b;

  cmp_res_a= gcalc_cmp_coord1(edge_a->ix, top_node->ix);
  cmp_res_b= gcalc_cmp_coord1(edge_b->ix, top_node->ix);

  if (cmp_res_a <= 0 && cmp_res_b > 0)
    return -1;
  if (cmp_res_b <= 0 && cmp_res_a > 0)
    return 1;
  if (cmp_res_a == 0 && cmp_res_b == 0)
    return 0;

  return node_on_right(edge_a, top_node, edge_b);
}


int Gcalc_scan_iterator::insert_top_node()
{
  point *sp= state.slice;
  point **prev_hook= &state.slice;
  point *sp1= NULL;
  point *sp0= new_slice_point();
  int cmp_res;

  GCALC_DBUG_ENTER("Gcalc_scan_iterator::insert_top_node");
  if (!sp0)
    GCALC_DBUG_RETURN(1);
  sp0->pi= m_cur_pi;
  sp0->next_pi= m_cur_pi->left;
#ifndef GCALC_DBUG_OFF
  sp0->thread= m_cur_thread++;
#endif /*GCALC_DBUG_OFF*/
  if (m_cur_pi->left)
  {
    calc_dx_dy(sp0);
    if (m_cur_pi->right)
    {
      if (!(sp1= new_slice_point()))
        GCALC_DBUG_RETURN(1);
      sp1->event= sp0->event= scev_two_threads;
      sp1->pi= m_cur_pi;
      sp1->next_pi= m_cur_pi->right;
#ifndef GCALC_DBUG_OFF
      sp1->thread= m_cur_thread++;
#endif /*GCALC_DBUG_OFF*/
      calc_dx_dy(sp1);
      /* We have two threads so should decide which one will be first */
      cmp_res= cmp_tops(m_cur_pi, m_cur_pi->left, m_cur_pi->right);
      if (cmp_res > 0)
      {
        point *tmp= sp0;
        sp0= sp1;
        sp1= tmp;
      }
      else if (cmp_res == 0)
      {
        /* Exactly same direction of the edges. */
        cmp_res= gcalc_cmp_coord1(m_cur_pi->left->iy, m_cur_pi->right->iy);
        if (cmp_res != 0)
        {
          if (cmp_res < 0)
          {
            if (add_eq_node(sp0->next_pi, sp1))
              GCALC_DBUG_RETURN(1);
          }
          else
          {
            if (add_eq_node(sp1->next_pi, sp0))
              GCALC_DBUG_RETURN(1);
          }
        }
        else
        {
          cmp_res= gcalc_cmp_coord1(m_cur_pi->left->ix, m_cur_pi->right->ix);
          if (cmp_res != 0)
          {
            if (cmp_res < 0)
            {
              if (add_eq_node(sp0->next_pi, sp1))
                GCALC_DBUG_RETURN(1);
            }
            else
            {
              if (add_eq_node(sp1->next_pi, sp0))
                GCALC_DBUG_RETURN(1);
            }
          }
        }
      }
    }
    else
      sp0->event= scev_thread;
  }
  else
    sp0->event= scev_single_point;


  /* Check if we already have an event - then we'll place the node there */
  for (; sp && !sp->event; prev_hook= sp->next_ptr(), sp=sp->get_next())
  {}
  if (!sp)
  {
    sp= state.slice;
    prev_hook= &state.slice;
    /* We need to find the place to insert. */
    for (; sp; prev_hook= sp->next_ptr(), sp=sp->get_next())
    {
      if (sp->event || gcalc_cmp_coord1(*sp->r_border, m_cur_pi->ix) < 0)
        continue;
      cmp_res= node_on_right(m_cur_pi, sp->pi, sp->next_pi);
      if (cmp_res == 0)
      {
        /* The top node lies on the edge. */
        /* Nodes of that edge will be handled in other places. */
        sp->event= scev_intersection;
      }
      else if (cmp_res < 0)
        break;
    }
  }

  if (sp0->event == scev_single_point)
  {
    /* Add single point to the bottom list. */
    *m_bottom_hook= sp0;
    m_bottom_hook= sp0->next_ptr();
    state.event_position_hook= prev_hook;
  }
  else
  {
    *prev_hook= sp0;
    sp0->next= sp;
    if (add_events_for_node(sp0))
      GCALC_DBUG_RETURN(1);

    if (sp0->event == scev_two_threads)
    {
      *prev_hook= sp1;
      sp1->next= sp;
      if (add_events_for_node(sp1))
        GCALC_DBUG_RETURN(1);

      sp0->next= sp1;
      *prev_hook= sp0;
    }
  }

  GCALC_DBUG_RETURN(0);
}


void Gcalc_scan_iterator::remove_bottom_node()
{
  point *sp= state.slice;
  point **sp_hook= &state.slice;
  point *first_bottom_point= NULL;

  GCALC_DBUG_ENTER("Gcalc_scan_iterator::remove_bottom_node");
  for (; sp; sp= sp->get_next())
  {
    if (sp->next_pi == m_cur_pi)
    {
      *sp_hook= sp->get_next();
      sp->pi= m_cur_pi;
      sp->next_pi= NULL;
      if (first_bottom_point)
      {
        first_bottom_point->event= sp->event= scev_two_ends;
        break;
      }
      first_bottom_point= sp;
      sp->event= scev_end;
      state.event_position_hook= sp_hook;
    }
    else
      sp_hook= sp->next_ptr();
  }
  GCALC_DBUG_ASSERT(first_bottom_point);
  *m_bottom_hook= first_bottom_point;
  m_bottom_hook= first_bottom_point->next_ptr();
  if (sp)
  {
    *m_bottom_hook= sp;
    m_bottom_hook= sp->next_ptr();
  }

  GCALC_DBUG_VOID_RETURN;
}


int Gcalc_scan_iterator::add_events_for_node(point *sp_node)
{
  point *sp= state.slice;
  int cur_pi_r, sp_pi_r;

  GCALC_DBUG_ENTER("Gcalc_scan_iterator::add_events_for_node");

  /* Scan to the event point. */
  for (; sp != sp_node; sp= sp->get_next())
  {
    GCALC_DBUG_ASSERT(!sp->is_bottom());
    GCALC_DBUG_PRINT(("left cut_edge %d", sp->thread));
    if (sp->next_pi == sp_node->next_pi ||
        gcalc_cmp_coord1(*sp->r_border, *sp_node->l_border) < 0)
      continue;
    sp_pi_r= node_on_right(sp->next_pi, sp_node->pi, sp_node->next_pi);
    if (sp_pi_r < 0)
      continue;
    cur_pi_r= node_on_right(sp_node->next_pi, sp->pi, sp->next_pi);
    if (cur_pi_r > 0)
      continue;
    if (cur_pi_r == 0 && sp_pi_r == 0)
    {
      int cmp_res= cmp_point_info(sp->next_pi, sp_node->next_pi);
      if (cmp_res > 0)
      {
        if (add_eq_node(sp_node->next_pi, sp))
          GCALC_DBUG_RETURN(1);
      }
      else if (cmp_res < 0)
      {
        if (add_eq_node(sp->next_pi, sp_node))
          GCALC_DBUG_RETURN(1);
      }
      continue;
    }

    if (cur_pi_r == 0)
    {
      if (add_eq_node(sp_node->next_pi, sp))
        GCALC_DBUG_RETURN(1);
      continue;
    }
    else if (sp_pi_r == 0)
    {
      if (add_eq_node(sp->next_pi, sp_node))
        GCALC_DBUG_RETURN(1);
      continue;
    }

    if (sp->event)
    {
#ifndef GCALC_DBUG_OFF
      cur_pi_r= node_on_right(sp_node->pi, sp->pi, sp->next_pi);
      GCALC_DBUG_ASSERT(cur_pi_r == 0);
#endif /*GCALC_DBUG_OFF*/
      continue;
    }
    cur_pi_r= node_on_right(sp_node->pi, sp->pi, sp->next_pi);
    GCALC_DBUG_ASSERT(cur_pi_r >= 0);
    //GCALC_DBUG_ASSERT(cur_pi_r > 0); /* Is it ever violated? */
    if (cur_pi_r > 0 && add_intersection(sp, sp_node, m_cur_pi))
      GCALC_DBUG_RETURN(1);
  }

  /* Scan to the end of the slice */
  sp= sp->get_next();

  for (; sp; sp= sp->get_next())
  {
    GCALC_DBUG_ASSERT(!sp->is_bottom());
    GCALC_DBUG_PRINT(("right cut_edge %d", sp->thread));
    if (sp->next_pi == sp_node->next_pi ||
        gcalc_cmp_coord1(*sp_node->r_border, *sp->l_border) < 0)
      continue;
    sp_pi_r= node_on_right(sp->next_pi, sp_node->pi, sp_node->next_pi);
    if (sp_pi_r > 0)
      continue;
    cur_pi_r= node_on_right(sp_node->next_pi, sp->pi, sp->next_pi);
    if (cur_pi_r < 0)
      continue;
    if (cur_pi_r == 0 && sp_pi_r == 0)
    {
      int cmp_res= cmp_point_info(sp->next_pi, sp_node->next_pi);
      if (cmp_res > 0)
      {
        if (add_eq_node(sp_node->next_pi, sp))
          GCALC_DBUG_RETURN(1);
      }
      else if (cmp_res < 0)
      {
        if (add_eq_node(sp->next_pi, sp_node))
          GCALC_DBUG_RETURN(1);
      }
      continue;
    }
    if (cur_pi_r == 0)
    {
      if (add_eq_node(sp_node->next_pi, sp))
        GCALC_DBUG_RETURN(1);
      continue;
    }
    else if (sp_pi_r == 0)
    {
      if (add_eq_node(sp->next_pi, sp_node))
        GCALC_DBUG_RETURN(1);
      continue;
    }

    if (sp->event)
    {
#ifndef GCALC_DBUG_OFF
      cur_pi_r= node_on_right(sp_node->pi, sp->pi, sp->next_pi);
      GCALC_DBUG_ASSERT(cur_pi_r == 0);
#endif /*GCALC_DBUG_OFF*/
      continue;
    }
    cur_pi_r= node_on_right(sp_node->pi, sp->pi, sp->next_pi);
    GCALC_DBUG_ASSERT(cur_pi_r <= 0);
    //GCALC_DBUG_ASSERT(cur_pi_r < 0); /* Is it ever violated? */
    if (cur_pi_r < 0 && add_intersection(sp_node, sp, m_cur_pi))
      GCALC_DBUG_RETURN(1);
  }

  GCALC_DBUG_RETURN(0);
}


int Gcalc_scan_iterator::node_scan()
{
  point *sp= state.slice;
  Gcalc_heap::Info *cur_pi= m_cur_pi;

  GCALC_DBUG_ENTER("Gcalc_scan_iterator::node_scan");

  /* Scan to the event point.                             */
  /* Can be avoided if we add link to the sp to the Info. */
  for (; sp->next_pi != cur_pi; sp= sp->get_next())
  {}

  GCALC_DBUG_PRINT(("node for %d", sp->thread));
  /* Handle the point itself. */
  sp->pi= cur_pi;
  sp->next_pi= cur_pi->left;
  sp->event= scev_point;
  calc_dx_dy(sp);

  GCALC_DBUG_RETURN(add_events_for_node(sp));
}


void Gcalc_scan_iterator::eq_scan()
{
  point *sp= eq_sp(m_cur_pi);
  GCALC_DBUG_ENTER("Gcalc_scan_iterator::eq_scan");
  
#ifndef GCALC_DBUG_OFF
  {
    point *cur_p= state.slice;
    for (; cur_p && cur_p != sp; cur_p= cur_p->get_next())
    {}
    GCALC_DBUG_ASSERT(cur_p);
  }
#endif /*GCALC_DBUG_OFF*/
  if (!sp->event)
  {
    sp->event= scev_intersection;
    sp->ev_pi= m_cur_pi;
  }

  GCALC_DBUG_VOID_RETURN;
}


void Gcalc_scan_iterator::intersection_scan()
{
  intersection_info *ii= i_data(m_cur_pi);
  GCALC_DBUG_ENTER("Gcalc_scan_iterator::intersection_scan");
  
#ifndef GCALC_DBUG_OFF
  {
    point *sp= state.slice;
    for (; sp && sp != ii->edge_a; sp= sp->get_next())
    {}
    GCALC_DBUG_ASSERT(sp);
    for (; sp && sp != ii->edge_b; sp= sp->get_next())
    {}
    GCALC_DBUG_ASSERT(sp);
  }
#endif /*GCALC_DBUG_OFF*/

  ii->edge_a->event= ii->edge_b->event= scev_intersection;
  ii->edge_a->ev_pi= ii->edge_b->ev_pi= m_cur_pi;
  free_item(ii);
  m_cur_pi->intersection_data= NULL;

  GCALC_DBUG_VOID_RETURN;
}


int Gcalc_scan_iterator::add_intersection(point *sp_a, point *sp_b,
                                          Gcalc_heap::Info *pi_from)
{
  Gcalc_heap::Info *ii;
  intersection_info *i_calc;
  int cmp_res;
  int skip_next= 0;

  GCALC_DBUG_ENTER("Gcalc_scan_iterator::add_intersection");
  if (!(i_calc= new_intersection_info(sp_a, sp_b)) ||
      !(ii= new_intersection(m_heap, i_calc)))
    GCALC_DBUG_RETURN(1);

  ii->equal_intersection= 0;

  for (;
       pi_from->get_next() != sp_a->next_pi &&
         pi_from->get_next() != sp_b->next_pi;
       pi_from= pi_from->get_next())
  {
    Gcalc_heap::Info *cur= pi_from->get_next();
    if (skip_next)
    {
      if (cur->type == Gcalc_heap::nt_intersection)
        skip_next= cur->equal_intersection;
      else
        skip_next= 0;
      continue;
    }
    if (cur->type == Gcalc_heap::nt_intersection)
    {
      cmp_res= cmp_intersections(cur, ii);
      skip_next= cur->equal_intersection;
    }
    else if (cur->type == Gcalc_heap::nt_eq_node)
      continue;
    else
      cmp_res= cmp_node_isc(cur, ii);
    if (cmp_res == 0)
    {
      ii->equal_intersection= 1;
      break;
    }
    else if (cmp_res > 0)
      break;
  }

  /* Intersection inserted before the equal point. */
  ii->next= pi_from->get_next();
  pi_from->next= ii;

  GCALC_DBUG_RETURN(0);
}


int Gcalc_scan_iterator::add_eq_node(Gcalc_heap::Info *node, point *sp)
{
  Gcalc_heap::Info *en;

  GCALC_DBUG_ENTER("Gcalc_scan_iterator::add_intersection");
  en= new_eq_point(m_heap, node, sp);
  if (!en)
    GCALC_DBUG_RETURN(1);

  /* eq_node iserted after teh equal point. */
  en->next= node->get_next();
  node->next= en;

  GCALC_DBUG_RETURN(0);
}


void calc_t(Gcalc_coord2 t_a, Gcalc_coord2 t_b,
            Gcalc_coord1 dxa, Gcalc_coord1 dxb,
            const Gcalc_heap::Info *p1, const Gcalc_heap::Info *p2,
            const Gcalc_heap::Info *p3, const Gcalc_heap::Info *p4)
{
  Gcalc_coord1 a2_a1x, a2_a1y;
  Gcalc_coord2 x1y2, x2y1;
  Gcalc_coord1 dya, dyb;

  gcalc_sub_coord1(a2_a1x, p3->ix, p1->ix);
  gcalc_sub_coord1(a2_a1y, p3->iy, p1->iy);

  gcalc_sub_coord1(dxa, p2->ix, p1->ix);
  gcalc_sub_coord1(dya, p2->iy, p1->iy);
  gcalc_sub_coord1(dxb, p4->ix, p3->ix);
  gcalc_sub_coord1(dyb, p4->iy, p3->iy);

  gcalc_mul_coord1(x1y2, dxa, dyb);
  gcalc_mul_coord1(x2y1, dya, dxb);
  gcalc_sub_coord(t_b, GCALC_COORD_BASE2, x1y2, x2y1);


  gcalc_mul_coord1(x1y2, a2_a1x, dyb);
  gcalc_mul_coord1(x2y1, a2_a1y, dxb);
  gcalc_sub_coord(t_a, GCALC_COORD_BASE2, x1y2, x2y1);
}


double Gcalc_scan_iterator::get_y() const
{
  if (state.pi->type == Gcalc_heap::nt_intersection)
  {
    Gcalc_coord1 dxa, dya;
    Gcalc_coord2 t_a, t_b;
    Gcalc_coord3 a_tb, b_ta, y_exp;
    calc_t(t_a, t_b, dxa, dya,
           state.pi->p1, state.pi->p2, state.pi->p3, state.pi->p4);


    gcalc_mul_coord(a_tb, GCALC_COORD_BASE3,
        t_b, GCALC_COORD_BASE2, state.pi->p1->iy, GCALC_COORD_BASE);
    gcalc_mul_coord(b_ta, GCALC_COORD_BASE3,
        t_a, GCALC_COORD_BASE2, dya, GCALC_COORD_BASE);

    gcalc_add_coord(y_exp, GCALC_COORD_BASE3, a_tb, b_ta);

    return (get_pure_double(y_exp, GCALC_COORD_BASE3) /
             get_pure_double(t_b, GCALC_COORD_BASE2)) / m_heap->coord_extent;
  }
  else
    return state.pi->y;
}


double Gcalc_scan_iterator::get_event_x() const
{
  if (state.pi->type == Gcalc_heap::nt_intersection)
  {
    Gcalc_coord1 dxa, dya;
    Gcalc_coord2 t_a, t_b;
    Gcalc_coord3 a_tb, b_ta, x_exp;
    calc_t(t_a, t_b, dxa, dya,
           state.pi->p1, state.pi->p2, state.pi->p3, state.pi->p4);


    gcalc_mul_coord(a_tb, GCALC_COORD_BASE3,
        t_b, GCALC_COORD_BASE2, state.pi->p1->ix, GCALC_COORD_BASE);
    gcalc_mul_coord(b_ta, GCALC_COORD_BASE3,
        t_a, GCALC_COORD_BASE2, dxa, GCALC_COORD_BASE);

    gcalc_add_coord(x_exp, GCALC_COORD_BASE3, a_tb, b_ta);

    return (get_pure_double(x_exp, GCALC_COORD_BASE3) /
             get_pure_double(t_b, GCALC_COORD_BASE2)) / m_heap->coord_extent;
  }
  else
    return state.pi->x;
}

double Gcalc_scan_iterator::get_h() const
{
  double cur_y= get_y();
  double next_y;
  if (state.pi->type == Gcalc_heap::nt_intersection)
  {
    double x;
    state.pi->calc_xy(&x, &next_y);
  }
  else
    next_y= state.pi->y;
  return next_y - cur_y;
}


double Gcalc_scan_iterator::get_sp_x(const point *sp) const
{
  double dy;
  if (sp->event & (scev_end | scev_two_ends | scev_point))
    return sp->pi->x;
  dy= sp->next_pi->y - sp->pi->y;
  if (fabs(dy) < 1e-12)
    return sp->pi->x;
  return (sp->next_pi->x - sp->pi->x) * dy;
}


double Gcalc_scan_iterator::get_pure_double(const Gcalc_internal_coord *d,
                                            int d_len)
{
  int n= 1;
  long double res= (long double) FIRST_DIGIT(d[0]);
  do
  {
    res*= (long double) GCALC_DIG_BASE;
    res+= (long double) d[n];
  } while(++n < d_len);

  if (GCALC_SIGN(d[0]))
    res*= -1.0;
  return res;
}


#endif /* HAVE_SPATIAL */
