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


#ifndef NGS_FILE_INTERFACE_H_
#define NGS_FILE_INTERFACE_H_

#include "ngs/memory.h"


namespace ngs
{

class File_interface
{
public:
  typedef ngs::shared_ptr<File_interface> Shared_ptr;

  virtual ~File_interface() {}

  virtual bool is_valid() = 0;
  virtual int close() = 0;
  virtual int read(void *buffer, int nbyte) = 0;
  virtual int write(void *buffer, int nbyte) = 0;
  virtual int fsync() = 0;
};

} // namespace ngs

#endif // NGS_FILE_INTERFACE_H_
