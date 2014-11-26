/*****************************************************************************

Copyright (c) 2014, 2014, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file ut/ut0stage.h
Supplementary code to performance schema stage instrumentation.

Created Nov 12, 2014 Vasil Dimov
*******************************************************/

#ifndef ut0stage_h
#define ut0stage_h

#include "my_global.h" /* needed for headers from mysql/psi/ */

#include "mysql/psi/mysql_stage.h" /* mysql_stage_inc_work_completed */
#include "mysql/psi/psi.h" /* HAVE_PSI_STAGE_INTERFACE, PSI_stage_progress */

#include "univ.i"

#ifdef HAVE_PSI_STAGE_INTERFACE

/** Increment a stage progress.
This function will take care to increment also the total work estimated
in case the work completed is going to become larger than it.
@param[in,out]	progress	progress to increment
*/
inline
void
ut_stage_inc(
	PSI_stage_progress*	progress)
{
	if (progress == NULL) {
		return;
	}

	const int	inc_val = 1;
	const ulonglong	old_work_estimated
		= mysql_stage_get_work_estimated(progress);

	if (mysql_stage_get_work_completed(progress) + inc_val
	    > old_work_estimated) {

		mysql_stage_set_work_estimated(progress,
					       old_work_estimated + inc_val);
	}

	mysql_stage_inc_work_completed(progress, inc_val);
}

/** Change the current stage to a new one and keep the "work completed"
and "work estimated" numbers.
@param[in,out]	progress	progress whose stage to change
@param[in]	new_stage	new stage to be set
*/
inline
void
ut_stage_change(
	PSI_stage_progress**	progress,
	const PSI_stage_info*	new_stage)
{
	if (*progress == NULL) {
		return;
	}

	const ulonglong	completed = mysql_stage_get_work_completed(*progress);
	const ulonglong	estimated = mysql_stage_get_work_estimated(*progress);

	*progress = mysql_set_stage(new_stage->m_key);

	if (*progress == NULL) {
		return;
	}

	mysql_stage_set_work_completed(*progress, completed);
	mysql_stage_set_work_estimated(*progress, estimated);
}

#endif /* HAVE_PSI_STAGE_INTERFACE */

#endif /* ut0stage_h */
