/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_PAGE_POOL_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_PAGE_POOL_H_

#include <stdint.h>
#include <atomic>
#include <list>

#include "plugin/x/ngs/include/ngs/memory.h"
#include "plugin/x/ngs/include/ngs/thread.h"

#define BUFFER_PAGE_SIZE 4096

namespace ngs {

class Page_pool;

// 4KB aligned buffer to be used for reading data from sockets.
class Page {
 public:
  Page(uint32_t pcapacity, char *pdata) {
    capacity = pcapacity;
    data = pdata;
    length = 0;
    references = 0;
    saved_length = 0;
  }

  Page(uint32_t pcapacity = BUFFER_PAGE_SIZE) {
    capacity = pcapacity;
    ngs::allocate_array(data, capacity, KEY_memory_x_recv_buffer);
    length = 0;
    references = 0;
    saved_length = 0;
  }

  virtual ~Page() { ngs::free_array(data); }

  void aquire() { ++references; }
  void release() {
    if (0 == --references) destroy();
  }

  void save_state() { saved_length = length; }
  void rollback() { length = saved_length; }

  uint32_t get_free_bytes() { return capacity - length; }

  uint8_t *get_free_ptr() { return (uint8_t *)data + length; }

  char *data;
  uint32_t capacity;
  uint32_t length;

 protected:
  virtual void destroy() {}

 private:
  Page(const Page &);
  void operator=(const Page &);

  uint16_t references;
  uint32_t saved_length;
};

template <typename ResType>
class Resource {
 public:
  Resource();
  Resource(ResType *res);
  Resource(const Resource<ResType> &resource);

  ~Resource();

  ResType *operator->();
  ResType *operator->() const;

 private:
  ResType *m_res;
};

struct Pool_config {
  int32_t pages_max;
  int32_t pages_cache_max;
  int32_t page_size;
};

class Page_pool {
 public:
  /* Unlimited allocation, no caching */
  Page_pool(const int32_t page_size = BUFFER_PAGE_SIZE);
  Page_pool(const Pool_config &pool_config);
  ~Page_pool();

  Resource<Page> allocate();

  class No_more_pages_exception : public std::exception {
   public:
    virtual const char *what() const throw() {
      return "No more memory pages available";
    }
  };

 private:
  Page_pool(const Page_pool &) = delete;
  Page_pool &operator=(const Page_pool &) = delete;

  class Page_memory_managed : public Page {
   public:
    Page_memory_managed(Page_pool &pool, uint32_t pcapacity, char *pdata)
        : Page(pcapacity, pdata), m_pool(pool) {}

    ~Page_memory_managed() { data = NULL; }

   private:
    virtual void destroy() { m_pool.deallocate(this); }

    Page_pool &m_pool;
  };

  void deallocate(Page *page);

  bool push_page(char *page_data);
  char *pop_page();

  std::list<char *> m_pages_list;
  int32_t m_pages_max;
  int32_t m_pages_cache_max;
  int32_t m_pages_cached;
  const int32_t m_page_size;
  Mutex m_mutex;
  std::atomic<int32_t> m_pages_allocated;
};

template <typename ResType>
Resource<ResType>::Resource() : m_res(NULL) {}

template <typename ResType>
Resource<ResType>::Resource(ResType *res) : m_res(res) {
  m_res->aquire();
}

template <typename ResType>
Resource<ResType>::Resource(const Resource<ResType> &resource)
    : m_res(resource.m_res) {
  if (NULL != m_res) m_res->aquire();
}

template <typename ResType>
Resource<ResType>::~Resource() {
  if (NULL != m_res) m_res->release();
}

template <typename ResType>
ResType *Resource<ResType>::operator->() {
  return m_res;
}

template <typename ResType>
ResType *Resource<ResType>::operator->() const {
  return m_res;
}
}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_PROTOCOL_PAGE_POOL_H_
