/* $Id: list.h,v 1.1 2002/12/13 12:17:20 hin Exp $  */

/*******************************************************************************
 *                                                                             *
 * Copyright (C) 1993 - 2000                                                   * 
 *         Dolphin Interconnect Solutions AS                                   *
 *                                                                             *
 * This program is free software; you can redistribute it and/or modify        * 
 * it under the terms of the GNU General Public License as published by        *
 * the Free Software Foundation; either version 2 of the License,              *
 * or (at your option) any later version.                                      *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               *
 * GNU General Public License for more details.                                *
 *                                                                             *
 * You should have received a copy of the GNU General Public License           *
 * along with this program; if not, write to the Free Software                 *
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA. *
 *                                                                             *
 *                                                                             *
 *******************************************************************************/ 



#ifndef _LIST_H
#define _LIST_H
#include "sci_types.h"


typedef struct ListElement *ListElement_t;
typedef struct List *List_t;

struct ListElement {
   void *element;
   u_vkaddr_t key;
   ListElement_t prev,next;
};

void          *Get_Element(ListElement_t el);
void          Set_Element(ListElement_t el,void *elptr,u_vkaddr_t key);
void          Create_Element(ListElement_t *el);
void          Destroy_Element(ListElement_t *el);
void          Create_List(List_t *list);
void          Destroy_List(List_t *list);
void          Add_Element(List_t list,ListElement_t el);
void          Remove_Element(List_t list,ListElement_t el);
ListElement_t Find_Element(List_t list,u_vkaddr_t key);
scibool       List_Empty(List_t);
scibool       List_Elements(List_t);
ListElement_t First_Element(List_t list);
ListElement_t Last_Element(List_t list);
ListElement_t Next_Element(ListElement_t el);

#endif /* _LIST_H */
