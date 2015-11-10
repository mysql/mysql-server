/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef DD_VIEW_INCLUDED
#define DD_VIEW_INCLUDED

#include "my_global.h"

class THD;
struct TABLE_LIST;
typedef struct st_mem_root MEM_ROOT;

namespace dd {
class View;

/** Store view metadata into dd.views */
bool create_view(THD *thd, TABLE_LIST *view,
                 const char *schema_name, const char *view_name);

/** Read view metadata from dd.views into TABLE_LIST */
void read_view(TABLE_LIST *view, const dd::View &view_ref,
               MEM_ROOT *mem_root);

} // namespace dd
#endif // DD_VIEW_INCLUDED
