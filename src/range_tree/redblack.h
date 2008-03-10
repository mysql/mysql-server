/*
 * RCS $Id: redblack.h,v 1.9 2003/10/24 01:31:21 damo Exp $
 */

/*
   Redblack balanced tree algorithm
   Copyright (C) Damian Ivereigh 2000

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version. See the file COPYING for details.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Header file for redblack.c, should be included by any code that 
** uses redblack.c since it defines the functions 
*/ 
 
/* Stop multiple includes */
#ifndef _REDBLACK_H

#ifndef RB_CUSTOMIZE
/*
 * Without customization, the data member in the tree nodes is a void
 * pointer, and you need to pass in a comparison function to be
 * applied at runtime.  With customization, you specify the data type
 * as the macro RB_ENTRY(data_t) (has to be a macro because compilers
 * gag on typdef void) and the name of the compare function as the
 * value of the macro RB_COMPARE. Because the comparison function is
 * compiled in, RB_COMPARE only needs to take two arguments.  If your
 * content type is not a pointer, define INLINE to get direct access.
 */
#define rbdata_t	void
#define RB_COMPARE(s, t, e)	(*rbinfo->rb_cmp)(s, t, e)
#undef RB_INLINE
#define RB_ENTRY(name)	rb##name
#else
//Not Customized
#define RB_COMPARE(s, t)	(*rbinfo->rb_cmp)(s, t)
#endif /* RB_CUSTOMIZE */

#ifndef RB_STATIC
#define RB_STATIC
#endif


/* Modes for rblookup */
#define RB_NONE -1	    /* None of those below */
#define RB_LUEQUAL 0	/* Only exact match */
#define RB_LUGTEQ 1		/* Exact match or greater */
#define RB_LULTEQ 2		/* Exact match or less */
#define RB_LULESS 3		/* Less than key (not equal to) */
#define RB_LUGREAT 4	/* Greater than key (not equal to) */
#define RB_LUNEXT 5		/* Next key after current */
#define RB_LUPREV 6		/* Prev key before current */
#define RB_LUFIRST 7	/* First key in index */
#define RB_LULAST 8		/* Last key in index */

/* For rbwalk - pinched from search.h */
typedef enum
{
  preorder,
  postorder,
  endorder,
  leaf
}
VISIT;

struct RB_ENTRY(lists) { 
const struct RB_ENTRY(node) *rootp; 
const struct RB_ENTRY(node) *nextp; 
}; 
 
#define RBLIST struct RB_ENTRY(lists) 

struct RB_ENTRY(tree) {
#ifndef RB_CUSTOMIZE
		/* comparison routine */
    int (*rb_cmp)(const void *, const void *, const void *);
		/* config data to be passed to rb_cmp */
    const void *rb_config;
		/* root of tree */
#else
    int (*rb_cmp)(const RB_ENTRY(data_t)*, const RB_ENTRY(data_t)*);
#endif /* RB_CUSTOMIZE */
struct RB_ENTRY(node) *rb_root;
};

RB_STATIC int RB_ENTRY(init) (
#ifndef RB_CUSTOMIZE
    int (*cmp)(const void *, const void *, const void *), const void *config,
#else
    int (*cmp)(const RB_ENTRY(data_t)*, const RB_ENTRY(data_t)*),
#endif /* RB_CUSTOMIZE */
    struct RB_ENTRY(tree)** ptree
);

#ifndef no_delete
RB_STATIC const RB_ENTRY(data_t) *RB_ENTRY(delete)(const RB_ENTRY(data_t) *, struct RB_ENTRY(tree) *);
#endif

#ifndef no_find
RB_STATIC const RB_ENTRY(data_t) *RB_ENTRY(find)(const RB_ENTRY(data_t) *, struct RB_ENTRY(tree) *);
#endif

#ifndef no_lookup
RB_STATIC int RB_ENTRY(lookup)(
    int mode,
    const  RB_ENTRY(data_t)*  key,
    struct RB_ENTRY(tree)*    rbinfo,
    struct RB_ENTRY(node)**   pinsert_finger,
    struct RB_ENTRY(node)**   pelement_finger,
    const  RB_ENTRY(data_t)** pdata
);
#endif

#ifndef no_search
RB_STATIC const RB_ENTRY(data_t) *RB_ENTRY(search)(const RB_ENTRY(data_t) *, struct RB_ENTRY(tree) *);
#endif

#ifndef no_finger_insert
RB_STATIC const RB_ENTRY(data_t)* RB_ENTRY(finger_insert)(
    const  RB_ENTRY(data_t)* key,
    struct RB_ENTRY(tree)*   rbinfo,
    struct RB_ENTRY(node)*   parent
);
#endif

#ifndef no_finger_delete
RB_STATIC int RB_ENTRY(finger_delete)(struct RB_ENTRY(node)* node, struct RB_ENTRY(tree) *rbinfo);
#endif

#ifndef no_pred
RB_STATIC int RB_ENTRY(finger_predecessor)(const struct RB_ENTRY(node)** pfinger, const RB_ENTRY(data_t)** ppred_data);
#endif

#ifndef no_succ
RB_STATIC int RB_ENTRY(finger_successor)(const struct RB_ENTRY(node)** pfinger, const RB_ENTRY(data_t)** psucc_data);
#endif

#ifndef no_destroy
RB_STATIC void RB_ENTRY(destroy)(struct RB_ENTRY(tree) *);
#endif

#ifndef no_walk
RB_STATIC void RB_ENTRY(walk)(const struct RB_ENTRY(tree) *,
		void (*)(const RB_ENTRY(data_t) *, const VISIT, const int, void *),
		void *); 
#endif

#ifndef no_readlist
RB_STATIC RBLIST *RB_ENTRY(openlist)(const struct RB_ENTRY(tree) *); 
RB_STATIC const RB_ENTRY(data_t) *RB_ENTRY(readlist)(RBLIST *); 
RB_STATIC void RB_ENTRY(closelist)(RBLIST *); 
#endif

/* Some useful macros */
#define rbmin(rbinfo) RB_ENTRY(lookup)(RB_LUFIRST, NULL, (rbinfo))
#define rbmax(rbinfo) RB_ENTRY(lookup)(RB_LULAST, NULL, (rbinfo))

#define _REDBLACK_H
#endif /* _REDBLACK_H */

/*
 *
 * $Log: redblack.h,v $
 * Revision 1.9  2003/10/24 01:31:21  damo
 * Patches from Eric Raymond: %prefix is implemented.  Various other small
 * changes avoid stepping on global namespaces and improve the documentation.
 *
 * Revision 1.8  2003/10/23 04:18:47  damo
 * Fixed up the rbgen stuff ready for the 1.3 release
 *
 * Revision 1.7  2002/08/26 03:11:40  damo
 * Fixed up a bunch of compiler warnings when compiling example4
 *
 * Tidies up the Makefile.am & Specfile.
 *
 * Renamed redblack to rbgen
 *
 * Revision 1.6  2002/08/26 01:03:35  damo
 * Patch from Eric Raymond to change the way the library is used:-
 *
 * Eric's idea is to convert libredblack into a piece of in-line code
 * generated by another program. This should be faster, smaller and easier
 * to use.
 *
 * This is the first check-in of his code before I start futzing with it!
 *
 * Revision 1.5  2002/01/30 07:54:53  damo
 * Fixed up the libtool versioning stuff (finally)
 * Fixed bug 500600 (not detecting a NULL return from malloc)
 * Fixed bug 509485 (no longer needs search.h)
 * Cleaned up debugging section
 * Allow multiple inclusions of redblack.h
 * Thanks to Matthias Andree for reporting (and fixing) these
 *
 * Revision 1.4  2000/06/06 14:43:43  damo
 * Added all the rbwalk & rbopenlist stuff. Fixed up malloc instead of sbrk.
 * Added two new examples
 *
 * Revision 1.3  2000/05/24 06:45:27  damo
 * Converted everything over to using const
 * Added a new example1.c file to demonstrate the worst case scenario
 * Minor fixups of the spec file
 *
 * Revision 1.2  2000/05/24 06:17:10  damo
 * Fixed up the License (now the LGPL)
 *
 * Revision 1.1  2000/05/24 04:15:53  damo
 * Initial import of files. Versions are now all over the place. Oh well
 *
 */

