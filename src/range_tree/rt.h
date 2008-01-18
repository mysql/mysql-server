/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

const char *toku_patent_string = "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it.";
const char *toku_copyright_string = "Copyright (c) 2007 Tokutek Inc.  All rights reserved.";

/*
 * Range trees
 *
 * This is the header file for range trees. */

#include <brttypes.h>


/* Represents a range of data with an extra value. */
typedef struct {
	void*   left;
  	void*   right;
	void*   data; 
} toku_range;

typedef struct {int dummy;} toku_range_tree;


int toku_rt_create(toku_range_tree**, 
                   int (*)(void*,void*), int (*)(void*,void*), 
		           BOOL allow_overlaps);

int toku_rt_close(toku_range_tree*);

int toku_rt_find(toku_range_tree*, toku_range*, int, toku_range**, int*, int*);

int toku_rt_insert(toku_range_tree*, toku_range*);
int toku_rt_delete(toku_range_tree*, toku_range*);

int toku_rt_predecessor (toku_range_tree*, void*, toku_range*);
int toku_rt_successor   (toku_range_tree*, void*, toku_range*);

