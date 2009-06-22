/*
 Copyright (C) 2009 Sun Microsystems, Inc.
 All rights reserved. Use is subject to license terms.

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

// ---------------------------------------------------------------------------
// generatable, unrolled macros for parameter handling
// ---------------------------------------------------------------------------

// template formal parameter list
#define TFPL(n) typename JP##n, typename CP##n,
#define TFPL0
#define TFPL1 TFPL0	TFPL(1)
#define TFPL2 TFPL1	TFPL(2)
#define TFPL3 TFPL2	TFPL(3)
#define TFPL4 TFPL3	TFPL(4)
#define TFPL5 TFPL4	TFPL(5)
#define TFPL6 TFPL5	TFPL(6)
#define TFPL7 TFPL6	TFPL(7)
#define TFPL8 TFPL7	TFPL(8)
#define TFPL9 TFPL8	TFPL(9)

//tfpl0: TFPL0
//tfpl1: TFPL1
//tfpl3: TFPL3

// C formal parameter list
#define CFPL(n) CP##n
#define CFPL0
#define CFPL1 CFPL0	CFPL(1)
#define CFPL2 CFPL1,	CFPL(2)
#define CFPL3 CFPL2,    CFPL(3)
#define CFPL4 CFPL3,    CFPL(4)
#define CFPL5 CFPL4,    CFPL(5)
#define CFPL6 CFPL5,    CFPL(6)
#define CFPL7 CFPL6,    CFPL(7)
#define CFPL8 CFPL7,    CFPL(8)
#define CFPL9 CFPL8,    CFPL(9)

//cfpl0: CFPL0
//cfpl1: CFPL1
//cfpl3: CFPL3

// Java formal parameter list
#define JFPL(n) JP##n jp##n
#define JFPL0
#define JFPL1 JFPL0	JFPL(1)
#define JFPL2 JFPL1,	JFPL(2)
#define JFPL3 JFPL2,    JFPL(3)
#define JFPL4 JFPL3,    JFPL(4)
#define JFPL5 JFPL4,    JFPL(5)
#define JFPL6 JFPL5,    JFPL(6)
#define JFPL7 JFPL6,    JFPL(7)
#define JFPL8 JFPL7,    JFPL(8)
#define JFPL9 JFPL8,    JFPL(9)

//jfpl0: JFPL0
//jfpl1: JFPL1
//jfpl3: JFPL3

// C actual parameter list
#define CAPL(n) cp##n
#define CAPL0
#define CAPL1 CAPL0	CAPL(1)
#define CAPL2 CAPL1,	CAPL(2)
#define CAPL3 CAPL2,    CAPL(3)
#define CAPL4 CAPL3,    CAPL(4)
#define CAPL5 CAPL4,    CAPL(5)
#define CAPL6 CAPL5,    CAPL(6)
#define CAPL7 CAPL6,    CAPL(7)
#define CAPL8 CAPL7,    CAPL(8)
#define CAPL9 CAPL8,    CAPL(9)

//capl0: CAPL0
//capl1: CAPL1
//capl3: CAPL3

// argument type conversion statements
#define ATCS(n) CP##n cp##n = jp##n;
#define ATCS0
#define ATCS1 ATCS0	ATCS(1)
#define ATCS2 ATCS1	ATCS(2)
#define ATCS3 ATCS2	ATCS(3)
#define ATCS4 ATCS3	ATCS(4)
#define ATCS5 ATCS4	ATCS(5)
#define ATCS6 ATCS5	ATCS(6)
#define ATCS7 ATCS6	ATCS(7)
#define ATCS8 ATCS7	ATCS(8)
#define ATCS9 ATCS8	ATCS(9)

//atcs0: ATCS0
//atcs1: ATCS1
//atcs3: ATCS3

// ---------------------------------------------------------------------------
// the "blueprint" macro generating wrapper template function definitions
// ---------------------------------------------------------------------------

// need two levels of macro expansion
#define TRACE0(rt, ptl) TRACE( #rt " gcall" #ptl );
#define TRACE1(rt, ptl) TRACE0( rt, ( ptl ) )

// template function definition
#define TFD(n)                                  \
    template< TFRT                              \
              TFPL##n                           \
              CFRT F( CFPL##n ) >               \
    JFRT                                        \
    gcall( JFPL##n ) {                          \
        TRACE1( JFRT, JFPL##n )                 \
        ATCS##n                                 \
        CFRV F( CAPL##n );                      \
        RTCS                                    \
        JFRS                                    \
    }

// ---------------------------------------------------------------------------
// issue wrapper template function definitions for void functions
// ---------------------------------------------------------------------------

//#define TFD3 TFD(0) TFD(1) TFD(2) TFD(3)

// generate void template function definitions
#define TFRT
#define JFRT void
#define CFRT void
#define CFRV
#define RTCS
#define JFRS
TFD(0)
TFD(1)
TFD(2)
TFD(3)
#undef TFRT
#undef JFRT
#undef CFRT
#undef CFRV
#undef RTCS
#undef JFRS

// ---------------------------------------------------------------------------
// issue wrapper template function definitions for result returning functions
// ---------------------------------------------------------------------------

// generate value template function definitions
#define TFRT typename JR, typename CR,
#define JFRT JR
#define CFRT CR
#define CFRV CR cr =
#define RTCS JR jr = cr;
#define JFRS return jr;
TFD(0)
TFD(1)
TFD(2)
TFD(3)
#undef TFRT
#undef JFRT
#undef CFRT
#undef CFRV
#undef RTCS
#undef JFRS

// ---------------------------------------------------------------------------
// that's it!
// ---------------------------------------------------------------------------
