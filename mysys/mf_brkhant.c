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

/* Dont let the user break when you are doing something important */
/* Remembers if it got 'SIGINT' and executes it on allow_break */
/* A static buffer is used; don't call dont_break() twice in a row */

#include "mysys_priv.h"
#include "my_static.h"

	/* Set variable that we can't break */

void dont_break(void)
{
#if !defined(THREAD)
  my_dont_interrupt=1;
#endif
  return;
} /* dont_break */

void allow_break(void)
{
#if !defined(THREAD)
  {
    reg1 int index;

    my_dont_interrupt=0;
    if (_my_signals)
    {
      if (_my_signals > MAX_SIGNALS)
	_my_signals=MAX_SIGNALS;
      for (index=0 ; index < _my_signals ; index++)
      {
	if (_my_sig_remember[index].func)			/* Safequard */
	{
	  (*_my_sig_remember[index].func)(_my_sig_remember[index].number);
	  _my_sig_remember[index].func=0;
	}
      }
      _my_signals=0;
    }
  }
#endif
} /* dont_break */

	/* Set old status */

#if !defined(THREAD)
void my_remember_signal(int signal_number, sig_handler (*func) (int))
{
#ifndef __WIN__
  reg1 int index;

  index=_my_signals++;			/* Nobody can break a ++ ? */
  if (index < MAX_SIGNALS)
  {
    _my_sig_remember[index].number=signal_number;
    _my_sig_remember[index].func=func;
  }
#endif /* __WIN__ */
} /* my_remember_signal */
#endif /* THREAD */
