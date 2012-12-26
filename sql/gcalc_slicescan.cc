/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

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


#include "sql_string.h"

#ifdef HAVE_SPATIAL

#include "gcalc_slicescan.h"


#define PH_DATA_OFFSET 8
#define coord_to_float(d) ((double) d)

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
#ifndef DBUG_OFF
  m_last_item_id(0),
#endif
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


bool Gcalc_dyn_list::alloc_new_blk()
{
  void *new_block= my_malloc(m_blk_size, MYF(MY_WME));
  if (!new_block)
    return true;
  *m_blk_hook= new_block;
  m_blk_hook= (void**)new_block;
  format_blk(new_block);
  return false;
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


static inline void trim_node(Gcalc_heap::Info *node, Gcalc_heap::Info *prev_node)
{
  if (!node)
    return;
  DBUG_ASSERT((node->left == prev_node) || (node->right == prev_node));
  if (node->left == prev_node)
    node->left= node->right;
  node->right= NULL;
}


static double find_first_different(const Gcalc_heap::Info *p)
{
  if (p->left && (p->left->y != p->y))
    return p->left->y;
  if (p->right && (p->right->y != p->y))
    return p->right->y;
  if (p->left && p->left->left && (p->left->left->y != p->y))
    return p->left->left->y;
  if (p->right && p->right->right && (p->right->right->y != p->y))
    return p->right->right->y;

  return p->y;
}


static int compare_point_info(const void *e0, const void *e1)
{
  const Gcalc_heap::Info *i0= (const Gcalc_heap::Info *)e0;
  const Gcalc_heap::Info *i1= (const Gcalc_heap::Info *)e1;
  if (i0->y != i1->y)
    return i0->y > i1->y;
  double i0_fd= find_first_different(i0);
  double i1_fd= find_first_different(i1);
  if (i0_fd != i1_fd)
    return i0_fd > i1_fd;
  return i0->x > i1->x;
}


#ifndef DBUG_OFF
void Gcalc_heap::Info::dbug_print() const
{
  char left_str[64]= "", right_str[64]= "";
  if (left)
    my_snprintf(left_str, sizeof(left_str),
                "(%g,%g,#%u)", left->x, left->y, left->shape);
  if (right)
    my_snprintf(right_str, sizeof(right_str), "(%g,%g,#%u)",
                right->x, right->y, right->shape);
  DBUG_PRINT("info", ("(%g,%g,#%u) left=%s right=%s",
                     x, y, shape, left_str, right_str));
}
#endif


void Gcalc_heap::prepare_operation()
{
  DBUG_ENTER("Gcalc_heap::prepare_operation");
  DBUG_PRINT("info", ("m_n_points=%d", m_n_points));
  DBUG_ASSERT(m_hook);
  *m_hook= NULL;
  m_first= sort_list(compare_point_info, m_first, m_n_points);
  m_hook= NULL; /* just to check it's not called twice */

  DBUG_PRINT("info", ("after sort_list:"));
  /* TODO - move this to the 'normal_scan' loop */
  for (Info *cur= get_first(); cur; cur= cur->get_next())
  {
    trim_node(cur->left, cur);
    trim_node(cur->right, cur);
#ifndef DBUG_OFF
    cur->dbug_print();
#endif
  }
  DBUG_VOID_RETURN;
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
	          sizeof(point) : sizeof(intersection)),
  m_slice0(NULL), m_slice1(NULL)
{}
		  
Gcalc_scan_iterator::point
  *Gcalc_scan_iterator::new_slice(Gcalc_scan_iterator::point *example)
{
  Gcalc_dyn_list::Item *item_result= NULL;
  Gcalc_dyn_list::Item **result_hook= &item_result;
  while (example)
  {
    *result_hook= new_slice_point();
    result_hook= &(*result_hook)->next;
    example= example->get_next();
  }
  *result_hook= NULL;
  point *result= static_cast<point*>(item_result);
  return result;
}


void Gcalc_scan_iterator::init(Gcalc_heap *points)
{
  DBUG_ASSERT(points->ready());
  DBUG_ASSERT(!m_slice0 && !m_slice1);

  if (!(m_cur_pi= points->get_first()))
    return;
  m_cur_thread= 0;
  m_sav_slice= NULL;
  m_intersections= NULL;
  m_cur_intersection= NULL;
  m_y1= m_cur_pi->y;
  m_next_is_top_point= true;
  m_bottom_points_count= 0;
}

void Gcalc_scan_iterator::reset()
{
  if (m_slice0)
    free_list(m_slice0);
  if (m_slice1)
    free_list(m_slice1);
  m_slice0= m_slice1= NULL;
  Gcalc_dyn_list::reset();
}

static bool slice_first_equal_x(const Gcalc_scan_iterator::point *p0,
				const Gcalc_scan_iterator::point *p1)
{
  if (p0->horiz_dir == p1->horiz_dir)
    return p0->dx_dy <= p1->dx_dy;
  if (p0->horiz_dir)
    return p0->dx_dy < 0;
  return p1->dx_dy > 0;  /* p1->horiz_dir case */
}


static inline bool slice_first(const Gcalc_scan_iterator::point *p0,
			       const Gcalc_scan_iterator::point *p1)
{
  if (p0->x != p1->x)
    return p0->x < p1->x;
  return slice_first_equal_x(p0, p1);
}


int Gcalc_scan_iterator::insert_top_point()
{
  point *sp0= new_slice_point();
  if (!sp0)
    return 1;

  sp0->pi= m_cur_pi;
  sp0->next_pi= m_cur_pi->left;
  sp0->thread= m_cur_thread++;
  sp0->x= coord_to_float(m_cur_pi->x);
  if (m_cur_pi->left)
  {
    sp0->horiz_dir= GET_DX_DY(&sp0->dx_dy, m_cur_pi, m_cur_pi->left);
    m_event1= scev_thread;

    /*Now just to increase the size of m_slice0 to be same*/
    point *sp1= new_slice_point();
    if (!sp1)
      return 1;
    sp1->next= m_slice0;
    m_slice0= sp1;
  }
  else
  {
    m_event1= scev_single_point;
    sp0->horiz_dir= 0;
    sp0->dx_dy= 0.0;
  }

  /* First we need to find the place to insert.
     Binary search could probably make things faster here,
     but structures used aren't suitable, and the
     scan is usually not really long */
  point *sp= m_slice1;
  point **prev_hook= &m_slice1;
  for (; sp && slice_first(sp, sp0); sp=sp->get_next())
  {
    prev_hook= reinterpret_cast<point**>(&(sp->next));
  }

  if (m_cur_pi->right)
  {
    m_event1= scev_two_threads;
    /*We have two threads so should decide which one will be first*/
    point *sp1= new_slice_point();
    if (!sp1)
      return 1;
    sp1->pi= m_cur_pi;
    sp1->next_pi= m_cur_pi->right;
    sp1->thread= m_cur_thread++;
    sp1->x= sp0->x;
    sp1->horiz_dir= GET_DX_DY(&sp1->dx_dy, m_cur_pi, m_cur_pi->right);
    // Find the slice with stronger left gradient
    if (slice_first_equal_x(sp1, sp0))
    {
      point *tmp= sp0;
      sp0= sp1;
      sp1= tmp;
    }
    sp1->next= sp;
    sp0->next= sp1;
    
    /*Now just to increase the size of m_slice0 to be same*/
    if (!(sp1= new_slice_point()))
      return 1;
    sp1->next= m_slice0;
    m_slice0= sp1;
  }
  else
    sp0->next= sp;

  *prev_hook= sp0;
  m_event_position1= sp0;

  return 0;
}

enum
{
  intersection_normal= 1,
  intersection_forced= 2
};


static int intersection_found(const Gcalc_scan_iterator::point *sp0,
			      const Gcalc_scan_iterator::point *sp1,
			      unsigned int bottom_points_count)
{
  if (sp1->x < sp0->x)
    return intersection_normal;
  if (sp1->is_bottom() && !sp0->is_bottom() &&
      (bottom_points_count > 1))
      return intersection_forced;
  return 0;
}


#ifndef DBUG_OFF
const char *Gcalc_scan_event_name(enum Gcalc_scan_events event)
{
  switch (event)
  {
    case scev_point:        return "scev_point";
    case scev_thread:       return "scev_thread";
    case scev_two_threads:  return "scev_two_threads";
    case scev_intersection: return "scev_intersection";
    case scev_end:          return "scev_end";
    case scev_two_ends:     return "scev_two_ends";
    case scev_single_point: return "scev_single_point";
  }
  return "scev_unknown";
}


void Gcalc_scan_iterator::point::dbug_print_slice(double y,
                                                  enum Gcalc_scan_events event)
                                                  const
{
  DBUG_PRINT("info", ("y=%g event=%s", y, Gcalc_scan_event_name(event)));
  for (const point *slice= this ; slice ; slice= slice->get_next())
  {
    if (slice->next_pi)
      DBUG_PRINT("into", ("(x=%g,thr#%d) pi=(%g,%g,#%u) next_pi=(%g,%g,#%u)",
                           slice->x, slice->thread,
                           slice->pi->x, slice->pi->y, slice->pi->shape,
                           slice->next_pi->x, slice->next_pi->y,
                           slice->next_pi->shape));
    else
      DBUG_PRINT("info", ("(x=%g,thr#%d) pi=(%g,%g,#%u)",
                           slice->x, slice->thread,
                           slice->pi->x, slice->pi->y, slice->pi->shape));
  }
}
#endif /* DBUG_OFF */


int Gcalc_scan_iterator::normal_scan()
{
  if (m_next_is_top_point)
    if (insert_top_point())
      return 1;

#ifndef DBUG_OFF
  m_slice1->dbug_print_slice(m_y1, m_event1);
#endif

  point *tmp= m_slice0;
  m_slice0= m_slice1;
  m_slice1= tmp;
  m_event0= m_event1;
  m_event_position0= m_event_position1;
  m_y0= m_y1;
  
  if (!(m_cur_pi= m_cur_pi->get_next()))
  {
    free_list(m_slice1);
    m_slice1= NULL;
    return 0;
  }
  
  Gcalc_heap::Info *cur_pi= m_cur_pi;
  m_y1= coord_to_float(cur_pi->y);
  m_h= m_y1 - m_y0; // vertical distance between slices

  point *sp0= m_slice0;
  point *sp1= m_slice1;
  point *prev_sp1= NULL;

  m_bottom_points_count= 0;
  m_next_is_top_point= true;
  bool intersections_found= false;

  for (; sp0; sp0= sp0->get_next())
  {
    if (sp0->next_pi == cur_pi) /* End of the segment */
    {
      sp1->x= coord_to_float(cur_pi->x);
      sp1->pi= cur_pi;
      sp1->thread= sp0->thread;
      sp1->next_pi= cur_pi->left;
      if (cur_pi->left)
	sp1->horiz_dir= GET_DX_DY(&sp1->dx_dy, m_cur_pi, m_cur_pi->left);

      m_next_is_top_point= false;
      
      if (sp1->is_bottom())
      {
	++m_bottom_points_count;
	if (m_bottom_points_count == 1)
	{
	  m_event1= scev_end;
	  m_event_position1= sp1;
	}
	else
	  m_event1= scev_two_ends;
      }
      else
      {
	m_event1= scev_point;
	m_event_position1= sp1;
      }
    }
    else if (!sp0->is_bottom())
    {
      /* Cut current string with the height of the new point*/
      sp1->copy_core(sp0);
      sp1->x= sp1->horiz_dir ? sp0->x :
	(coord_to_float(sp1->pi->x) +
	 (m_y1-coord_to_float(sp1->pi->y)) * sp1->dx_dy);
    }
    else  /* Skip the bottom point in slice0 */
      continue;

    intersections_found= intersections_found ||
      (prev_sp1 && intersection_found(prev_sp1, sp1, m_bottom_points_count));

    prev_sp1= sp1;
    sp1= sp1->get_next();
  }

  if (sp1)
  {
    if (prev_sp1)
      prev_sp1->next= NULL;
    else
      m_slice1= NULL;
    free_list(sp1);
  }

  if (intersections_found)
    return handle_intersections();

  return 0;
}


int Gcalc_scan_iterator::add_intersection(const point *a, const point *b,
				   int isc_kind, Gcalc_dyn_list::Item ***p_hook)
{
  intersection *isc= new_intersection();

  if (!isc)
    return 1;
  m_n_intersections++;
  **p_hook= isc;
  *p_hook= &isc->next;
  isc->thread_a= a->thread;
  isc->thread_b= b->thread;
  if (isc_kind == intersection_forced)
  {
    isc->y= m_y1;
    isc->x= a->x;
    return 0;
  }

  /* intersection_normal */
  const point *a0= a->precursor;
  const point *b0= b->precursor;
  if (!a0->horiz_dir && !b0->horiz_dir)
  {
    double dk= a0->dx_dy - b0->dx_dy;
    double dy= (b0->x - a0->x)/dk;
    isc->y= m_y0 + dy;
    isc->x= a0->x + dy*a0->dx_dy;
    return 0;
  }
  isc->y= m_y1;
  isc->x= a0->horiz_dir ? b->x : a->x;
  return 0;
}


int Gcalc_scan_iterator::find_intersections()
{
  point *sp1= m_slice1;

  m_n_intersections= 0;
  {
    /* Set links between slicepoints */
    point *sp0= m_slice0;
    for (; sp1; sp0= sp0->get_next(),sp1= sp1->get_next())
    {
      while (sp0->is_bottom())
	sp0= sp0->get_next();
      DBUG_ASSERT(sp0->thread == sp1->thread);
      sp1->precursor= sp0;
    }
  }

  Gcalc_dyn_list::Item **hook=
    reinterpret_cast<Gcalc_dyn_list::Item **>(&m_intersections);
  bool intersections_found;

  point *last_possible_isc= NULL;
  do
  {
    sp1= m_slice1;
    point **pprev_s1= &m_slice1;
    intersections_found= false;
    unsigned int bottom_points_count= sp1->is_bottom() ? 1:0;
    sp1= m_slice1->get_next();
    int isc_kind;
    point *cur_possible_isc= NULL;
    for (; sp1 != last_possible_isc;
	 pprev_s1= (point **)(&(*pprev_s1)->next), sp1= sp1->get_next())
    {
      if (sp1->is_bottom())
	++bottom_points_count;
      if (!(isc_kind=intersection_found(*pprev_s1, sp1, bottom_points_count)))
	continue;
      point *prev_s1= *pprev_s1;
      intersections_found= true;
      if (add_intersection(prev_s1, sp1, isc_kind, &hook))
	return 1;
      *pprev_s1= sp1;
      prev_s1->next= sp1->next;
      sp1->next= prev_s1;
      sp1= prev_s1;
      cur_possible_isc= sp1;
    }
    last_possible_isc= cur_possible_isc;
  } while (intersections_found);

  *hook= NULL;
  return 0;
}


static int compare_intersections(const void *e0, const void *e1)
{
  Gcalc_scan_iterator::intersection *i0= (Gcalc_scan_iterator::intersection *)e0;
  Gcalc_scan_iterator::intersection *i1= (Gcalc_scan_iterator::intersection *)e1;
  return i0->y > i1->y;
}


inline void Gcalc_scan_iterator::sort_intersections()
{
  m_intersections= (intersection *)sort_list(compare_intersections,
                                             m_intersections,m_n_intersections);
}


int Gcalc_scan_iterator::handle_intersections()
{
  DBUG_ASSERT(m_slice1->next);

  if (find_intersections())
    return 1;
  sort_intersections();

  m_sav_slice= m_slice1;
  m_sav_y= m_y1;
  m_slice1= new_slice(m_sav_slice);
  
  m_cur_intersection= m_intersections;
  m_pre_intersection_hook= NULL;
  return intersection_scan();
}


void Gcalc_scan_iterator::pop_suitable_intersection()
{
  intersection *prev_i= m_cur_intersection;
  intersection *cur_i= prev_i->get_next();
  for (; cur_i; prev_i= cur_i, cur_i= cur_i->get_next())
  {
    point *prev_p= m_slice0;
    point *sp= prev_p->get_next();
    for (; sp; prev_p= sp, sp= sp->get_next())
    {
      if ((prev_p->thread == cur_i->thread_a) &&
	  (sp->thread == cur_i->thread_b))
      {
	/* Move cur_t on the top of the list */
	if (prev_i == m_cur_intersection)
	{
	  m_cur_intersection->next= cur_i->next;
	  cur_i->next= m_cur_intersection;
	  m_cur_intersection= cur_i;
	}
	else
	{
          Gcalc_dyn_list::Item *tmp= m_cur_intersection->next;
	  m_cur_intersection->next= cur_i->next;
	  prev_i->next= m_cur_intersection;
	  m_cur_intersection= cur_i;
	  cur_i->next= tmp;
	}
	return;
      }
    }
  }
  DBUG_ASSERT(0);
}


int Gcalc_scan_iterator::intersection_scan()
{
  if (m_pre_intersection_hook) /*Skip the first point*/
  {
    point *next= (*m_pre_intersection_hook)->get_next();
    (*m_pre_intersection_hook)->next= next->next;
    next->next= *m_pre_intersection_hook;
    *m_pre_intersection_hook= next;
    m_event0= scev_intersection;
    m_event_position0= next;
    point *tmp= m_slice1;
    m_slice1= m_slice0;
    m_slice0= tmp;
    m_y0= m_y1;
    m_cur_intersection= m_cur_intersection->get_next();
    if (!m_cur_intersection)
    {
      m_h= m_sav_y - m_y1;
      m_y1= m_sav_y;
      free_list(m_slice1);
      m_slice1= m_sav_slice;
      free_list(m_intersections);
      return 0;
    }
  }

  m_y1= m_cur_intersection->y;
  m_h= m_y1 - m_y0;

  point *sp0;
  point **psp1;

redo_loop:
  sp0= m_slice0;
  psp1= &m_slice1;
  for (; sp0; sp0= sp0->get_next())
  {
    point *sp1= *psp1;
    if (sp0->thread == m_cur_intersection->thread_a)
    {
      point *next_s0= sp0;
      /* Skip Bottom points */
      do
	next_s0= next_s0->get_next();
      while(next_s0->is_bottom()); /* We always find nonbottom point here*/
      /* If the next point's thread isn't the thread of intersection,
	 we try to find suitable intersection */
      if (next_s0->thread != m_cur_intersection->thread_b)
      {
	/* It's really rare case - sometimes happen when
	   there's two intersections with the same Y
	   Move suitable one to the beginning of the list
	*/
	pop_suitable_intersection();
	goto redo_loop;
      }
      m_pre_intersection_hook= psp1;
      sp1->copy_core(sp0);
      sp1->x= m_cur_intersection->x;
      sp0= next_s0;
      sp1= sp1->get_next();
      sp1->copy_core(sp0);
      sp1->x= m_cur_intersection->x;
      psp1= (point **)&sp1->next;
      continue;
    }
    if (!sp0->is_bottom())
    {
      sp1->copy_core(sp0);
      sp1->x= sp1->horiz_dir ? sp0->x :
	(coord_to_float(sp1->pi->x) +
	 (m_y1-coord_to_float(sp1->pi->y)) * sp1->dx_dy);
    }
    else
      /* Skip bottom point */
      continue;
    psp1= (point **)&sp1->next;
  }

  if (*psp1)
  {
    free_list(*psp1);
    *psp1= NULL;
  }

  return 0;
}

#endif /* HAVE_SPATIAL */
