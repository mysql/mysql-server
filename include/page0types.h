/******************************************************
Index page routines

(c) 1994-1996 Innobase Oy

Created 2/2/1994 Heikki Tuuri
*******************************************************/

#ifndef page0types_h
#define page0types_h

#include "univ.i"

/* Type of the index page */
/* The following define eliminates a name collision on HP-UX */
#define page_t     ib_page_t
typedef	byte		page_t;
typedef struct page_search_struct	page_search_t;
typedef struct page_cur_struct	page_cur_t;


#endif 
