/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "my_config.h"

#include "ngs/protocol/buffer.h"
#include "ngs/log.h"

#include <new>
#include <utility>


using namespace ngs;

Buffer::Buffer(Page_pool& page_pool)
  : m_capacity(0), m_length(0), m_page_pool(page_pool)
{
}

Buffer::~Buffer()
{
}

/*
NOTE: Commented for coverage. Uncomment when needed.

size_t Buffer::capacity() const
{
  return m_capacity;
}
*/


size_t Buffer::length() const
{
  return m_length;
}


size_t Buffer::available_space() const
{
  return m_capacity - m_length;
}


Alloc_result Buffer::reserve(size_t space)
{
  size_t available = available_space();

  while (available < space)
  {
    try
    {
      Buffer_page p = m_page_pool.allocate();
      available += p->capacity;
      m_capacity += p->capacity;

      m_pages.push_back(p);
    }
    catch (const std::bad_alloc &exc)
    {
      log_error("Error allocating Buffer_page: %s", exc.what());
      return Memory_error;
    }
    catch (const Page_pool::No_more_pages_exception &)
    {
      return Memory_no_free_pages;
    }
  }
  return Memory_allocated;
}


Alloc_result Buffer::add_pages(unsigned int npages)
{
  for (unsigned int i = 0; i < npages; i++)
  {
    try
    {
      Buffer_page p = m_page_pool.allocate();
      m_capacity += p->capacity;

      m_pages.push_back(p);
    }
    catch (std::bad_alloc &exc)
    {
      log_error("Error allocating Buffer_page: %s", exc.what());
      return Memory_error;
    }
    catch (const Page_pool::No_more_pages_exception &)
    {
      return Memory_no_free_pages;
    }
  }

  return Memory_allocated;
}

/*
NOTE: Commented for coverage. Uncomment when needed.

bool Buffer::uint32_at(size_t offset, uint32_t &ret)
{
  return int32_at(offset, *reinterpret_cast<int32_t*>(&ret));
}
*/


bool Buffer::int32_at(size_t offset, int32_t &ret_int)
{
  char tmp[4];
  size_t offs = 0;

  for (Page_list::const_iterator p = m_pages.begin();
       p != m_pages.end(); ++p)
  {
    if (offs + (*p)->length < offset)
    {
      offs += (*p)->length;
    }
    else
    {
      if ((*p)->length - (offset - offs) >= 4)
      {
        // full word is in a single page
        memcpy(tmp, (*p)->data + (offset-offs), 4);
      }
      else
      {
        const char *data = (*p)->data + offset-offs;
        for (int o = 0; o < 4; o++)
        {
          tmp[o] = *data++;

          if ((*p)->length - (data - (*p)->data) <= 0)
          {
            ++p;
            if (p == m_pages.end())
            {
              if (o < 3)
                return false;
              break;
            }
            data = (*p)->data;
          }
        }
      }

#ifdef WORDS_BIGENDIAN
      std::swap(tmp[0], tmp[3]);
      std::swap(tmp[1], tmp[2]);
#endif
      const uint32_t* ret_ptr = (uint32_t*)(tmp);
      ret_int = *ret_ptr;

      return true;
    }
  }

  return false;
}


/*
NOTE: Commented for coverage. Uncomment when needed.

bool Buffer::int8_at(size_t offset, int8_t &ret_int)
{
  for (Page_list::const_iterator i = m_pages.begin();
       i != m_pages.end(); ++i)
  {
    if (offset < (*i)->length)
    {
      ret_int = *(int8_t*)((*i)->data + offset);
      return true;
    }
    else
      offset -= (*i)->length;
  }
  return false;
}


Resource<Page> Buffer::pop_front()
{
  Buffer_page result = m_pages.front();

  m_length -= result->length;
  m_capacity -= result->capacity;

  m_pages.pop_front();

  return result;
}
*/


void Buffer::push_back(const Resource<Page> &page)
{
  m_length += page->length;
  m_capacity += page->capacity;

  m_pages.push_back(page);
}


/*
NOTE: Commented for coverage. Uncomment when needed.

void Buffer::add_bytes_transferred(size_t nbytes)
{
  m_length += nbytes;

  // update the length field of all pages to reflect the number of bytes that got transferred to them
  Page_list::iterator p = pages().begin();
  while (p != pages().end())
  {
    if ((*p)->length < (*p)->capacity && nbytes > 0)
    {
      size_t bytes_filled = std::min(nbytes, (size_t)((*p)->capacity - (*p)->length));

      nbytes -= bytes_filled;
      (*p)->length += bytes_filled;
      ++p;
      break;
    }
    ++p;
  }

  for (; p != pages().end() && nbytes > 0; ++p)
  {
    // invariant: all pages after the 1st page with available space will be empty
    DBUG_ASSERT((*p)->length == 0);
    if (nbytes >= (*p)->capacity)
    {
      nbytes -= (*p)->capacity;
      (*p)->length = (*p)->capacity;
    }
    else
    {
      (*p)->length = nbytes;
      nbytes = 0;
    }
  }
}
*/


void Buffer::reset()
{
  for (Page_list::const_iterator p = pages().begin(); p != pages().end(); ++p)
    (*p)->length = 0;
}
