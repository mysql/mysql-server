/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


/*
things to define before including the file:

#define LS_LIST_ITEM ListItem
#define LS_COMPARE_FUNC_DECL compare_func var_name,
#define LS_COMPARE_FUNC_CALL(list_el1, list_el2) (*var_name)(list_el1, list_el2)
#define LS_NEXT(A) (A)->next
#define LS_SET_NEXT(A,val) (A)->next= val
#define LS_P_NEXT(A) &(A)->next
#define LS_NAME plistsort
#define LS_SCOPE static
#define LS_STRUCT_NAME ls_struct_name
*/

typedef struct LS_STRUCT_NAME
{
  LS_LIST_ITEM *list1;
  int list_len;
  int return_point;
} LS_STRUCT_NAME;

LS_SCOPE LS_LIST_ITEM* LS_NAME(LS_COMPARE_FUNC_DECL LS_LIST_ITEM *list, int list_len)
{
  LS_LIST_ITEM *list_end;
  LS_LIST_ITEM *sorted_list;

  struct LS_STRUCT_NAME stack[63], *sp= stack;

  if (list_len < 2)
    return list;

  sp->list_len= list_len;
  sp->return_point= 2;

recursion_point:

  if (sp->list_len < 4)
  {
    LS_LIST_ITEM *e1, *e2;
    sorted_list= list;
    e1= LS_NEXT(sorted_list);
    list_end= LS_NEXT(e1);
    if (LS_COMPARE_FUNC_CALL(sorted_list, e1))
    {
      sorted_list= e1;
      e1= list;
    }
    if (sp->list_len == 2)
    {
      LS_SET_NEXT(sorted_list, e1);
      LS_SET_NEXT(e1, NULL);
      goto exit_point;
    }
    e2= list_end;
    list_end= LS_NEXT(e2);
    if (LS_COMPARE_FUNC_CALL(e1, e2))
    {
      {
        LS_LIST_ITEM *tmp_e= e1;
        e1= e2;
        e2= tmp_e;
      }
      if (LS_COMPARE_FUNC_CALL(sorted_list, e1))
      {
        LS_LIST_ITEM *tmp_e= sorted_list;
        sorted_list= e1;
        e1= tmp_e;
      }
    }

    LS_SET_NEXT(sorted_list, e1);
    LS_SET_NEXT(e1, e2);
    LS_SET_NEXT(e2, NULL);
    goto exit_point;
  }

  {
    register struct LS_STRUCT_NAME *sp0= sp++;
    sp->list_len= sp0->list_len >> 1;
    sp0->list_len-= sp->list_len;
    sp->return_point= 0;
  }
  goto recursion_point;
return_point0:
  sp->list1= sorted_list;
  {
    register struct LS_STRUCT_NAME *sp0= sp++;
    list= list_end;
    sp->list_len= sp0->list_len;
    sp->return_point= 1;
  }
  goto recursion_point;
return_point1:
  {
    register LS_LIST_ITEM **hook= &sorted_list;
    register LS_LIST_ITEM *list1= sp->list1;
    register LS_LIST_ITEM *list2= sorted_list;

    if (LS_COMPARE_FUNC_CALL(list1, list2))
    {
      LS_LIST_ITEM *tmp_e= list2;
      list2= list1;
      list1= tmp_e;
    }
    for (;;)
    {
      *hook= list1;
      do
      {
        if (!(list1= *(hook= LS_P_NEXT(list1))))
        {
          *hook= list2;
          goto exit_point;
        }
      } while (LS_COMPARE_FUNC_CALL(list2, list1));

      *hook= list2;
      do
      {
        if (!(list2= *(hook= LS_P_NEXT(list2))))
        {
          *hook= list1;
          goto exit_point;
        }
      } while (LS_COMPARE_FUNC_CALL(list1, list2));
    }
  }

exit_point:
  switch ((sp--)->return_point)
  {
    case 0: goto return_point0;
    case 1: goto return_point1;
    default:;
  }

  return sorted_list;
}


#undef LS_LIST_ITEM
#undef LS_NEXT
#undef LS_SET_NEXT
#undef LS_P_NEXT
#undef LS_NAME
#undef LS_STRUCT_NAME
#undef LS_SCOPE
#undef LS_COMPARE_FUNC_DECL
#undef LS_COMPARE_FUNC_CALL

