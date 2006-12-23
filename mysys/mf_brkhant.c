/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* Dont let the user break when you are doing something important */
/* Remembers if it got 'SIGINT' and executes it on allow_break */
/* A static buffer is used; don't call dont_break() twice in a row */

#include "mysys_priv.h"
#include "my_static.h"

	/* Set variable that we can't break */

#if !defined(THREAD)
void dont_break(void)
{
  my_dont_interrupt=1;
  return;
} /* dont_break */

void allow_break(void)
{
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
} /* dont_break */
#endif

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
