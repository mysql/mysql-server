/* Copyright (C) 2008-2009 Sun Microsystems, Inc

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <my_global.h>
#include <my_sys.h>
#include <pfs_global.h>

bool pfs_initialized= false;

bool stub_alloc_always_fails= true;
int stub_alloc_fails_after_count= 0;

void *pfs_malloc(size_t, myf)
{
  static char garbage[100];

  if (stub_alloc_always_fails)
    return NULL;

  if (--stub_alloc_fails_after_count <= 0)
    return NULL;

  return garbage;
}

void pfs_free(void *)
{
}

