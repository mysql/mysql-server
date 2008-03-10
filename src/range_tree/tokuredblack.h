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

#ifndef TOKU_REDBLACK_H
#define TOKU_REDBLACK_H

#include <rangetree.h>
#define toku_range toku_range
#define RB_INLINE

/* Modes for rblookup */
typedef enum {
   RB_NONE    = -1,    /* None of those below */
   RB_LUEQUAL =  0,    /* Only exact match */
   RB_LUGTEQ  =  1,    /* Exact match or greater */
   RB_LULTEQ  =  2,    /* Exact match or less */
   RB_LULESS  =  3,    /* Less than key (not equal to) */
   RB_LUGREAT =  4,    /* Greater than key (not equal to) */
   RB_LUNEXT  =  5,    /* Next key after current */
   RB_LUPREV  =  6,    /* Prev key before current */
   RB_LUFIRST =  7,    /* First key in index */
   RB_LULAST  =  8    /* Last key in index */
} toku_rbt_look_mode;

struct toku_rbt_lists { 
const struct toku_rbt_node *rootp; 
const struct toku_rbt_node *nextp; 
}; 
 
struct toku_rbt_tree {
    int (*rb_cmp)(const toku_range*, const toku_range*);
    struct toku_rbt_node *rb_root;
    void* (*rb_malloc) (size_t);
    void  (*rb_free)   (void*);
    void* (*rb_realloc)(void*, size_t);
};

int toku_rbt_init (
    int (*cmp)(const toku_range*, const toku_range*),
    struct toku_rbt_tree** ptree
);

int toku_rbt_lookup(
    int mode,
    const  toku_range*  key,
    struct toku_rbt_tree*    rbinfo,
    struct toku_rbt_node**   pinsert_finger,
    struct toku_rbt_node**   pelement_finger,
    const  toku_range** pdata
);

const toku_range* toku_rbt_finger_insert(
    const  toku_range* key,
    struct toku_rbt_tree*   rbinfo,
    struct toku_rbt_node*   parent
);

int toku_rbt_finger_delete(struct toku_rbt_node* node, struct toku_rbt_tree *rbinfo);

int toku_rbt_finger_predecessor(const struct toku_rbt_node** pfinger, const toku_range** ppred_data);

int toku_rbt_finger_successor(const struct toku_rbt_node** pfinger, const toku_range** psucc_data);

void toku_rbt_destroy(struct toku_rbt_tree *);

enum nodecolour { BLACK, RED };

struct toku_rbt_node
{
    struct toku_rbt_node *left;        /* Left down */
    struct toku_rbt_node *right;        /* Right down */
    struct toku_rbt_node *up;        /* Up */
    enum nodecolour colour;        /* Node colour */
#ifdef RB_INLINE
    toku_range key;        /* User's key (and data) */
#define RB_GET(x,y)     &x->y
#define RB_SET(x,y,v)   x->y = *(v)
#else
    const toku_range *key;    /* Pointer to user's key (and data) */
#define RB_GET(x,y)     x->y
#define RB_SET(x,y,v)   x->y = v
#endif /* RB_INLINE */
};

#endif /* TOKU_REDBLACK_H */
