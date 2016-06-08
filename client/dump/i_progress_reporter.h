/*
  Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef I_PROGRESS_REPORTER_INCLUDED
#define I_PROGRESS_REPORTER_INCLUDED

namespace Mysql{
namespace Tools{
namespace Dump{

class I_progress_watcher;

class I_progress_reporter
{
public:
  virtual ~I_progress_reporter();

  /**
    Add new Progress Watcher to report to.
   */
  virtual void register_progress_watcher(
    I_progress_watcher* new_progress_watcher)= 0;
};

}
}
}

#endif
