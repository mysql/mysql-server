/* -*- C++ -*- */
/* Copyright (C) 2002 MySQL AB

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

#ifndef _SP_H_
#define _SP_H_

//
// Finds a stored procedure given its name. Returns NULL if not found.
//
sp_head *
sp_find_procedure(THD *thd, Item_string *name);

int
sp_create_procedure(THD *thd, char *name, uint namelen, char *def, uint deflen);

int
sp_drop_procedure(THD *thd, char *name, uint namelen);

#if 0
sp_head *
sp_find_function(THD *thd, Item_string *name);

int
sp_create_function(THD *thd, char *name, uint namelen, char *def, uint deflen);

int
sp_drop_function(THD *thd, char *name, uint namelen);
#endif

#endif /* _SP_H_ */
