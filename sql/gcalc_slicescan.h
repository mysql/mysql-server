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


#ifndef GCALC_SLICESCAN_INCLUDED
#define GCALC_SLICESCAN_INCLUDED


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
#ifndef DBUG_OFF
  uint m_last_item_id;
#endif
public:
  class Item
  {
#ifndef DBUG_OFF
    uint m_item_id;
  public:
    uint item_id() const { return m_item_id; }
    void set_item_id(uint id) { m_item_id= id; }
#endif
  public:
    Item *next;
  };

  Gcalc_dyn_list(size_t blk_size, size_t sizeof_item);
  ~Gcalc_dyn_list();
  Item *new_item()
  {
    Item *result;
    if (!m_free && alloc_new_blk())
      return NULL;

    DBUG_ASSERT(m_free);
    result= m_free;
    m_free= m_free->next;

#ifndef DBUG_OFF
    result->set_item_id(++m_last_item_id);
#endif
    result->next= NULL;
    return result;
  }
  inline void free_item(Item *item)
  {
    item->next= m_free;
    m_free= item;
  }
  inline void free_list(Item *list, Item **hook)
  {
    *hook= m_free;
    m_free= list;
  }

  void free_list(Item *list)
  {
    Item **hook= &list;
    while (*hook)
      hook= &(*hook)->next;
    free_list(list, hook);
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

  bool alloc_new_blk();
  void format_blk(void* block);
  inline Item *ptr_add(Item *ptr, int n_items)
  {
    return (Item *)(((char*)ptr) + n_items * m_sizeof_item);
  }
};

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
  class Info : public Gcalc_dyn_list::Item
  {
  public:
    gcalc_shape_info shape;
    Info *left;
    Info *right;
    double x,y;

    inline bool is_bottom() const { return !left; }
    inline Info *get_next() { return (Info *)next; }
    inline const Info *get_next() const { return (const Info *)next; }
#ifndef DBUG_OFF
    inline void dbug_print() const;
#endif
  };

  Gcalc_heap(size_t blk_size=8192) :
    Gcalc_dyn_list(blk_size, sizeof(Info)), m_hook(&m_first), m_n_points(0) {}
  Info *new_point_info(double x, double y, gcalc_shape_info shape)
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
    return result;
  }
  void prepare_operation();
  inline bool ready() const { return m_hook == NULL; }
  Info *get_first() { return (Info *)m_first; }
  const Info *get_first() const { return (const Info *)m_first; }
  Gcalc_dyn_list::Item **get_last_hook() { return m_hook; }
  void reset();
private:
  Gcalc_dyn_list::Item *m_first;
  Gcalc_dyn_list::Item **m_hook;
  int m_n_points;
};


/**
  A class to store a state of a geometry transformation of
  Gcalc_shape_transformer::store_shapes()
  and other related methods (e.g. start_line/complete_line etc).
*/
class Gcalc_shape_status
{
public:
  int m_nshapes;
  int m_last_shape_pos;
  Gcalc_shape_status()
  {
    m_nshapes= 0;        // How many shapes have been collected
    m_last_shape_pos= 0; // Last shape start position in function_buffer
  }
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

  /* Transformation event methods */
  virtual int single_point(Gcalc_shape_status *st, double x, double y)=0;
  virtual int start_line(Gcalc_shape_status *st)=0;
  virtual int complete_line(Gcalc_shape_status *st)=0;
  virtual int start_poly(Gcalc_shape_status *st)=0;
  virtual int complete_poly(Gcalc_shape_status *st)=0;
  virtual int start_ring(Gcalc_shape_status *st)=0;
  virtual int complete_ring(Gcalc_shape_status *st)=0;
  virtual int add_point(Gcalc_shape_status *st, double x, double y)=0;
  virtual int start_collection(Gcalc_shape_status *st, int nshapes)= 0;
  virtual int complete_collection(Gcalc_shape_status *st)= 0;
  virtual int collection_add_item(Gcalc_shape_status *st_collection,
                                  Gcalc_shape_status *st_item)= 0;
  int start_simple_poly(Gcalc_shape_status *st)
  {
    return start_poly(st) || start_ring(st);
  }
  int complete_simple_poly(Gcalc_shape_status *st)
  {
    return complete_ring(st) || complete_poly(st);
  }

  /*
    Filter methods: in some cases we are not interested in certain
    geometry types and can skip them during transformation instead
    of inserting "no operation" actions.
    For example, ST_Buffer() called  with a negative distance argument
    does not need any Points and LineStrings.
  */
  virtual bool skip_point() const { return false; }
  virtual bool skip_line_string() const { return false; }
  virtual bool skip_poly() const { return false; }

  virtual ~Gcalc_shape_transporter() {}
};


enum Gcalc_scan_events
{
  scev_point= 1,         /* Just a new point in thread */
  scev_thread= 2,        /* Start of the new thread */
  scev_two_threads= 4,   /* A couple of new threads started */
  scev_intersection= 8,  /* Intersection happened */
  scev_end= 16,          /* Single thread finished */
  scev_two_ends= 32,     /* A couple of threads finished */
  scev_single_point= 64  /* Got single point */
};

#ifndef DBUG_OFF
const char *Gcalc_scan_event_name(enum Gcalc_scan_events event);
#endif

typedef int sc_thread_id;

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
    double x;
    double dx_dy;
    int horiz_dir;
    Gcalc_heap::Info *pi;
    Gcalc_heap::Info *next_pi;
    sc_thread_id thread;
    const point *precursor; /* used as a temporary field */

    inline const point *c_get_next() const
      { return (const point *)next; }
    inline bool is_bottom() const { return pi->is_bottom(); }
    gcalc_shape_info get_shape() const { return pi->shape; }
    inline point *get_next() { return (point *)next; }
    inline const point *get_next() const { return (const point *)next; }

    /* copies all but 'next' 'x' and 'precursor' */
    void copy_core(const point *from)
    {
      dx_dy= from->dx_dy;
      horiz_dir= from->horiz_dir;
      pi= from->pi;
      next_pi= from->next_pi;
      thread= from->thread;
    }
#ifndef DBUG_OFF
    inline void dbug_print_slice(double y, enum Gcalc_scan_events event) const;
#endif
  };

  class intersection : public Gcalc_dyn_list::Item
  {
  public:
    sc_thread_id thread_a;
    sc_thread_id thread_b;
    double x;
    double y;
    inline intersection *get_next() { return (intersection *)next; }
  };

public:
  Gcalc_scan_iterator(size_t blk_size= 8192);

  void init(Gcalc_heap *points); /* Iterator can be reused */
  void reset();
  int step()
  {
    DBUG_ASSERT(more_points());
    return m_cur_intersection ? intersection_scan() : normal_scan();
  }

  inline Gcalc_heap::Info *more_points() { return m_cur_pi; }
  inline bool more_trapezoids()
    { return m_cur_pi && m_cur_pi->next; }

  inline Gcalc_scan_events get_event() const { return m_event0; }
  inline const point *get_event_position() const
    { return m_event_position0; }
  inline const point *get_b_slice() const { return m_slice0; }
  inline const point *get_t_slice() const { return m_slice1; }
  inline double get_h() const { return m_h; }
  inline double get_y() const { return m_y0; }

private:
  Gcalc_heap::Info *m_cur_pi;
  point *m_slice0;
  point *m_slice1;
  point *m_sav_slice;
  intersection *m_intersections;
  int m_n_intersections;
  intersection *m_cur_intersection;
  point **m_pre_intersection_hook;
  double m_h;
  double m_y0;
  double m_y1;
  double m_sav_y;
  bool m_next_is_top_point;
  unsigned int m_bottom_points_count;
  sc_thread_id m_cur_thread;
  Gcalc_scan_events m_event0, m_event1;
  point *m_event_position0;
  point *m_event_position1;

  int normal_scan();
  int intersection_scan();
  void sort_intersections();
  int handle_intersections();
  int insert_top_point();
  int add_intersection(const point *a, const point *b,
		       int isc_kind, Gcalc_dyn_list::Item ***p_hook);
  int find_intersections();
  void pop_suitable_intersection();

  intersection *new_intersection()
  {
    return (intersection *)new_item();
  }
  point *new_slice_point()
  {
    return (point *)new_item();
  }
  point *new_slice(point *example);
};


/* 
   Gcalc_trapezoid_iterator simplifies the calculations on
   the current slice of the Gcalc_scan_iterator.
   One can walk through the trapezoids formed between
   previous and current slices.
*/

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
  inline double get_x() const { return sp->x; }
};

#endif /*GCALC_SLICESCAN_INCLUDED*/

