/***********************************************************************

Copyright (c) 2012, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

***********************************************************************/
/**************************************************//**
@file innodb_list.h

Created 03/15/2011      Jimmy Yang
*******************************************************/

#ifndef INNODB_LIST_H
#define INNODB_LIST_H

#include "config.h"

#define UT_LIST_NODE_T(TYPE)						\
struct {								\
        TYPE *  prev;   /*!< pointer to the previous node,		\
                        NULL if start of list */			\
        TYPE *  next;   /*!< pointer to next node, NULL if end of list */\
}									\

#define UT_LIST_BASE_NODE_T(TYPE)					\
struct {								\
        int   count;  /*!< count of nodes in list */			\
        TYPE *  start;  /*!< pointer to list start, NULL if empty */	\
        TYPE *  end;    /*!< pointer to list end, NULL if empty */	\
}

/** Some Macros to manipulate the list, extracted from "ut0lst.h" */
#define UT_LIST_INIT(BASE)						\
{									\
        (BASE).count = 0;						\
        (BASE).start = NULL;						\
        (BASE).end   = NULL;						\
}									\

#define UT_LIST_ADD_LAST(NAME, BASE, N)					\
{									\
        ((BASE).count)++;						\
        ((N)->NAME).prev = (BASE).end;					\
        ((N)->NAME).next = NULL;					\
        if ((BASE).end != NULL) {					\
                (((BASE).end)->NAME).next = (N);			\
        }								\
        (BASE).end = (N);						\
        if ((BASE).start == NULL) {					\
                (BASE).start = (N);					\
        }								\
}									\

#define UT_LIST_ADD_FIRST(NAME, BASE, N)				\
{									\
        ((BASE).count)++;						\
        ((N)->NAME).next = (BASE).start;				\
        ((N)->NAME).prev = NULL;					\
        if (UNIV_LIKELY((BASE).start != NULL)) {			\
                (((BASE).start)->NAME).prev = (N);			\
        }								\
        (BASE).start = (N);						\
        if (UNIV_UNLIKELY((BASE).end == NULL)) {			\
                (BASE).end = (N);					\
        }								\
}									\

# define UT_LIST_REMOVE_CLEAR(NAME, N)					\
((N)->NAME.prev = (N)->NAME.next = (void*) -1)

/** Removes a node from a linked list. */
#define UT_LIST_REMOVE(NAME, BASE, N)                                   \
do {                                                                    \
        ((BASE).count)--;                                               \
        if (((N)->NAME).next != NULL) {                                 \
                ((((N)->NAME).next)->NAME).prev = ((N)->NAME).prev;     \
        } else {                                                        \
                (BASE).end = ((N)->NAME).prev;                          \
        }                                                               \
        if (((N)->NAME).prev != NULL) {                                 \
                ((((N)->NAME).prev)->NAME).next = ((N)->NAME).next;     \
        } else {                                                        \
                (BASE).start = ((N)->NAME).next;                        \
        }                                                               \
        UT_LIST_REMOVE_CLEAR(NAME, N);                                  \
} while (0)

#define UT_LIST_GET_NEXT(NAME, N)					\
        (((N)->NAME).next)

#define UT_LIST_GET_LEN(BASE)						\
        (BASE).count

#define UT_LIST_GET_FIRST(BASE)						\
        (BASE).start

#endif /* INNODB_LIST_H */
