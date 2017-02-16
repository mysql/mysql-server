/*
   Copyright (c) 2010, 2011, Monty Program Ab

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
  size_t size1;
  size_t size2;

public:
  /**
    write() will put into buffer size1 bytes pointed by write_ptr1. If
    size2!=0, then they will be accompanied by size2 bytes pointed by
    write_ptr2.
  */
  uchar *write_ptr1;
  uchar *write_ptr2;

  /**
    read() will do reading by storing pointers to read data into read_ptr1 or
    into (read_ptr1, read_ptr2), depending on whether the buffer was set to
    store single objects or pairs.
  */
  uchar *read_ptr1;
  uchar *read_ptr2;

protected:
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
    if (end != start)
      TRASH(start, end - start);
    reset();
  }
  
  /** 
    Specify where write() should get the source data from, as well as source
    data size.
  */
  void setup_writing(size_t len1, size_t len2)
  {
    size1= len1;
    size2= len2;
  }

  /** 
    Specify where read() should store pointers to read data, as well as read
    data size. The sizes must match those passed to setup_writing().
  */
  void setup_reading(size_t len1, size_t len2)
  {
    DBUG_ASSERT(len1 == size1);
    DBUG_ASSERT(len2 == size2);
  }
  
  bool can_write()
  {
    return have_space_for(size1 + size2);
  }
  virtual void write() = 0;

  bool is_empty() { return used_size() == 0; }
  virtual bool read() = 0;
  
  void sort(qsort2_cmp cmp_func, void *cmp_func_arg)
  {
    size_t elem_size= size1 + size2;
    size_t n_elements= used_size() / elem_size;
    my_qsort2(used_area(), n_elements, elem_size, cmp_func, cmp_func_arg);
  }

  virtual void reset() = 0;
  virtual uchar *end_of_space() = 0;
protected:
  virtual size_t used_size() = 0;
  
  /* To be used only by iterator class: */
  virtual uchar *get_pos()= 0;
  virtual bool read(uchar **position, uchar **ptr1, uchar **ptr2)= 0;
  friend class Lifo_buffer_iterator;
public:
  virtual bool have_space_for(size_t bytes) = 0;

  virtual void remove_unused_space(uchar **unused_start, uchar **unused_end)=0;
  virtual uchar *used_area() = 0; 
  virtual ~Lifo_buffer() {};
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
    return (size_t)(pos - start);
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
    write_bytes(write_ptr1, size1);
    if (size2)
      write_bytes(write_ptr2, size2);
  }
  void write_bytes(const uchar *data, size_t bytes)
  {
    DBUG_ASSERT(have_space_for(bytes));
    memcpy(pos, data, bytes);
    pos += bytes;
  }
  bool have_data(uchar *position, size_t bytes)
  {
    return ((position - start) >= (ptrdiff_t)bytes);
  }
  uchar *read_bytes(uchar **position, size_t bytes)
  {
    DBUG_ASSERT(have_data(*position, bytes));
    *position= (*position) - bytes;
    return *position;
  }
  bool read() { return read(&pos, &read_ptr1, &read_ptr2); }
  bool read(uchar **position, uchar **ptr1, uchar **ptr2)
  {
    if (!have_data(*position, size1 + size2))
      return TRUE;
    if (size2)
      *ptr2= read_bytes(position, size2);
    *ptr1= read_bytes(position, size1);
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
  friend class Lifo_buffer_iterator;
  uchar *get_pos() { return pos; }
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
    return (size_t)(end - pos);
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
      write_bytes(write_ptr2, size2);
    write_bytes(write_ptr1, size1);
  }
  void write_bytes(const uchar *data, size_t bytes)
  {
    DBUG_ASSERT(have_space_for(bytes));
    pos -= bytes;
    memcpy(pos, data, bytes);
  }
  bool read()
  {
    return read(&pos, &read_ptr1, &read_ptr2);
  }
  bool read(uchar **position, uchar **ptr1, uchar **ptr2)
  {
    if (!have_data(*position, size1 + size2))
      return TRUE;
    *ptr1= read_bytes(position, size1);
    if (size2)
      *ptr2= read_bytes(position, size2);
    return FALSE;
  }
  bool have_data(uchar *position, size_t bytes)
  {
    return ((end - position) >= (ptrdiff_t)bytes);
  }
  uchar *read_bytes(uchar **position, size_t bytes)
  {
    DBUG_ASSERT(have_data(*position, bytes));
    uchar *ret= *position;
    *position= *position + bytes;
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
  friend class Lifo_buffer_iterator;
  uchar *get_pos() { return pos; }
};


/** Iterator to walk over contents of the buffer without reading from it */
class Lifo_buffer_iterator
{
  uchar *pos;
  Lifo_buffer *buf;
  
public:
  /* The data is read to here */
  uchar *read_ptr1;
  uchar *read_ptr2;

  void init(Lifo_buffer *buf_arg)
  {
    buf= buf_arg;
    pos= buf->get_pos();
  }
  /*
    Read the next value. The calling convention is the same as buf->read()
    has.

    @retval FALSE - ok
    @retval TRUE  - EOF, reached the end of the buffer
  */
  bool read() 
  {
    return buf->read(&pos, &read_ptr1, &read_ptr2);
  }
};


