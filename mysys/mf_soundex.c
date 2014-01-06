/* Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.

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

/****************************************************************
*	SOUNDEX ALGORITHM in C					*
*								*
*	The basic Algorithm source is taken from EDN Nov.	*
*	14, 1985 pg. 36.					*
*								*
*	As a test Those in Illinois will find that the		*
*	first group of numbers in their drivers license		*
*	number is the soundex number for their last name.	*
*								*
*	RHW  PC-IBBS ID. #1230					*
*								*
*	As an extension if remove_garbage is set then all non-	*
*	alpha characters are skipped				*
*                                                               *
*       Note, that this implementation corresponds to the       *
*       original version of the algorithm, not to the more      *
*       popular "enhanced" version, described by Knuth.         *
****************************************************************/

#include "mysys_priv.h"
#include <m_ctype.h>
#include "my_static.h"

static char get_scode(CHARSET_INFO * cs, char **ptr,pbool remove_garbage);

		/* outputed string is 4 byte long */
		/* out_pntr can be == in_pntr */

void soundex(CHARSET_INFO * cs, char * out_pntr, char * in_pntr,
	     pbool remove_garbage)
{
  char ch,last_ch;
  char *end;
  const uchar *map=cs->to_upper;

  if (remove_garbage)
  {
    while (*in_pntr && !my_isalpha(cs,*in_pntr)) /* Skip pre-space */
      in_pntr++;
  }
  *out_pntr++ = map[(uchar)*in_pntr];	/* Copy first letter		 */
  last_ch = get_scode(cs,&in_pntr,0);	/* code of the first letter	 */
					/* for the first 'double-letter  */
					/* check.			 */
  end=out_pntr+3;			/* Loop on input letters until	 */
					/* end of input (null) or output */
					/* letter code count = 3	 */

  in_pntr++;
  while (out_pntr < end && (ch = get_scode(cs,&in_pntr,remove_garbage)) != 0)
  {
    in_pntr++;
    if ((ch != '0') && (ch != last_ch)) /* if not skipped or double */
    {
      *out_pntr++ = ch;			/* letter, copy to output */
    }					/* for next double-letter check */
    last_ch = ch;			/* save code of last input letter */
  }
  while (out_pntr < end)
    *out_pntr++ = '0';
  *out_pntr=0;				/* end string */
  return;
} /* soundex */


  /*
    If alpha, map input letter to soundex code.
    If not alpha and remove_garbage is set then skip to next char
    else return 0
    */

static char get_scode(CHARSET_INFO * cs,char **ptr, pbool remove_garbage)
{
  uchar ch;

  if (remove_garbage)
  {
    while (**ptr && !my_isalpha(cs,**ptr))
      (*ptr)++;
  }
  ch=my_toupper(cs,**ptr);
  if (ch < 'A' || ch > 'Z')
  {
    if (my_isalpha(cs,ch))		/* If extended alfa (country spec) */
      return '0';			/* threat as vokal */
    return 0;				/* Can't map */
  }
  return(soundex_map[ch-'A']);
} /* get_scode */
