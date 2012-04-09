/*
   Copyright (C) 2005, 2006 MySQL AB
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

#ifndef MACROS_H
#define MACROS_H

#include <ndb_global.h>
#include <NdbOut.hpp>

#define ERROR(x) {ndbout_c((x));}
#define ERROR1(x,y) {ndbout_c((x), (y));}
#define ERROR2(x,y,z) {ndbout_c((x), (y), (z));}
#define ERROR3(x,y,z,u) {ndbout_c((x), (y), (z), (u));}
#define ERROR4(x,y,z,u,w) {ndbout_c((x), (y), (z), (u), (w));}

#define INIT_RANDOM(x) srand48((x))
#define UI_RANDOM(x)   ((unsigned int)(lrand48()%(x)))

#define ASSERT(cond, message) \
  { if(!(cond)) { ERROR(message); exit(-1); }}

#ifdef DEBUG_ON
#define DEBUG(x) {ndbout_c((x));}
#define DEBUG1(x,y) {ndbout_c((x), (y));}
#define DEBUG2(x,y,z) {ndbout_c((x), (y), (z));}
#define DEBUG3(x,y,z,u) {ndbout_c((x), (y), (z), (u));}
#define DEBUG4(x,y,z,u,w) {ndbout_c((x), (y), (z), (u), (w));}
#define DEBUG5(x,y,z,u,w, v) {ndbout_c((x), (y), (z), (u), (w), (v));}
#else
#define DEBUG(x)
#define DEBUG1(x,y)
#define DEBUG2(x,y,z)
#define DEBUG3(x,y,z,u)
#define DEBUG4(x,y,z,u,w)
#define DEBUG5(x,y,z,u,w, v)
#endif

#endif
