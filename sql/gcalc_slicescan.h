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


#ifndef GCALC_SLICESCAN_INCLUDED
#define GCALC_SLICESCAN_INCLUDED

#ifndef DBUG_OFF
// #define GCALC_CHECK_WITH_FLOAT
#else
#define GCALC_DBUG_OFF
#endif /*DBUG_OFF*/

#ifndef GCALC_DBUG_OFF
#define GCALC_DBUG_PRINT(b) DBUG_PRINT("Gcalc", b)
#define GCALC_DBUG_ENTER(a) DBUG_ENTER("Gcalc " a)
#define GCALC_DBUG_RETURN(r) DBUG_RETURN(r)
#define GCALC_DBUG_VOID_RETURN DBUG_VOID_RETURN
#define GCALC_DBUG_ASSERT(r) DBUG_ASSERT(r)
#else
#define GCALC_DBUG_PRINT(b)     do {} while(0)
#define GCALC_DBUG_ENTER(a)     do {} while(0)
#define GCALC_DBUG_RETURN(r)    return (r)
#define GCALC_DBUG_VOID_RETURN  do {} while(0)
#define GCALC_DBUG_ASSERT(r)    do {} while(0)
#endif /*GCALC_DBUG_OFF*/

#define GCALC_TERMINATED(state_var) (state_var && (*state_var))
#define GCALC_SET_TERMINATED(state_var, val) state_var= val
#define GCALC_DECL_TERMINATED_STATE(varname) \
  volatile int *varname;

/*
  Gcalc_dyn_list class designed to manage long lists of same-size objects
  with the possible efficiency.
  It allocates fixed-size blocks of memory (blk_size specified at the time
  of creation). When new object is added to the list, it occupies part of
  this block until it's full. Then the new block is allocated.
  Freed objects are chained to the m_free list, and if it's not empty, the
  newly added object is taken from this list instead the block.
*/

class Gcalc_dyn_list
{
public:
  class Item
  {
  public:
    Item *next;
  };

  Gcalc_dyn_list(size_t blk_size, size_t sizeof_item);
  ~Gcalc_dyn_list();
  Item *new_item()
  {
    Item *result;
    if (m_free)
    {
      result= m_free;
      m_free= m_free->next;
    }
    else
      result= alloc_new_blk();

    return result;
  }
  inline void free_item(Item *item)
  {
    item->next= m_free;
    m_free= item;
  }
  inline void free_list(Item **list, Item **hook)
  {
    *hook= m_free;
    m_free= *list;
  }

  void free_list(Item *list)
  {
    Item **hook= &list;
    while (*hook)
      hook= &(*hook)->next;
    free_list(&list, hook);
  }

  void reset();
  void cleanup();

protected:
  size_t m_blk_size;
  size_t m_sizeof_item;
  unsigned int m_points_per_blk;
  void *m_first_blk;
  void **m_blk_hook;
  Item *m_free;
  Item *m_keep;

  Item *alloc_new_blk();
  void format_blk(void* block);
  inline Item *ptr_add(Item *ptr, int n_items)
  {
    return (Item *)(((char*)ptr) + n_items * m_sizeof_item);
  }
};

/* Internal Gcalc coordinates to provide the precise calculations */

#define GCALC_DIG_BASE     1000000000
typedef uint32 gcalc_digit_t;
typedef unsigned long long gcalc_coord2;
typedef gcalc_digit_t Gcalc_internal_coord;
#define GCALC_COORD_BASE 2
#define GCALC_COORD_BASE2 4
#define GCALC_COORD_BASE3 6
#define GCALC_COORD_BASE4 8
#define GCALC_COORD_BASE5 10

typedef gcalc_digit_t Gcalc_coord1[GCALC_COORD_BASE];
typedef gcalc_digit_t Gcalc_coord2[GCALC_COORD_BASE*2];
typedef gcalc_digit_t Gcalc_coord3[GCALC_COORD_BASE*3];


void gcalc_mul_coord(Gcalc_internal_coord *result, int result_len,
                     const Gcalc_internal_coord *a, int a_len,
                     const Gcalc_internal_coord *b, int b_len);

void gcalc_add_coord(Gcalc_internal_coord *result, int result_len,
                     const Gcalc_internal_coord *a,
                     const Gcalc_internal_coord *b);

void gcalc_sub_coord(Gcalc_internal_coord *result, int result_len,
                     const Gcalc_internal_coord *a,
                     const Gcalc_internal_coord *b);

int gcalc_cmp_coord(const Gcalc_internal_coord *a,
                    const Gcalc_internal_coord *b, int len);

/* Internal coordinates declarations end. */


typedef uint gcalc_shape_info;

/*
  Gcalc_heap represents the 'dynamic list' of Info objects, that
  contain information about vertexes of all the shapes that take
  part in some spatial calculation. Can become quite long.
  After filled, the list is usually sorted and then walked through
  in the slicescan algorithm.
  The Gcalc_heap and the algorithm can only operate with two
  kinds of shapes - polygon and polyline. So all the spatial
  objects should be represented as sets of these two.
*/

class Gcalc_heap : public Gcalc_dyn_list
{
public:
  enum node_type
  {
    nt_shape_node,
    nt_intersection,
    nt_eq_node
  };
  class Info : public Gcalc_dyn_list::Item
  {
  public:
    node_type type;
    union
    {
      struct
      {
        /* nt_shape_node */
        gcalc_shape_info shape;
        Info *left;
        Info *right;
        double x,y;
        Gcalc_coord1 ix, iy;
        int top_node;
      } shape;
      struct
      {
        /* nt_intersection */
        /* Line p1-p2 supposed to intersect line p3-p4 */
        const Info *p1;
        const Info *p2;
        const Info *p3;
        const Info *p4;
        void *data;
        int equal;
      } intersection;
      struct
      {
        /* nt_eq_node */
        const Info *node;
        void *data;
      } eq;
    } node;

    bool is_bottom() const
      { GCALC_DBUG_ASSERT(type == nt_shape_node); return !node.shape.left; }
    bool is_top() const
      { GCALC_DBUG_ASSERT(type == nt_shape_node); return node.shape.top_node; }
    bool is_single_node() const
      { return is_bottom() && is_top(); }

    void calc_xy(double *x, double *y) const;
    int equal_pi(const Info *pi) const;
#ifdef GCALC_CHECK_WITH_FLOAT
    void calc_xy_ld(long double *x, long double *y) const;
#endif /*GCALC_CHECK_WITH_FLOAT*/

    Info *get_next() { return (Info *)next; }
    const Info *get_next() const { return (const Info *)next; }
  };

  Gcalc_heap(size_t blk_size=8192) :
    Gcalc_dyn_list(blk_size, sizeof(Info)),
    m_hook(&m_first), m_n_points(0)
  {}
  void set_extent(double xmin, double xmax, double ymin, double ymax);
  Info *new_point_info(double x, double y, gcalc_shape_info shape);
  void free_point_info(Info *i, Gcalc_dyn_list::Item **i_hook);
  Info *new_intersection(const Info *p1, const Info *p2,
                         const Info *p3, const Info *p4);
  void prepare_operation();
  inline bool ready() const { return m_hook == NULL; }
  Info *get_first() { return (Info *)m_first; }
  const Info *get_first() const { return (const Info *)m_first; }
  Gcalc_dyn_list::Item **get_last_hook() { return m_hook; }
  void reset();
#ifdef GCALC_CHECK_WITH_FLOAT
  long double get_double(const Gcalc_internal_coord *c) const;
#endif /*GCALC_CHECK_WITH_FLOAT*/
  double coord_extent;
  Gcalc_dyn_list::Item **get_cur_hook() { return m_hook; }

private:
  Gcalc_dyn_list::Item *m_first;
  Gcalc_dyn_list::Item **m_hook;
  int m_n_points;
};


/*
  the spatial object has to be represented as a set of
  simple polygones and polylines to be sent to the slicescan.

  Gcalc_shape_transporter class and his descendants are used to
  simplify storing the information about the shape into necessary structures.
  This base class only fills the Gcalc_heap with the information about
  shapes and vertices.

  Normally the Gcalc_shape_transporter family object is sent as a parameter
  to the 'get_shapes' method of an 'spatial' object so it can pass
  the spatial information about itself. The virtual methods are
  treating this data in a way the caller needs.
*/

class Gcalc_shape_transporter
{
private:
  Gcalc_heap::Info *m_first;
  Gcalc_heap::Info *m_prev;
  Gcalc_dyn_list::Item **m_prev_hook;
  int m_shape_started;
  void int_complete();
protected:
  Gcalc_heap *m_heap;
  int int_single_point(gcalc_shape_info Info, double x, double y);
  int int_add_point(gcalc_shape_info Info, double x, double y);
  void int_start_line()
  {
    DBUG_ASSERT(!m_shape_started);
    m_shape_started= 1;
    m_first= m_prev= NULL;
  }
  void int_complete_line()
  {
    DBUG_ASSERT(m_shape_started== 1);
    int_complete();
    m_shape_started= 0;
  }
  void int_start_ring()
  {
    DBUG_ASSERT(m_shape_started== 2);
    m_shape_started= 3;
    m_first= m_prev= NULL;
  }
  void int_complete_ring()
  {
    DBUG_ASSERT(m_shape_started== 3);
    int_complete();
    m_shape_started= 2;
  }
  void int_start_poly()
  {
    DBUG_ASSERT(!m_shape_started);
    m_shape_started= 2;
  }
  void int_complete_poly()
  {
    DBUG_ASSERT(m_shape_started== 2);
    m_shape_started= 0;
  }
  bool line_started() { return m_shape_started == 1; };
public:
  Gcalc_shape_transporter(Gcalc_heap *heap) :
    m_shape_started(0), m_heap(heap) {}

  virtual int single_point(double x, double y)=0;
  virtual int start_line()=0;
  virtual int complete_line()=0;
  virtual int start_poly()=0;
  virtual int complete_poly()=0;
  virtual int start_ring()=0;
  virtual int complete_ring()=0;
  virtual int add_point(double x, double y)=0;
  virtual int start_collection(int n_objects) { return 0; }
  virtual int empty_shape() { return 0; }
  int start_simple_poly()
  {
    return start_poly() || start_ring();
  }
  int complete_simple_poly()
  {
    return complete_ring() || complete_poly();
  }
  virtual ~Gcalc_shape_transporter() {}
};


enum Gcalc_scan_events
{
  scev_none= 0,
  scev_point= 1,         /* Just a new point in thread */
  scev_thread= 2,        /* Start of the new thread */
  scev_two_threads= 4,   /* A couple of new threads started */
  scev_intersection= 8,  /* Intersection happened */
  scev_end= 16,          /* Single thread finished */
  scev_two_ends= 32,     /* A couple of threads finished */
  scev_single_point= 64  /* Got single point */
};


/* 
   Gcalc_scan_iterator incapsulates the slisescan algorithm.
   It takes filled Gcalc_heap as an datasource. Then can be
   iterated trought the vertexes and intersection points with
   the step() method. After the 'step()' one usually observes
   the current 'slice' to do the necessary calculations, like
   looking for intersections, calculating the area, whatever.
*/

class Gcalc_scan_iterator : public Gcalc_dyn_list
{
public:
  class point : public Gcalc_dyn_list::Item
  {
  public:
    Gcalc_coord1 dx;
    Gcalc_coord1 dy;
    Gcalc_heap::Info *pi;
    Gcalc_heap::Info *next_pi;
    Gcalc_heap::Info *ev_pi;
    const Gcalc_coord1 *l_border;
    const Gcalc_coord1 *r_border;
    point *ev_next;

    Gcalc_scan_events event;

    inline const point *c_get_next() const
      { return (const point *)next; }
    inline bool is_bottom() const { return !next_pi; }
    gcalc_shape_info get_shape() const { return pi->node.shape.shape; }
    inline point *get_next() { return (point *)next; }
    inline const point *get_next() const { return (const point *)next; }
    /* Compare the dx_dy parameters regarding the horiz_dir */
    /* returns -1 if less, 0 if equal, 1 if bigger          */
    static int cmp_dx_dy(const Gcalc_coord1 dx_a,
                         const Gcalc_coord1 dy_a,
                         const Gcalc_coord1 dx_b,
                         const Gcalc_coord1 dy_b);
    static int cmp_dx_dy(const Gcalc_heap::Info *p1,
                         const Gcalc_heap::Info *p2,
                         const Gcalc_heap::Info *p3,
                         const Gcalc_heap::Info *p4);
    int cmp_dx_dy(const point *p) const;
    point **next_ptr() { return (point **) &next; }
#ifndef GCALC_DBUG_OFF
    unsigned int thread;
#endif /*GCALC_DBUG_OFF*/
#ifdef GCALC_CHECK_WITH_FLOAT
    void calc_x(long double *x, long double y, long double ix) const;
#endif /*GCALC_CHECK_WITH_FLOAT*/
  };

  /* That class introduced mostly for the 'typecontrol' reason.      */
  /* only difference from the point classis the get_next() function. */
  class event_point : public point
  {
  public:
    inline const event_point *get_next() const
    { return (const event_point*) ev_next; }
    int simple_event() const
    {
      return !ev_next ? (event & (scev_point | scev_end)) : 
        (!ev_next->ev_next && event == scev_two_ends);
    }
  };

  class intersection_info : public Gcalc_dyn_list::Item
  {
  public:
    point *edge_a;
    point *edge_b;

    Gcalc_coord2 t_a;
    Gcalc_coord2 t_b;
    int t_calculated;
    Gcalc_coord3 x_exp;
    int x_calculated;
    Gcalc_coord3 y_exp;
    int y_calculated;
    void calc_t()
    {if (!t_calculated) do_calc_t(); }
    void calc_y_exp()
    { if (!y_calculated) do_calc_y(); }
    void calc_x_exp()
    { if (!x_calculated) do_calc_x(); }

    void do_calc_t();
    void do_calc_x();
    void do_calc_y();
  };


  class slice_state
  {
  public:
    point *slice;
    point **event_position_hook;
    point *event_end;
    const Gcalc_heap::Info *pi;
  };

public:
  Gcalc_scan_iterator(size_t blk_size= 8192);

  GCALC_DECL_TERMINATED_STATE(killed)

  void init(Gcalc_heap *points); /* Iterator can be reused */
  void reset();
  int step();

  Gcalc_heap::Info *more_points() { return m_cur_pi; }
  bool more_trapezoids()
    { return m_cur_pi && m_cur_pi->next; }

  const point *get_bottom_points() const
    { return m_bottom_points; }
  const point *get_event_position() const
    { return *state.event_position_hook; }
  const point *get_event_end() const
    { return state.event_end; }
  const event_point *get_events() const
    { return (const event_point *)
        (*state.event_position_hook == state.event_end ?
            m_bottom_points : *state.event_position_hook); }
  const point *get_b_slice() const { return state.slice; }
  double get_h() const;
  double get_y() const;
  double get_event_x() const;
  double get_sp_x(const point *sp) const;
  int intersection_step() const
    { return state.pi->type == Gcalc_heap::nt_intersection; }
  const Gcalc_heap::Info *get_cur_pi() const
  {
    return state.pi;
  }

private:
  Gcalc_heap *m_heap;
  Gcalc_heap::Info *m_cur_pi;
  slice_state state;

#ifndef GCALC_DBUG_OFF
  unsigned int m_cur_thread;
#endif /*GCALC_DBUG_OFF*/

  point *m_bottom_points;
  point **m_bottom_hook;

  int node_scan();
  void eq_scan();
  void intersection_scan();
  void remove_bottom_node();
  int insert_top_node();
  int add_intersection(point *sp_a, point *sp_b,
                       Gcalc_heap::Info *pi_from);
  int add_eq_node(Gcalc_heap::Info *node, point *sp);
  int add_events_for_node(point *sp_node);

  point *new_slice_point()
  {
    point *new_point= (point *)new_item();
    return new_point;
  }
  intersection_info *new_intersection_info(point *a, point *b)
  {
    intersection_info *ii= (intersection_info *)new_item();
    ii->edge_a= a;
    ii->edge_b= b;
    ii->t_calculated= ii->x_calculated= ii->y_calculated= 0;
    return ii;
  }
  int arrange_event(int do_sorting, int n_intersections);
  static double get_pure_double(const Gcalc_internal_coord *d, int d_len);
};


/* 
   Gcalc_trapezoid_iterator simplifies the calculations on
   the current slice of the Gcalc_scan_iterator.
   One can walk through the trapezoids formed between
   previous and current slices.
*/

#ifdef TMP_BLOCK
class Gcalc_trapezoid_iterator
{
protected:
  const Gcalc_scan_iterator::point *sp0;
  const Gcalc_scan_iterator::point *sp1;
public:
  Gcalc_trapezoid_iterator(const Gcalc_scan_iterator *scan_i) :
    sp0(scan_i->get_b_slice()),
    sp1(scan_i->get_t_slice())
    {}

  inline bool more() const { return sp1 && sp1->next; }

  const Gcalc_scan_iterator::point *lt() const { return sp1; }
  const Gcalc_scan_iterator::point *lb() const { return sp0; }
  const Gcalc_scan_iterator::point *rb() const
  {
    const Gcalc_scan_iterator::point *result= sp0;
    while ((result= result->c_get_next())->is_bottom())
    {}
    return result;
  }
  const Gcalc_scan_iterator::point *rt() const
    { return sp1->c_get_next(); }

  void operator++()
  {
    sp0= rb();
    sp1= rt();
  }
};
#endif /*TMP_BLOCK*/


/* 
   Gcalc_point_iterator simplifies the calculations on
   the current slice of the Gcalc_scan_iterator.
   One can walk through the points on the current slice.
*/

class Gcalc_point_iterator
{
protected:
  const Gcalc_scan_iterator::point *sp;
public:
  Gcalc_point_iterator(const Gcalc_scan_iterator *scan_i):
    sp(scan_i->get_b_slice())
    {}

  inline bool more() const { return sp != NULL; }
  inline void operator++() { sp= sp->c_get_next(); }
  inline const Gcalc_scan_iterator::point *point() const { return sp; }
  inline const Gcalc_heap::Info *get_pi() const { return sp->pi; }
  inline gcalc_shape_info get_shape() const { return sp->get_shape(); }
  inline void restart(const Gcalc_scan_iterator *scan_i)
  { sp= scan_i->get_b_slice(); }
};

#endif /*GCALC_SLICESCAN_INCLUDED*/

