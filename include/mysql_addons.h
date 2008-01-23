/******************************************************
This file contains functions that need to be added to
MySQL code but have not been added yet.

Whenever you add a function here submit a MySQL bug
report (feature request) with the implementation. Then
write the bug number in the comment before the
function in this file.

When MySQL commits the function it can be deleted from
here. In a perfect world this file exists but is empty.

(c) 2007 Innobase Oy

Created November 07, 2007 Vasil Dimov
*******************************************************/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "univ.i"

/***********************************************************************
Retrieve THD::thread_id
http://bugs.mysql.com/30930 */

unsigned long
ib_thd_get_thread_id(
/*=================*/
				/* out: THD::thread_id */
	const void*	thd);	/* in: THD */

#ifdef __cplusplus
}
#endif /* __cplusplus */
