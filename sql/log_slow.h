/* Copyright (C) 2009 Monty Program Ab

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 or later of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* Defining what to log to slow log */

#define LOG_SLOW_VERBOSITY_INIT           0
#define LOG_SLOW_VERBOSITY_INNODB         1 << 0
#define LOG_SLOW_VERBOSITY_QUERY_PLAN     1 << 1

#define QPLAN_INIT            QPLAN_QC_NO

#define QPLAN_ADMIN           1 << 0
#define QPLAN_FILESORT        1 << 1
#define QPLAN_FILESORT_DISK   1 << 2
#define QPLAN_FULL_JOIN       1 << 3
#define QPLAN_FULL_SCAN       1 << 4
#define QPLAN_QC              1 << 5
#define QPLAN_QC_NO           1 << 6
#define QPLAN_TMP_DISK        1 << 7
#define QPLAN_TMP_TABLE       1 << 8
/* ... */
#define QPLAN_MAX             ((ulong) 1) << 31 /* reserved as placeholder */

