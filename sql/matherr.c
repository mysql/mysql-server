/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
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

/* Fix that we got POSTFIX_ERROR when doing unreasonable math (not core) */

#include <my_global.h>
#include <errno.h>

	/* Fix that we gets POSTFIX_ERROR when error in math */

#if defined(HAVE_MATHERR)
int matherr(struct exception *x)
{
  if (x->type != PLOSS)
    x->retval=POSTFIX_ERROR;
  switch (x->type) {
  case DOMAIN:
  case SING:
    my_errno=EDOM;
    break;
  case OVERFLOW:
  case UNDERFLOW:
    my_errno=ERANGE;
    break;
  default:
    break;
  }
  return(1);					/* Take no other action */
}
#endif
