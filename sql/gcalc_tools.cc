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


#include "my_global.h"

#ifdef HAVE_SPATIAL

#include "sql_string.h"
#include "gcalc_tools.h"
#include "gstream.h"                            // Gis_read_stream
#include "spatial.h"

#define float_to_coord(d) ((double) d)


/*
  Adds new shape to the relation.
  After that it can be used as an argument of an operation.
*/

gcalc_shape_info Gcalc_function::add_new_shape(uint32 shape_id,
                                               shape_type shape_kind)
{
  shapes_buffer.q_append((uint32) shape_kind);
  return n_shapes++;
}


/*
  Adds new operation to the constructed relation.
  To construct the complex relation one has to specify operations
  in prefix style.
*/

void Gcalc_function::add_operation(op_type operation, uint32 n_operands)
{
  uint32 op_code= (uint32 ) operation + n_operands;
  function_buffer.q_append(op_code);
}


/*
  Sometimes the number of arguments is unknown at the moment the operation
  is added. That allows to specify it later.
*/

void Gcalc_function::add_operands_to_op(uint32 operation_pos, uint32 n_operands)
{
  uint32 op_code= uint4korr(function_buffer.ptr() + operation_pos) + n_operands;
  function_buffer.write_at_position(operation_pos, op_code);
}


/*
  Just like the add_operation() but the result will be the inverted
  value of an operation.
*/

void Gcalc_function::add_not_operation(op_type operation, uint32 n_operands)
{
  uint32 op_code= ((uint32) op_not | (uint32 ) operation) + n_operands;
  function_buffer.q_append(op_code);
}


int Gcalc_function::single_shape_op(shape_type shape_kind, gcalc_shape_info *si)
{
  if (reserve_shape_buffer(1) || reserve_op_buffer(1))
    return 1;
  *si= add_new_shape(0, shape_kind);
  add_operation(op_shape, *si);
  return 0;
}


/*
  Specify how many arguments we're going to have.
*/

int Gcalc_function::reserve_shape_buffer(uint n_shapes)
{
  return shapes_buffer.reserve(n_shapes * 4, 512);
}


/*
  Specify how many operations we're going to have.
*/

int Gcalc_function::reserve_op_buffer(uint n_ops)
{
  return function_buffer.reserve(n_ops * 4, 512);
}


int Gcalc_function::alloc_states()
{
  if (function_buffer.reserve((n_shapes+1) * sizeof(int)))
    return 1;
  i_states= (int *) (function_buffer.ptr() + ALIGN_SIZE(function_buffer.length()));
  return 0;
}


int Gcalc_function::count_internal()
{
  int c_op= uint4korr(cur_func);
  op_type next_func= (op_type) (c_op & op_any);
  int mask= (c_op & op_not) ? 1:0;
  int n_ops= c_op & ~op_any;
  int result;

  cur_func+= 4;
  if (next_func == op_shape)
    return i_states[c_op & ~(op_any | op_not)] ^ mask;

  result= count_internal();

  while (--n_ops)
  {
    int next_res= count_internal();
    switch (next_func)
    {
      case op_union:
        result= result | next_res;
        break;
      case op_intersection:
        result= result & next_res;
        break;
      case op_symdifference:
        result= result ^ next_res;
        break;
      case op_difference:
        result= result & !next_res;
        break;
      case op_backdifference:
        result= !result & next_res;
        break;
      default:
        DBUG_ASSERT(FALSE);
    };
  }

  return result ^ mask;
}


/*
  Clear the state of the object.
*/

void Gcalc_function::reset()
{
  n_shapes= 0;
  shapes_buffer.length(0);
  function_buffer.length(0);
}


int Gcalc_function::find_function(Gcalc_scan_iterator &scan_it)
{
  while (scan_it.more_points())
  {
    if (scan_it.step())
      return -1;
    Gcalc_scan_events ev= scan_it.get_event();
    const Gcalc_scan_iterator::point *evpos= scan_it.get_event_position();
    if (ev & (scev_point | scev_end | scev_two_ends))
      continue;

    clear_state();
    for (Gcalc_point_iterator pit(&scan_it); pit.point() != evpos; ++pit)
    {
      gcalc_shape_info si= pit.point()->get_shape();
      if ((get_shape_kind(si) == Gcalc_function::shape_polygon))
        invert_state(si);
    }
    invert_state(evpos->get_shape());

    if (ev == scev_intersection)
    {
      const Gcalc_scan_iterator::point *evnext= evpos->c_get_next();
      if ((get_shape_kind(evpos->get_shape()) !=
             Gcalc_function::shape_polygon)             ||
          (get_shape_kind(evnext->get_shape()) !=
             Gcalc_function::shape_polygon))
        invert_state(evnext->get_shape());
    }

    if (count())
      return 1;
  }
  return 0;
}


int Gcalc_operation_transporter::single_point(double x, double y)
{
  gcalc_shape_info si;
  return m_fn->single_shape_op(Gcalc_function::shape_point, &si) ||
         int_single_point(si, x, y);
}


int Gcalc_operation_transporter::start_line()
{
  int_start_line();
  return m_fn->single_shape_op(Gcalc_function::shape_line, &m_si);
}


int Gcalc_operation_transporter::complete_line()
{
  int_complete_line();
  return 0;
}


int Gcalc_operation_transporter::start_poly()
{
  int_start_poly();
  return m_fn->single_shape_op(Gcalc_function::shape_polygon, &m_si);
}


int Gcalc_operation_transporter::complete_poly()
{
  int_complete_poly();
  return 0;
}


int Gcalc_operation_transporter::start_ring()
{
  int_start_ring();
  return 0;
}


int Gcalc_operation_transporter::complete_ring()
{
  int_complete_ring();
  return 0;
}


int Gcalc_operation_transporter::add_point(double x, double y)
{
  return int_add_point(m_si, x, y);
}


int Gcalc_operation_transporter::start_collection(int n_objects)
{
  if (m_fn->reserve_shape_buffer(n_objects) || m_fn->reserve_op_buffer(1))
        return 1;
  m_fn->add_operation(Gcalc_function::op_union, n_objects);
  return 0;
}


int Gcalc_result_receiver::start_shape(Gcalc_function::shape_type shape)
{
  if (buffer.reserve(4*2, 512))
    return 1;
  cur_shape= shape;
  shape_pos= buffer.length();
  buffer.length(shape_pos + ((shape == Gcalc_function::shape_point) ? 4:8));
  n_points= 0;
  shape_area= 0.0;

  return 0;
}


int Gcalc_result_receiver::add_point(double x, double y)
{
  if (n_points && x == prev_x && y == prev_y)
    return 0;

  if (!n_points++)
  {
    prev_x= first_x= x;
    prev_y= first_y= y;
    return 0;
  }

  shape_area+= prev_x*y - prev_y*x;

  if (buffer.reserve(8*2, 512))
    return 1;
  buffer.q_append(prev_x);
  buffer.q_append(prev_y);
  prev_x= x;
  prev_y= y;
  return 0;
}


int Gcalc_result_receiver::complete_shape()
{
  if (n_points == 0)
  {
    buffer.length(shape_pos);
    return 0;
  }
  if (n_points == 1)
  {
    if (cur_shape != Gcalc_function::shape_point)
    {
      cur_shape= Gcalc_function::shape_point;
      buffer.length(buffer.length()-4);
    }
  }
  else
  {
    DBUG_ASSERT(cur_shape != Gcalc_function::shape_point);
    if (cur_shape == Gcalc_function::shape_hole)
    {
      shape_area+= prev_x*first_y - prev_y*first_x;
      if (fabs(shape_area) < 1e-8)
      {
        buffer.length(shape_pos);
        return 0;
      }
    }

    if ((cur_shape == Gcalc_function::shape_polygon ||
          cur_shape == Gcalc_function::shape_hole) &&
        prev_x == first_x && prev_y == first_y)
    {
      n_points--;
      buffer.write_at_position(shape_pos+4, n_points);
      goto do_complete;
    }
    buffer.write_at_position(shape_pos+4, n_points);
  }

  if (buffer.reserve(8*2, 512))
    return 1;
  buffer.q_append(prev_x);
  buffer.q_append(prev_y);
  
do_complete:
  buffer.write_at_position(shape_pos, (uint32) cur_shape);

  if (!n_shapes++)
  {
    DBUG_ASSERT(cur_shape != Gcalc_function::shape_hole);
    common_shapetype= cur_shape;
  }
  else if (cur_shape == Gcalc_function::shape_hole)
  {
    ++n_holes;
  }
  else if (!collection_result && (cur_shape != common_shapetype))
  {
      collection_result= true;
  }
  return 0;
}


int Gcalc_result_receiver::single_point(double x, double y)
{
  return start_shape(Gcalc_function::shape_point) ||
         add_point(x, y) ||
         complete_shape();
}


int Gcalc_result_receiver::done()
{
  return 0;
}


void Gcalc_result_receiver::reset()
{
  buffer.length(0);
  collection_result= FALSE;
  n_shapes= n_holes= 0;
}


int Gcalc_result_receiver::get_result_typeid()
{
  if (!n_shapes)
    return 0;

  if (collection_result)
    return Geometry::wkb_geometrycollection;
  switch (common_shapetype)
  {
    case Gcalc_function::shape_polygon:
      return (n_shapes - n_holes == 1) ?
              Geometry::wkb_polygon : Geometry::wkb_multipolygon;
    case Gcalc_function::shape_point:
      return (n_shapes == 1) ? Geometry::wkb_point : Geometry::wkb_multipoint;
    case Gcalc_function::shape_line:
      return (n_shapes == 1) ? Geometry::wkb_linestring :
                               Geometry::wkb_multilinestring;
    default:
      DBUG_ASSERT(0);
  }
  return 0;
}


int Gcalc_result_receiver::move_hole(uint32 dest_position, uint32 source_position,
                                     uint32 *new_dest_position)
{
  char *ptr;
  int source_len;
  if (dest_position == source_position)
  {
    *new_dest_position= position();
    return 0;
  }

  source_len= buffer.length() - source_position;
  if (buffer.reserve(source_len, MY_ALIGN(source_len, 512)))
    return 1;

  ptr= (char *) buffer.ptr();
  memmove(ptr + dest_position + source_len, ptr + dest_position,
          buffer.length() - dest_position);
  memcpy(ptr + dest_position, ptr + buffer.length(), source_len);
  *new_dest_position= dest_position + source_len;
  return 0;
}


Gcalc_operation_reducer::Gcalc_operation_reducer(size_t blk_size) :
  Gcalc_dyn_list(blk_size, sizeof(res_point)),
  m_res_hook((Gcalc_dyn_list::Item **)&m_result),
  m_first_active_thread(NULL)
{}


void Gcalc_operation_reducer::init(Gcalc_function *fn, modes mode)
{
  m_fn= fn;
  m_mode= mode;
  m_first_active_thread= NULL;
}


Gcalc_operation_reducer::
Gcalc_operation_reducer(Gcalc_function *fn, modes mode, size_t blk_size) :
  Gcalc_dyn_list(blk_size, sizeof(res_point)),
  m_res_hook((Gcalc_dyn_list::Item **)&m_result)
{
  init(fn, mode);
}


inline int Gcalc_operation_reducer::continue_range(active_thread *t,
						const Gcalc_heap::Info *p)
{
  DBUG_ASSERT(t->result_range);
  res_point *rp= add_res_point();
  if (!rp)
    return 1;
  rp->glue= NULL;
  rp->down= t->rp;
  t->rp->up= rp;
  rp->intersection_point= false;
  rp->pi= p;
  t->rp= rp;
  return 0;
}


inline int Gcalc_operation_reducer::continue_i_range(active_thread *t,
						  const Gcalc_heap::Info *p,
						  double x, double y)
{
  DBUG_ASSERT(t->result_range);
  res_point *rp= add_res_point();
  if (!rp)
    return 1;
  rp->glue= NULL;
  rp->down= t->rp;
  t->rp->up= rp;
  rp->intersection_point= true;
  rp->x= x;
  rp->pi= p;
  rp->y= y;
  t->rp= rp;
  return 0;
}

inline int Gcalc_operation_reducer::start_range(active_thread *t,
					     const Gcalc_heap::Info *p)
{
  res_point *rp= add_res_point();
  if (!rp)
    return 1;
  rp->glue= rp->down= NULL;
  rp->intersection_point= false;
  rp->pi= p;
  t->result_range= 1;
  t->rp= rp;
  return 0;
}

inline int Gcalc_operation_reducer::start_i_range(active_thread *t,
					       const Gcalc_heap::Info *p,
					       double x, double y)
{
  res_point *rp= add_res_point();
  if (!rp)
    return 1;
  rp->glue= rp->down= NULL;
  rp->intersection_point= true;
  rp->x= x;
  rp->y= y;
  rp->pi= p;
  t->result_range= 1;
  t->rp= rp;
  return 0;
}

inline int Gcalc_operation_reducer::end_range(active_thread *t,
					   const Gcalc_heap::Info *p)
{
  res_point *rp= add_res_point();
  if (!rp)
    return 1;
  rp->glue= rp->up= NULL;
  rp->down= t->rp;
  rp->intersection_point= false;
  rp->pi= p;
  t->rp->up= rp;
  t->result_range= 0;
  return 0;
}

inline int Gcalc_operation_reducer::end_i_range(active_thread *t,
					     const Gcalc_heap::Info *p,
					     double x, double y)
{
  res_point *rp= add_res_point();
  if (!rp)
    return 1;
  rp->glue= rp->up= NULL;
  rp->down= t->rp;
  rp->intersection_point= true;
  rp->x= x;
  rp->pi= p;
  rp->y= y;
  t->rp->up= rp;
  t->result_range= 0;
  return 0;
}

int Gcalc_operation_reducer::start_couple(active_thread *t0, active_thread *t1,
				       const Gcalc_heap::Info *p,
                                       const active_thread *prev_range)
{
  res_point *rp0, *rp1;
  if (!(rp0= add_res_point()) || !(rp1= add_res_point()))
    return 1;
  rp0->glue= rp1;
  rp1->glue= rp0;
  rp0->intersection_point= rp1->intersection_point= false;
  rp0->down= rp1->down= NULL;
  rp0->pi= rp1->pi= p;
  t0->rp= rp0;
  t1->rp= rp1;
  if (prev_range)
  {
    rp0->outer_poly= t1->thread_start= prev_range->thread_start;
  }
  else
  {
    rp0->outer_poly= 0;
    t0->thread_start= rp0;
  }
  return 0;
}

int Gcalc_operation_reducer::start_i_couple(active_thread *t0, active_thread *t1,
					 const Gcalc_heap::Info *p0,
					 const Gcalc_heap::Info *p1,
					 double x, double y,
                                         const active_thread *prev_range)
{
  res_point *rp0, *rp1;
  if (!(rp0= add_res_point()) || !(rp1= add_res_point()))
    return 1;
  rp0->glue= rp1;
  rp1->glue= rp0;
  rp0->pi= p0;
  rp1->pi= p1;
  rp0->intersection_point= rp1->intersection_point= true;
  rp0->down= rp1->down= NULL;
  rp0->x= rp1->x= x;
  rp0->y= rp1->y= y;
  t0->result_range= t1->result_range= 1;
  t0->rp= rp0;
  t1->rp= rp1;
  if (prev_range)
  {
    rp0->outer_poly= t1->thread_start= prev_range->thread_start;
  }
  else
  {
    rp0->outer_poly= 0;
    t0->thread_start= rp0;
  }
  return 0;
}

int Gcalc_operation_reducer::end_couple(active_thread *t0, active_thread *t1,
				     const Gcalc_heap::Info *p)
{
  res_point *rp0, *rp1;
  DBUG_ASSERT(t1->result_range);
  if (!(rp0= add_res_point()) || !(rp1= add_res_point()))
    return 1;
  rp0->down= t0->rp;
  rp1->down= t1->rp;
  rp1->glue= rp0;
  rp0->glue= rp1;
  rp0->up= rp1->up= NULL;
  t0->rp->up= rp0;
  t1->rp->up= rp1;
  rp0->intersection_point= rp1->intersection_point= false;
  rp0->pi= rp1->pi= p;
  t0->result_range= t1->result_range= 0;
  return 0;
}

int Gcalc_operation_reducer::end_i_couple(active_thread *t0, active_thread *t1,
				       const Gcalc_heap::Info *p0,
				       const Gcalc_heap::Info *p1,
				       double x, double y)
{
  res_point *rp0, *rp1;
  if (!(rp0= add_res_point()) || !(rp1= add_res_point()))
    return 1;
  rp0->down= t0->rp;
  rp1->down= t1->rp;
  rp0->pi= p0;
  rp1->pi= p1;
  rp1->glue= rp0;
  rp0->glue= rp1;
  rp0->up= rp1->up= NULL;
  rp0->intersection_point= rp1->intersection_point= true;
  rp0->x= rp1->x= x;
  rp0->y= rp1->y= y;
  t0->result_range= t1->result_range= 0;
  t0->rp->up= rp0;
  t1->rp->up= rp1;
  return 0;
}

int Gcalc_operation_reducer::add_single_point(const Gcalc_heap::Info *p)
{
  res_point *rp= add_res_point();
  if (!rp)
    return 1;
  rp->glue= rp->up= rp->down= NULL;
  rp->intersection_point= false;
  rp->pi= p;
  rp->x= p->x;
  rp->y= p->y;
  return 0;
}

int Gcalc_operation_reducer::add_i_single_point(const Gcalc_heap::Info *p,
					     double x, double y)
{
  res_point *rp= add_res_point();
  if (!rp)
    return 1;
  rp->glue= rp->up= rp->down= NULL;
  rp->intersection_point= true;
  rp->x= x;
  rp->pi= p;
  rp->y= y;
  return 0;
}

int Gcalc_operation_reducer::
handle_lines_intersection(active_thread *t0, active_thread *t1,
			  const Gcalc_heap::Info *p0, const Gcalc_heap::Info *p1,
			  double x, double y)
{
  m_fn->invert_state(p0->shape);
  m_fn->invert_state(p1->shape);
  int intersection_state= m_fn->count();
  if ((t0->result_range | t1->result_range) == intersection_state)
    return 0;

  if (t0->result_range &&
      (end_i_range(t0, p1, x, y) || start_i_range(t0, p1, x, y)))
    return 1;

  if (t1->result_range &&
      (end_i_range(t1, p0, x, y) || start_i_range(t1, p0, x, y)))
    return 1;

  if (intersection_state &&
      add_i_single_point(p0, x, y))
    return 1;
    
  return 0;
}

inline int Gcalc_operation_reducer::
handle_line_polygon_intersection(active_thread *l, const Gcalc_heap::Info *pl,
				 int line_state, int poly_state,
				 double x, double y)
{
  int range_after= ~poly_state & line_state;
  if (l->result_range == range_after)
    return 0;
  return range_after ? start_i_range(l, pl, x, y) : end_i_range(l, pl, x, y);
}

static inline void switch_athreads(Gcalc_operation_reducer::active_thread *t0,
				   Gcalc_operation_reducer::active_thread *t1,
				   Gcalc_dyn_list::Item **hook)
{
  *hook= t1;
  t0->next= t1->next;
  t1->next= t0;
}

inline int Gcalc_operation_reducer::
handle_polygons_intersection(active_thread *t0, active_thread *t1,
			     Gcalc_dyn_list::Item **t_hook,
			     const Gcalc_heap::Info *p0,
			     const Gcalc_heap::Info *p1,
			     int prev_state, double x, double y,
                             const active_thread *prev_range)
{
  m_fn->invert_state(p0->shape);
  int state_11= m_fn->count();
  m_fn->invert_state(p1->shape);
  int state_2= m_fn->count();
  int state_01= prev_state ^ t0->result_range;
  if ((prev_state == state_01) && (prev_state == state_2))
  {
    if (state_11 == prev_state)
    {
      switch_athreads(t0, t1, t_hook);
      return 0;
    }
    return start_i_couple(t0, t1, p0, p1, x, y, prev_range);
  }
  if (prev_state == state_2)
  {
    if (state_01 == state_11)
    {
      if (m_mode & polygon_selfintersections_allowed)
      {
	switch_athreads(t0, t1, t_hook);
	return 0;
      }
      if (prev_state != (m_mode & prefer_big_with_holes))
	return continue_i_range(t0, p0, x, y) || continue_i_range(t1, p1, x, y);
      return end_i_couple(t0, t1, p0, p1, x, y) ||
	start_i_couple(t0, t1, p0, p1, x, y, prev_range);
    }
    else
      return end_i_couple(t0, t1, p0, p1, x, y);
  }
  if (state_01 ^ state_11)
  {
    switch_athreads(t0, t1, t_hook);
    return 0;
  }

  active_thread *thread_to_continue;
  const Gcalc_heap::Info *way_to_go;
  if (prev_state == state_01)
  {
    thread_to_continue= t1;
    way_to_go= p1;
  }
  else 
  {
    thread_to_continue= t0;
    way_to_go= p0;
  }
  return continue_i_range(thread_to_continue, way_to_go, x, y);
}

int Gcalc_operation_reducer::count_slice(Gcalc_scan_iterator *si)
{
  Gcalc_point_iterator pi(si);
  active_thread *cur_t= m_first_active_thread;
  Gcalc_dyn_list::Item **at_hook= (Gcalc_dyn_list::Item **)&m_first_active_thread;
  const active_thread *prev_range;
  int prev_state;

  if (si->get_event() & (scev_point | scev_end | scev_two_ends))
  {
    for (; pi.point() != si->get_event_position(); ++pi, cur_t= cur_t->get_next())
      at_hook= &cur_t->next;

    switch (si->get_event())
    {
      case scev_point:
      {
        if (cur_t->result_range &&
            continue_range(cur_t, pi.get_pi()))
          return 1;
        break;
      }
      case scev_end:
      {
        if (cur_t->result_range &&
            end_range(cur_t, pi.get_pi()))
          return 1;
        *at_hook= cur_t->next;
        free_item(cur_t);
        break;
      }
      case scev_two_ends:
      {
        active_thread *cur_t1= cur_t->get_next();
        if (cur_t->result_range &&
            end_couple(cur_t, cur_t1, pi.get_pi()))
          return 1;

        *at_hook= cur_t1->next;
        free_list(cur_t, &cur_t1->next);
        break;
      }
      default:
        DBUG_ASSERT(0);
    }
    return 0;
  }

  prev_state= 0;
  prev_range= 0;

  m_fn->clear_state();
  for (; pi.point() != si->get_event_position(); ++pi, cur_t= cur_t->get_next())
  {
    if (m_fn->get_shape_kind(pi.get_shape()) == Gcalc_function::shape_polygon)
    {
      m_fn->invert_state(pi.get_shape());
      prev_state^= cur_t->result_range;
    }
    at_hook= &cur_t->next;
    if (cur_t->result_range)
      prev_range= prev_state ? cur_t : 0;
  }

  switch (si->get_event())
  {
  case scev_thread:
  {
    active_thread *new_t= new_active_thread();
    if (!new_t)
      return 1;
    m_fn->invert_state(pi.get_shape());
    new_t->result_range= prev_state ^ m_fn->count();
    new_t->next= *at_hook;
    *at_hook= new_t;
    if (new_t->result_range &&
	start_range(new_t, pi.get_pi()))
      return 1;
    break;
  }
  case scev_two_threads:
  {
    active_thread *new_t0, *new_t1;
    int fn_result;
    if (!(new_t0= new_active_thread()) || !(new_t1= new_active_thread()))
      return 1;
    
    m_fn->invert_state(pi.get_shape());
    fn_result= m_fn->count();
    new_t0->result_range= new_t1->result_range= prev_state ^ fn_result;
    new_t1->next= *at_hook;
    new_t0->next= new_t1;
    *at_hook= new_t0;
    if (new_t0->result_range &&
	start_couple(new_t0, new_t1, pi.get_pi(), prev_range))
      return 1;
    break;
  }
  case scev_intersection:
  {
    active_thread *cur_t1= cur_t->get_next();
    const Gcalc_heap::Info *p0, *p1;
    p0= pi.get_pi();
    ++pi;
    p1= pi.get_pi();
    bool line0= m_fn->get_shape_kind(p0->shape) == Gcalc_function::shape_line;
    bool line1= m_fn->get_shape_kind(p1->shape) == Gcalc_function::shape_line;

    if (!line0 && !line1) /* two polygons*/
    {
      if (handle_polygons_intersection(cur_t, cur_t1, at_hook, p0, p1,
				       prev_state, pi.get_x(), si->get_y(),
                                       prev_range))
	return 1;
    }
    else if (line0 && line1)
    {
      if (!prev_state &&
	  handle_lines_intersection(cur_t, cur_t1,
				    p0, p1, pi.get_x(), si->get_y()))
	return 1;
      switch_athreads(cur_t, cur_t1, at_hook);
    }
    else
    {
      int poly_state;
      int line_state;
      const Gcalc_heap::Info *line;
      active_thread *line_t;
      m_fn->invert_state(p0->shape);
      if (line0)
      {
	line_state= m_fn->count();
	poly_state= prev_state;
	line= p0;
	line_t= cur_t1;
      }
      else
      {
	poly_state= m_fn->count();
	m_fn->invert_state(p1->shape);
	line_state= m_fn->count();
	line= p1;
	line_t= cur_t;
      }
      if (handle_line_polygon_intersection(line_t, line,
					   line_state, poly_state,
					   pi.get_x(), si->get_y()))
	return 1;
      switch_athreads(cur_t, cur_t1, at_hook);
    }
    break;
  }
  case scev_single_point:
  {
    m_fn->invert_state(pi.get_shape());
    if ((prev_state ^ m_fn->count()) &&
	add_single_point(pi.get_pi()))
      return 1;
    break;
  }
  default:
    DBUG_ASSERT(0);
  }

  return 0;
}

int Gcalc_operation_reducer::count_all(Gcalc_heap *hp)
{
  Gcalc_scan_iterator si;
  si.init(hp);
  while (si.more_points())
  {
    if (si.step())
      return 1;
    if (count_slice(&si))
      return 1;
  }
  return 0;
}

inline void Gcalc_operation_reducer::free_result(res_point *res)
{
  if ((*res->prev_hook= res->next))
  {
    res->get_next()->prev_hook= res->prev_hook;
  }
  free_item(res);
}


inline int Gcalc_operation_reducer::get_single_result(res_point *res,
						   Gcalc_result_receiver *storage)
{
  if (res->intersection_point)
  {
    if (storage->single_point(float_to_coord(res->x),
			      float_to_coord(res->y)))
      return 1;
  }
  else
    if (storage->single_point(res->x, res->y))
      return 1;
  free_result(res);
  return 0;
}


int Gcalc_operation_reducer::get_result_thread(res_point *cur,
                                               Gcalc_result_receiver *storage,
                                               int move_upward)
{
  res_point *next;
  bool glue_step= false;
  res_point *first_poly_node= cur;
  double x, y;
  while (cur)
  {
    if (!glue_step)
    {
      if (cur->intersection_point)
      {
        x= float_to_coord(cur->x);
        y= float_to_coord(cur->y);
      }
      else
      {
	x= cur->pi->x;
        y= cur->pi->y;
      }
      if (storage->add_point(x, y))
        return 1;
    }
    
    next= move_upward ? cur->up : cur->down;
    if (!next && !glue_step)
    {
      next= cur->glue;
      move_upward^= 1;
      glue_step= true;
      if (next)
	next->glue= NULL;
    }
    else
      glue_step= false;

    cur->first_poly_node= first_poly_node;
    free_result(cur);
    cur= next;
  }
  return 0;
}


int Gcalc_operation_reducer::get_polygon_result(res_point *cur,
                                                Gcalc_result_receiver *storage)
{
  res_point *glue= cur->glue;
  glue->up->down= NULL;
  free_result(glue);
  return get_result_thread(cur, storage, 1) ||
         storage->complete_shape();
}


int Gcalc_operation_reducer::get_line_result(res_point *cur,
                                             Gcalc_result_receiver *storage)
{
  res_point *next;
  int move_upward= 1;
  if (cur->glue)
  {
    /* Here we have to find the beginning of the line */
    next= cur->up;
    move_upward= 1;
    while (next)
    {
      cur= next;
      next= move_upward ? next->up : next->down;
      if (!next)
      {
	next= cur->glue;
	move_upward^= 1;
      }
    }
  }

  return get_result_thread(cur, storage, move_upward) ||
         storage->complete_shape();
}


int Gcalc_operation_reducer::get_result(Gcalc_result_receiver *storage)
{
  *m_res_hook= NULL;
  while (m_result)
  {
    if (!m_result->up)
    {
      if (get_single_result(m_result, storage))
	return 1;
      continue;
    }
    Gcalc_function::shape_type shape= m_fn->get_shape_kind(m_result->pi->shape);
    if (shape == Gcalc_function::shape_polygon)
    {
      if (m_result->outer_poly)
      {
        uint32 *insert_position, hole_position;
        insert_position= &m_result->outer_poly->first_poly_node->poly_position;
        DBUG_ASSERT(*insert_position);
        hole_position= storage->position();
        storage->start_shape(Gcalc_function::shape_hole);
        if (get_polygon_result(m_result, storage) ||
            storage->move_hole(*insert_position, hole_position,
                               insert_position))
          return 1;
      }
      else
      {
        uint32 *poly_position= &m_result->poly_position;
        storage->start_shape(Gcalc_function::shape_polygon);
        if (get_polygon_result(m_result, storage))
          return 1;
        *poly_position= storage->position();
      }
    }
    else
    {
      storage->start_shape(shape);
      if (get_line_result(m_result, storage))
	return 1;
    }
  }
  
  m_res_hook= (Gcalc_dyn_list::Item **)&m_result;
  storage->done();
  return 0;
}


void Gcalc_operation_reducer::reset()
{
  free_list(m_result, m_res_hook);
  m_res_hook= (Gcalc_dyn_list::Item **)&m_result;
  free_list(m_first_active_thread);
}

#endif /*HAVE_SPATIAL*/

