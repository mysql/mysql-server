/*
   Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

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

/* Defines that are unique to the embedded version of MySQL */

#ifdef EMBEDDED_LIBRARY

/* Things we don't need in the embedded version of MySQL */
/* TODO HF add #undef HAVE_VIO if we don't want client in embedded library */

#undef HAVE_OPENSSL
#undef HAVE_SMEM				/* No shared memory */
#undef HAVE_NDBCLUSTER_DB /* No NDB cluster */

#define DONT_USE_RAID

#endif /* EMBEDDED_LIBRARY */
