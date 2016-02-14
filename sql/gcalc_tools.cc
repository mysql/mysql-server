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

#ifdef HAVE_SPATIAL

#include "gcalc_tools.h"
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

void Gcalc_function::add_operation(uint operation, uint32 n_operands)
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


int Gcalc_function::repeat_expression(uint32 exp_pos)
{
  if (reserve_op_buffer(1))
    return 1;
  add_operation(op_repeat, exp_pos);
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
  if (function_buffer.reserve((n_shapes+1) * 2 * sizeof(int)))
    return 1;
  i_states= (int *) (function_buffer.ptr() + ALIGN_SIZE(function_buffer.length()));
  b_states= i_states + (n_shapes + 1);
  return 0;
}


int Gcalc_function::count_internal(const char *cur_func, uint set_type,
                                   const char **end)
{
  uint c_op= uint4korr(cur_func);
  op_type next_func= (op_type) (c_op & op_any);
  int mask= (c_op & op_not) ? 1:0;
  uint n_ops= c_op & ~(op_any | op_not | v_mask);
  uint n_shape= c_op & ~(op_any | op_not | v_mask); /* same as n_ops */
  value v_state= (value) (c_op & v_mask);
  int result= 0;
  const char *sav_cur_func= cur_func;

  // GCALC_DBUG_ENTER("Gcalc_function::count_internal");

  cur_func+= 4;
  if (next_func == op_shape)
  {
    if (set_type == 0)
      result= i_states[n_shape] | b_states[n_shape];
    else if (set_type == op_border)
      result= b_states[n_shape];
    else if (set_type == op_internals)
      result= i_states[n_shape] && !b_states[n_shape];
    goto exit;
  }

  if (next_func == op_false)
  {
    result= 0;
    goto exit;
  }

  if (next_func == op_border || next_func == op_internals)
  {
    result= count_internal(cur_func, next_func, &cur_func);
    goto exit;
  }

  if (next_func == op_repeat)
  {
    result= count_internal(function_buffer.ptr() + n_ops, set_type, 0);
    goto exit;
  }

  if (n_ops == 0)
    return mask;
    //GCALC_DBUG_RETURN(mask);

  result= count_internal(cur_func, set_type, &cur_func);

  while (--n_ops)
  {
    int next_res= count_internal(cur_func, set_type, &cur_func);
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
      default:
        GCALC_DBUG_ASSERT(FALSE);
    };
  }

exit:
  result^= mask;
  if (v_state != v_empty)
  {
    switch (v_state)
    {
      case v_find_t:
        if (result)
        {
          c_op= (c_op & ~v_mask) | v_t_found;
          int4store(sav_cur_func, c_op);
        };
        break;
      case v_find_f:
        if (!result)
        {
          c_op= (c_op & ~v_mask) | v_f_found;
          int4store(sav_cur_func, c_op);
        };
        break;
      case v_t_found:
        result= 1;
        break;
      case v_f_found:
        result= 0;
        break;
      default:
        GCALC_DBUG_ASSERT(0);
    };
  }
  
  if (end)
    *end= cur_func;
  return result;
  //GCALC_DBUG_RETURN(result);
}


void Gcalc_function::clear_i_states()
{
  for (uint i= 0; i < n_shapes; i++)
    i_states[i]= 0;
}


void Gcalc_function::clear_b_states()
{
  for (uint i= 0; i < n_shapes; i++)
    b_states[i]= 0;
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


int Gcalc_function::check_function(Gcalc_scan_iterator &scan_it)
{
  const Gcalc_scan_iterator::point *eq_start, *cur_eq;
  const Gcalc_scan_iterator::event_point *events;
  GCALC_DBUG_ENTER("Gcalc_function::check_function");

  while (scan_it.more_points())
  {
    if (scan_it.step())
      GCALC_DBUG_RETURN(-1);
    events= scan_it.get_events();

    /* these kinds of events don't change the function */
    Gcalc_point_iterator pit(&scan_it);
    clear_b_states();
    clear_i_states();
    /* Walk to the event, marking polygons we met */
    for (; pit.point() != scan_it.get_event_position(); ++pit)
    {
      gcalc_shape_info si= pit.point()->get_shape();
      if ((get_shape_kind(si) == Gcalc_function::shape_polygon))
        invert_i_state(si);
    }
    if (events->simple_event())
    {
      if (events->event == scev_end)
        set_b_state(events->get_shape());

      if (count())
        GCALC_DBUG_RETURN(1);
      clear_b_states();
      continue;
    }

    /* Check the status of the event point */
    for (; events; events= events->get_next())
    {
      gcalc_shape_info si= events->get_shape();
      if (events->event == scev_thread ||
          events->event == scev_end ||
          events->event == scev_single_point ||
          (get_shape_kind(si) == Gcalc_function::shape_polygon))
        set_b_state(si);
      else if (get_shape_kind(si) == Gcalc_function::shape_line)
        set_i_state(si);
    }

    if (count())
      GCALC_DBUG_RETURN(1);

    /* Set back states changed in the loop above. */
    for (events= scan_it.get_events(); events; events= events->get_next())
    {
      gcalc_shape_info si= events->get_shape();
      if (events->event == scev_thread ||
          events->event == scev_end ||
          events->event == scev_single_point ||
          (get_shape_kind(si) == Gcalc_function::shape_polygon))
        clear_b_state(si);
      else if (get_shape_kind(si) == Gcalc_function::shape_line)
        clear_i_state(si);
    }

    if (scan_it.get_event_position() == scan_it.get_event_end())
      continue;

    /* Check the status after the event */
    eq_start= pit.point();
    do
    {
      ++pit;
      if (pit.point() != scan_it.get_event_end() &&
          eq_start->cmp_dx_dy(pit.point()) == 0)
        continue;
      for (cur_eq= eq_start; cur_eq != pit.point();
          cur_eq= cur_eq->get_next())
      {
        gcalc_shape_info si= cur_eq->get_shape();
        if (get_shape_kind(si) == Gcalc_function::shape_polygon)
          set_b_state(si);
        else
          invert_i_state(si);
      }
      if (count())
        GCALC_DBUG_RETURN(1);

      for (cur_eq= eq_start; cur_eq != pit.point(); cur_eq= cur_eq->get_next())
      {
        gcalc_shape_info si= cur_eq->get_shape();
        if ((get_shape_kind(si) == Gcalc_function::shape_polygon))
        {
          clear_b_state(si);
          invert_i_state(si);
        }
        else
          invert_i_state(cur_eq->get_shape());
      }
      if (count())
        GCALC_DBUG_RETURN(1);
      eq_start= pit.point();
    } while (pit.point() != scan_it.get_event_end());
  }
  GCALC_DBUG_RETURN(0);
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


int Gcalc_operation_transporter::empty_shape()
{
  if (m_fn->reserve_op_buffer(1))
        return 1;
  m_fn->add_operation(Gcalc_function::op_false, 0);
  return 0;
}


int Gcalc_result_receiver::start_shape(Gcalc_function::shape_type shape)
{
  GCALC_DBUG_ENTER("Gcalc_result_receiver::start_shape");
  if (buffer.reserve(4*2, 512))
    GCALC_DBUG_RETURN(1);
  cur_shape= shape;
  shape_pos= buffer.length();
  buffer.length(shape_pos + ((shape == Gcalc_function::shape_point) ? 4:8));
  n_points= 0;
  shape_area= 0.0;

  GCALC_DBUG_RETURN(0);
}


int Gcalc_result_receiver::add_point(double x, double y)
{
  GCALC_DBUG_ENTER("Gcalc_result_receiver::add_point");
  if (n_points && x == prev_x && y == prev_y)
    GCALC_DBUG_RETURN(0);

  if (!n_points++)
  {
    prev_x= first_x= x;
    prev_y= first_y= y;
    GCALC_DBUG_RETURN(0);
  }

  shape_area+= prev_x*y - prev_y*x;

  if (buffer.reserve(8*2, 512))
    GCALC_DBUG_RETURN(1);
  buffer.q_append(prev_x);
  buffer.q_append(prev_y);
  prev_x= x;
  prev_y= y;
  GCALC_DBUG_RETURN(0);
}


int Gcalc_result_receiver::complete_shape()
{
  GCALC_DBUG_ENTER("Gcalc_result_receiver::complete_shape");
  if (n_points == 0)
  {
    buffer.length(shape_pos);
    GCALC_DBUG_RETURN(0);
  }
  if (n_points == 1)
  {
    if (cur_shape != Gcalc_function::shape_point)
    {
      if (cur_shape == Gcalc_function::shape_hole)
      {
        buffer.length(shape_pos);
        GCALC_DBUG_RETURN(0);
      }
      cur_shape= Gcalc_function::shape_point;
      buffer.length(buffer.length()-4);
    }
  }
  else
  {
    GCALC_DBUG_ASSERT(cur_shape != Gcalc_function::shape_point);
    if (cur_shape == Gcalc_function::shape_hole)
    {
      shape_area+= prev_x*first_y - prev_y*first_x;
      if (fabs(shape_area) < 1e-8)
      {
        buffer.length(shape_pos);
        GCALC_DBUG_RETURN(0);
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
    GCALC_DBUG_RETURN(1);
  buffer.q_append(prev_x);
  buffer.q_append(prev_y);
  
do_complete:
  buffer.write_at_position(shape_pos, (uint32) cur_shape);

  if (!n_shapes++)
  {
    GCALC_DBUG_ASSERT(cur_shape != Gcalc_function::shape_hole);
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
  GCALC_DBUG_RETURN(0);
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
  if (!n_shapes || collection_result)
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
      GCALC_DBUG_ASSERT(0);
  }
  return 0;
}


int Gcalc_result_receiver::move_hole(uint32 dest_position, uint32 source_position,
                                     uint32 *position_shift)
{
  char *ptr;
  int source_len;
  GCALC_DBUG_ENTER("Gcalc_result_receiver::move_hole");
  GCALC_DBUG_PRINT(("ps %d %d", dest_position, source_position));

  *position_shift= source_len= buffer.length() - source_position;

  if (dest_position == source_position)
    GCALC_DBUG_RETURN(0);

  if (buffer.reserve(source_len, MY_ALIGN(source_len, 512)))
    GCALC_DBUG_RETURN(1);

  ptr= (char *) buffer.ptr();
  memmove(ptr + dest_position + source_len, ptr + dest_position,
          buffer.length() - dest_position);
  memcpy(ptr + dest_position, ptr + buffer.length(), source_len);
  GCALC_DBUG_RETURN(0);
}


Gcalc_operation_reducer::Gcalc_operation_reducer(size_t blk_size) :
  Gcalc_dyn_list(blk_size, sizeof(res_point)),
#ifndef GCALC_DBUG_OFF
  n_res_points(0),
#endif /*GCALC_DBUG_OFF*/
  m_res_hook((Gcalc_dyn_list::Item **)&m_result),
  m_first_active_thread(NULL)
{}


void Gcalc_operation_reducer::init(Gcalc_function *fn, modes mode)
{
  m_fn= fn;
  m_mode= mode;
  m_first_active_thread= NULL;
  m_lines= NULL;
  m_lines_hook= (Gcalc_dyn_list::Item **) &m_lines;
  m_poly_borders= NULL;
  m_poly_borders_hook= (Gcalc_dyn_list::Item **) &m_poly_borders;
  GCALC_SET_TERMINATED(killed, 0);
}


Gcalc_operation_reducer::
Gcalc_operation_reducer(Gcalc_function *fn, modes mode, size_t blk_size) :
  Gcalc_dyn_list(blk_size, sizeof(res_point)),
  m_res_hook((Gcalc_dyn_list::Item **)&m_result)
{
  init(fn, mode);
}


void Gcalc_operation_reducer::res_point::set(const Gcalc_scan_iterator *si)
{
  intersection_point= si->intersection_step();
  pi= si->get_cur_pi();
}


Gcalc_operation_reducer::res_point *
  Gcalc_operation_reducer::add_res_point(Gcalc_function::shape_type type)
{
  GCALC_DBUG_ENTER("Gcalc_operation_reducer::add_res_point");
  res_point *result= (res_point *)new_item();
  *m_res_hook= result;
  result->prev_hook= m_res_hook;
  m_res_hook= &result->next;
  result->type= type;
#ifndef GCALC_DBUG_OFF
  result->point_n= n_res_points++;
#endif /*GCALC_DBUG_OFF*/
  GCALC_DBUG_RETURN(result);
}

int Gcalc_operation_reducer::add_line(int incoming, active_thread *t,
    const Gcalc_scan_iterator::point *p)
{
  line *l= new_line();
  GCALC_DBUG_ENTER("Gcalc_operation_reducer::add_line");
  if (!l)
    GCALC_DBUG_RETURN(1);
  l->incoming= incoming;
  l->t= t;
  l->p= p;
  *m_lines_hook= l;
  m_lines_hook= &l->next;
  GCALC_DBUG_RETURN(0);
}


int Gcalc_operation_reducer::add_poly_border(int incoming,
    active_thread *t, int prev_state, const Gcalc_scan_iterator::point *p)
{
  poly_border *b= new_poly_border();
  GCALC_DBUG_ENTER("Gcalc_operation_reducer::add_poly_border");
  if (!b)
    GCALC_DBUG_RETURN(1);
  b->incoming= incoming;
  b->t= t;
  b->prev_state= prev_state;
  b->p= p;
  *m_poly_borders_hook= b;
  m_poly_borders_hook= &b->next;
  GCALC_DBUG_RETURN(0);
}


int Gcalc_operation_reducer::continue_range(active_thread *t,
                                            const Gcalc_heap::Info *p,
                                            const Gcalc_heap::Info *p_next)
{
  res_point *rp= add_res_point(t->rp->type);
  GCALC_DBUG_ENTER("Gcalc_operation_reducer::continue_range");
  if (!rp)
    GCALC_DBUG_RETURN(1);
  rp->glue= NULL;
  rp->down= t->rp;
  t->rp->up= rp;
  rp->intersection_point= false;
  rp->pi= p;
  t->rp= rp;
  t->p1= p;
  t->p2= p_next;
  GCALC_DBUG_RETURN(0);
}


inline int Gcalc_operation_reducer::continue_i_range(active_thread *t,
			            const Gcalc_heap::Info *ii)
{
  res_point *rp= add_res_point(t->rp->type);
  GCALC_DBUG_ENTER("Gcalc_operation_reducer::continue_i_range");
  if (!rp)
    GCALC_DBUG_RETURN(1);
  rp->glue= NULL;
  rp->down= t->rp;
  t->rp->up= rp;
  rp->intersection_point= true;
  rp->pi= ii;
  t->rp= rp;
  GCALC_DBUG_RETURN(0);
}

int Gcalc_operation_reducer::end_couple(active_thread *t0, active_thread *t1,
				     const Gcalc_heap::Info *p)
{
  res_point *rp0, *rp1;
  GCALC_DBUG_ENTER("Gcalc_operation_reducer::end_couple");
  GCALC_DBUG_ASSERT(t0->rp->type == t1->rp->type);
  if (!(rp0= add_res_point(t0->rp->type)) ||
      !(rp1= add_res_point(t0->rp->type)))
    GCALC_DBUG_RETURN(1);
  rp0->down= t0->rp;
  rp1->down= t1->rp;
  rp1->glue= rp0;
  rp0->glue= rp1;
  rp0->up= rp1->up= NULL;
  t0->rp->up= rp0;
  t1->rp->up= rp1;
  rp0->intersection_point= rp1->intersection_point= false;
  rp0->pi= rp1->pi= p;
  GCALC_DBUG_RETURN(0);
}


int Gcalc_operation_reducer::count_slice(Gcalc_scan_iterator *si)
{
  Gcalc_point_iterator pi(si);
  int prev_state= 0;
  int sav_prev_state;
  active_thread *prev_range= NULL;
  const Gcalc_scan_iterator::event_point *events;
  const Gcalc_scan_iterator::point *eq_start;
  active_thread **cur_t_hook= &m_first_active_thread;
  active_thread **starting_t_hook;
  active_thread *bottom_threads= NULL;
  active_thread *eq_thread, *point_thread;;
  GCALC_DBUG_ENTER("Gcalc_operation_reducer::count_slice");

  m_fn->clear_i_states();
  /* Walk to the event, remembering what is needed. */
  for (; pi.point() != si->get_event_position();
       ++pi, cur_t_hook= (active_thread **) &(*cur_t_hook)->next)
  {
    active_thread *cur_t= *cur_t_hook;
    if (cur_t->enabled() &&
        cur_t->rp->type == Gcalc_function::shape_polygon)
    {
      prev_state^= 1;
      prev_range= prev_state ? cur_t : 0;
    }
    if (m_fn->get_shape_kind(pi.get_shape()) == Gcalc_function::shape_polygon)
      m_fn->invert_i_state(pi.get_shape());
  }

  events= si->get_events();
  if (events->simple_event())
  {
    active_thread *cur_t= *cur_t_hook;
    switch (events->event)
    {
      case scev_point:
      {
        if (cur_t->enabled() &&
            continue_range(cur_t, events->pi, events->next_pi))
          GCALC_DBUG_RETURN(1);
        break;
      }
      case scev_end:
      {
        if (cur_t->enabled() && end_line(cur_t, si))
          GCALC_DBUG_RETURN(1);
        *cur_t_hook= cur_t->get_next();
        free_item(cur_t);
        break;
      }
      case scev_two_ends:
      {
        if (cur_t->enabled() && cur_t->get_next()->enabled())
        {
          /* When two threads are ended here */
          if (end_couple(cur_t, cur_t->get_next(), events->pi))
            GCALC_DBUG_RETURN(1);
        }
        else if (cur_t->enabled() || cur_t->get_next()->enabled())
        {
          /* Rare case when edges of a polygon coincide */
          if (end_line(cur_t->enabled() ? cur_t : cur_t->get_next(), si))
            GCALC_DBUG_RETURN(1);
        }
        *cur_t_hook= cur_t->get_next()->get_next();
        free_item(cur_t->next);
        free_item(cur_t);
        break;
      }
      default:
        GCALC_DBUG_ASSERT(0);
    }
    GCALC_DBUG_RETURN(0);
  }

  starting_t_hook= cur_t_hook;
  sav_prev_state= prev_state;

  /* Walk through the event, collecting all the 'incoming' threads */
  for (; events; events= events->get_next())
  {
    active_thread *cur_t= *cur_t_hook;

    if (events->event == scev_single_point)
      continue;

    if (events->event == scev_thread ||
        events->event == scev_two_threads)
    {
      active_thread *new_t= new_active_thread();
      if (!new_t)
        GCALC_DBUG_RETURN(1);
      new_t->rp= NULL;
      /* Insert into the main thread list before the current */
      new_t->next= cur_t;
      *cur_t_hook= new_t;
      cur_t_hook= (active_thread **) &new_t->next;
    }
    else
    {
      if (events->is_bottom())
      {
        /* Move thread from the main list to the bottom_threads. */
        *cur_t_hook= cur_t->get_next();
        cur_t->next= bottom_threads;
        bottom_threads= cur_t;
      }
      if (cur_t->enabled())
      {
        if (cur_t->rp->type == Gcalc_function::shape_line)
        {
          GCALC_DBUG_ASSERT(!prev_state);
          add_line(1, cur_t, events);
        }
        else
        {
          add_poly_border(1, cur_t, prev_state, events);
          prev_state^= 1;
        }
        if (!events->is_bottom())
        {
          active_thread *new_t= new_active_thread();
          if (!new_t)
            GCALC_DBUG_RETURN(1);
          new_t->rp= NULL;
          /* Replace the current thread with the new. */
          new_t->next= cur_t->next;
          *cur_t_hook= new_t;
          cur_t_hook= (active_thread **) &new_t->next;
          /* And move old to the bottom list */
          cur_t->next= bottom_threads;
          bottom_threads= cur_t;
        }
      }
      else if (!events->is_bottom())
        cur_t_hook= (active_thread **) &cur_t->next;
    }
  }
  prev_state= sav_prev_state;
  cur_t_hook= starting_t_hook;

  eq_start= pi.point();
  eq_thread= point_thread= *starting_t_hook;
  m_fn->clear_b_states();
  while (eq_start != si->get_event_end())
  {
    const Gcalc_scan_iterator::point *cur_eq;
    int in_state, after_state;

    ++pi;
    point_thread= point_thread->get_next();

    if (pi.point() != si->get_event_end() &&
        eq_start->cmp_dx_dy(pi.point()) == 0)
      continue;

    for (cur_eq= eq_start; cur_eq != pi.point(); cur_eq= cur_eq->get_next())
      m_fn->set_b_state(cur_eq->get_shape());
    in_state= m_fn->count();

    m_fn->clear_b_states();
    for (cur_eq= eq_start; cur_eq != pi.point(); cur_eq= cur_eq->get_next())
    {
      gcalc_shape_info si= cur_eq->get_shape();
      if ((m_fn->get_shape_kind(si) == Gcalc_function::shape_polygon))
        m_fn->invert_i_state(si);
    }
    after_state= m_fn->count();
    if (prev_state != after_state)
    {
      if (add_poly_border(0, eq_thread, prev_state, eq_start))
        GCALC_DBUG_RETURN(1);
    }
    else if (!prev_state /* &&!after_state */ && in_state)
    {
      if (add_line(0, eq_thread, eq_start))
        GCALC_DBUG_RETURN(1);
    }

    prev_state= after_state;
    eq_start= pi.point();
    eq_thread= point_thread;
  }

  if (!sav_prev_state && !m_poly_borders && !m_lines)
  {
    /* Check if we need to add the event point itself */
    m_fn->clear_i_states();
    /* b_states supposed to be clean already */
    for (pi.restart(si); pi.point() != si->get_event_position(); ++pi)
    {
      if (m_fn->get_shape_kind(pi.get_shape()) == Gcalc_function::shape_polygon)
        m_fn->invert_i_state(pi.get_shape());
    }
    for (events= si->get_events(); events; events= events->get_next())
      m_fn->set_b_state(events->get_shape());

    GCALC_DBUG_RETURN(m_fn->count() ? add_single_point(si) : 0);
  }

  if (m_poly_borders)
  {
    *m_poly_borders_hook= NULL;
    while (m_poly_borders)
    {
      poly_border *pb1, *pb2;
      pb1= m_poly_borders;
      GCALC_DBUG_ASSERT(m_poly_borders->next);

      pb2= get_pair_border(pb1);
      /* Remove pb1 from the list. The pb2 already removed in get_pair_border. */
      m_poly_borders= pb1->get_next();
      if (connect_threads(pb1->incoming, pb2->incoming,
                          pb1->t, pb2->t, pb1->p, pb2->p,
                          prev_range, si, Gcalc_function::shape_polygon))
        GCALC_DBUG_RETURN(1);

      free_item(pb1);
      free_item(pb2);
    }
    m_poly_borders_hook= (Gcalc_dyn_list::Item **) &m_poly_borders;
    m_poly_borders= NULL;
  }

  if (m_lines)
  {
    *m_lines_hook= NULL;
    if (m_lines->get_next() &&
        !m_lines->get_next()->get_next())
    {
      if (connect_threads(m_lines->incoming, m_lines->get_next()->incoming,
                          m_lines->t, m_lines->get_next()->t,
                          m_lines->p, m_lines->get_next()->p,
                          NULL, si, Gcalc_function::shape_line))
        GCALC_DBUG_RETURN(1);
    }
    else
    {
      for (line *cur_line= m_lines; cur_line; cur_line= cur_line->get_next())
      {
        if (cur_line->incoming)
        {
          if (end_line(cur_line->t, si))
            GCALC_DBUG_RETURN(1);
        }
        else
          start_line(cur_line->t, cur_line->p, si);
      }
    }
    free_list(m_lines);
    m_lines= NULL;
    m_lines_hook= (Gcalc_dyn_list::Item **) &m_lines;
  }

  if (bottom_threads)
    free_list(bottom_threads);

  GCALC_DBUG_RETURN(0);
}


int Gcalc_operation_reducer::add_single_point(const Gcalc_scan_iterator *si)
{
  res_point *rp= add_res_point(Gcalc_function::shape_point);
  GCALC_DBUG_ENTER("Gcalc_operation_reducer::add_single_point");
  if (!rp)
    GCALC_DBUG_RETURN(1);
  rp->glue= rp->up= rp->down= NULL;
  rp->set(si);
  GCALC_DBUG_RETURN(0);
}


Gcalc_operation_reducer::poly_border
  *Gcalc_operation_reducer::get_pair_border(poly_border *b1)
{
  poly_border *prev_b= b1;
  poly_border *result= b1->get_next();
  GCALC_DBUG_ENTER("Gcalc_operation_reducer::get_pair_border");
  if (b1->prev_state)
  {
    if (b1->incoming)
    {
      /* Find the first outgoing, otherwise the last one. */
      while (result->incoming && result->get_next())
      {
        prev_b= result;
        result= result->get_next();
      }
    }
    else
    {
      /* Get the last one */
      while (result->get_next())
      {
        prev_b= result;
        result= result->get_next();
      }
    }
  }
  else /* !b1->prev_state */
  {
    if (b1->incoming)
    {
      /* Get the next incoming, otherwise the last one. */
      while (!result->incoming && result->get_next())
      {
        prev_b= result;
        result= result->get_next();
      }
    }
    else
    {
      /* Just pick the next one */
    }
  }
  /* Delete the result from the list. */
  prev_b->next= result->next;
  GCALC_DBUG_RETURN(result);
}


int Gcalc_operation_reducer::connect_threads(
    int incoming_a, int incoming_b,
    active_thread *ta, active_thread *tb,
    const Gcalc_scan_iterator::point *pa, const Gcalc_scan_iterator::point *pb,
    active_thread *prev_range,
    const Gcalc_scan_iterator *si, Gcalc_function::shape_type s_t)
{
  GCALC_DBUG_ENTER("Gcalc_operation_reducer::connect_threads");
  GCALC_DBUG_PRINT(("incoming %d %d", incoming_a, incoming_b));
  if (incoming_a && incoming_b)
  {
    res_point *rpa, *rpb;
    GCALC_DBUG_ASSERT(ta->rp->type == tb->rp->type);
    if (!(rpa= add_res_point(ta->rp->type)) ||
        !(rpb= add_res_point(ta->rp->type)))
      GCALC_DBUG_RETURN(1);
    rpa->down= ta->rp;
    rpb->down= tb->rp;
    rpb->glue= rpa;
    rpa->glue= rpb;
    rpa->up= rpb->up= NULL;
    ta->rp->up= rpa;
    tb->rp->up= rpb;
    rpa->set(si);
    rpb->set(si);
    ta->rp= tb->rp= NULL;
    GCALC_DBUG_RETURN(0);
  }
  if (!incoming_a)
  {
    GCALC_DBUG_ASSERT(!incoming_b);

    res_point *rp0, *rp1;
    if (!(rp0= add_res_point(s_t)) || !(rp1= add_res_point(s_t)))
      GCALC_DBUG_RETURN(1);
    rp0->glue= rp1;
    rp1->glue= rp0;
    rp0->set(si);
    rp1->set(si);
    rp0->down= rp1->down= NULL;
    ta->rp= rp0;
    tb->rp= rp1;
    ta->p1= pa->pi;
    ta->p2= pa->next_pi;

    tb->p1= pb->pi;
    tb->p2= pb->next_pi;

    if (prev_range)
    {
      rp0->outer_poly= prev_range->thread_start;
      tb->thread_start= prev_range->thread_start;
      /* Chack if needed */
      ta->thread_start= prev_range->thread_start;
    }
    else
    {
      rp0->outer_poly= 0;
      ta->thread_start= rp0;
      /* Chack if needed */
      tb->thread_start= rp0;
    }
    GCALC_DBUG_RETURN(0);
  }
  /* else, if only ta is incoming */

  GCALC_DBUG_ASSERT(tb != ta);
  tb->rp= ta->rp;
  tb->thread_start= ta->thread_start;
  if (Gcalc_scan_iterator::point::
      cmp_dx_dy(ta->p1, ta->p2, pb->pi, pb->next_pi) != 0)
  {
    if (si->intersection_step() ?
          continue_i_range(tb, si->get_cur_pi()) :
          continue_range(tb, si->get_cur_pi(), pb->next_pi))
      GCALC_DBUG_RETURN(1);
  }
  tb->p1= pb->pi;
  tb->p2= pb->next_pi;

  GCALC_DBUG_RETURN(0);
}


int Gcalc_operation_reducer::start_line(active_thread *t,
                                        const Gcalc_scan_iterator::point *p,
                                        const Gcalc_scan_iterator *si)
{
  res_point *rp= add_res_point(Gcalc_function::shape_line);
  GCALC_DBUG_ENTER("Gcalc_operation_reducer::start_line");
  if (!rp)
    GCALC_DBUG_RETURN(1);
  rp->glue= rp->down= NULL;
  rp->set(si);
  t->rp= rp;
  t->p1= p->pi;
  t->p2= p->next_pi;
  GCALC_DBUG_RETURN(0);
}


int Gcalc_operation_reducer::end_line(active_thread *t,
                                      const Gcalc_scan_iterator *si)
{
  GCALC_DBUG_ENTER("Gcalc_operation_reducer::end_line");
  GCALC_DBUG_ASSERT(t->rp->type == Gcalc_function::shape_line);
  res_point *rp= add_res_point(Gcalc_function::shape_line);
  if (!rp)
    GCALC_DBUG_RETURN(1);
  rp->glue= rp->up= NULL;
  rp->down= t->rp;
  rp->set(si);
  t->rp->up= rp;
  t->rp= NULL;

  GCALC_DBUG_RETURN(0);
}


int Gcalc_operation_reducer::count_all(Gcalc_heap *hp)
{
  Gcalc_scan_iterator si;
  GCALC_DBUG_ENTER("Gcalc_operation_reducer::count_all");
  si.init(hp);
  GCALC_SET_TERMINATED(si.killed, killed);
  while (si.more_points())
  {
    if (si.step())
      GCALC_DBUG_RETURN(1);
    if (count_slice(&si))
      GCALC_DBUG_RETURN(1);
  }
  GCALC_DBUG_RETURN(0);
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
  GCALC_DBUG_ENTER("Gcalc_operation_reducer::get_single_result");
  if (res->intersection_point)
  {
    double x, y;
    res->pi->calc_xy(&x, &y);
    if (storage->single_point(x,y))
      GCALC_DBUG_RETURN(1);
  }
  else
    if (storage->single_point(res->pi->node.shape.x, res->pi->node.shape.y))
      GCALC_DBUG_RETURN(1);
  free_result(res);
  GCALC_DBUG_RETURN(0);
}


int Gcalc_operation_reducer::get_result_thread(res_point *cur,
                                               Gcalc_result_receiver *storage,
                                               int move_upward,
                                               res_point *first_poly_node)
{
  res_point *next;
  bool glue_step= false;
  double x, y;
  GCALC_DBUG_ENTER("Gcalc_operation_reducer::get_result_thread");
  while (cur)
  {
    if (!glue_step)
    {
      if (cur->intersection_point)
      {
        cur->pi->calc_xy(&x, &y);
      }
      else
      {
	x= cur->pi->node.shape.x;
        y= cur->pi->node.shape.y;
      }
      if (storage->add_point(x, y))
        GCALC_DBUG_RETURN(1);
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
  GCALC_DBUG_RETURN(0);
}


int Gcalc_operation_reducer::get_polygon_result(res_point *cur,
                                                Gcalc_result_receiver *storage,
                                                res_point *first_poly_node)
{
  GCALC_DBUG_ENTER("Gcalc_operation_reducer::get_polygon_result");
  res_point *glue= cur->glue;
  glue->up->down= NULL;
  free_result(glue);
  GCALC_DBUG_RETURN(get_result_thread(cur, storage, 1, first_poly_node) ||
                    storage->complete_shape());
}


int Gcalc_operation_reducer::get_line_result(res_point *cur,
                                             Gcalc_result_receiver *storage)
{
  res_point *next;
  res_point *cur_orig= cur;
  int move_upward= 1;
  GCALC_DBUG_ENTER("Gcalc_operation_reducer::get_line_result");
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
        if (next == cur_orig)
        {
          /* It's the line loop */
          cur= cur_orig;
          cur->glue->glue= NULL;
          move_upward= 1;
          break;
        }
	move_upward^= 1;
      }
    }
  }

  GCALC_DBUG_RETURN(get_result_thread(cur, storage, move_upward, 0) ||
                    storage->complete_shape());
}


int Gcalc_operation_reducer::get_result(Gcalc_result_receiver *storage)
{
  poly_instance *polygons= NULL;

  GCALC_DBUG_ENTER("Gcalc_operation_reducer::get_result");
  *m_res_hook= NULL;

  /* This is to workaround an old gcc's bug */
  if (m_res_hook == (Gcalc_dyn_list::Item **) &m_result)
    goto done;

  while (m_result)
  {
    Gcalc_function::shape_type shape= m_result->type;
    if (shape == Gcalc_function::shape_point)
    {
      if (get_single_result(m_result, storage))
        GCALC_DBUG_RETURN(1);
      continue;
    }
    if (shape == Gcalc_function::shape_polygon)
    {
      if (m_result->outer_poly)
      {
        uint32 insert_position, hole_position, position_shift;
        poly_instance *cur_poly;
        insert_position= m_result->outer_poly->first_poly_node->poly_position;
        GCALC_DBUG_ASSERT(insert_position);
        hole_position= storage->position();
        storage->start_shape(Gcalc_function::shape_hole);
        if (get_polygon_result(m_result, storage,
                               m_result->outer_poly->first_poly_node) ||
            storage->move_hole(insert_position, hole_position,
                               &position_shift))
          GCALC_DBUG_RETURN(1);
        for (cur_poly= polygons;
             cur_poly && *cur_poly->after_poly_position >= insert_position;
             cur_poly= cur_poly->get_next())
          *cur_poly->after_poly_position+= position_shift;
      }
      else
      {
        uint32 *poly_position= &m_result->poly_position;
        poly_instance *p= new_poly();
        p->after_poly_position= poly_position;
        p->next= polygons;
        polygons= p;
        storage->start_shape(Gcalc_function::shape_polygon);
        if (get_polygon_result(m_result, storage, m_result))
          GCALC_DBUG_RETURN(1);
        *poly_position= storage->position();
      }
    }
    else
    {
      storage->start_shape(shape);
      if (get_line_result(m_result, storage))
        GCALC_DBUG_RETURN(1);
    }
  }
  
done:
  m_res_hook= (Gcalc_dyn_list::Item **)&m_result;
  storage->done();
  GCALC_DBUG_RETURN(0);
}


void Gcalc_operation_reducer::reset()
{
  free_list((Gcalc_heap::Item **) &m_result, m_res_hook);
  m_res_hook= (Gcalc_dyn_list::Item **)&m_result;
  free_list(m_first_active_thread);
}

#endif /*HAVE_SPATIAL*/

