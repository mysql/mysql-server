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


#ifndef GCALC_TOOLS_INCLUDED
#define GCALC_TOOLS_INCLUDED

#include "gcalc_slicescan.h"


/*
  The Gcalc_function class objects are used to check for a binary relation.
  The relation can be constructed with the prefix notation using predicates as
        op_not (as !A)
        op_union ( A || B || C... )
        op_intersection ( A && B && C ... )
        op_symdifference ( A+B+C+... == 1 )
        op_difference ( A && !(B||C||..))
  with the calls of the add_operation(operation, n_operands) method.
  The relation is calculated over a set of shapes, that in turn have
  to be added with the add_new_shape() method. All the 'shapes' can
  be set to 0 with clear_shapes() method and single value
  can be changed with the invert_state() method.
  Then the value of the relation can be calculated with the count() method.
  Frequently used method is find_function(Gcalc_scan_iterator it) that
  iterates through the 'it' until the relation becomes TRUE.
*/

class Gcalc_function
{
private:
  static const uint32 function_buffer_item_size= 4;
  static const uint32 shape_buffer_item_size= 4;
  String shapes_buffer;
  String function_buffer;
  const char *cur_func;
  int *i_states;
  uint32 cur_object_id;
  uint n_shapes;
  int count_internal();
public:
#ifndef DBUG_OFF
  /**
    Convert operation code to its readable name.
  */
  static const char *op_name(int code);
  /**
    Convert shape code to its readable name.
  */
  static const char *shape_name(int code);
#endif
  enum op_type
  {
    op_shape= 0,
    op_not= 0x80000000,
    op_union= 0x10000000,
    op_intersection= 0x20000000,
    op_symdifference= 0x30000000,
    op_difference= 0x40000000,
    op_backdifference= 0x50000000,
    op_any= 0x70000000
  };
  enum shape_type
  {
    shape_point= 0,
    shape_line= 1,
    shape_polygon= 2,
    shape_hole= 3
  };
  Gcalc_function() : n_shapes(0) {}
  gcalc_shape_info add_new_shape(uint32 shape_id, shape_type shape_kind);
  /*
    Adds the leaf operation that returns the shape value.
    Also adds the shape to the list of operands.
  */
  int single_shape_op(shape_type shape_kind, gcalc_shape_info *si);
  void add_operation(op_type operation, uint32 n_operands);
  void add_not_operation(op_type operation, uint32 n_operands);
  uint32 get_next_operation_pos() { return function_buffer.length(); }
  /**
    Read operand number from the given position and add more operands.
  */
  void add_operands_to_op(uint32 operation_pos, uint32 n_operands);
  /**
    Set operand number at the given position (replace the old operand number).
  */
  void set_operands_to_op(uint32 operation_pos, uint32 n_operands);
  void set_cur_obj(uint32 cur_obj) { cur_object_id= cur_obj; }
  int reserve_shape_buffer(uint n_shapes);
  int reserve_op_buffer(uint n_ops);
  uint get_nshapes() const { return n_shapes; }
  shape_type get_shape_kind(gcalc_shape_info si) const
  {
    return (shape_type) uint4korr(shapes_buffer.ptr() + (si*4));
  }

  void set_states(int *shape_states) { i_states= shape_states; }
  int alloc_states();
  void invert_state(gcalc_shape_info shape) { i_states[shape]^= 1; }
  int get_state(gcalc_shape_info shape) { return i_states[shape]; }
  int count()
  {
    cur_func= function_buffer.ptr();
    return count_internal();
  }
  void clear_state() { memset(i_states, 0, n_shapes * sizeof(int)); }
  void reset();

  int find_function(Gcalc_scan_iterator &scan_it);

#ifndef DBUG_OFF
  /**
    Print function buffer created after shape transformation
    into mysqld log and into client side warnings.
    Printing to mysqld log is useful when server crashed during an operation.
    Printing to client side warnings is useful for mtr purposes.
  */
  void debug_print_function_buffer();
#endif
};


/*
  Gcalc_operation_transporter class extends the Gcalc_shape_transporter.
  In addition to the parent's functionality, it fills the Gcalc_function
  object so it has the function that determines the proper shape.
  For example Multipolyline will be represented as an union of polylines.
*/

class Gcalc_operation_transporter : public Gcalc_shape_transporter
{
protected:
  Gcalc_function *m_fn;
  gcalc_shape_info m_si;
public:
  Gcalc_operation_transporter(Gcalc_function *fn, Gcalc_heap *heap) :
    Gcalc_shape_transporter(heap), m_fn(fn) {}

  int single_point(Gcalc_shape_status *st, double x, double y);
  int start_line(Gcalc_shape_status *st);
  int complete_line(Gcalc_shape_status *st);
  int start_poly(Gcalc_shape_status *st);
  int complete_poly(Gcalc_shape_status *st);
  int start_ring(Gcalc_shape_status *st);
  int complete_ring(Gcalc_shape_status *st);
  int add_point(Gcalc_shape_status *st, double x, double y);
  int start_collection(Gcalc_shape_status *st, int nshapes);
  int complete_collection(Gcalc_shape_status *st);
  int collection_add_item(Gcalc_shape_status *st_collection,
                          Gcalc_shape_status *st_item);
};


/*
   When we calculate the result of an spatial operation like
   Union or Intersection, we receive vertexes of the result
   one-by-one, and probably need to treat them in variative ways.
   So, the Gcalc_result_receiver class designed to get these
   vertexes and construct shapes/objects out of them.
   and to store the result in an appropriate format
*/

class Gcalc_result_receiver
{
  String buffer;
  uint32 n_points;
  Gcalc_function::shape_type common_shapetype;
  bool collection_result;
  uint32 n_shapes;
  uint32 n_holes;

  Gcalc_function::shape_type cur_shape;
  uint32 shape_pos;
  double first_x, first_y, prev_x, prev_y;
  double shape_area;
public:

  class chunk_info
  {
  public:
    void *first_point;
    uint32 order;
    uint32 position;
    uint32 length;
    bool is_poly_hole;
#ifndef DBUG_OFF
    inline void dbug_print() const;
#endif
  };

  Gcalc_result_receiver() : collection_result(FALSE), n_shapes(0), n_holes(0)
    {}
  int start_shape(Gcalc_function::shape_type shape);
  int add_point(double x, double y);
  int complete_shape();
  int single_point(double x, double y);
  int done();
  void reset();

  const char *result() { return buffer.ptr(); }
  uint length() { return buffer.length(); }
  int get_nshapes() { return n_shapes; }
  int get_nholes() { return n_holes; }
  int get_result_typeid();
  uint32 position() { return buffer.length(); }
  int reorder_chunks(chunk_info *chunks, int nchunks);
};


/*
  Gcalc_operation_reducer class incapsulates the spatial
  operation functionality. It analyses the slices generated by
  the slicescan and calculates the shape of the result defined
  by some Gcalc_function.
*/

class Gcalc_operation_reducer : public Gcalc_dyn_list
{
public:
  enum modes
  {
    /* Numeric values important here - careful with changing */
    default_mode= 0,
    prefer_big_with_holes= 1,
    polygon_selfintersections_allowed= 2,  /* allowed in the result */
    line_selfintersections_allowed= 4      /* allowed in the result */
  };

  Gcalc_operation_reducer(size_t blk_size=8192);
  void init(Gcalc_function *fn, modes mode= default_mode);
  Gcalc_operation_reducer(Gcalc_function *fn, modes mode= default_mode,
		       size_t blk_size=8192);
  int count_slice(Gcalc_scan_iterator *si);
  int count_all(Gcalc_heap *hp);
  int get_result(Gcalc_result_receiver *storage);
  void reset();

  class res_point : public Gcalc_dyn_list::Item
  {
    res_point *m_outer_poly;
  public:
    bool intersection_point;
    double x,y;
    res_point *up;
    res_point *down;
    res_point *glue;
    union
    {
      const Gcalc_heap::Info *pi; // is valid before get_result_thread()
      res_point *first_poly_node; // is valid after get_result_thread()
    };
    Gcalc_dyn_list::Item **prev_hook;
    res_point *get_next() { return (res_point *)next; }
    void set_outer_poly(res_point *p)
    {
      m_outer_poly= p;
      DBUG_PRINT("info", ("setting outer_poly of #%u to #%u",
                          item_id(),
                          m_outer_poly ? m_outer_poly->item_id() : 0));
    }
    res_point *get_outer_poly() { return m_outer_poly; }
  };

  class active_thread : public Gcalc_dyn_list::Item
  {
    res_point *m_thread_start;
  public:
    res_point *rp;
    int result_range;
    void init()
    {
      rp= m_thread_start= NULL;
      result_range= 0;
      DBUG_PRINT("info", ("setting m_thread_start of #%u to NULL (reset)",
                          item_id()));
    }
    active_thread *get_next() { return (active_thread *)next; }
    void set_thread_start(res_point *p)
    {
      DBUG_PRINT("info", ("setting m_thread_start of #%u to #%u",
                          item_id(), p ? p->item_id() : 0));
      m_thread_start= p;
    }
    res_point *thread_start() const { return m_thread_start; }
  };

protected:
  Gcalc_function *m_fn;
  Gcalc_dyn_list::Item **m_res_hook;
  res_point *m_result;
  int m_mode;

  res_point *result_heap;
  active_thread *m_first_active_thread;

  res_point *add_res_point(const Gcalc_heap::Info *pi)
  {
    res_point *rp= new_res_point(pi, false);
    if (!rp)
      return NULL;
    DBUG_PRINT("info", ("add_res_point #%u: pi=(%g,%g,#%u)",
                        rp->item_id(), pi->x, pi->y, pi->shape));
    return rp;
  }

  res_point *add_res_i_point(const Gcalc_heap::Info *pi, double x, double y)
  {
    res_point *rp= new_res_point(pi, true);
    if (!rp)
      return NULL;
    DBUG_PRINT("info", ("add_res_i_point #%u: pi=(%g,%g,#%u) xy=(%g,%g)",
                        rp->item_id(), pi->x, pi->y, pi->shape, x, y));
    rp->x= x;
    rp->y= y;
    return rp;
  }

  res_point *add_res_single_point(const Gcalc_heap::Info *pi)
  {
    res_point *rp= new_res_point(pi, false);
    if (!rp)
      return NULL;
    DBUG_PRINT("info", ("add_res_single_point #%u: pi=(%g,%g,#%u)",
                        rp->item_id(), pi->x, pi->y, pi->shape));
    rp->x= pi->x;
    rp->y= pi->y;
    return rp;
  }

  active_thread *new_active_thread()
  {
    active_thread *tmp= (active_thread *) new_item();
    if (tmp)
      tmp->init();
    return tmp;
  }

private:

  res_point *new_res_point(const Gcalc_heap::Info *pi,
                           bool intersection_point)
  {
    res_point *result= (res_point *) new_item();
    *m_res_hook= result;
    result->prev_hook= m_res_hook;
    m_res_hook= &result->next;
    result->pi= pi;
    result->intersection_point= intersection_point;
    return result;
  }

  int continue_range(active_thread *t, const Gcalc_heap::Info *p);
  int continue_i_range(active_thread *t, const Gcalc_heap::Info *p,
		       double x, double y);
  int start_range(active_thread *t, const Gcalc_heap::Info *p);
  int start_i_range(active_thread *t, const Gcalc_heap::Info *p,
		    double x, double y);
  int end_range(active_thread *t, const Gcalc_heap::Info *p);
  int end_i_range(active_thread *t, const Gcalc_heap::Info *p,
		  double x, double y);
  int start_couple(active_thread *t0, active_thread *t1,const Gcalc_heap::Info *p,
                     const active_thread *prev_range);
  int start_i_couple(active_thread *t0, active_thread *t1,
		     const Gcalc_heap::Info *p0,
		     const Gcalc_heap::Info *p1,
		     double x, double y,
                     const active_thread *prev_range);
  int end_couple(active_thread *t0, active_thread *t1, const Gcalc_heap::Info *p);
  int end_i_couple(active_thread *t0, active_thread *t1,
		   const Gcalc_heap::Info *p0,
		   const Gcalc_heap::Info *p1,
		   double x, double y);
  int add_single_point(const Gcalc_heap::Info *p);
  int add_i_single_point(const Gcalc_heap::Info *p, double x, double y);

  int handle_lines_intersection(active_thread *t0, active_thread *t1,
				const Gcalc_heap::Info *p0,
				const Gcalc_heap::Info *p1,
				double x, double y);
  int handle_polygons_intersection(active_thread *t0, active_thread *t1,
				   Gcalc_dyn_list::Item **t_hook,
				   const Gcalc_heap::Info *p0,
				   const Gcalc_heap::Info *p1,
				   int prev_state, double x, double y,
                                   const active_thread *prev_range);
  int handle_line_polygon_intersection(active_thread *l,
				       const Gcalc_heap::Info *pl,
				       int line_state, int poly_state,
				       double x, double y);

  int get_single_result(res_point *res, Gcalc_result_receiver *storage);
  int get_result_thread(res_point *cur, Gcalc_result_receiver *storage,
			int move_upward);
  int get_polygon_result(res_point *cur, Gcalc_result_receiver *storage);
  int get_line_result(res_point *cur, Gcalc_result_receiver *storage);

  void free_result(res_point *res);
};

#endif /*GCALC_TOOLS_INCLUDED*/

