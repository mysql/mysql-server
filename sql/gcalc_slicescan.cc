/* Copyright (c) 2000, 2010 Oracle and/or its affiliates. All rights reserved.

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


#include "mysql_priv.h"

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
  DBUG_ASSERT(m_free == NULL);
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
    my_free(list, MYF(0));
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

void Gcalc_internal_coord::set_zero()
{
  int n_res= 0;
  do
  {
    digits[n_res++]= 0;
  } while (n_res < n_digits); 
  sign= 0;
}


int Gcalc_internal_coord::is_zero() const
{
  int n_res= 0;
  do
  {
    if (digits[n_res++] != 0)
      return 0;
  } while (n_res < n_digits); 
  return 1;
}


#ifdef GCALC_CHECK_WITH_FLOAT
long double Gcalc_internal_coord::get_double() const
{
  int n= 0;
  long double res= 0.0;
  do
  {
    res*= (long double) DIG_BASE;
    res+= digits[n];
  } while(++n < n_digits);

  n= 0;
  do
  {
    if (n & 1)
      res/= (long double) C_SCALE;
  } while(++n < n_digits);

  if (sign)
    res*= -1.0;
  return res;
}
#endif /*GCALC_CHECK_WITH_FLOAT*/


static void do_add(Gcalc_internal_coord *result,
                   const Gcalc_internal_coord *a,
                   const Gcalc_internal_coord *b)
{
  DBUG_ASSERT(a->n_digits == b->n_digits);
  DBUG_ASSERT(a->n_digits == result->n_digits);
  int n_digit= a->n_digits-1;
  coord_digit_t carry= 0;

  do
  {
    if ((result->digits[n_digit]=
          a->digits[n_digit] + b->digits[n_digit] + carry) >= DIG_BASE)
    {
      carry= 1;
      result->digits[n_digit]-= DIG_BASE;
    }
    else
      carry= 0;
  } while (n_digit--);
  DBUG_ASSERT(carry == 0);
  result->sign= a->sign;
}


static void do_sub(Gcalc_internal_coord *result,
                   const Gcalc_internal_coord *a,
                   const Gcalc_internal_coord *b)
{
  DBUG_ASSERT(a->n_digits == b->n_digits);
  DBUG_ASSERT(a->n_digits == result->n_digits);
  int n_digit= a->n_digits-1;
  coord_digit_t carry= 0;

  do
  {
    if ((result->digits[n_digit]=
          a->digits[n_digit] - b->digits[n_digit] - carry) < 0)
    {
      carry= 1;
      result->digits[n_digit]+= DIG_BASE;
    }
    else
      carry= 0;
  } while (n_digit--);
  DBUG_ASSERT(carry == 0);
  if (a->sign && result->is_zero())
    result->sign= 0;
  else
    result->sign= a->sign;
}


static int do_cmp(const Gcalc_internal_coord *a,
                  const Gcalc_internal_coord *b)
{
  DBUG_ASSERT(a->n_digits == b->n_digits);
  int n_digit= 0;

  do
  {
    coord_digit_t d= a->digits[n_digit] - b->digits[n_digit];
    if (d > 0)
      return 1;
    if (d < 0)
      return -1;
    n_digit++;
  } while (n_digit < a->n_digits);

  return 0;
}


#ifdef GCALC_CHECK_WITH_FLOAT
static int de_check(long double a, long double b)
{
  long double d= a - b;
  if (d < (long double) 1e-6 && d > (long double) -1e-6)
    return 1;
  printf("xxx\n");
  return 0;
}

static int de_check1(long double a, long double b)
{
  long double d= a - b;
  if (d < (long double) 1e-6 && d > (long double) -1e-6)
    return 1;
  return 0;
}
static int de_weak(long double a, long double b)
{
  long double d= a - b;
  if (d < (long double) 1 && d > (long double) -1)
    return 1;
  return 0;
}
#endif /*GCALC_CHECK_WITH_FLOAT*/


void gcalc_mul_coord(Gcalc_internal_coord *result,
                     const Gcalc_internal_coord *a,
                     const Gcalc_internal_coord *b)
{
  DBUG_ASSERT(result->n_digits == a->n_digits + b->n_digits);
  int n_a, n_b, n_res;
  coord_digit_t carry= 0;

  result->set_zero();
  if (a->is_zero() || b->is_zero())
    return;

  n_a= a->n_digits - 1;
  do
  {
    n_b= b->n_digits - 1;
    do
    {
      coord2 mul= ((coord2) a->digits[n_a]) * b->digits[n_b] + carry +
        result->digits[n_a + n_b + 1];
      result->digits[n_a + n_b + 1]= mul % DIG_BASE;
      carry= mul / DIG_BASE;
    } while (n_b--);
    if (carry)
    {
      for (n_res= n_a; (result->digits[n_res]+= carry) >= DIG_BASE; n_res--)
      {
        result->digits[n_res]-= DIG_BASE;
        carry= 1;
      }
      carry= 0;
    }
  } while (n_a--);
  result->sign= a->sign != b->sign;
#ifdef GCALC_CHECK_WITH_FLOAT
  DBUG_ASSERT(de_check(a->get_double() * b->get_double(),
                       result->get_double()));
#endif /*GCALC_CHECK_WITH_FLOAT*/
}


void gcalc_add_coord(Gcalc_internal_coord *result,
                     const Gcalc_internal_coord *a,
                     const Gcalc_internal_coord *b)
{
  if (a->sign == b->sign)
    do_add(result, a, b);
  else
  {
    int cmp_res= do_cmp(a, b);
    if (cmp_res == 0)
      result->set_zero();
    else if (cmp_res > 0)
      do_sub(result, a, b);
    else
      do_sub(result, b, a);
  }
#ifdef GCALC_CHECK_WITH_FLOAT
  DBUG_ASSERT(de_check(a->get_double() + b->get_double(),
                       result->get_double()));
#endif /*GCALC_CHECK_WITH_FLOAT*/
}


void gcalc_sub_coord(Gcalc_internal_coord *result,
                     const Gcalc_internal_coord *a,
                     const Gcalc_internal_coord *b)
{
  if (a->sign != b->sign)
    do_add(result, a, b);
  else
  {
    int cmp_res= do_cmp(a, b);
    if (cmp_res == 0)
      result->set_zero();
    else if (cmp_res > 0)
      do_sub(result, a, b);
    else
    {
      do_sub(result, b, a);
      result->sign= 1 - result->sign;
    }
  }
#ifdef GCALC_CHECK_WITH_FLOAT
  DBUG_ASSERT(de_check(a->get_double() - b->get_double(),
                       result->get_double()));
#endif /*GCALC_CHECK_WITH_FLOAT*/
}


int gcalc_cmp_coord(const Gcalc_internal_coord *a,
                    const Gcalc_internal_coord *b)
{
  int result;
  if (a->sign != b->sign)
    return a->sign ? -1 : 1;
  result= a->sign ? do_cmp(b, a) : do_cmp(a, b);
#ifdef GCALC_CHECK_WITH_FLOAT
  if (result == 0)
    DBUG_ASSERT(de_check(a->get_double(), b->get_double()));
  else if (result == 1)
    DBUG_ASSERT(de_check1(a->get_double(), b->get_double()) ||
                a->get_double() > b->get_double());
  else
    DBUG_ASSERT(de_check1(a->get_double(), b->get_double()) ||
                a->get_double() < b->get_double());
#endif /*GCALC_CHECK_WITH_FLOAT*/
  return result;
}


int Gcalc_coord1::set_double(double d)
{
  double ds= d * C_SCALE;
  init();
  if ((sign= ds < 0))
    ds= -ds;
  c[0]= (coord_digit_t) (ds / (double) DIG_BASE);
  c[1]= (coord_digit_t) (ds - ((double) c[0]) * DIG_BASE);
#ifdef GCALC_CHECK_WITH_FLOAT
  DBUG_ASSERT(de_check(d, get_double()));
#endif /*GCALC_CHECK_WITH_FLOAT*/
  return 0;
} 


void Gcalc_coord1::copy(const Gcalc_coord1 *from)
{
  c[0]= from->c[0];
  c[1]= from->c[1];
  sign= from->sign;
}

/* Internal coordinates implementation end */


Gcalc_heap::Info *Gcalc_heap::new_point_info(double x, double y,
                                             gcalc_shape_info shape)
{
  Info *result= (Info *)new_item();
  if (!result)
    return NULL;
  *m_hook= result;
  m_hook= &result->next;
  m_n_points++;
  result->x= x;
  result->y= y;
  result->shape= shape;
  result->ix.set_double(x);
  result->iy.set_double(y);
  return result;
}


Gcalc_heap::Intersection_info *Gcalc_heap::new_intersection(
    const Info *p1, const Info *p2,
    const Info *p3, const Info *p4)
{
  Intersection_info *isc= (Intersection_info *)new_item();
  if (!isc)
    return 0;
  isc->p1= p1;
  isc->p2= p2;
  isc->p3= p3;
  isc->p4= p4;
  *m_intersection_hook= isc;
  m_intersection_hook= &isc->next;
  return isc;
}


class Gcalc_coord3 : public Gcalc_internal_coord
{
  coord_digit_t c[COORD_BASE*3];
  public:
  void init()
  {
    n_digits= COORD_BASE*3;
    digits= c;
  }
};


static void calc_t(Gcalc_coord2 *t_a, Gcalc_coord2 *t_b,
                   Gcalc_coord1 *b1x,
                   Gcalc_coord1 *b1y,
                   const Gcalc_heap::Info *p1,
                   const Gcalc_heap::Info *p2,
                   const Gcalc_heap::Info *p3,
                   const Gcalc_heap::Info *p4)
{
  Gcalc_coord1 a2_a1x, a2_a1y;
  Gcalc_coord1 b2x, b2y;
  Gcalc_coord2 x1y2, x2y1;

  a2_a1x.init();
  a2_a1y.init();
  x1y2.init();
  x2y1.init();
  t_a->init();
  t_b->init();
  b1y->init();
  b1x->init();
  b2x.init();
  b2y.init();

  gcalc_sub_coord(&a2_a1x, &p3->ix, &p1->ix);
  gcalc_sub_coord(&a2_a1y, &p3->iy, &p1->iy);
  gcalc_sub_coord(b1x, &p2->ix, &p1->ix);
  gcalc_sub_coord(b1y, &p2->iy, &p1->iy);
  gcalc_sub_coord(&b2x, &p4->ix, &p3->ix);
  gcalc_sub_coord(&b2y, &p4->iy, &p3->iy);

  gcalc_mul_coord(&x1y2, b1x, &b2y);
  gcalc_mul_coord(&x2y1, &b2x, b1y);
  gcalc_sub_coord(t_b, &x1y2, &x2y1);


  gcalc_mul_coord(&x1y2, &a2_a1x, &b2y);
  gcalc_mul_coord(&x2y1, &a2_a1y, &b2x);
  gcalc_sub_coord(t_a, &x1y2, &x2y1);
}


inline void calc_t(Gcalc_coord2 *t_a, Gcalc_coord2 *t_b,
                   Gcalc_coord1 *b1x,
                   Gcalc_coord1 *b1y,
                   const Gcalc_heap::Intersection_info *isc)
{
  calc_t(t_a, t_b, b1x, b1y, isc->p1, isc->p2, isc->p3, isc->p4);
}


void Gcalc_heap::Intersection_info::calc_xy(double *x, double *y) const
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
void Gcalc_heap::Intersection_info::calc_xy_ld(long double *x,
                                               long double *y) const
{
  long double b0_x= p2->x - p1->x;
  long double b0_y= p2->y - p1->y;
  long double b1_x= p4->x - p3->x;
  long double b1_y= p4->y - p3->y;
  long double b0xb1= b0_x * b1_y - b0_y * b1_x;
  long double t= (p3->x - p1->x) * b1_y - (p3->y - p1->y) * b1_x;
  long double cx, cy;

  t/= b0xb1;

  cx= (long double) p1->x + b0_x * t;
  cy= (long double) p1->y + b0_y * t;

  Gcalc_coord2 t_a, t_b;
  Gcalc_coord1 yb, xb;
  Gcalc_coord3 m1, m2, sum;

  calc_t(&t_a, &t_b, &xb, &yb, this);
  if (t_b.is_zero())
  {
    *x= p1->x;
    *y= p1->y;
    return;
  }

  m1.init();
  m2.init();
  sum.init();
  gcalc_mul_coord(&m1, &p1->ix, &t_b);
  gcalc_mul_coord(&m2, &xb, &t_a);
  gcalc_add_coord(&sum, &m1, &m2);
  *x= sum.get_double() / t_b.get_double();

  gcalc_mul_coord(&m1, &p1->iy, &t_b);
  gcalc_mul_coord(&m2, &yb, &t_a);
  gcalc_add_coord(&sum, &m1, &m2);
  *y= sum.get_double() / t_b.get_double();
}
#endif /*GCALC_CHECK_WITH_FLOAT*/


static inline void trim_node(Gcalc_heap::Info *node, Gcalc_heap::Info *prev_node)
{
  if (!node)
    return;
  DBUG_ASSERT((node->left == prev_node) || (node->right == prev_node));
  if (node->left == prev_node)
    node->left= node->right;
  node->right= NULL;
}


static int cmp_point_info(const Gcalc_heap::Info *i0,
                          const Gcalc_heap::Info *i1)
{
  int cmp_y= gcalc_cmp_coord(&i0->iy, &i1->iy);
  if (cmp_y)
    return cmp_y;
  return gcalc_cmp_coord(&i0->ix, &i1->ix);
}


static int compare_point_info(const void *e0, const void *e1)
{
  const Gcalc_heap::Info *i0= (const Gcalc_heap::Info *)e0;
  const Gcalc_heap::Info *i1= (const Gcalc_heap::Info *)e1;
  return cmp_point_info(i0, i1) > 0;
}


void Gcalc_heap::prepare_operation()
{
  DBUG_ASSERT(m_hook);
  *m_hook= NULL;
  m_first= sort_list(compare_point_info, m_first, m_n_points);
  m_hook= NULL; /* just to check it's not called twice */

  /* TODO - move this to the 'normal_scan' loop */
  for (Info *cur= get_first(); cur; cur= cur->get_next())
  {
    trim_node(cur->left, cur);
    trim_node(cur->right, cur);
  }
}


void Gcalc_heap::reset()
{
#ifdef TMP_BLOCK
  if (!m_hook)
  {
    m_hook= &m_first;
    for (; *m_hook; m_hook= &(*m_hook)->next)
    {}
  }

  *m_hook= m_free;
  m_free= m_first;
#endif /*TMP_BLOCK*/
  if (m_n_points)
  {
    free_list(m_first);
    free_list(m_first_intersection, m_intersection_hook);
    m_intersection_hook= (Gcalc_dyn_list::Item **) &m_first_intersection;
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
  DBUG_ASSERT(!m_prev || m_prev->x != x || m_prev->y != y);

  if (!(point= m_heap->new_point_info(x, y, Info)))
    return 1;
  if (m_first)
  {
    m_prev->left= point;
    point->right= m_prev;
  }
  else
    m_first= point;
  m_prev= point;
  return 0;
}


void Gcalc_shape_transporter::int_complete()
{
  DBUG_ASSERT(m_shape_started == 1 || m_shape_started == 3);

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

  DBUG_ASSERT(m_prev->x != m_first->x || m_prev->y != m_first->y);
  /* polygon */
  m_first->right= m_prev;
  m_prev->left= m_first;
}


#ifdef TMP_BLOCK
inline int GET_DX_DY(double *dxdy,
                     const Gcalc_heap::Info *p0, const Gcalc_heap::Info *p1)
{
  double dy= p1->y - p0->y;
  *dxdy= p1->x - p0->x;
  return (dy == 0.0) ||
         (*dxdy/= dy)>DBL_MAX ||
         (*dxdy)<-DBL_MAX;
}
#endif /*TMP_BLOCK*/
inline void calc_dx_dy(Gcalc_scan_iterator::point *p)
{
  gcalc_sub_coord(&p->dx, &p->next_pi->ix, &p->pi->ix);
  gcalc_sub_coord(&p->dy, &p->next_pi->iy, &p->pi->iy);
}


Gcalc_scan_iterator::Gcalc_scan_iterator(size_t blk_size) :
  Gcalc_dyn_list(blk_size,
	         (sizeof(point) > sizeof(intersection)) ?
	          sizeof(point) : sizeof(intersection))
{}
		  
Gcalc_scan_iterator::point
  *Gcalc_scan_iterator::new_slice(Gcalc_scan_iterator::point *example)
{
  point *result= NULL;
  Gcalc_dyn_list::Item **result_hook= (Gcalc_dyn_list::Item **) &result;
  while (example)
  {
    *result_hook= new_slice_point();
    result_hook= &(*result_hook)->next;
    example= example->get_next();
  }
  *result_hook= NULL;
  return result;
}


void Gcalc_scan_iterator::init(Gcalc_heap *points)
{
  DBUG_ASSERT(points->ready());
  DBUG_ASSERT(!state0.slice && !state1.slice);

  if (!(m_cur_pi= points->get_first()))
    return;
  m_heap= points;
  m_cur_thread= 0;
  m_intersections= NULL;
  m_cur_intersection= NULL;
  m_next_is_top_point= true;
  m_events= NULL;
  current_state= &state0;
  next_state= &state1;
  saved_state= &state_s;
#ifdef TMP_BLOCK
  next_state->y= m_cur_pi->y;
#endif /*TMP_BLOCK*/
  next_state->intersection_scan= 0;
  next_state->pi= m_cur_pi;
}

void Gcalc_scan_iterator::reset()
{
  state0.slice= state1.slice= m_events= state_s.slice= NULL;
  m_intersections= NULL;
  Gcalc_dyn_list::reset();
}


void Gcalc_scan_iterator::point::copy_core(const point *from)
{
#ifdef TMP_BLOCK
  dx_dy= from->dx_dy;
  horiz_dir= from->horiz_dir;
#endif /*TMP_BLOCK*/
  pi= from->pi;
  next_pi= from->next_pi;
  thread= from->thread;
  dx.copy(&from->dx);
  dy.copy(&from->dy);
#ifdef TO_REMOVE
  from->next_link= this;
#endif /*TO_REMOVE*/
}


void Gcalc_scan_iterator::point::copy_all(const point *from)
{
  pi= from->pi;
  next_pi= from->next_pi;
  thread= from->thread;
  dx.copy(&from->dx);
  dy.copy(&from->dy);
  intersection_link= from->intersection_link;
  event= from->event;
}
#ifdef TMP_BLOCK
int Gcalc_scan_iterator::point::cmp_dx_dy(int horiz_dir_a, double dx_dy_a,
                                          int horiz_dir_b, double dx_dy_b)
{
  if (!horiz_dir_a && !horiz_dir_b)
  {
    if (coord_eq(dx_dy_a, dx_dy_b))
      return 0;
    return dx_dy_a < dx_dy_b ? -1 : 1;
  }
  if (!horiz_dir_a)
    return -1;
  if (!horiz_dir_b)
    return 1;

  return 0;
}


int Gcalc_scan_iterator::point::cmp_dx_dy(const point *p) const
{
  if (is_bottom())
    return p->is_bottom() ? 0 : -1;
  if (p->is_bottom())
    return 1;
  return cmp_dx_dy(horiz_dir, dx_dy, p->horiz_dir, p->dx_dy);
}
#endif /*TMP_BLOCK*/
int Gcalc_scan_iterator::point::cmp_dx_dy(const Gcalc_coord1 *dx_a,
                                          const Gcalc_coord1 *dy_a,
                                          const Gcalc_coord1 *dx_b,
                                          const Gcalc_coord1 *dy_b)
{
  Gcalc_coord2 dx_a_dy_b;
  Gcalc_coord2 dy_a_dx_b;
  dx_a_dy_b.init();
  dy_a_dx_b.init();
  gcalc_mul_coord(&dx_a_dy_b, dx_a, dy_b);
  gcalc_mul_coord(&dy_a_dx_b, dy_a, dx_b);

  return gcalc_cmp_coord(&dx_a_dy_b, &dy_a_dx_b);
}


int Gcalc_scan_iterator::point::cmp_dx_dy(const Gcalc_heap::Info *p1,
                                          const Gcalc_heap::Info *p2,
                                          const Gcalc_heap::Info *p3,
                                          const Gcalc_heap::Info *p4)
{
  Gcalc_coord1 dx_a, dy_a, dx_b, dy_b;
  dx_a.init();
  dx_b.init();
  dy_a.init();
  dy_b.init();
  gcalc_sub_coord(&dx_a, &p2->ix, &p1->ix);
  gcalc_sub_coord(&dy_a, &p2->iy, &p1->iy);
  gcalc_sub_coord(&dx_b, &p4->ix, &p3->ix);
  gcalc_sub_coord(&dy_b, &p4->iy, &p3->iy);
  return cmp_dx_dy(&dx_a, &dy_a, &dx_b, &dy_b);
}


int Gcalc_scan_iterator::point::cmp_dx_dy(const point *p) const
{
  if (is_bottom())
    return p->is_bottom() ? 0 : -1;
  if (p->is_bottom())
    return 1;
  return cmp_dx_dy(&dx, &dy, &p->dx, &p->dy);
}


#ifdef GCALC_CHECK_WITH_FLOAT
void Gcalc_scan_iterator::point::calc_x(long double *x, long double y,
                                        long double ix) const
{
  if (fabsl(dy.get_double()) < (long double) 1e-15)
  {
    *x= ix;
  }
  else
    *x= (long double) pi->x + dx.get_double()/dy.get_double() * (y - pi->y);
}
#endif /*GCALC_CHECK_WITH_FLOAT*/


static int cmp_sp_pi(const Gcalc_scan_iterator::point *sp,
                     const Gcalc_heap::Info *pi)
{
  Gcalc_coord1 dx_pi, dy_pi;
  Gcalc_coord2 dx_sp_dy_pi, dy_sp_dx_pi;

  if (!sp->next_pi)
    return cmp_point_info(sp->pi, pi);

  dx_pi.init();
  dy_pi.init();
  dx_sp_dy_pi.init();
  dy_sp_dx_pi.init();


  gcalc_sub_coord(&dx_pi, &pi->ix, &sp->pi->ix);
  gcalc_sub_coord(&dy_pi, &pi->iy, &sp->pi->iy);
  gcalc_mul_coord(&dx_sp_dy_pi, &sp->dx, &dy_pi);
  gcalc_mul_coord(&dy_sp_dx_pi, &sp->dy, &dx_pi);

  int result= gcalc_cmp_coord(&dx_sp_dy_pi, &dy_sp_dx_pi);
#ifdef GCALC_CHECK_WITH_FLOAT
  long double sp_x;
  sp->calc_x(&sp_x, pi->y, pi->x);
  if (result == 0)
    DBUG_ASSERT(de_check1(sp->dy.get_double(), 0.0) ||
                de_check(sp_x, pi->x));
  if (result < 0)
    DBUG_ASSERT(de_check1(sp->dy.get_double(), 0.0) ||
                de_check1(sp_x, pi->x) ||
                sp_x < pi->x);
  if (result > 0)
    DBUG_ASSERT(de_check1(sp->dy.get_double(), 0.0) ||
                de_check1(sp_x, pi->x) ||
                sp_x > pi->x);
#endif /*GCALC_CHECK_WITH_FLOAT*/
  return result;
}


static void calc_cmp_sp_sp_exp(Gcalc_coord3 *result,
                               const Gcalc_scan_iterator::point *a,
                               const Gcalc_scan_iterator::point *b,
                               const Gcalc_coord1 *ly)
{
  Gcalc_coord2 x_dy, dx_ly, sum;

  x_dy.init();
  dx_ly.init();
  sum.init();
  result->init();

  gcalc_mul_coord(&x_dy, &a->pi->ix, &a->dy);
  gcalc_mul_coord(&dx_ly, &a->dx, ly);
  gcalc_add_coord(&sum, &x_dy, &dx_ly);
  gcalc_mul_coord(result, &sum, &b->dy);
}


static int cmp_sp_sp_cnt(const Gcalc_scan_iterator::point *a,
                         const Gcalc_scan_iterator::point *b,
                         const Gcalc_coord1 *y)
{
  Gcalc_coord1 lya, lyb;
  Gcalc_coord3 a_exp, b_exp;

  lya.init();
  lyb.init();
  gcalc_sub_coord(&lya, y, &a->pi->iy);
  gcalc_sub_coord(&lyb, y, &b->pi->iy);

  calc_cmp_sp_sp_exp(&a_exp, a, b, &lya);
  calc_cmp_sp_sp_exp(&b_exp, b, a, &lyb);

  int result= gcalc_cmp_coord(&a_exp, &b_exp);
#ifdef GCALC_CHECK_WITH_FLOAT
  long double a_x, b_x;
  a->calc_x(&a_x, y->get_double(), 0);
  b->calc_x(&b_x, y->get_double(), 0);
  if (result == 0)
    DBUG_ASSERT(de_check(a_x, b_x));
  if (result < 0)
    DBUG_ASSERT(de_check1(a_x, b_x) || a_x < b_x);
  if (result > 0)
    DBUG_ASSERT(de_check1(a_x, b_x) || a_x > b_x);
#endif /*GCALC_CHECK_WITH_FLOAT*/
  return result;
}


static int cmp_sp_sp(const Gcalc_scan_iterator::point *a,
                     const Gcalc_scan_iterator::point *b,
                     const Gcalc_heap::Info *pi)
{
  if (a->event == scev_none && b->event == scev_none)
    return cmp_sp_sp_cnt(a, b, &pi->iy);
  if (a->event == scev_none)
    return cmp_sp_pi(a, pi);
  if (b->event == scev_none)
    return -1 * cmp_sp_pi(b, pi);

  return 0;
}


void Gcalc_scan_iterator::mark_event_position1(
    point *ep, Gcalc_dyn_list::Item **ep_hook)
{
  if (!next_state->event_position)
  {
    next_state->event_position= ep;
    next_state->event_position_hook= ep_hook;
  }
  next_state->event_end_hook= &ep->next;
}


static int compare_events(const void *e0, const void *e1)
{
  const Gcalc_scan_iterator::point *p0= (const Gcalc_scan_iterator::point *)e0;
  const Gcalc_scan_iterator::point *p1= (const Gcalc_scan_iterator::point *)e1;
  return p0->cmp_dx_dy(p1) > 0;
}


int Gcalc_scan_iterator::arrange_event()
{
  int ev_counter;
  point *sp, *new_sp;
  point *after_event;
  Gcalc_dyn_list::Item **ae_hook= (Gcalc_dyn_list::Item **) &after_event;

  if (m_events)
    free_list(m_events);
  ev_counter= 0;
  DBUG_ASSERT(current_state->event_position ==
              *current_state->event_position_hook);
  for (sp= current_state->event_position;
       sp != *current_state->event_end_hook; sp= sp->get_next())
  {
    if (sp->is_bottom())
      continue;
    if (!(new_sp= new_slice_point()))
      return 1;
    new_sp->copy_all(sp);
    *ae_hook= new_sp;
    ae_hook= &new_sp->next;
#ifdef TO_REMOVE
    sp->intersection_link= new_sp;
#endif /*TO_REMOVE*/
    ev_counter++;
  }
  *ae_hook= NULL;
  m_events= current_state->event_position;
  if (after_event)
  {
    if (after_event->get_next())
    {
      point *cur_p;
      after_event= (point *) sort_list(compare_events, after_event, ev_counter);
      /* Find last item in the list, ae_hook can change after the sorting */
      for (cur_p= after_event->get_next(); cur_p->get_next();
           cur_p= cur_p->get_next());
      ae_hook= &cur_p->next;

    }
    *ae_hook= *current_state->event_end_hook;
    *current_state->event_end_hook= NULL;
    *current_state->event_position_hook= after_event;
    current_state->event_end_hook= ae_hook;
    current_state->event_position= after_event;
  }
  else
  {
    *current_state->event_position_hook= *current_state->event_end_hook;
    *current_state->event_end_hook= NULL;
    current_state->event_position= sp;
    current_state->event_end_hook= current_state->event_position_hook;
  }

  return 0;
}


int Gcalc_scan_iterator::insert_top_point()
{
  point *sp= next_state->slice;
  Gcalc_dyn_list::Item **prev_hook=
    (Gcalc_dyn_list::Item **) &next_state->slice;
  point *sp1;
  point *sp0= new_slice_point();
  point *sp_inc;

  if (!sp0)
    return 1;
  sp0->pi= m_cur_pi;
  sp0->next_pi= m_cur_pi->left;
  sp0->thread= m_cur_thread++;
#ifdef TMP_BLOCK
  sp0->x= coord_to_float(m_cur_pi->x);
#endif /*TMP_BLOCK*/
  if (m_cur_pi->left)
  {
    calc_dx_dy(sp0);
    sp0->event= scev_thread;

    /*Now just to increase the size of m_slice0 to be same*/
    if (!(sp_inc= new_slice_point()))
      return 1;
    sp_inc->next= current_state->slice;
    current_state->slice= sp_inc;
    if (m_cur_pi->right)
    {
      if (!(sp1= new_slice_point()))
        return 1;
      sp1->event= sp0->event= scev_two_threads;
      sp1->pi= m_cur_pi;
      sp1->next_pi= m_cur_pi->right;
      sp1->thread= m_cur_thread++;
#ifdef TMP_BLOCK
      sp1->x= sp0->x;
#endif /*TMP_BLOCK*/
      calc_dx_dy(sp1);
      /* We have two threads so should decide which one will be first */
      if (sp0->cmp_dx_dy(sp1)>0)
      {
        point *tmp= sp0;
        sp0= sp1;
        sp1= tmp;
      }

      /*Now just to increase the size of m_slice0 to be same*/
      if (!(sp_inc= new_slice_point()))
        return 1;
      sp_inc->next= current_state->slice;
      current_state->slice= sp_inc;
    }
  }
  else
  {
    sp0->event= scev_single_point;
#ifdef TMP_BLOCK
    sp0->horiz_dir= 0;
    sp0->dx_dy= 0.0;
#endif /*TMP_BLOCK*/
  }


  /* We need to find the place to insert. */
  for (; sp && cmp_sp_pi(sp, m_cur_pi) < 0;
         prev_hook= &sp->next, sp=sp->get_next())
  {}

  next_state->event_position_hook= prev_hook;
  if (sp && cmp_sp_pi(sp, m_cur_pi) == 0)
  {
    next_state->event_position= sp;
    do
    {
      if (!sp->event)
        sp->event= scev_intersection;
      prev_hook= &sp->next;
      sp= sp->get_next();
    } while (sp && cmp_sp_pi(sp, m_cur_pi) == 0);
  }
  else
    next_state->event_position= sp0;

  *prev_hook= sp0;
  if (sp0->event == scev_two_threads)
  {
    sp1->next= sp;
    sp0->next= sp1;
    next_state->event_end_hook= &sp1->next;
  }
  else
  {
    sp0->next= sp;
    next_state->event_end_hook= &sp0->next;
  }
  return 0;
}


#ifndef NO_TESTING
const char *pev(int ev)
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
  };
  return "fck";
}
extern int ca_counter;
#endif /*NO_TESTING*/
int Gcalc_scan_iterator::normal_scan()
{
  point *sp;
  Gcalc_dyn_list::Item **sp_hook;
  Gcalc_heap::Info *next_pi;
  point *first_bottom_point;

  if (m_next_is_top_point && insert_top_point())
    return 1;

  for (next_pi= m_cur_pi->get_next();
       next_pi && cmp_point_info(m_cur_pi, next_pi) == 0;
       next_pi= next_pi->get_next())
  {
    next_state->clear_event_position();
    m_next_is_top_point= true;
    first_bottom_point= NULL;
    for (sp= next_state->slice,
         sp_hook= (Gcalc_dyn_list::Item **) &next_state->slice; sp;
         sp_hook= &sp->next, sp= sp->get_next())
    {
#ifndef NO_TESTING
    //  if (ca_counter == 21)
        printf("%s%d\t", pev(sp->event), sp->thread);
#endif /*NO_TESTING*/
      if (sp->next_pi == next_pi) /* End of the segment */
      {
#ifdef TMP_BLOCK
        sp->x= coord_to_float(next_pi->x);
        sp->pi= next_pi;
#endif /*TMP_BLOCK*/
        if (cmp_point_info(sp->pi, next_pi))
          sp->pi= next_pi;
        sp->next_pi= next_pi->left;
        m_next_is_top_point= false;
        if (next_pi->is_bottom())
        {
          if (sp->event == scev_thread)
            sp->event= scev_single_point;
          else if (sp->event == scev_two_threads)
          {
            if (sp->get_next() && sp->get_next()->pi == sp->pi)
              sp->get_next()->event= scev_thread;
            else if (sp != next_state->slice)
            {
              point *fnd_sp;
              for (fnd_sp= next_state->slice; fnd_sp->get_next() != sp;
                   fnd_sp= fnd_sp->get_next());
              DBUG_ASSERT(fnd_sp->pi == sp->pi);
              fnd_sp->event= scev_thread;
            }
            sp->event= scev_single_point;
          }
          else if (first_bottom_point)
          {
            first_bottom_point->event= sp->event= scev_two_ends;
          }
          else
          {
            first_bottom_point= sp;
            sp->event= scev_end;
          }
        }
        else
        {
          if ((sp->event & (scev_point | scev_thread | scev_two_threads)) == 0)
            sp->event= scev_point;
          calc_dx_dy(sp);
        }
        mark_event_position1(sp, sp_hook);
      }
      else if (sp->event || (cmp_sp_pi(sp, next_pi) == 0))
      {
        if (!sp->event)
          sp->event= scev_intersection;
        mark_event_position1(sp, sp_hook);
      }
    }
#ifndef NO_TESTING
    //if (ca_counter == 21)
      printf("\n");
#endif /*NO_TESTING*/
    m_cur_pi= next_pi;
    if (m_next_is_top_point)
    {
      if (insert_top_point())
        return 1;
      /* Set proper values to the event position */
      /* TODO: can be done faster                */
      next_state->clear_event_position();
      if (next_state->slice->event)
        mark_event_position1(next_state->slice,
          (Gcalc_dyn_list::Item **) &next_state->slice);
#ifndef NO_TESTING
      //if (ca_counter == 21)
        printf("*%s%d\t", pev(next_state->slice->event), next_state->slice->thread);
#endif /*NO_TESTING*/
      for (sp= next_state->slice; sp->get_next(); sp= sp->get_next())
      {
#ifndef NO_TESTING
      //if (ca_counter == 21)
        printf("%s%d\t", pev(sp->get_next()->event), sp->get_next()->thread);
#endif /*NO_TESTING*/
        if (sp->get_next()->event)
          mark_event_position1(sp->get_next(), &sp->next);
      }
#ifndef NO_TESTING
      //if (ca_counter == 21)
        printf("\n");
#endif /*NO_TESTING*/
    }
  }

  /* Swap current <-> next */
  {
    slice_state *tmp= current_state;
    current_state= next_state;
    next_state= tmp;
  }

  if (arrange_event())
    return 1;

  point *sp0= current_state->slice;
  point *sp1= next_state->slice;
  point *prev_sp1= NULL;
#ifndef NO_TESTING
  //if (ca_counter == 21)
  {
    point *sp= current_state->slice;
    printf("After arrange\n");
    for (; sp; sp= sp->get_next())
      printf("%s%d\t", pev(sp->event), sp->thread);
    printf("\nEvent\n");
    for (sp= m_events; sp; sp= sp->get_next())
      printf("%s%d\t", pev(sp->event), sp->thread);
    printf("\n");
  }
#endif /*NO_TESTING*/

  if (!(m_cur_pi= next_pi))
  {
    free_list(sp1);
    next_state->slice= NULL;
#ifdef TO_REMOVE
    for (; sp0; sp0= sp0->get_next())
      sp0->next_link= NULL;
#endif /*TO_REMOVE*/
    return 0;
  }
  
  next_state->intersection_scan= 0;
  next_state->pi= m_cur_pi;
  Gcalc_heap::Info *cur_pi= m_cur_pi;
#ifdef TMP_BLOCK
  next_state->y= coord_to_float(cur_pi->y);
#endif /*TMP_BLOCK*/


  first_bottom_point= NULL;
  m_next_is_top_point= true;
  bool intersections_found= false;
  next_state->clear_event_position();

  for (; sp0; sp0= sp0->get_next())
  {
    DBUG_ASSERT(!sp0->is_bottom());
    if (sp0->next_pi == cur_pi) /* End of the segment */
    {
#ifdef TMP_BLOCK
      sp1->x= coord_to_float(cur_pi->x);
#endif /*TMP_BLOCK*/
      sp1->pi= cur_pi;
      sp1->thread= sp0->thread;
      sp1->next_pi= cur_pi->left;
#ifdef TO_REMOVE
      sp0->next_link= sp1;
#endif /*TO_REMOVE*/

      m_next_is_top_point= false;
      
      if (sp1->is_bottom())
      {
	if (!first_bottom_point)
	{
          sp1->event= scev_end;
          first_bottom_point= sp1;
	}
	else
        {
          first_bottom_point->event= sp1->event= scev_two_ends;
        }
      }
      else
      {
        sp1->event= scev_point;
        calc_dx_dy(sp1);
      }
      mark_event_position1(sp1,
        prev_sp1 ? &prev_sp1->next :
                   (Gcalc_dyn_list::Item **) &next_state->slice);
    }
    else
    {
      /* Cut current string with the height of the new point*/
      sp1->copy_core(sp0);
#ifdef TMP_BLOCK
      sp1->x= sp1->horiz_dir ? coord_to_float(cur_pi->x) :
	(coord_to_float(sp1->pi->x) +
	 (next_state->y-coord_to_float(sp1->pi->y)) * sp1->dx_dy);
#endif /*TMP_BLOCK*/
      if (cmp_sp_pi(sp1, cur_pi) == 0)
      {
        mark_event_position1(sp1,
          prev_sp1 ? &prev_sp1->next :
                     (Gcalc_dyn_list::Item **) &next_state->slice);
        sp1->event= scev_intersection;
      }
      else
        sp1->event= scev_none;
    }

    intersections_found= intersections_found ||
                         (prev_sp1 && cmp_sp_sp(prev_sp1, sp1, cur_pi) > 0);

    prev_sp1= sp1;
    sp1= sp1->get_next();
  }

  if (sp1)
  {
    if (prev_sp1)
      prev_sp1->next= NULL;
    else
      next_state->slice= NULL;
    free_list(sp1);
  }

#ifndef NO_TESTING
  //if (ca_counter == 21)
  {
    point *sp= next_state->slice;
    printf("After slice\n");
    for (; sp; sp= sp->get_next())
      printf("%s%d\t", pev(sp->event), sp->thread);
    printf("\n");
  }
#endif /*NO_TESTING*/
  if (intersections_found)
    return handle_intersections();

  return 0;
}


#ifndef NO_TESTING
int isc_counter= 0;
#endif /*NO_TESTING*/
int Gcalc_scan_iterator::add_intersection(int n_row,
                                          const point *a, const point *b,
		                          Gcalc_dyn_list::Item ***p_hook)
{
  const point *a0= a->intersection_link;
  const point *b0= b->intersection_link;
  intersection *isc= new_intersection();

  if (!isc)
    return 1;

  m_n_intersections++;
  **p_hook= isc;
  *p_hook= &isc->next;
  isc->n_row= n_row;
  isc->thread_a= a->thread;
  isc->thread_b= b->thread;

  isc->ii= m_heap->new_intersection(a0->pi, a0->next_pi,
                                    b0->pi, b0->next_pi);
#ifndef NO_TESTING
  //if (isc_counter == 40)
  {
    long double ix, iy;
    isc->ii->calc_xy_ld(&ix, &iy);
    printf("%d\t%d\t%.20LG\t%.20LG\t%d\n", isc->thread_a, isc->thread_b, ix, iy, isc->n_row);
  }
#endif /*NO_TESTING*/
  return isc->ii == NULL;
}


int Gcalc_scan_iterator::find_intersections()
{
  Gcalc_dyn_list::Item **hook;

#ifndef NO_TESTING
  ++isc_counter;
  printf("Looking for intersections\n");
#endif /*NO_TESTING*/
  m_n_intersections= 0;
  {
    /* Set links between slicepoints */
    point *sp0= current_state->slice;
    point *sp1= next_state->slice;
    for (; sp1; sp0= sp0->get_next(),sp1= sp1->get_next())
    {
      DBUG_ASSERT(!sp0->is_bottom());
      DBUG_ASSERT(sp0->thread == sp1->thread);
      sp1->intersection_link= sp0;
#ifndef NO_TESTING
      //if (isc_counter == 40)
      {
        long double spx, spx1;
        sp0->calc_x(&spx, current_state->pi->y, current_state->pi->x);
        sp1->calc_x(&spx1, m_cur_pi->y, m_cur_pi->x);
        printf("%d\t%.20LG\t%.20LG\n", sp0->thread, spx, spx1);
      }
#endif /*NO_TESTING*/
    }
  }

  hook= (Gcalc_dyn_list::Item **)&m_intersections;
  bool intersections_found;
  int n_row= 0;

  do
  {
    point **pprev_s1= &next_state->slice;
    intersections_found= false;
    n_row++;
    for (;;)
    {
      point *prev_s1= *pprev_s1;
      point *s1= prev_s1->get_next();
      if (!s1)
        break;
      if (cmp_sp_sp(prev_s1, s1, m_cur_pi) <= 0)
      {
        pprev_s1= (point **) &prev_s1->next;
        continue;
      }
      intersections_found= true;
      if (add_intersection(n_row, prev_s1, s1, &hook))
	return 1;
      *pprev_s1= s1;
      prev_s1->next= s1->next;
      s1->next= prev_s1;
      pprev_s1= (point **) &prev_s1->next;
      if (!*pprev_s1)
        break;
    };
  } while (intersections_found);

  *hook= NULL;
  return 0;
}


class Gcalc_coord4 : public Gcalc_internal_coord
{
  coord_digit_t c[COORD_BASE*4];
  public:
  void init()
  {
    n_digits= COORD_BASE*4;
    digits= c;
  }
};

class Gcalc_coord5 : public Gcalc_internal_coord
{
  coord_digit_t c[COORD_BASE*5];
  public:
  void init()
  {
    n_digits= COORD_BASE*5;
    digits= c;
  }
};


static void calc_isc_exp(Gcalc_coord5 *exp,
                         const Gcalc_coord2 *bb2,
                         const Gcalc_coord1 *ya1,
                         const Gcalc_coord2 *bb1,
                         const Gcalc_coord1 *yb1,
                         const Gcalc_coord2 *a21_b1)
{
  Gcalc_coord3 p1, p2, sum;
  p1.init();
  p2.init();
  sum.init();
  exp->init();

  gcalc_mul_coord(&p1, ya1, bb1);
  gcalc_mul_coord(&p2, yb1, a21_b1);
  gcalc_add_coord(&sum, &p1, &p2);
  gcalc_mul_coord(exp, bb2, &sum);
}


#ifdef TMP_BLOCK
static void calc_aa1_b(Gcalc_coord2 *res,
                       const Gcalc_heap::Info *a0,
                       const Gcalc_heap::Info *a1,
                       const Gcalc_coord1 *xb,
                       const Gcalc_coord1 *yb)
{
  Gcalc_coord1 aa1_x, aa1_y;
  Gcalc_coord2 p1, p2;
  res->init();
  aa1_x.init();
  aa1_y.init();
  p1.init();
  p2.init();

  gcalc_sub_coord(&aa1_x, &a1->ix, &a0->ix);
  gcalc_sub_coord(&aa1_y, &a1->iy, &a0->iy);
  gcalc_mul_coord(&p1, &aa1_x, yb);
  gcalc_mul_coord(&p2, &aa1_y, xb);
  gcalc_sub_coord(res, &p1, &p2);
}


static int cmp_intersections_y(const Gcalc_heap::Intersection_info *i1,
                               const Gcalc_heap::Intersection_info *i2)
{
  Gcalc_coord2 t_a1, t_b1;
  Gcalc_coord2 t_a2, t_b2;
  Gcalc_coord1 yb1, yb2;
  Gcalc_coord1 xb1, xb2;
  Gcalc_coord5 exp_a, exp_b;

  calc_t(&t_a1, &t_b1, &xb1, &yb1, i1);
  calc_t(&t_a2, &t_b2, &xb2, &yb2, i2);

  calc_isc_exp(&exp_a, &t_b2, &i1->p1->iy, &t_b1, &yb1, &t_a1);
  calc_isc_exp(&exp_b, &t_b1, &i2->p1->iy, &t_b2, &yb2, &t_a2);

  int result= gcalc_cmp_coord(&exp_a, &exp_b);
#ifdef GCALC_CHECK_WITH_FLOAT
  long double x1, y1, x2,y2;
  i1->calc_xy_ld(&x1, &y1);
  i2->calc_xy_ld(&x2, &y2);

  if (result == 0)
    DBUG_ASSERT(de_check(y1, y2));
  if (result < 0)
    DBUG_ASSERT(de_check1(y1, y2) || y1 < y2);
  if (result > 0)
    DBUG_ASSERT(de_check1(y1, y2) || y1 > y2);
#endif /*GCALC_CHECK_WITH_FLOAT*/
  return result;
}


static int compare_intersections(const void *e1, const void *e2)
{
  Gcalc_scan_iterator::intersection *i1= (Gcalc_scan_iterator::intersection *)e1;
  Gcalc_scan_iterator::intersection *i2= (Gcalc_scan_iterator::intersection *)e2;
  int result= cmp_intersections_y(i1->ii, i2->ii);
  if (result != 0)
    return result > 0;
  return (i1->n_row > i2->n_row);
}

#endif /*TMP_BLOCK*/

#ifndef NO_TESTING
extern int ca_counter;
#endif /*NO_TESTING*/
static int cmp_intersections(const Gcalc_heap::Intersection_info *i1,
                             const Gcalc_heap::Intersection_info *i2)
{
  Gcalc_coord2 t_a1, t_b1;
  Gcalc_coord2 t_a2, t_b2;
  Gcalc_coord1 yb1, yb2;
  Gcalc_coord1 xb1, xb2;
  Gcalc_coord5 exp_a, exp_b;
  int result;

  calc_t(&t_a1, &t_b1, &xb1, &yb1, i1);
  calc_t(&t_a2, &t_b2, &xb2, &yb2, i2);

  calc_isc_exp(&exp_a, &t_b2, &i1->p1->iy, &t_b1, &yb1, &t_a1);
  calc_isc_exp(&exp_b, &t_b1, &i2->p1->iy, &t_b2, &yb2, &t_a2);

  result= gcalc_cmp_coord(&exp_a, &exp_b);
#ifdef GCALC_CHECK_WITH_FLOAT
  long double x1, y1, x2, y2;
  i1->calc_xy_ld(&x1, &y1);
  i2->calc_xy_ld(&x2, &y2);

  if (result == 0)
    DBUG_ASSERT(de_weak(y1, y2));
#ifndef NO_TESTING
  if (ca_counter == 7)
    printf("77");
#endif /*NO_TESTING*/
  if (result < 0)
    DBUG_ASSERT(de_weak(y1, y2) || y1 < y2);
  if (result > 0)
    DBUG_ASSERT(de_weak(y1, y2) || y1 > y2);
#endif /*GCALC_CHECK_WITH_FLOAT*/

  if (result != 0)
    return result;


  calc_isc_exp(&exp_a, &t_b2, &i1->p1->ix, &t_b1, &xb1, &t_a1);
  calc_isc_exp(&exp_b, &t_b1, &i2->p1->ix, &t_b2, &xb2, &t_a2);
  result= gcalc_cmp_coord(&exp_a, &exp_b);
#ifdef GCALC_CHECK_WITH_FLOAT
  if (result == 0)
    DBUG_ASSERT(de_weak(x1, x2));
  if (result < 0)
    DBUG_ASSERT(de_weak(x1, x2) || x1 < x2);
  if (result > 0)
    DBUG_ASSERT(de_weak(x1, x2) || x1 > x2);
#endif /*GCALC_CHECK_WITH_FLOAT*/
  return result;
}

static int compare_intersections(const void *e1, const void *e2)
{
  Gcalc_scan_iterator::intersection *i1= (Gcalc_scan_iterator::intersection *)e1;
  Gcalc_scan_iterator::intersection *i2= (Gcalc_scan_iterator::intersection *)e2;
  int result= cmp_intersections(i1->ii, i2->ii);
  if (result != 0)
    return result > 0;
  return (i1->n_row > i2->n_row);
}

inline int intersections_eq(const Gcalc_heap::Intersection_info *i1,
                            const Gcalc_heap::Intersection_info *i2)
{
  return cmp_intersections(i1, i2) == 0;
}


#ifdef TMP_BLOCK
static void calc_isc_sp_exp(Gcalc_coord4 *exp,
                            const Gcalc_coord2 *bb,
                            const Gcalc_coord1 *x1,
                            const Gcalc_coord1 *y1,
                            const Gcalc_coord1 *x2,
                            const Gcalc_coord1 *y2)
{
  Gcalc_coord2 p1, p2, sum;
  p1.init();
  p2.init();
  sum.init();
  exp->init();

  gcalc_mul_coord(&p1, x1, y1);
  gcalc_mul_coord(&p2, x2, y2);
  gcalc_add_coord(&sum, &p1, &p2);
  gcalc_mul_coord(exp, bb, &sum);
}
#endif /*TMP_BLOCK*/


static int sp_isc_eq(const Gcalc_scan_iterator::point *sp,
                     const Gcalc_heap::Intersection_info *isc)
{

  Gcalc_coord4 exp_a, exp_b;
  Gcalc_coord1 xb1, yb1;
  Gcalc_coord2 t_a, t_b;
  Gcalc_coord2 t_sp_a, t_sp_b;
#ifdef TMP_BLOCK
  xa1a0.init();
  ya1a0.init();

  gcalc_sub_coord(&xa1a0, &isc->p1->ix, &sp->pi->ix);
  gcalc_sub_coord(&ya1a0, &isc->p1->iy, &sp->pi->iy);
  calc_isc_sp_exp(&exp_a, &t_b, &sp->pi->ix, &ya1a0, &sp->pi->iy, &xa1a0);
  calc_isc_sp_exp(&exp_b, &t_a, &sp->pi->iy, &isc->p1->ix,
                                &sp->pi->ix, &isc->p1->iy);
  return gcalc_cmp_coord(&exp_a, &exp_b);
#endif /*TMP_BLOCK*/
  exp_a.init();
  exp_b.init();
  calc_t(&t_a, &t_b, &xb1, &yb1, isc);
  calc_t(&t_sp_a, &t_sp_b, &xb1, &yb1, isc->p1, isc->p2, sp->pi, sp->next_pi);

  gcalc_mul_coord(&exp_a, &t_a, &t_sp_b);
  gcalc_mul_coord(&exp_b, &t_b, &t_sp_a);
  int result= gcalc_cmp_coord(&exp_a, &exp_b);
#ifdef GCALC_CHECK_WITH_FLOAT
  long double int_x, int_y, sp_x;
  isc->calc_xy_ld(&int_x, &int_y);
  sp->calc_x(&sp_x, int_y, int_x);
  if (result == 0)
    DBUG_ASSERT(de_check1(sp->dy.get_double(), 0.0) ||
                de_check(sp_x, int_x));
#ifdef TMP_BLOCK
  else
    DBUG_ASSERT(!de_check1(sp_x, int_x));
#endif /*TMP_BLOCK*/
#endif /*GCALC_CHECK_WITH_FLOAT*/
  return result == 0;
}


inline void Gcalc_scan_iterator::sort_intersections()
{
  m_intersections= (intersection *)sort_list(compare_intersections,
                                             m_intersections,m_n_intersections);
}


int Gcalc_scan_iterator::handle_intersections()
{
  DBUG_ASSERT(next_state->slice->next);

  if (find_intersections())
    return 1;
  sort_intersections();

  /* Swap saved <-> next */
  {
    slice_state *tmp= next_state;
    next_state= saved_state;
    saved_state= tmp;
  }
  /* We need the next slice to be just equal */
  next_state->slice= new_slice(saved_state->slice);
  m_cur_intersection= m_intersections;
#ifndef NO_TESTING
  //if (isc_counter == 40)
  {
    printf("Sorted\n");
    for (intersection *isc= m_intersections; isc; isc= isc->get_next())
    {
      long double ix, iy;
      isc->ii->calc_xy_ld(&ix, &iy);
      printf("%d\t%d\t%.20LG\t%.20LG\t%d\n", isc->thread_a, isc->thread_b, ix, iy, isc->n_row);
    }
  }
#endif /*NO_TESTING*/
  return intersection_scan();
}


int Gcalc_scan_iterator::intersection_scan()
{
  point *sp0, *sp1;
  Gcalc_dyn_list::Item **hook;
  intersection *next_intersection= NULL;

  if (m_cur_intersection != m_intersections)
  {
    /* Swap current <-> next */
    {
      slice_state *tmp= current_state;
      current_state= next_state;
      next_state= tmp;
    }

    if (arrange_event())
      return 1;

#ifdef TMP_BLOCK
    if (!m_cur_intersection)
    {
      saved_state->event_position_hook=
        (Gcalc_dyn_list::Item **) &saved_state->slice;

      for (sp1= saved_state->slice; sp1; sp1= sp1->get_next())
      {
        if (sp1->get_next() == saved_state->event_position)
          saved_state->event_position_hook= &sp1->next;
      }
      /* Swap saved <-> next */
      {
        slice_state *tmp= next_state;
        next_state= saved_state;
        saved_state= tmp;
      }
      free_list(saved_state->slice);
      saved_state->slice= NULL;

      free_list(m_intersections);
      m_intersections= NULL;
      return 0;
    }
#endif /*TMP_BLOCK*/
    if (!m_cur_intersection)
    {
      /* Swap saved <-> next */
      {
        slice_state *tmp= next_state;
        next_state= saved_state;
        saved_state= tmp;
      }
      Gcalc_dyn_list::Item **n_hook=
        (Gcalc_dyn_list::Item **) &next_state->slice;

      next_state->clear_event_position();
      sp0= current_state->slice;
      sp1= next_state->slice;

      for (; sp0;
           sp0= sp0->get_next(), n_hook= &sp1->next, sp1= sp1->get_next())
      {
        if (sp0->thread != sp1->thread)
        {
          point *fnd_s= sp1->get_next();
          Gcalc_dyn_list::Item **fnd_hook= &sp1->next;
          for (; fnd_s && fnd_s->thread != sp0->thread;
               fnd_hook= &fnd_s->next, fnd_s= fnd_s->get_next());
          DBUG_ASSERT(fnd_s && fnd_s == *fnd_hook);
          /* Now swap items of the next_state->slice */
          *n_hook= fnd_s;
          *fnd_hook= fnd_s->next;
          fnd_s->next= sp1;
          sp1= fnd_s;
        }
        if (sp1->event)
          mark_event_position1(sp1, n_hook);
      }
#ifndef DBUG_OFF
      sp0= current_state->slice;
      sp1= next_state->slice;
      for (; sp0; sp0= sp0->get_next(), sp1= sp1->get_next())
        DBUG_ASSERT(sp0->thread == sp1->thread);
      DBUG_ASSERT(!sp1);
#endif /*DBUG_OFF*/
      free_list(saved_state->slice);
      saved_state->slice= NULL;

      free_list(m_intersections);
      m_intersections= NULL;
      return 0;
    }
  }

  sp0= current_state->slice;
  hook= (Gcalc_dyn_list::Item **) &next_state->slice;
  sp1= next_state->slice;
  next_state->clear_event_position();
  next_state->intersection_scan= 1;
  next_state->isc= m_cur_intersection->ii;

  for (; sp0;
       hook= &sp1->next, sp1= sp1->get_next(), sp0= sp0->get_next())
  {
    if (sp0->thread == m_cur_intersection->thread_a ||
        sp0->thread == m_cur_intersection->thread_b)
    {
      DBUG_ASSERT(sp0->thread != m_cur_intersection->thread_a ||
        sp0->get_next()->thread == m_cur_intersection->thread_b ||
        sp_isc_eq(sp0->get_next(), m_cur_intersection->ii));
      sp1->copy_core(sp0);
      sp1->event= scev_intersection;
      mark_event_position1(sp1, hook);
    }
    else
    {
      sp1->copy_core(sp0);
      if (sp_isc_eq(sp1, m_cur_intersection->ii))
      {
        sp1->event= scev_intersection;
        mark_event_position1(sp1, hook);
      }
      else
        sp1->event= scev_none;
    }
  }

  if (sp1)
  {
    free_list(sp1);
    *hook= NULL;
  }

  /* Now check equal intersections */
  for (next_intersection= m_cur_intersection->get_next();
       next_intersection &&
         intersections_eq(next_intersection->ii, m_cur_intersection->ii);
       next_intersection= next_intersection->get_next())
  {
    /* Handle equal intersections. We only need to set proper events */
    sp0= current_state->slice;
    hook= (Gcalc_dyn_list::Item **) &next_state->slice;
    sp1= next_state->slice;
    next_state->clear_event_position();

    for (; sp0;
        hook= &sp1->next, sp1= sp1->get_next(), sp0= sp0->get_next())
    {
      if (sp0->thread == next_intersection->thread_a ||
          sp0->thread == next_intersection->thread_b ||
          sp1->event == scev_intersection)
      {
        sp1->event= scev_intersection;
        mark_event_position1(sp1, hook);
      }
    }
  }
  m_cur_intersection= next_intersection;

  return 0;
}


double Gcalc_scan_iterator::get_y() const
{
  if (current_state->intersection_scan)
  {
    double x, y;
    current_state->isc->calc_xy(&x, &y);
    return y;
  }
  else
    return current_state->pi->y;
}


double Gcalc_scan_iterator::get_event_x() const
{
  if (current_state->intersection_scan)
  {
    double x, y;
    current_state->isc->calc_xy(&x, &y);
    return x;
  }
  else
    return current_state->pi->x;
}

double Gcalc_scan_iterator::get_h() const
{
  double cur_y= get_y();
  double next_y;
  if (next_state->intersection_scan)
  {
    double x;
    next_state->isc->calc_xy(&x, &next_y);
  }
  else
    next_y= next_state->pi->y;
  return next_y - cur_y;
}


double Gcalc_scan_iterator::get_sp_x(const point *sp) const
{
  double dy;
  if (sp->event == scev_end | scev_two_ends | scev_point)
    return sp->pi->x;
  dy= sp->next_pi->y - sp->pi->y;
  if (fabs(dy) < 1e-12)
    return sp->pi->x;
  return (sp->next_pi->x - sp->pi->x) * dy;
}


#endif /* HAVE_SPATIAL */

