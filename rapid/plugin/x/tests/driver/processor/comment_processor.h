/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef X_TESTS_DRIVER_PROCESSOR_COMMENT_PROCESSOR_H_
#define X_TESTS_DRIVER_PROCESSOR_COMMENT_PROCESSOR_H_

#include "processor/block_processor.h"


class Comment_processor : public Block_processor {
 public:
  Result feed(std::istream &input, const char *linebuf) override {
    while(*linebuf) {
      if (*linebuf != ' ' &&
          *linebuf != '\t')
        break;

      ++linebuf;
    }

    if (linebuf[0] == '#' || linebuf[0] == 0) {  // # comment
      return Result::Eaten_but_not_hungry;
    }

    return Result::Not_hungry;
  }
};

#endif  // X_TESTS_DRIVER_PROCESSOR_COMMENT_PROCESSOR_H_
