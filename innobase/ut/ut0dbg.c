/*********************************************************************
Debug utilities for Innobase.

(c) 1994, 1995 Innobase Oy

Created 1/30/1994 Heikki Tuuri
**********************************************************************/

#include "univ.i"

/* This is used to eliminate compiler warnings */
ulint	ut_dbg_zero	= 0;

/* If this is set to TRUE all threads will stop into the next assertion
and assert */
ibool	ut_dbg_stop_threads	= FALSE;

/* Null pointer used to generate memory trap */

ulint*	ut_dbg_null_ptr		= NULL;

/* Dummy function to prevent gcc from ignoring this file */
void
ut_dummy(void)
{
  printf("Hello world\n");
}
