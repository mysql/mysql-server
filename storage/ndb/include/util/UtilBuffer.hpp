/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef __BUFFER_HPP_INCLUDED__
#define __BUFFER_HPP_INCLUDED__

#include <ndb_global.h>

/* This class represents a buffer of binary data, where you can append
 * data at the end, and later read the entire bunch.
 * It will take care of the hairy details of realloc()ing the space
 * for you.
 */
class UtilBuffer {
public:
  UtilBuffer() : data(nullptr), len(0), alloc_size(0) { }
  ~UtilBuffer() { free(data); }

  /* Grow buffer to specified length.
     On success, returns 0. On failure, returns -1 and sets errno.
  */
  int grow(size_t l) {
    if(l > alloc_size)
      return reallocate(l);
    return 0;
  }

  /* Append to current data.
     On success, returns 0. On failure, returns -1 and sets errno.
  */
  int append(const void *d, size_t l) {
    if (likely(l > 0))
    {
      if (unlikely(d == nullptr))
      {
        errno = EINVAL;
        return -1;
      }

      void * pos = append(l);
      if(pos == nullptr)
      {
        return -1;
      }

      memcpy(pos, d, l);
    }
    return 0;
  }

  /* Append to current data.
     Returns pointer where data of length l can be written.
     On failure, returns nullptr and sets errno.
  */
  void * append(size_t l){
    if(grow(len+l) != 0)
      return nullptr;

    void * ret = (char*)data+len;
    len += l;
    return ret;
  }
  
  /* Free the current buffer memory, and assign new content.
     On success, returns 0. On failure, returns -1 and sets errno.
  */
  int assign(const void * d, size_t l) {
    /* Free the old data only after copying, in case d==data. */
    void *old_data= data;
    data = nullptr;
    len = 0;
    alloc_size = 0;
    int ret= append(d, l);
    if (old_data)
      free(old_data);
    return ret;
  }

  /* Truncate contents to 0 length without freeing buffer memory. */
  void clear() {
    len = 0;
  }

  int length() const { assert(Uint64(len) == Uint32(len)); return (int)len; }

  void *get_data() const { return data; }

  bool empty () const { return len == 0; }

  bool equal(const UtilBuffer &cmp) const {
    if(len==0 && cmp.len==0)
      return true;
    else if(len!=cmp.len)
      return false;
    else
      return (memcmp(get_data(), cmp.get_data(), len) == 0);
  }

  int assign(const UtilBuffer& buf) {
    int ret = 0;
    if(this != &buf) {
      ret = assign(buf.get_data(), buf.length());
    }
    return ret;
  }

private:

  int reallocate(size_t newsize) {
    if(newsize < len) {
      errno = EINVAL;
      return -1;
    }
    void *newdata;
    if((newdata = realloc(data, newsize)) == NULL) {
      errno = ENOMEM;
      return -1;
    }
    alloc_size = newsize;
    data = newdata;
    return 0;
  }

  void *data;          /* Pointer to data storage */
  size_t len;          /* Size of the stored data */
  size_t alloc_size;   /* Size of the allocated space,
			*  i.e. len can grow to this size */
};

#endif /* !__BUFFER_HPP_INCLUDED__ */
