/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

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
