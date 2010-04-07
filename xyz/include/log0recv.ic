/*****************************************************************************

Copyright (c) 1997, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/**************************************************//**
@file include/log0recv.ic
Recovery

Created 9/20/1997 Heikki Tuuri
*******************************************************/

#include "univ.i"

/*******************************************************************//**
Returns TRUE if recovery is currently running.
@return	recv_recovery_on */
UNIV_INLINE
ibool
recv_recovery_is_on(void)
/*=====================*/
{
	return(UNIV_UNLIKELY(recv_recovery_on));
}

#ifdef UNIV_LOG_ARCHIVE
/** TRUE when applying redo log records from an archived log file */
extern ibool	recv_recovery_from_backup_on;

/*******************************************************************//**
Returns TRUE if recovery from backup is currently running.
@return	recv_recovery_from_backup_on */
UNIV_INLINE
ibool
recv_recovery_from_backup_is_on(void)
/*=================================*/
{
	return(recv_recovery_from_backup_on);
}
#endif /* UNIV_LOG_ARCHIVE */
