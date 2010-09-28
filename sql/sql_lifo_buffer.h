/**
  @defgroup Bi-directional LIFO buffers used by DS-MRR implementation
  @{
*/

class Forward_lifo_buffer;
class Backward_lifo_buffer;


/*
  A base class for in-memory buffer used by DS-MRR implementation. Common
  properties:
  - The buffer is last-in-first-out, i.e. elements that are written last are
    read first.
  - The buffer contains fixed-size elements. The elements are either atomic
    byte sequences or pairs of them.
  - The buffer resides in the memory provided by the user. It is possible to
     = dynamically (ie. between write operations) add ajacent memory space to
       the buffer
     = dynamically remove unused space from the buffer.
    The intent of this is to allow to have two buffers on adjacent memory
    space, one is being read from (and so its space shrinks), while the other 
    is being written to (and so it needs more and more space).

  There are two concrete classes, Forward_lifo_buffer and Backward_lifo_buffer.
*/

class Lifo_buffer 
{
protected:
  /**
    Pointers to data to be written. write() call will assume that 
    (*write_ptr1) points to size1 bytes of data to be written.
    If write_ptr2 != NULL then the buffer stores pairs, and (*write_ptr2) 
    points to size2 bytes of data that form the second component.
  */
  uchar **write_ptr1;
  size_t size1;
  uchar **write_ptr2;
  size_t size2;

  /**
    read() will do reading by storing pointer to read data into *read_ptr1 (if
    the buffer stores atomic elements), or into {*read_ptr1, *read_ptr2} (if
    the buffer stores pairs).
  */
  uchar **read_ptr1;
  uchar **read_ptr2;

  uchar *start; /**< points to start of buffer space */
  uchar *end;   /**< points to just beyond the end of buffer space */
public:

  enum enum_direction {
    BACKWARD=-1, /**< buffer is filled/read from bigger to smaller memory addresses */
    FORWARD=1  /**< buffer is filled/read from smaller to bigger memory addresses */
  };

  virtual enum_direction type() = 0;

  /* Buffer space control functions */

  /** Let the buffer store data in the given space. */
  void set_buffer_space(uchar *start_arg, uchar *end_arg) 
  {
    start= start_arg;
    end= end_arg;
    TRASH(start, end - start);
    reset();
  }
  
  /** 
    Specify where write() should get the source data from, as well as source
    data size.
  */
  void setup_writing(uchar **data1, size_t len1, uchar **data2, size_t len2)
  {
    write_ptr1= data1;
    size1= len1;
    write_ptr2= data2;
    size2= len2;
  }

  /** 
    Specify where read() should store pointers to read data, as well as read
    data size. The sizes must match those passed to setup_writing().
  */
  void setup_reading(uchar **data1, size_t len1, uchar **data2, size_t len2)
  {
    read_ptr1= data1;
    DBUG_ASSERT(len1 == size1);
    read_ptr2= data2;
    DBUG_ASSERT(len2 == size2);
  }
  
  bool can_write()
  {
    return have_space_for(size1 + (write_ptr2 ? size2 : 0));
  }
  virtual void write() = 0;

  bool is_empty() { return used_size() == 0; }
  virtual bool read() = 0;
  
  void sort(qsort2_cmp cmp_func, void *cmp_func_arg)
  {
    uint elem_size= size1 + (write_ptr2 ? size2 : 0);
    uint n_elements= used_size() / elem_size;
    my_qsort2(used_area(), n_elements, elem_size, cmp_func, cmp_func_arg);
  }

  virtual void reset() = 0;
  virtual uchar *end_of_space() = 0;
protected:
  bool have_data(size_t bytes)
  {
    return (used_size() >= bytes);
  }
  virtual bool have_space_for(size_t bytes) = 0;
  virtual size_t used_size() = 0;

public:

  virtual void remove_unused_space(uchar **unused_start, uchar **unused_end)=0;
  virtual uchar *used_area() = 0;
   
  /** Iterator to walk over contents of the buffer without reading it. */
  class Iterator
  {
  public:
    virtual void init(Lifo_buffer *buf) = 0;
    /*
      Read the next value. The calling convention is the same as buf->read()
      has.

      @retval FALSE - ok
      @retval TRUE  - EOF, reached the end of the buffer
    */
    virtual bool read_next()= 0;
    virtual ~Iterator() {}
  protected:
    Lifo_buffer *buf;
    virtual uchar *get_next(size_t nbytes)=0;
  };
  virtual ~Lifo_buffer() {};

  friend class Forward_iterator;
  friend class Backward_iterator;
};


/**
  Forward LIFO buffer

  The buffer that is being written to from start to end and read in the
  reverse.  'pos' points to just beyond the end of used space.

  It is possible to grow/shink the buffer at the end bound

     used space      unused space  
   *==============*-----------------*
   ^              ^                 ^
   |              |                 +--- end
   |              +---- pos              
   +--- start           
*/

class Forward_lifo_buffer: public Lifo_buffer
{
  uchar *pos;
public:
  enum_direction type() { return FORWARD; }
  size_t used_size()
  {
    return pos - start;
  }
  void reset()
  {
    pos= start;
  }
  uchar *end_of_space() { return pos; }
  bool have_space_for(size_t bytes)
  {
    return (pos + bytes < end);
  }

  void write()
  {
    write_bytes(*write_ptr1, size1);
    if (write_ptr2)
      write_bytes(*write_ptr2, size2);
  }
  void write_bytes(const uchar *data, size_t bytes)
  {
    DBUG_ASSERT(have_space_for(bytes));
    memcpy(pos, data, bytes);
    pos += bytes;
  }
  uchar *read_bytes(size_t bytes)
  {
    DBUG_ASSERT(have_data(bytes));
    pos= pos - bytes;
    return pos;
  }
  bool read()
  {
    if (!have_data(size1 + (read_ptr2 ? size2 : 0)))
      return TRUE;
    if (read_ptr2)
      *read_ptr2= read_bytes(size2);
    *read_ptr1= read_bytes(size1);
    return FALSE;
  }
  void remove_unused_space(uchar **unused_start, uchar **unused_end)
  {
    DBUG_ASSERT(0); /* Don't need this yet */
  }
  /**
    Add more space to the buffer. The caller is responsible that the space
    being added is adjacent to the end of the buffer.

    @param unused_start Start of space
    @param unused_end   End of space
  */
  void grow(uchar *unused_start, uchar *unused_end)
  {
    DBUG_ASSERT(unused_end >= unused_start);
    DBUG_ASSERT(end == unused_start);
    TRASH(unused_start, unused_end - unused_start);
    end= unused_end;
  }
  /* Return pointer to start of the memory area that is occupied by the data */
  uchar *used_area() { return start; }
  friend class Forward_iterator;
};


/**
  Iterator for Forward_lifo_buffer
*/

class Forward_iterator : public Lifo_buffer::Iterator
{
  uchar *pos;

  /** Return pointer to next chunk of nbytes bytes and avance over it */
  uchar *get_next(size_t nbytes)
  {
    if (pos - nbytes < ((Forward_lifo_buffer*)buf)->start)
      return NULL;
    pos -= nbytes;
    return pos;
  }
public:
  bool read_next()
  {
    uchar *res;
    if (buf->read_ptr2)
    {
      if ((res= get_next(buf->size2)))
      {
        *(buf->read_ptr2)= res;
        *buf->read_ptr1= get_next(buf->size1);
        return FALSE;
      }
    }
    else
    {
      if ((res= get_next(buf->size1)))
      {
        *(buf->read_ptr1)= res;
        return FALSE;
      }
    }
    return TRUE; /* EOF */
  }

  void init(Lifo_buffer *buf_arg)
  {
    DBUG_ASSERT(buf_arg->type() == Lifo_buffer::FORWARD);
    buf= buf_arg;
    pos= ((Forward_lifo_buffer*)buf)->pos;
  }
};


/**
  Backward LIFO buffer

  The buffer that is being written to from start to end and read in the
  reverse.  'pos' points to the start of used space.

  It is possible to grow/shink the buffer at the start.

     unused space      used space  
   *--------------*=================*
   ^              ^                 ^
   |              |                 +--- end
   |              +---- pos              
   +--- start           
*/
class Backward_lifo_buffer: public Lifo_buffer
{
  uchar *pos;
public:
  enum_direction type() { return BACKWARD; }
 
  size_t used_size()
  {
    return end - pos;
  }
  void reset()
  {
    pos= end;
  }
  uchar *end_of_space() { return end; }
  bool have_space_for(size_t bytes)
  {
    return (pos - bytes >= start);
  }
  void write()
  {
    if (write_ptr2)
      write_bytes(*write_ptr2, size2);
    write_bytes(*write_ptr1, size1);
  }
  void write_bytes(const uchar *data, size_t bytes)
  {
    DBUG_ASSERT(have_space_for(bytes));
    pos -= bytes;
    memcpy(pos, data, bytes);
  }
  bool read()
  {
    if (!have_data(size1 + (read_ptr2 ? size2 : 0)))
      return TRUE;
    *read_ptr1= read_bytes(size1);
    if (read_ptr2)
      *read_ptr2= read_bytes(size2);
    return FALSE;
  }
  uchar *read_bytes(size_t bytes)
  {
    DBUG_ASSERT(have_data(bytes));
    uchar *ret= pos;
    pos= pos + bytes;
    return ret;
  }
  /**
    Stop using/return the unused part of the space
    @param unused_start  OUT Start of the unused space
    @param unused_end    OUT End of the unused space
  */
  void remove_unused_space(uchar **unused_start, uchar **unused_end)
  {
    *unused_start= start;
    *unused_end= pos;
    start= pos;
  }
  void grow(uchar *unused_start, uchar *unused_end)
  {
    DBUG_ASSERT(0); /* Not used for backward buffers */
  }
  /* Return pointer to start of the memory area that is occupied by the data */
  uchar *used_area() { return pos; }
  friend class Backward_iterator;
};


/**
  Iterator for Backward_lifo_buffer
*/

class Backward_iterator : public Lifo_buffer::Iterator
{
  uchar *pos;
  /* Return pointer to next chunk of nbytes bytes and advance over it */
  uchar *get_next(size_t nbytes)
  {
    if (pos + nbytes > ((Backward_lifo_buffer*)buf)->end)
      return NULL;
    uchar *res= pos;
    pos += nbytes;
    return res;
  }
public:
  bool read_next()
  {
    /*
      Always read the first component first (if the buffer is backwards, we
      have written the second component first).
    */
    uchar *res;
    if ((res= get_next(buf->size1)))
    {
      *(buf->read_ptr1)= res;
      if (buf->read_ptr2)
        *buf->read_ptr2= get_next(buf->size2);
      return FALSE;
    }
    return TRUE; /* EOF */
  }
  void init(Lifo_buffer *buf_arg)
  {
    DBUG_ASSERT(buf_arg->type() == Lifo_buffer::BACKWARD);
    buf= buf_arg;
    pos= ((Backward_lifo_buffer*)buf)->pos;
  }
};



