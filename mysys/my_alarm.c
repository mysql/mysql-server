/* Copyright (C) 2000 MySQL AB
   Use is subject to license terms

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

/* Function to set a varible when we got a alarm */
/* Used by my_lock samt functions in m_alarm.h */


#include "mysys_priv.h"
#include "my_alarm.h"

#ifdef HAVE_ALARM

	/* ARGSUSED */
sig_handler my_set_alarm_variable(int signo __attribute__((unused)))
{
  my_have_got_alarm=1;			/* Tell program that time expired */
  return;
}

#endif /* HAVE_ALARM */
