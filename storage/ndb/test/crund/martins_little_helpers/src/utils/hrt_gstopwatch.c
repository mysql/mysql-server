/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
 * hrt_gstopwatch.c
 *
 */

#include "hrt_gstopwatch.h"

/*
 * High-Resolution Time Global Stopwatch Utility -- Implementation
 */

static hrt_stopwatch gsw;

extern void
hrt_gsw_init(int cap)
{
    hrt_sw_init(&gsw, cap);
}

extern void
hrt_gsw_close(void)
{
    hrt_sw_close(&gsw);
}

extern int
hrt_gsw_top(void)
{
    return hrt_sw_top(&gsw);
}

extern int
hrt_gsw_capacity(void)
{
    return hrt_sw_capacity(&gsw);
}

extern int
hrt_gsw_pushmark(void)
{
    return hrt_sw_pushmark(&gsw);
}

extern void
hrt_gsw_popmark(void)
{
    hrt_sw_popmark(&gsw);
}

extern double
hrt_gsw_rtmicros(int y, int x)
{
    return hrt_sw_rtmicros(&gsw, y, x);
}

extern double
hrt_gsw_ctmicros(int y, int x)
{
    return hrt_sw_ctmicros(&gsw, y, x);
}

extern void
hrt_gsw_clear(void)
{
    hrt_sw_clear(&gsw);
}
