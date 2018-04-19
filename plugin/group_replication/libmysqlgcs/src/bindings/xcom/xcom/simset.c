/* Copyright (c) 2012, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/simset.h"

#include <assert.h>
#include <stdio.h>

#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task_debug.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/x_platform.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/xcom_profile.h"

/* purecov: begin deadcode */
linkage *link_first(linkage *self) { return self->suc; }

linkage *link_last(linkage *self) { return self->pred; }
/* purecov: end */
#if 0
linkage *link_extract_first(linkage *self)
{
	return link_out(self->suc);
}
#endif

linkage *link_extract_last(linkage *self) { return link_out(self->pred); }

#if 0
int	link_empty(linkage *self)
{
	return self == self->suc;
}
#endif

linkage *link_init(linkage *self, unsigned int type) {
  /* XDBG("%s ",__func__); */
  self->type = type;
  self->suc = self->pred = self;
  LINK_SANITY_CHECK(self);
  return self;
}

linkage *link_out(linkage *self) {
  /* XDBG("%s ",__func__); */
  if (!link_empty(self)) {
    TYPE_SANITY_CHECK(self, self->suc);
    TYPE_SANITY_CHECK(self, self->pred);
    self->suc->pred = self->pred;
    self->pred->suc = self->suc;
    self->suc = self->pred = self;
  }
  LINK_SANITY_CHECK(self);
  return self;
}

void link_follow(linkage *self, linkage *ptr) {
  /* XDBG("%s ",__func__); */
  link_out(self);
  if (ptr) {
    TYPE_SANITY_CHECK(self, ptr);
    LINK_SANITY_CHECK(ptr);
    self->pred = ptr;
    self->suc = ptr->suc;
    self->suc->pred = ptr->suc = self;
    LINK_SANITY_CHECK(self);
  }
}

void link_precede(linkage *self, linkage *ptr) {
  /* XDBG("%s ",__func__); */
  link_out(self);
  if (ptr) {
    TYPE_SANITY_CHECK(self, ptr);
    LINK_SANITY_CHECK(ptr);
    self->suc = ptr;
    self->pred = ptr->pred;
    self->pred->suc = ptr->pred = self;
    LINK_SANITY_CHECK(self);
  }
}

#if 0
void link_into(linkage *self, linkage *s)
{
	/* XDBG("%s ",__func__); */
	link_precede(self, s);
}
#endif
/* purecov: begin deadcode */
int cardinal(linkage *self) {
  int n = 0;
  FWD_ITER(self, linkage, n++);
  return n;
}

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
/* purecov: end */
#if 0
#define FNVSTART 0x811c9dc5

/* Fowler-Noll-Vo type multiplicative hash */
unsigned int	type_hash(const char *byte)
{
	uint32_t sum = 0;
	while (*byte) {
		sum = sum * (uint32_t)0x01000193 ^ (uint32_t)(*byte);
		byte++;
	}
	return (unsigned int) sum;
}

#else
unsigned int type_hash(const char *byte MY_ATTRIBUTE((unused))) { return 0; }

/* unsigned int	type_hash(const char *byte)
{
        unsigned int	sum = 0;
        while (*byte) {
                sum = sum * 7 + *byte++;
        }
        return sum;
} */
#endif
