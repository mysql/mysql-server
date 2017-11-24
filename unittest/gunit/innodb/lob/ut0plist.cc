/*****************************************************************************

Copyright (c) 2016, 2017 Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

#include "lot0plist.h"
#include "lot0buf.h"

void basic_0()
{
  buf_block_t* block = btr_page_alloc();
  byte* frame = buf_block_get_frame(block);

  plist_base_node_t base(frame, frame);
  base.init();

  byte* ptr = frame + plist_base_node_t::SIZE;

  for (ulint i = 0; i < 1; ++i) {
    plist_node_t node(frame, ptr);
    base.push_back(node);
    ptr += plist_node_t::SIZE;
  }

  base.print_list(std::cout);
}

void test_00()
{
  buf_block_t* block = btr_page_alloc();
  byte* frame = buf_block_get_frame(block);

  plist_base_node_t base(frame, frame);
  base.init();

  byte* ptr = frame + plist_base_node_t::SIZE;

  for (ulint i = 0; i < 5; ++i) {
    plist_node_t node(frame, ptr);
    base.push_back(node);
    ptr += plist_node_t::SIZE;
  }

  base.print_list(std::cout);

  plist_node_t first = base.get_first_node();
  plist_node_t cur = first.get_next_node();
  cur = cur.get_next_node();
  base.remove(cur);
  base.insert_before(first, cur);

  std::cout << "-----" << std::endl;
  base.print_list(std::cout);
}

void test_01()
{
  buf_block_t* block = btr_page_alloc();
  byte* frame = buf_block_get_frame(block);

  plist_base_node_t base(frame, frame);
  base.init();

  byte* ptr = frame + plist_base_node_t::SIZE;

  for (ulint i = 0; i < 5; ++i) {
    plist_node_t node(frame, ptr);
    base.push_back(node);
    ptr += plist_node_t::SIZE;
  }

  base.print_list(std::cout);

  plist_node_t first = base.get_first_node();
  plist_node_t last = base.get_last_node();
  plist_node_t cur = first.get_next_node();
  cur = cur.get_next_node();
  base.remove(cur);
  base.insert_before(last, cur);

  std::cout << "-----" << std::endl;
  base.print_list(std::cout);
}

int main()
{
  basic_0();
  test_00();
  test_01();
}

