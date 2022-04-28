/*
   Copyright (c) 2010, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
 * hrt_stopwatch_test.c
 *
 */

#include <assert.h>
#include <stdio.h>

#include "hrt_stopwatch.h"
#include "hrt_gstopwatch.h"

/*
 * High-Resolution Time Stopwatch Utility -- Test
 */


static void
do_something(void)
{
    const unsigned long loop = 1000000000L;
    unsigned long i;
    static volatile unsigned long dummy;
    for (i = 0; i < loop; i++)
	dummy = i;
}

int
main(int argc, const char* argv[])
{
    printf("--> main()\n");

    printf("init stopwatches...\n");
    hrt_gsw_init(10);
    assert(hrt_gsw_top() == -1);
    hrt_stopwatch sw;
    hrt_sw_init(&sw, 10);
    assert(hrt_sw_top(&sw) == -1);
    
    printf("marking global time...\n");
    int g0 = hrt_gsw_pushmark();
    do_something();

    printf("marking time...\n");
    int t0 = hrt_sw_pushmark(&sw);
    do_something();

    printf("marking time...\n");
    int t1 = hrt_sw_pushmark(&sw);
    do_something();

    printf("marking time...\n");
    int t2 = hrt_sw_pushmark(&sw);
    do_something();

    printf("marking global time...\n");
    int g2 = hrt_gsw_pushmark();

    assert(hrt_gsw_top() == 1);
    assert(hrt_sw_top(&sw) == 2);

    printf("\namount of times:\n");
    double rt0 = hrt_sw_rtmicros(&sw, t1, t0);
    double rt1 = hrt_sw_rtmicros(&sw, t2, t1);
    double rt2 = hrt_sw_rtmicros(&sw, t2, t0);
    double grt2 = hrt_gsw_rtmicros(g2, g0);
    printf("[t0..t1] real   = %.3f us\n", rt0);
    printf("[t1..t2] real   = %.3f us\n", rt1);
    printf("[t0..t2] real   = %.3f us\n", rt2);
    printf("[g0..g2] real   = %.3f us\n", grt2);
    double ct0 = hrt_sw_ctmicros(&sw, t1, t0);
    double ct1 = hrt_sw_ctmicros(&sw, t2, t1);
    double ct2 = hrt_sw_ctmicros(&sw, t2, t0);
    double gct2 = hrt_gsw_ctmicros(g2, g0);
    printf("[t0..t1] cpu    = %.3f us\n", ct0);
    printf("[t1..t2] cpu    = %.3f us\n", ct1);
    printf("[t0..t2] cpu    = %.3f us\n", ct2);
    printf("[g0..g2] cpu    = %.3f us\n", gct2);

    printf("\npopping timemarks...\n");
    hrt_gsw_popmark();
    assert(hrt_gsw_top() == 0);
    hrt_gsw_popmark();
    assert(hrt_gsw_top() == -1);
    hrt_sw_popmark(&sw);
    assert(hrt_sw_top(&sw) == 1);
    hrt_sw_popmark(&sw);
    assert(hrt_sw_top(&sw) == 0);
    hrt_sw_popmark(&sw);
    assert(hrt_sw_top(&sw) == -1);

    printf("closing stopwatches...\n");
    hrt_sw_close(&sw);
    hrt_gsw_close();

    printf("\n<-- main()\n");
    return 0;
}
