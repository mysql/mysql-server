/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include "NdbHost.h"
#include <unistd.h>


#include <inet.sig>
#include <string.h>

union SIGNAL 
{
  SIGSELECT sigNo;
  struct InetIfUp inetIfUp;
};

int NdbHost_GetHostName(char* buf)
{
#if 0
  extern PROCESS          ose_inet_;
  union SIGNAL           *signal;
  static const SIGSELECT  select_if_up_reply[]  = { 1, INET_IF_UP_REPLY };

  signal = alloc(sizeof(struct InetIfUp), INET_IF_UP_REQUEST);
  strcpy(signal->inetIfUp.ifName, "*");
  send((union SIGNAL **)&signal, ose_inet_);
  signal = receive((SIGSELECT *)select_if_up_reply);
  strcpy(buf, signal->inetIfUp.ifName); 
  free_buf(&signal);
  return 0;
#else
  return -1;
#endif
}


int NdbHost_GetProcessId(void)
{
  return current_process();
}

