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

/* Get date in a printable form: yyyy-mm-dd hh:mm:ss */

#include "mysys_priv.h"
#include <m_string.h>

	/*
	  If flag & 1 Return date and time
	  If flag & 2 Return short date format YYMMDD
	  */


void get_date(register my_string to, int flag, time_t date)
{
   reg2 struct tm *start_time;
   time_t skr;
#if defined(HAVE_LOCALTIME_R) && defined(_REENTRANT)
  struct tm tm_tmp;
#endif

   skr=date ? (time_t) date : time((time_t*) 0);
#if defined(HAVE_LOCALTIME_R) && defined(_REENTRANT)
   localtime_r(&skr,&tm_tmp);
   start_time= &tm_tmp;
#else
   start_time=localtime(&skr);
#endif
   if (flag & 2)
     sprintf(to,"%02d%02d%02d",
	     start_time->tm_year % 100,
	     start_time->tm_mon+1,
	     start_time->tm_mday);
   else
     sprintf(to,"%d-%02d-%02d",
	     start_time->tm_year+1900,
	     start_time->tm_mon+1,
	     start_time->tm_mday);
   if (flag & 1)
     sprintf(strend(to)," %2d:%02d:%02d",
	     start_time->tm_hour,
	     start_time->tm_min,
	     start_time->tm_sec);
} /* get_date */
