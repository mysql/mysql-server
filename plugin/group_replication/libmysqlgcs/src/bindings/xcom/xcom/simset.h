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

#ifndef SIMSET_H
#define SIMSET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#ifndef XCOM_STANDALONE
#include "my_compiler.h"
#endif
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/x_platform.h"

struct linkage;
typedef struct linkage linkage;

struct linkage {
  unsigned int type; /* Not strictly necessary, used for rudimentary run time
                        type check */
  linkage *suc;      /* Next in list */
  linkage *pred;     /* Previous in list */
};

/* extern int link_empty(linkage *self) ;  */ /* True if list is empty */
extern linkage *link_first(linkage *self);    /* First in list */
extern linkage *link_last(linkage *self);     /* Last in list */
/*extern linkage *link_extract_first(linkage *self);  */ /* Extract first from
                                                            list */
extern linkage *link_extract_last(linkage *self); /* Extract last from list */
extern linkage *link_init(linkage *self,
                          unsigned int type); /* Initialize a link */
extern linkage *link_out(linkage *self);      /* Remove link from list */
extern void link_follow(linkage *self,
                        linkage *ptr); /* Insert self in list after ptr */
extern void link_precede(linkage *self,
                         linkage *ptr); /* Insert self in list before ptr */
/* extern void link_into(linkage *self, linkage *s); */ /* Insert self in list
                                                           before ptr */
extern char *dbg_linkage(linkage *self);                /* Debug link */
extern unsigned int type_hash(
    const char *byte); /* Hash character string to unsigned */
extern int cardinal(linkage *self);

/* Forward iterator */
#define FWD_ITER(head, type, action)                      \
  {                                                       \
    linkage *p = link_first(head);                        \
    while (p != (head)) {                                 \
      linkage *_next = link_first(p);                     \
      {                                                   \
        type *link_iter = (type *)p;                      \
        (void)link_iter;                                  \
        action;                                           \
      } /* Cast to void avoids unused variable warning */ \
      p = _next;                                          \
    }                                                     \
  }

/* Reverse iterator */
#define REV_ITER(head, type, action)                      \
  {                                                       \
    linkage *p = link_last(head);                         \
    while (p != (head)) {                                 \
      linkage *_next = link_last(p);                      \
      {                                                   \
        type *link_iter = (type *)p;                      \
        (void)link_iter;                                  \
        action;                                           \
      } /* Cast to void avoids unused variable warning */ \
      p = _next;                                          \
    }                                                     \
  }

/* Get containing struct from pointer to member and type */
#define container_of(ptr, type, member) \
  ((type *)(((char *)(ptr)) - offsetof(type, member)))

#define NULL_TYPE 0xdefaced

#if 0
#define LINK_SANITY_CHECK(x) \
  {                          \
    assert((x)->suc);        \
    assert((x)->pred);       \
  }
#define TYPE_SANITY_CHECK(x, y) \
  { assert((x)->type == (y)->type); }
#else
#define LINK_SANITY_CHECK(x)
#define TYPE_SANITY_CHECK(x, y)
#endif

#define link_into(self, s) link_precede(self, s)
#define link_extract_first(self) link_out((self)->suc)
#define link_empty(self) ((self) == (self)->suc)

#if 0
static inline linkage *link_out(linkage *self)
{
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

#endif

#ifdef __cplusplus
}
#endif

#endif
