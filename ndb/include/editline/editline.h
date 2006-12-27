/* Copyright (C) 2003 MySQL AB

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

/* $Id: editline.h,v 1.1 2002/12/11 13:53:46 hin Exp $ */

/*
 * Public include file for editline, to be included instead of readline.h
 */

#ifndef __EDITLINE_H_INCLUDED__
#define __EDITLINE_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

extern char	*readline(const char *);
extern void	add_history(char *);

#ifdef __cplusplus
}
#endif

#endif /* !__EDITLINE_H_INCLUDED__ */

