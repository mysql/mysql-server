#ifndef gcalc_slicescan_h
#define gcalc_slicescan_h

/*#define GCALC_DBUG*/

#ifdef GCALC_DBUG
class gcalc_dbug_env_struct
{
  FILE *recfile;
  bool first_point;
public:
  gcalc_dbug_env_struct() : recfile(NULL) {}
  virtual void start_ring();
  virtual void start_line();
  virtual void add_point(double x, double y);
  virtual void complete();
  virtual void print(const char *ln);
  
  void start_newfile(const char *filename);
  void start_append(const char *filename);
  void stop_recording();
  virtual ~gcalc_dbug_env_struct();
};

extern gcalc_dbug_env_struct *gcalc_dbug_cur_env;
void gcalc_dbug_do_print(const char* fmt, ...);
#define GCALC_DBUG_START_RING gcalc_dbug_cur_env->start_ring()
#define GCALC_DBUG_START_LINE gcalc_dbug_cur_env->start_line()
#define GCALC_DBUG_ADD_POINT(X,Y) gcalc_dbug_cur_env->add_point(X,Y)
#define GCALC_DBUG_COMPLETE gcalc_dbug_cur_env->complete()
#define GCALC_DBUG_SLICE(SLICE) SLICE->dbug_print()
#define GCALC_DBUG_STARTFILE(FILENAME) gcalc_dbug_cur_env->start_newfile(FILENAME)
#define GCALC_DBUG_STOP gcalc_dbug_cur_env->stop_recording()
#define GCALC_DBUG_PRINT(ARGLIST) gcalc_dbug_do_print ARGLIST
#else
#define GCALC_DBUG_START_RING
#define GCALC_DBUG_START_LINE
#define GCALC_DBUG_ADD_POINT(X,Y)
#define GCALC_DBUG_COMPLETE
#define GCALC_DBUG_SLICE(SLICE)
#define GCALC_DBUG_STARTFILE(FILENAME)
#define GCALC_DBUG_STOP
#define GCALC_DBUG_PRINT(ARGLIST)
#endif /*GCLAC_DBUG*/

class gcalc_dyn_list
{
public:
  class item
  {
  public:
    item *next;
  };

  gcalc_dyn_list(size_t blk_size, size_t sizeof_item);
  ~gcalc_dyn_list();
  item *new_item()
  {
    item *result;
    if (m_free)
    {
      result= m_free;
      m_free= m_free->next;
    }
    else
      result= alloc_new_blk();

    return result;
  }
  inline void free_item(item *item)
  {
    item->next= m_free;
    m_free= item;
  }
  inline void free_list(item *list, item **hook)
  {
    *hook= m_free;
    m_free= list;
  }

  void free_list(item *list)
  {
    item **hook= &list;
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
  item *m_free;
  item *m_keep;

  item *alloc_new_blk();
  inline item *ptr_add(item *ptr, int n_items)
  {
    return (item *)(((char*)ptr) + n_items * m_sizeof_item);
  }
};

typedef uint gcalc_shape_info;

class gcalc_heap : public gcalc_dyn_list
{
public:
  class info : public gcalc_dyn_list::item
  {
  public:
    gcalc_shape_info shape;
    info *left;
    info *right;
    double x,y;

    inline bool is_bottom() const { return !left; }
    inline info *get_next() { return (info *)next; }
    inline const info *get_next() const { return (const info *)next; }
  };

  gcalc_heap(size_t blk_size=8192) :
    gcalc_dyn_list(blk_size, sizeof(info)), m_hook(&m_first), m_n_points(0) {}
/*  void reset_keep(int (*keep_point)(sc_point_info info, const void *parameter),
		  const void *parameter);
*/
  info *new_point_info(double x, double y, gcalc_shape_info shape)
  {
    info *result= (info *)new_item();
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
  /* should be removed
  void free_point_info(info **pi_hook);
  */
  void prepare_operation();
  inline bool ready() const { return m_hook == NULL; }
  info *get_first() { return (info *)m_first; }
  const info *get_first() const { return (const info *)m_first; }
  gcalc_dyn_list::item **get_last_hook() { return m_hook; }
  void reset();
private:
  gcalc_dyn_list::item *m_first;
  gcalc_dyn_list::item **m_hook;
  int m_n_points;
};


class gcalc_shape_transporter
{
private:
  gcalc_heap::info *m_first;
  gcalc_heap::info *m_prev;
  int m_shape_started;
  void int_complete();
protected:
  gcalc_heap *m_heap;
  int int_single_point(gcalc_shape_info info, double x, double y);
  int int_add_point(gcalc_shape_info info, double x, double y);
  void int_start_line()
  {
    DBUG_ASSERT(!m_shape_started);
    m_shape_started= 1;
    m_first= m_prev= NULL;
    GCALC_DBUG_START_LINE;
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
    GCALC_DBUG_START_RING;
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
  gcalc_shape_transporter(gcalc_heap *heap) :
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
  int start_simple_poly()
  {
    return start_poly() || start_ring();
  }
  int complete_simple_poly()
  {
    return complete_ring() || complete_poly();
  }
  virtual ~gcalc_shape_transporter() {}
};


enum gcalc_scan_events
{
  scev_point= 1,         /* Just a new point in thread */
  scev_thread= 2,        /* Start of the new thread */
  scev_two_threads= 4,   /* A couple of new threads started */
  scev_intersection= 8,  /* Intersection happened */
  scev_end= 16,          /* Single thread finished */
  scev_two_ends= 32,     /* A couple of threads finished */
  scev_single_point= 64  /* Got single point */
};

typedef int sc_thread_id;

/* int-returning methods return nonzero value if there was an memory
   allocation error */
class gcalc_scan_iterator : public gcalc_dyn_list
{
public:
  class point : public gcalc_dyn_list::item
  {
  public:
    double x;
    double dx_dy;
    int horiz_dir;
    gcalc_heap::info *pi;
    gcalc_heap::info *next_pi;
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
#ifdef GCALC_DBUG
    void dbug_print();
#endif /*GCALC_DBUG*/
  };

  class intersection : public gcalc_dyn_list::item
  {
  public:
    sc_thread_id thread_a;
    sc_thread_id thread_b;
    double x;
    double y;
    inline intersection *get_next() { return (intersection *)next; }
  };

public:
  gcalc_scan_iterator(size_t blk_size= 8192);

  void init(gcalc_heap *points); /* Iterator can be reused */
  void reset();
  int step()
  {
    DBUG_ASSERT(more_points());
    return m_cur_intersection ? intersection_scan() : normal_scan();
  }

  inline gcalc_heap::info *more_points() { return m_cur_pi; }
  inline bool more_trapeziums()
    { return m_cur_pi && m_cur_pi->next; }

  inline gcalc_scan_events get_event() const { return m_event0; }
  inline const point *get_event_position() const
    { return m_event_position0; }
  inline const point *get_b_slice() const { return m_slice0; }
  inline const point *get_t_slice() const { return m_slice1; }
  inline double get_h() const { return m_h; }
  inline double get_y() const { return m_y0; }

private:
  gcalc_heap::info *m_cur_pi;
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
  gcalc_scan_events m_event0, m_event1;
  point *m_event_position0;
  point *m_event_position1;

  int normal_scan();
  int intersection_scan();
  void sort_intersections();
  int handle_intersections();
  int insert_top_point();
  int add_intersection(const point *a, const point *b,
		       int isc_kind, gcalc_dyn_list::item ***p_hook);
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

class gcalc_trapezium_iterator
{
protected:
  const gcalc_scan_iterator::point *sp0;
  const gcalc_scan_iterator::point *sp1;
public:
  gcalc_trapezium_iterator(const gcalc_scan_iterator *scan_i) :
    sp0(scan_i->get_b_slice()),
    sp1(scan_i->get_t_slice())
    {}

  inline bool more() const { return sp1 && sp1->next; }

  const gcalc_scan_iterator::point *lt() const { return sp1; }
  const gcalc_scan_iterator::point *lb() const { return sp0; }
  const gcalc_scan_iterator::point *rb() const
  {
    const gcalc_scan_iterator::point *result= sp0;
    while ((result= result->c_get_next())->is_bottom());
    return result;
  }
  const gcalc_scan_iterator::point *rt() const
    { return sp1->c_get_next(); }

  void operator++()
  {
    sp0= rb();
    sp1= rt();
  }
};

class gcalc_point_iterator
{
protected:
  const gcalc_scan_iterator::point *sp;
public:
  gcalc_point_iterator(const gcalc_scan_iterator *scan_i):
    sp(scan_i->get_b_slice())
    {}

  inline bool more() const { return sp != NULL; }
  inline void operator++() { sp= sp->c_get_next(); }
  inline const gcalc_scan_iterator::point *point() const { return sp; }
  inline const gcalc_heap::info *get_pi() const { return sp->pi; }
  inline gcalc_shape_info get_shape() const { return sp->get_shape(); }
  inline double get_x() const { return sp->x; }
};

#endif /*gcalc_slicescan_h*/

