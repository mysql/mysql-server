/* Copyright (c) 2012, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <assert.h>
#include <stdio.h>
#ifdef _MSC_VER
#include <stdint.h>
#endif

#include "xcom/simset.h"
#include "xcom/task_debug.h"
#include "xcom/x_platform.h"
#include "xcom/xcom_profile.h"

char *dbg_linkage(linkage *self) {
  GET_NEW_GOUT;
  PTREXP(self);
  NDBG(self->type, u);
  NDBG(cardinal(self), d);
  PTREXP(self->suc);
  PTREXP(self->pred);
  FWD_ITER(self, linkage, STRLIT("->"); PTREXP(link_iter);
           PTREXP(link_iter->suc); PTREXP(link_iter->pred));
  RET_GOUT;
}
