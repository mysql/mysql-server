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


static inline void trim_node(Gcalc_heap::Info *node, Gcalc_heap::Info *prev_node)
{
  if (!node)
    return;
  DBUG_ASSERT((node->left == prev_node) || (node->right == prev_node));
  if (node->left == prev_node)
    node->left= node->right;
  node->right= NULL;
}


static int compare_point_info(const void *e0, const void *e1)
{
  const Gcalc_heap::Info *i0= (const Gcalc_heap::Info *)e0;
  const Gcalc_heap::Info *i1= (const Gcalc_heap::Info *)e1;
  if (!coord_eq(i0->y, i1->y))
    return i0->y > i1->y;
  return i0->x > i1->x;
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
  if (!m_hook)
  {
    m_hook= &m_first;
    for (; *m_hook; m_hook= &(*m_hook)->next)
    {}
  }

  *m_hook= m_free;
  m_free= m_first;
  m_hook= &m_first;
  m_n_points= 0;
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


inline int GET_DX_DY(double *dxdy,
                     const Gcalc_heap::Info *p0, const Gcalc_heap::Info *p1)
{
  double dy= p1->y - p0->y;
  *dxdy= p1->x - p0->x;
  return (dy == 0.0) ||
         (*dxdy/= dy)>DBL_MAX ||
         (*dxdy)<-DBL_MAX;
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
  m_cur_thread= 0;
  m_intersections= NULL;
  m_next_is_top_point= true;
  m_events= NULL;
  current_state= &state0;
  next_state= &state1;
  saved_state= &state_s;
  next_state->y= m_cur_pi->y;
}

void Gcalc_scan_iterator::reset()
{
  state0.slice= state1.slice= m_events= state_s.slice= NULL;
  m_intersections= NULL;
  Gcalc_dyn_list::reset();
}


void Gcalc_scan_iterator::point::copy_core(point *from)
{
  dx_dy= from->dx_dy;
  horiz_dir= from->horiz_dir;
  pi= from->pi;
  next_pi= from->next_pi;
  thread= from->thread;
#ifdef TO_REMOVE
  from->next_link= this;
#endif /*TO_REMOVE*/
}


int Gcalc_scan_iterator::point::compare_dx_dy(int horiz_dir_a, double dx_dy_a,
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
  return compare_dx_dy(horiz_dir, dx_dy, p->horiz_dir, p->dx_dy);
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
    *new_sp= *sp;
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
  sp0->x= coord_to_float(m_cur_pi->x);
  if (m_cur_pi->left)
  {
    sp0->horiz_dir= GET_DX_DY(&sp0->dx_dy, m_cur_pi, m_cur_pi->left);
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
#ifdef TO_REMOVE
      sp1->event_pair= sp0;
      sp0->event_pair= sp1;
#endif /*TO_REMOVE*/
      sp1->pi= m_cur_pi;
      sp1->next_pi= m_cur_pi->right;
      sp1->thread= m_cur_thread++;
      sp1->x= sp0->x;
      sp1->horiz_dir= GET_DX_DY(&sp1->dx_dy, m_cur_pi, m_cur_pi->right);
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
    sp0->horiz_dir= 0;
    sp0->dx_dy= 0.0;
  }


  /* We need to find the place to insert. */
  for (; sp && (sp->x < sp0->x); prev_hook= &sp->next, sp=sp->get_next())
  {}

  next_state->event_position_hook= prev_hook;
  if (sp && coord_eq(sp->x, sp0->x))
  {
    next_state->event_position= sp;
    do
    {
      if (!sp->event)
        sp->event= scev_intersection;
      prev_hook= &sp->next;
      sp= sp->get_next();
    } while (sp && coord_eq(sp->x, sp0->x));
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


int Gcalc_scan_iterator::normal_scan()
{
  point *sp;
  Gcalc_dyn_list::Item **sp_hook;
  Gcalc_heap::Info *next_pi;
  point *first_bottom_point= NULL;

  if (m_next_is_top_point && insert_top_point())
    return 1;

  for (next_pi= m_cur_pi->get_next();
       next_pi &&
         coord_eq(m_cur_pi->x, next_pi->x) &&
         coord_eq(m_cur_pi->y, next_pi->y);
       next_pi= next_pi->get_next())
  {
    DBUG_ASSERT(coord_eq(next_state->event_position->x, next_pi->x));
    next_state->clear_event_position();
    m_next_is_top_point= true;
    for (sp= next_state->slice,
         sp_hook= (Gcalc_dyn_list::Item **) &next_state->slice; sp;
         sp_hook= &sp->next, sp= sp->get_next())
    {
      if (sp->next_pi == next_pi) /* End of the segment */
      {
        sp->x= coord_to_float(next_pi->x);
        sp->pi= next_pi;
        sp->next_pi= next_pi->left;
        m_next_is_top_point= false;
        if (next_pi->is_bottom())
        {
          if (first_bottom_point)
          {
            first_bottom_point->event= sp->event= scev_two_ends;
#ifdef TO_REMOVE
            first_bottom_point->event_pair= sp;
            sp->event_pair= first_bottom_point;
#endif /*TO_REMOVE*/
          }
          else
          {
            first_bottom_point= sp;
            sp->event= scev_end;
          }
        }
        else
        {
          sp->event= scev_point;
          sp->horiz_dir= GET_DX_DY(&sp->dx_dy, next_pi, next_pi->left);
        }
        mark_event_position1(sp, sp_hook);
      }
      else if (coord_eq(sp->x, next_pi->x))
      {
        if (!sp->event)
          sp->event= scev_intersection;
        mark_event_position1(sp, sp_hook);
      }
    }
    m_cur_pi= next_pi;
    if (m_next_is_top_point && insert_top_point())
      return 1;
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
  
  Gcalc_heap::Info *cur_pi= m_cur_pi;
  next_state->y= coord_to_float(cur_pi->y);


  first_bottom_point= NULL;
  m_next_is_top_point= true;
  bool intersections_found= false;
  next_state->clear_event_position();

  for (; sp0; sp0= sp0->get_next())
  {
    if (sp0->next_pi == cur_pi) /* End of the segment */
    {
      sp1->x= coord_to_float(cur_pi->x);
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
#ifdef TO_REMOVE
          sp1->event_pair= first_bottom_point;
          first_bottom_point->event_pair= sp1;
#endif /*TO_REMOVE*/
        }
      }
      else
      {
        sp1->event= scev_point;
	sp1->horiz_dir= GET_DX_DY(&sp1->dx_dy, m_cur_pi, m_cur_pi->left);
      }
      mark_event_position1(sp1,
        prev_sp1 ? &prev_sp1->next :
                   (Gcalc_dyn_list::Item **) &next_state->slice);
    }
    else
    {
      /* Cut current string with the height of the new point*/
      sp1->copy_core(sp0);
      sp1->x= sp1->horiz_dir ? coord_to_float(cur_pi->x) :
	(coord_to_float(sp1->pi->x) +
	 (next_state->y-coord_to_float(sp1->pi->y)) * sp1->dx_dy);
      if (coord_eq(sp1->x, cur_pi->x))
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
                         (prev_sp1 && prev_sp1->x > sp1->x);

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

  if (intersections_found)
    return handle_intersections();

  return 0;
}


#define INTERSECTION_ZERO 0.000000000001

int Gcalc_scan_iterator::add_intersection(int n_row,
                                          const point *a, const point *b,
		                          Gcalc_dyn_list::Item ***p_hook)
{
  intersection *isc= new_intersection();

  if (!isc)
    return 1;
  m_n_intersections++;
  **p_hook= isc;
  *p_hook= &isc->next;
  isc->n_row= n_row;
  isc->thread_a= a->thread;
  isc->thread_b= b->thread;

  /* intersection_normal */
  const point *a0= a->intersection_link;
  const point *b0= b->intersection_link;
  DBUG_ASSERT(!a0->horiz_dir || !b0->horiz_dir);

  if (!a0->horiz_dir && !b0->horiz_dir)
  {
    double b0_x= a0->next_pi->x - a0->pi->x;
    double b0_y= a0->next_pi->y - a0->pi->y;
    double b1_x= b0->next_pi->x - b0->pi->x;
    double b1_y= b0->next_pi->y - b0->pi->y;
    double b1xb0= b1_x * b0_y - b1_y * b0_x;
    double t= (a0->pi->x - b0->pi->x) * b0_y - (a0->pi->y - b0->pi->y) * b0_x;
    if (fabs(b1xb0) < INTERSECTION_ZERO)
    {
      isc->y= current_state->y;
      isc->x= a0->x;
      return 0;
    }

    t/= b1xb0;
    isc->x= b0->pi->x + b1_x*t;
    isc->y= b0->pi->y + b1_y*t;
    return 0;
  }
  isc->y= next_state->y;
  isc->x= a0->horiz_dir ? b->x : a->x;
  return 0;
}


int Gcalc_scan_iterator::find_intersections()
{
  Gcalc_dyn_list::Item **hook;

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
      if (prev_s1->x <= s1->x)
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


static int compare_intersections(const void *e0, const void *e1)
{
  Gcalc_scan_iterator::intersection *i0= (Gcalc_scan_iterator::intersection *)e0;
  Gcalc_scan_iterator::intersection *i1= (Gcalc_scan_iterator::intersection *)e1;

  if (fabs(i0->y - i1->y) > 0.00000000000001)
    return i0->y > i1->y;

  if (i0->n_row != i1->n_row)
    return i0->n_row > i1->n_row;
  if (!coord_eq(i0->y, i1->y))
    return i0->y > i1->y;
  return i0->x > i1->x;
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
  return intersection_scan();
}


int Gcalc_scan_iterator::intersection_scan()
{
  point *sp0, *sp1;
  Gcalc_dyn_list::Item **hook;
  intersection *next_intersection= NULL;
  int met_equal= 0;

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

    if (!m_cur_intersection)
    {
      saved_state->event_position_hook=
        (Gcalc_dyn_list::Item **) &saved_state->slice;
#ifdef TO_REMOVE
      for (sp0= current_state->slice, sp1= saved_state->slice;
           sp0;
           sp0= sp0->get_next(), sp1= sp1->get_next())
      {
        sp0->next_link= sp1;
        if (sp1->get_next() == saved_state->event_position)
          saved_state->event_position_hook= &sp1->next;
      }
#endif /*TO_REMOVE*/
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
  }

  next_state->y= m_cur_intersection->y;

  sp0= current_state->slice;
  hook= (Gcalc_dyn_list::Item **) &next_state->slice;
  sp1= next_state->slice;
  next_state->clear_event_position();

  for (; sp0;
       hook= &sp1->next, sp1= sp1->get_next(), sp0= sp0->get_next())
  {
    if (sp0->thread == m_cur_intersection->thread_a ||
        sp0->thread == m_cur_intersection->thread_b)
    {
#ifdef REACTIVATE_THIS
      DBUG_ASSERT(sp0->thread != m_cur_intersection->thread_a ||
        sp0->get_next()->thread == m_cur_intersection->thread_b);
#endif /*REACTIVATE_THIS*/
      sp1->copy_core(sp0);
      sp1->x= m_cur_intersection->x;
      sp1->event= scev_intersection;
      mark_event_position1(sp1, hook);
    }
    else
    {
      sp1->copy_core(sp0);
      sp1->x= sp1->horiz_dir ?
                m_cur_intersection->x :
                coord_to_float(sp1->pi->x) +
                  (next_state->y-coord_to_float(sp1->pi->y)) * sp1->dx_dy;
      if (coord_eq(sp1->x, m_cur_intersection->x))
      {
        sp1->event= scev_intersection;
        mark_event_position1(sp1, hook);
        met_equal= 1;
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

  if (met_equal)
  {
    /* Remove superfluous intersections. */
    /* Double operations can produce unexact result, so it's needed. */
    for (sp0= next_state->event_position;
        sp0 != *next_state->event_end_hook;
        sp0= sp0->get_next())
    {
      for (sp1= sp0->get_next();
          sp1 != *next_state->event_end_hook;
          sp1= sp1->get_next())
      {
        intersection *isc= m_cur_intersection;
        while (isc->get_next())
        {
          intersection *cur_isc= isc->get_next();
          if ((cur_isc->thread_a == sp0->thread &&
                cur_isc->thread_b == sp1->thread) ||
              (cur_isc->thread_a == sp1->thread &&
               cur_isc->thread_b == sp0->thread))
          {
            /* The superfluous intersection should be close to the current. */
            DBUG_ASSERT(fabs(cur_isc->x-m_cur_intersection->x) +
                        fabs(cur_isc->y-m_cur_intersection->y) <
                        INTERSECTION_ZERO);
            isc->next= isc->next->next;
            free_item(cur_isc);
          }
          else
            isc= isc->get_next();
        }
      }
    }
  }

  for (next_intersection= m_cur_intersection->get_next();
       next_intersection &&
         coord_eq(next_intersection->x, m_cur_intersection->x) &&
         coord_eq(next_intersection->y, m_cur_intersection->y);
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

#endif /* HAVE_SPATIAL */

