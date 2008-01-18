/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/*
 * Range trees
 *
 * This is the header file for range trees. */

//Currently this is a stub implementation just so we can write and compile tests
//before actually implementing the range tree.
#include "rangetree.h"

#define AA __attribute__((__unused__))
int toku_rt_create(toku_range_tree** a AA, 
                   int (*b)(void*,void*) AA, int (*c)(void*,void*) AA, 
		           BOOL d AA)
{return 0;}

int toku_rt_close(toku_range_tree*a AA)
{return 0;}

int toku_rt_find(toku_range_tree*b AA, toku_range*c AA, unsigned d AA, toku_range**e AA, unsigned*g AA, unsigned*f AA)
{return 0;}

int toku_rt_insert(toku_range_tree*a AA, toku_range*v AA)
{return 0;}
int toku_rt_delete(toku_range_tree*c AA, toku_range*q AA)
{return 0;}

int toku_rt_predecessor (toku_range_tree*f AA, void*g AA, toku_range*q AA, BOOL* v AA)
{return 0;}
int toku_rt_successor   (toku_range_tree*e AA, void*c AA, toku_range*a AA, BOOL* q AA)
{return 0;}

/*
\marginpar{
static int __toku_lt_infinity;
static int __toku_lt_neg_infinity;
extern void* toku_lt_infinity = &__toku_lt_infinity;
extern void* toku_lt_neg_infinity = &__toku_lt_infinity;}
*/
