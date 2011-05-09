/******************************************************
Full Text Search functionality.

(c) 2006 Innobase Oy

Created 2006-02-15 Jimmy Yang
*******************************************************/

#ifndef INNODB_FTS0OPT_H
#define INNODB_FTS0OPT_H

/********************************************************************
Callback function to fetch the rows in an FTS INDEX record. */
UNIV_INTERN
ibool
fts_optimize_index_fetch_node(
/*==========================*/
                                        /* out: always returns non-NULL */
        void*           row,		/* in: sel_node_t* */
        void*           user_arg);	/* in: pointer to ib_vector_t */
#endif
