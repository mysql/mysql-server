/* Copyright (C) 2000 MySQL AB

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

#include <my_global.h>
#include <m_ctype.h>

CHARSET_INFO compiled_charsets[] = {
  {
    0,0,0,		/* end-of-list marker */
    0,			/* state      */
    NullS,		/* cs name    */
    NullS,		/* name       */
    NullS,		/* comment    */
    NULL,		/* tailoring  */
    NULL,		/* ctype      */
    NULL,		/* to_lower   */
    NULL,		/* to_upper   */
    NULL,		/* sort_order */
    NULL,		/* contractions */
    NULL,		/* sort_order_big*/
    NULL,		/* tab_to_uni   */
    NULL,		/* tab_from_uni */
    my_unicase_default, /* caseinfo     */
    NULL,		/* state_map    */
    NULL,		/* ident_map    */
    0,			/* strxfrm_mul  */
    0,			/* caseup_mul   */
    0,			/* casedn_mul   */
    0,			/* mbminlen     */
    0,			/* mbmaxlen     */
    0,			/* min_sort_ord */
    0,			/* max_sort_ord */
    ' ',                /* pad char     */
    0,                  /* escape_with_backslash_is_dangerous */
    NULL,		/* cset handler */
    NULL		/* coll handler */
  }
};
