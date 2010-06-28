#ifndef gcalc_tools_h
#define gcalc_tools_h

#include "gcalc_slicescan.h"


class gcalc_function
{
private:
  String shapes_buffer;
  String function_buffer;
  const char *cur_func;
  int *i_states;
  uint32 cur_object_id;
  uint n_shapes;
  int count_internal();
public:
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
  gcalc_function() : n_shapes(0) {}
  gcalc_shape_info add_new_shape(uint32 shape_id, shape_type shape_kind);
  int single_shape_op(shape_type shape_kind, gcalc_shape_info *si);
  void add_operation(op_type operation, uint32 n_operands);
  void add_not_operation(op_type operation, uint32 n_operands);
  uint32 get_next_operation_pos() { return function_buffer.length(); }
  void add_operands_to_op(uint32 operation_pos, uint32 n_operands);
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
  void clear_state() { bzero(i_states, n_shapes * sizeof(int)); }
  void reset();

  int find_function(gcalc_scan_iterator &scan_it);
};


class gcalc_operation_transporter : public gcalc_shape_transporter
{
protected:
  gcalc_function *m_fn;
  gcalc_shape_info m_si;
public:
  gcalc_operation_transporter(gcalc_function *fn, gcalc_heap *heap) :
    gcalc_shape_transporter(heap), m_fn(fn) {}

  int single_point(double x, double y);
  int start_line();
  int complete_line();
  int start_poly();
  int complete_poly();
  int start_ring();
  int complete_ring();
  int add_point(double x, double y);
  int start_collection(int n_objects);
};


/* class to receive the result of the geometry operation */
/* to store it in appropriate format                     */

class gcalc_result_receiver
{
  String buffer;
  uint32 n_points;
  gcalc_function::shape_type common_shapetype;
  bool collection_result;
  uint32 n_shapes;
  uint32 n_holes;

  gcalc_function::shape_type cur_shape;
  uint32 shape_pos;
  double first_x, first_y, prev_x, prev_y;
public:
  gcalc_result_receiver() : collection_result(FALSE), n_shapes(0), n_holes(0)
    {}
  int start_shape(gcalc_function::shape_type shape);
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
  int move_hole(uint32 dest_position, uint32 source_position,
                uint32 *new_dest_position);
};


class gcalc_operation_reducer : public gcalc_dyn_list
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

  gcalc_operation_reducer(size_t blk_size=8192);
  void init(gcalc_function *fn, modes mode= default_mode);
  gcalc_operation_reducer(gcalc_function *fn, modes mode= default_mode,
		       size_t blk_size=8192);
  int count_slice(gcalc_scan_iterator *si);
  int count_all(gcalc_heap *hp);
  int get_result(gcalc_result_receiver *storage);
  void reset();

  class res_point : public gcalc_dyn_list::item
  {
  public:
    bool intersection_point;
    double x,y;
    res_point *up;
    res_point *down;
    res_point *glue;
    union
    {
      const gcalc_heap::info *pi;
      res_point *first_poly_node;
    };
    union
    {
      res_point *outer_poly;
      uint32 poly_position;
    };
    gcalc_dyn_list::item **prev_hook;
    res_point *get_next() { return (res_point *)next; }
  };

  class active_thread : public gcalc_dyn_list::item
  {
  public:
    res_point *rp;
    int result_range;
    res_point *thread_start;
    active_thread *get_next() { return (active_thread *)next; }
  };

protected:
  gcalc_function *m_fn;
  gcalc_dyn_list::item **m_res_hook;
  res_point *m_result;
  int m_mode;

  res_point *result_heap;
  active_thread *m_first_active_thread;

  res_point *add_res_point()
  {
    res_point *result= (res_point *)new_item();
    *m_res_hook= result;
    result->prev_hook= m_res_hook;
    m_res_hook= &result->next;
    return result;
  }

  active_thread *new_active_thread() { return (active_thread *)new_item(); }

private:
  int continue_range(active_thread *t, const gcalc_heap::info *p);
  int continue_i_range(active_thread *t, const gcalc_heap::info *p,
		       double x, double y);
  int start_range(active_thread *t, const gcalc_heap::info *p);
  int start_i_range(active_thread *t, const gcalc_heap::info *p,
		    double x, double y);
  int end_range(active_thread *t, const gcalc_heap::info *p);
  int end_i_range(active_thread *t, const gcalc_heap::info *p,
		  double x, double y);
  int start_couple(active_thread *t0, active_thread *t1,const gcalc_heap::info *p,
                     const active_thread *prev_range);
  int start_i_couple(active_thread *t0, active_thread *t1,
		     const gcalc_heap::info *p0,
		     const gcalc_heap::info *p1,
		     double x, double y,
                     const active_thread *prev_range);
  int end_couple(active_thread *t0, active_thread *t1, const gcalc_heap::info *p);
  int end_i_couple(active_thread *t0, active_thread *t1,
		   const gcalc_heap::info *p0,
		   const gcalc_heap::info *p1,
		   double x, double y);
  int add_single_point(const gcalc_heap::info *p);
  int add_i_single_point(const gcalc_heap::info *p, double x, double y);

  int handle_lines_intersection(active_thread *t0, active_thread *t1,
				const gcalc_heap::info *p0,
				const gcalc_heap::info *p1,
				double x, double y);
  int handle_polygons_intersection(active_thread *t0, active_thread *t1,
				   gcalc_dyn_list::item **t_hook,
				   const gcalc_heap::info *p0,
				   const gcalc_heap::info *p1,
				   int prev_state, double x, double y,
                                   const active_thread *prev_range);
  int handle_line_polygon_intersection(active_thread *l,
				       const gcalc_heap::info *pl,
				       int line_state, int poly_state,
				       double x, double y);

  int get_single_result(res_point *res, gcalc_result_receiver *storage);
  int get_result_thread(res_point *cur, gcalc_result_receiver *storage,
			int move_upward);
  int get_polygon_result(res_point *cur, gcalc_result_receiver *storage);
  int get_line_result(res_point *cur, gcalc_result_receiver *storage);

  void free_result(res_point *res);
};

#endif /*gcalc_tools_h*/

