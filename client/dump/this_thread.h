/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef THIS_THREAD_INCLUDED
#define THIS_THREAD_INCLUDED

#include "my_global.h"
#include "my_thread.h"
#include "my_sys.h"

namespace my_boost{

class this_thread
{
public:
  template<typename TPeriod> static void sleep(TPeriod time)
  {
    my_sleep(time.total_microseconds());
    my_thread_yield();
  }
};

}

#endif
