/* Copyright (C) 2007 MySQL AB

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

#ifndef __NDBINFO_H__
#define __NDBINFO_H__

#ifdef __cplusplus
extern "C" {
#endif

#define NDBINFO_TYPE_STRING 1
#define NDBINFO_TYPE_NUMBER 2

struct ndbinfo_column {
  char name[50];
  int coltype;
};

#define NDBINFO_CONSTANT_TABLE 0x1

struct ndbinfo_table {
  char name[50];
  int ncols;
  int flags;
  struct ndbinfo_column col[];
};

#define DECLARE_NDBINFO_TABLE(var, num)         \
struct ndbinfostruct##var {                      \
  struct ndbinfo_table t;                       \
  struct ndbinfo_column col[num];               \
} var

int ndbinfo_create_sql(struct ndbinfo_table *t, char* sql, int len);

#ifdef __cplusplus
}
#endif

#endif
