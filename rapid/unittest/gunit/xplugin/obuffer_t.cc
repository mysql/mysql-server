
/* Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */


#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <iostream>

#include <gtest/gtest.h>

#include "ngs/protocol/output_buffer.h"

using namespace ngs;

namespace im
{
  const ngs::Pool_config default_pool_config = { 0, 0, BUFFER_PAGE_SIZE };

  namespace tests {
    std::vector<ngs::shared_ptr<Page> > page_del;

    static void add_pages(Output_buffer &ob, const size_t no_of_pages, const size_t page_size)
    {
      for (size_t i = 0; i < no_of_pages; i++)
      {
        page_del.push_back(ngs::shared_ptr<Page>(new Page(static_cast<uint32_t>(page_size))));
        ob.push_back(page_del.back().get());
      }
    }

    TEST(OBuffer, Next)
    {
      Page_pool page_pool(default_pool_config);
      Output_buffer obuffer(page_pool);

      EXPECT_EQ(0u, obuffer.length());

      void *ptr;
      int size;

      EXPECT_TRUE(obuffer.Next(&ptr, &size));
      EXPECT_EQ(BUFFER_PAGE_SIZE, size);

      EXPECT_EQ(BUFFER_PAGE_SIZE, obuffer.ByteCount());
      EXPECT_EQ(0u + BUFFER_PAGE_SIZE, obuffer.length());
    }

    TEST(OBuffer, obuffer)
    {
      Page_pool page_pool(default_pool_config);
      Output_buffer obuffer(page_pool);

      EXPECT_EQ(0u, obuffer.length());

      EXPECT_TRUE(obuffer.add_int32(0x12345678));

      int32_t r;
      EXPECT_TRUE(obuffer.int32_at(0, r));
      EXPECT_EQ(0x12345678, r);

      EXPECT_TRUE(obuffer.add_int8(0x42));
      EXPECT_TRUE(obuffer.add_bytes("hello", 6));

      EXPECT_EQ(11u, obuffer.length());
    }

    TEST(OBuffer, split_int_write)
    {
      Page_pool page_pool(default_pool_config);
      Output_buffer obuffer(page_pool);

      add_pages(obuffer, 1, 2);
      add_pages(obuffer, 1, 4);

      EXPECT_EQ(0u, obuffer.length());
      EXPECT_EQ(6u, obuffer.available_space());

      EXPECT_TRUE(obuffer.add_int32(0x12345678));

      EXPECT_EQ(4u, obuffer.length());

      int32_t r;
      EXPECT_TRUE(obuffer.int32_at(0, r));
      EXPECT_EQ(0x12345678, r);
    }


    TEST(OBuffer, split_str_write)
    {
      Page_pool page_pool(default_pool_config);
      Output_buffer obuffer(page_pool);
      const size_t page_size = 8;
      const size_t no_of_pages = 2;

      add_pages(obuffer,no_of_pages, page_size);

      EXPECT_EQ(0u, obuffer.length());
      EXPECT_EQ(16u, obuffer.available_space());

      size_t len = strlen("helloworld");
      EXPECT_TRUE(obuffer.add_bytes("helloworld", len));

      EXPECT_EQ(len, obuffer.length());

      Buffer::Page_list &p(obuffer.pages());
      Buffer::Buffer_page &p1 = *(p.begin());
      EXPECT_EQ("hellowor", std::string(p1->data, p1->length));
      Buffer::Buffer_page &p2 = *(++p.begin());
      EXPECT_EQ("ld", std::string(p2->data, p2->length));
    }


    TEST(OBuffer, write_big_buffer)
    {
      // write 300k simulating protobuf and ensure everything got there
      char data[300001];

      data[0] = '>';
      for (size_t i = 1; i < sizeof(data)-1; i++)
        data[i] = '.';
      data[300000-1] = '<';
      data[300000] = 0;

      Page_pool page_pool(default_pool_config);
      Output_buffer obuffer(page_pool);

      int written = 0;
      char *ptr;
      int size;
      while (written < 300000 && obuffer.Next((void**)&ptr, &size))
      {
        if (written + size <= 300000)
        {
          memcpy(ptr, data + written, size);
          written += size;
        }
        else
        {
          obuffer.BackUp(size - (300000 - written));
          memcpy(ptr, data + written, (300000 - written));
          written += (300000 - written);
        }
      }

      EXPECT_EQ(300000u, obuffer.length());

      int total = 0;
      Const_buffer_sequence buffers(obuffer.get_buffers());
      for (Const_buffer_sequence::const_iterator buf = buffers.begin(); buf != buffers.end(); ++buf)
      {
        size_t size = (*buf).second;
        const char* d = (*buf).first;
        int first = 0;

        if (total == 0)
        {
          EXPECT_EQ('>', d[0]);
          first++;
        }
        total += static_cast<int>(size);
        if (total == 300000)
        {
          EXPECT_EQ('<', d[size-1]);
          size--;
        }

        for (size_t i = first; i < size; i++)
          EXPECT_EQ('.', d[i]);
      }
      EXPECT_EQ(300000, total);
    }

    TEST(OBuffer, save_rollback)
    {
      Page_pool page_pool(default_pool_config);
      Output_buffer obuffer(page_pool);
      const size_t size_of_page = 8;

      add_pages(obuffer,2, size_of_page);

      EXPECT_EQ(0u, obuffer.length());
      EXPECT_EQ(16u, obuffer.available_space());

      obuffer.save_state();

      size_t len = strlen("helloworld");
      EXPECT_TRUE(obuffer.add_bytes("helloworld", len));

      EXPECT_EQ(len, obuffer.length());

      obuffer.rollback();

      EXPECT_EQ(0u, obuffer.length());
      EXPECT_EQ(16u, obuffer.available_space());
      Buffer::Page_list::iterator it = obuffer.pages().begin();
      for (; it != obuffer.pages().end(); ++it)
      {
        EXPECT_EQ(0u, (*it)->length);
      }
    }
  }
}

