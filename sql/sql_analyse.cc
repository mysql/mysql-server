/*
   Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


/* Analyse database */

/* TODO: - Check if any character fields can be of any date type
**	   (date, datetime, year, time, timestamp, newdate)
**	 - Check if any number field should be a timestamp
**	 - type set is out of optimization yet
*/

#include "sql_analyse.h"

#include "procedure.h"       // Item_proc
#include "sql_yacc.h"        // DECIMAL_NUM

#include <algorithm>
using std::min;
using std::max;

int sortcmp2(void* cmp_arg MY_ATTRIBUTE((unused)),
	     const String *a,const String *b)
{
  return sortcmp(a,b,a->charset());
}

int compare_double2(void* cmp_arg MY_ATTRIBUTE((unused)),
		    const double *s, const double *t)
{
  return compare_double(s,t);
}

int compare_longlong2(void* cmp_arg MY_ATTRIBUTE((unused)),
		      const longlong *s, const longlong *t)
{
  return compare_longlong(s,t);
}

int compare_ulonglong2(void* cmp_arg MY_ATTRIBUTE((unused)),
		       const ulonglong *s, const ulonglong *t)
{
  return compare_ulonglong(s,t);
}

int compare_decimal2(int* len, const char *s, const char *t)
{
  return memcmp(s, t, *len);
}

/**
  Create column data accumulator structures 

  @param field_list     Output columns of the original SELECT

  @retval false         Success
  @retval true          Failure (OOM)
*/
bool
Query_result_analyse::init(List<Item> &field_list)
{
  DBUG_ENTER("proc_analyse_init");

  DBUG_ASSERT(thd->lex->sql_command == SQLCOM_SELECT);

  if (!(f_info=
        (field_info**)sql_alloc(sizeof(field_info*)*field_list.elements)))
    DBUG_RETURN(true);

  f_end= f_info + field_list.elements;

  {
    List_iterator_fast<Item> it(field_list);
    field_info **info= f_info;
    Item *item;
    while ((item = it++))
    {
      field_info *new_field;
      switch (item->result_type()) {
      case INT_RESULT:
        // Check if fieldtype is ulonglong
        if (item->type() == Item::FIELD_ITEM &&
            ((Item_field*) item)->field->type() == MYSQL_TYPE_LONGLONG &&
            ((Field_longlong*) ((Item_field*) item)->field)->unsigned_flag)
          new_field= new field_ulonglong(item, this);
        else
          new_field= new field_longlong(item, this);
        break;
      case REAL_RESULT:
        new_field= new field_real(item, this);
        break;
      case DECIMAL_RESULT:
        new_field= new field_decimal(item, this);
        break;
      case STRING_RESULT:
        new_field= new field_str(item, this);
        break;
      default:
        DBUG_RETURN(true);
      }
      if (new_field == NULL)
        DBUG_RETURN(true);
      *info++= new_field;
    }
  }
  DBUG_RETURN(false);
}


/*
  Return 1 if number, else return 0
  store info about found number in info
  NOTE:It is expected, that elements of 'info' are all zero!
*/

bool test_if_number(NUM_INFO *info, const char *str, uint str_len)
{
  const char *begin, *end= str + str_len;
  DBUG_ENTER("test_if_number");

  /*
    MySQL removes any endspaces of a string, so we must take care only of
    spaces in front of a string
  */
  for (; str != end && my_isspace(system_charset_info, *str); str++) ;
  if (str == end)
    DBUG_RETURN(0);

  if (*str == '-')
  {
    info->negative = 1;
    if (++str == end || *str == '0')    // converting -0 to a number
      DBUG_RETURN(0);                   // might lose information
  }
  else
    info->negative = 0;
  begin = str;
  for (; str != end && my_isdigit(system_charset_info,*str); str++)
  {
    if (!info->integers && *str == '0' && (str + 1) != end &&
	my_isdigit(system_charset_info,*(str + 1)))
      info->zerofill = 1;	     // could be a postnumber for example
    info->integers++;
  }
  if (str == end && info->integers)
  {
    char *endpos= (char*) end;
    int error;
    info->ullval= (ulonglong) my_strtoll10(begin, &endpos, &error);
    if (info->integers == 1)
      DBUG_RETURN(0);                   // single number can't be zerofill
    info->maybe_zerofill = 1;
    DBUG_RETURN(1);                     // a zerofill number, or an integer
  }
  if (*str == '.' || *str == 'e' || *str == 'E')
  {
    if (info->zerofill)                 // can't be zerofill anymore
      DBUG_RETURN(0);
    if ((str + 1) == end)               // number was something like '123[.eE]'
    {
      char *endpos= (char*) str;
      int error;
      info->ullval= (ulonglong) my_strtoll10(begin, &endpos, &error);
      DBUG_RETURN(1);
    }
    if (*str == 'e' || *str == 'E')     // number may be something like '1e+50'
    {
      str++;
      if (*str != '-' && *str != '+')
	DBUG_RETURN(0);
      for (str++; str != end && my_isdigit(system_charset_info,*str); str++) ;
      if (str == end)
      {
	info->is_float = 1;             // we can't use variable decimals here
	DBUG_RETURN(1);
      }
      DBUG_RETURN(0);
    }
    for (str++; *(end - 1) == '0'; end--) ; // jump over zeros at the end
    if (str == end)		     // number was something like '123.000'
    {
      char *endpos= (char*) str;
      int error;
      info->ullval= (ulonglong) my_strtoll10(begin, &endpos, &error);
      DBUG_RETURN(1);
    }
    for (; str != end && my_isdigit(system_charset_info,*str); str++)
      info->decimals++;
    if (str == end)
    {
      info->dval = my_atof(begin);
      DBUG_RETURN(1);
    }
  }
  DBUG_RETURN(0);
}


/*
  Stores the biggest and the smallest value from current 'info'
  to ev_num_info
  If info contains an ulonglong number, which is bigger than
  biggest positive number able to be stored in a longlong variable
  and is marked as negative, function will return 0, else 1.
*/

bool get_ev_num_info(EV_NUM_INFO *ev_info, NUM_INFO *info, const char *num)
{
  if (info->negative)
  {
    if (((longlong) info->ullval) < 0)
      return 0; // Impossible to store as a negative number
    ev_info->llval =  - max<longlong>((ulonglong) -ev_info->llval, 
                                           info->ullval);
    ev_info->min_dval = - max<double>(-ev_info->min_dval, info->dval);
  }
  else		// ulonglong is as big as bigint in MySQL
  {
    if ((check_ulonglong(num, info->integers) == DECIMAL_NUM))
      return 0;
    ev_info->ullval = max<ulonglong>(ev_info->ullval, info->ullval);
    ev_info->max_dval = max<double>(ev_info->max_dval, info->dval);
  }
  return 1;
} // get_ev_num_info


void free_string(void *s_void, TREE_FREE, const void*)
{
  String *s= static_cast<String*>(s_void);
  s->mem_free();
}


void field_str::add()
{
  char buff[MAX_FIELD_WIDTH], *ptr;
  String s(buff, sizeof(buff),&my_charset_bin), *res;
  size_t length;

  if (!(res= item->str_result(&s)))
  {
    nulls++;
    return;
  }

  if (!(length = res->length()))
    empty++;
  else
  {
    ptr = (char*) res->ptr();
    if (*(ptr + (length - 1)) == ' ')
      must_be_blob = 1;
  }

  if (can_be_still_num)
  {
    memset(&num_info, 0, sizeof(num_info));
    if (!test_if_number(&num_info, res->ptr(), (uint) length))
      can_be_still_num = 0;
    if (!found)
    {
      memset(&ev_num_info, 0, sizeof(ev_num_info));
      was_zero_fill = num_info.zerofill;
    }
    else if (num_info.zerofill != was_zero_fill && !was_maybe_zerofill)
      can_be_still_num = 0;  // one more check needed, when length is counted
    if (can_be_still_num)
      can_be_still_num = get_ev_num_info(&ev_num_info, &num_info, res->ptr());
    was_maybe_zerofill = num_info.maybe_zerofill;
  }

  /* Update min and max arguments */
  if (!found)
  {
    found = 1;
    min_arg.copy(*res);
    max_arg.copy(*res);
    min_length = max_length = length; sum=length;
  }
  else if (length)
  {
    sum += length;
    if (length < min_length)
      min_length = length;
    if (length > max_length)
      max_length = length;

    if (sortcmp(res, &min_arg,item->collation.collation) < 0)
      min_arg.copy(*res);
    if (sortcmp(res, &max_arg,item->collation.collation) > 0)
      max_arg.copy(*res);
  }

  if (room_in_tree)
  {
    if (res != &s)
      s.copy(*res);
    if (!tree_search(&tree, (void*) &s, tree.custom_arg)) // If not in tree
    {
      s.copy();        // slow, when SAFE_MALLOC is in use
      if (!tree_insert(&tree, (void*) &s, 0, tree.custom_arg))
      {
	room_in_tree = 0;      // Remove tree, out of RAM ?
	delete_tree(&tree);
      }
      else
      {
        ::new (&s) String; // Let tree handle free of this
	if ((treemem += length) > pc->max_treemem)
	{
	  room_in_tree = 0;	 // Remove tree, too big tree
	  delete_tree(&tree);
	}
      }
    }
  }

  if ((num_info.zerofill && (max_length != min_length)) ||
      (was_zero_fill && (max_length != min_length)))
    can_be_still_num = 0; // zerofilled numbers must be of same length
} // field_str::add


void field_real::add()
{
  char buff[MAX_FIELD_WIDTH], *ptr, *end;
  double num= item->val_result();
  size_t length;
  uint zero_count, decs;
  TREE_ELEMENT *element;

  if (item->null_value)
  {
    nulls++;
    return;
  }
  if (num == 0.0)
    empty++;

  if ((decs = decimals()) == NOT_FIXED_DEC)
  {
    length= sprintf(buff, "%g", num);
    if (rint(num) != num)
      max_notzero_dec_len = 1;
  }
  else
  {
    buff[sizeof(buff)-1]=0;			// Safety
    my_snprintf(buff, sizeof(buff)-1, "%-.*f", (int) decs, num);
    length= strlen(buff);

    // We never need to check further than this
    end = buff + length - 1 - decs + max_notzero_dec_len;

    zero_count = 0;
    for (ptr = buff + length - 1; ptr > end && *ptr == '0'; ptr--)
      zero_count++;

    if ((decs - zero_count > max_notzero_dec_len))
      max_notzero_dec_len = decs - zero_count;
  }

  if (room_in_tree)
  {
    if (!(element = tree_insert(&tree, (void*) &num, 0, tree.custom_arg)))
    {
      room_in_tree = 0;    // Remove tree, out of RAM ?
      delete_tree(&tree);
    }
    /*
      if element->count == 1, this element can be found only once from tree
      if element->count == 2, or more, this element is already in tree
    */
    else if (element->count == 1 && (tree_elements++) >= pc->max_tree_elements)
    {
      room_in_tree = 0;  // Remove tree, too many elements
      delete_tree(&tree);
    }
  }

  if (!found)
  {
    found = 1;
    min_arg = max_arg = sum = num;
    sum_sqr = num * num;
    min_length = max_length = length;
  }
  else if (num != 0.0)
  {
    sum += num;
    sum_sqr += num * num;
    if (length < min_length)
      min_length = length;
    if (length > max_length)
      max_length = length;
    if (compare_double(&num, &min_arg) < 0)
      min_arg = num;
    if (compare_double(&num, &max_arg) > 0)
      max_arg = num;
  }
} // field_real::add


void field_decimal::add()
{
  /*TODO - remove rounding stuff after decimal_div returns proper frac */
  my_decimal dec_buf, *dec= item->val_decimal_result(&dec_buf);
  my_decimal rounded;
  uint length;
  TREE_ELEMENT *element;

  if (item->null_value)
  {
    nulls++;
    return;
  }

  my_decimal_round(E_DEC_FATAL_ERROR, dec, item->decimals, FALSE,&rounded);
  dec= &rounded;

  length= my_decimal_string_length(dec);

  if (decimal_is_zero(dec))
    empty++;

  if (room_in_tree)
  {
    uchar buf[DECIMAL_MAX_FIELD_SIZE];
    my_decimal2binary(E_DEC_FATAL_ERROR, dec, buf,
                      item->max_length, item->decimals);
    if (!(element = tree_insert(&tree, (void*)buf, 0, tree.custom_arg)))
    {
      room_in_tree = 0;    // Remove tree, out of RAM ?
      delete_tree(&tree);
    }
    /*
      if element->count == 1, this element can be found only once from tree
      if element->count == 2, or more, this element is already in tree
    */
    else if (element->count == 1 && (tree_elements++) >= pc->max_tree_elements)
    {
      room_in_tree = 0;  // Remove tree, too many elements
      delete_tree(&tree);
    }
  }

  if (!found)
  {
    found = 1;
    min_arg = max_arg = sum[0] = *dec;
    my_decimal_mul(E_DEC_FATAL_ERROR, sum_sqr, dec, dec);
    cur_sum= 0;
    min_length = max_length = length;
  }
  else if (!decimal_is_zero(dec))
  {
    int next_cur_sum= cur_sum ^ 1;
    my_decimal sqr_buf;

    my_decimal_add(E_DEC_FATAL_ERROR, sum+next_cur_sum, sum+cur_sum, dec);
    my_decimal_mul(E_DEC_FATAL_ERROR, &sqr_buf, dec, dec);
    my_decimal_add(E_DEC_FATAL_ERROR,
                   sum_sqr+next_cur_sum, sum_sqr+cur_sum, &sqr_buf);
    cur_sum= next_cur_sum;
    if (length < min_length)
      min_length = length;
    if (length > max_length)
      max_length = length;
    if (my_decimal_cmp(dec, &min_arg) < 0)
    {
      min_arg= *dec;
    }
    if (my_decimal_cmp(dec, &max_arg) > 0)
    {
      max_arg= *dec;
    }
  }
}


void field_longlong::add()
{
  char buff[MAX_FIELD_WIDTH];
  longlong num= item->val_int_result();
  uint length= (uint) (longlong10_to_str(num, buff, -10) - buff);
  TREE_ELEMENT *element;

  if (item->null_value)
  {
    nulls++;
    return;
  }
  if (num == 0)
    empty++;

  if (room_in_tree)
  {
    if (!(element = tree_insert(&tree, (void*) &num, 0, tree.custom_arg)))
    {
      room_in_tree = 0;    // Remove tree, out of RAM ?
      delete_tree(&tree);
    }
    /*
      if element->count == 1, this element can be found only once from tree
      if element->count == 2, or more, this element is already in tree
    */
    else if (element->count == 1 && (tree_elements++) >= pc->max_tree_elements)
    {
      room_in_tree = 0;  // Remove tree, too many elements
      delete_tree(&tree);
    }
  }

  if (!found)
  {
    found = 1;
    min_arg = max_arg = sum = num;
    sum_sqr = num * num;
    min_length = max_length = length;
  }
  else if (num != 0)
  {
    sum += num;
    sum_sqr += num * num;
    if (length < min_length)
      min_length = length;
    if (length > max_length)
      max_length = length;
    if (compare_longlong(&num, &min_arg) < 0)
      min_arg = num;
    if (compare_longlong(&num, &max_arg) > 0)
      max_arg = num;
  }
} // field_longlong::add


void field_ulonglong::add()
{
  char buff[MAX_FIELD_WIDTH];
  longlong num= item->val_int_result();
  uint length= (uint) (longlong10_to_str(num, buff, 10) - buff);
  TREE_ELEMENT *element;

  if (item->null_value)
  {
    nulls++;
    return;
  }
  if (num == 0)
    empty++;

  if (room_in_tree)
  {
    if (!(element = tree_insert(&tree, (void*) &num, 0, tree.custom_arg)))
    {
      room_in_tree = 0;    // Remove tree, out of RAM ?
      delete_tree(&tree);
    }
    /*
      if element->count == 1, this element can be found only once from tree
      if element->count == 2, or more, this element is already in tree
    */
    else if (element->count == 1 && (tree_elements++) >= pc->max_tree_elements)
    {
      room_in_tree = 0;  // Remove tree, too many elements
      delete_tree(&tree);
    }
  }

  if (!found)
  {
    found = 1;
    min_arg = max_arg = sum = num;
    sum_sqr = num * num;
    min_length = max_length = length;
  }
  else if (num != 0)
  {
    sum += num;
    sum_sqr += num * num;
    if (length < min_length)
      min_length = length;
    if (length > max_length)
      max_length = length;
    if (compare_ulonglong((ulonglong*) &num, &min_arg) < 0)
      min_arg = num;
    if (compare_ulonglong((ulonglong*) &num, &max_arg) > 0)
      max_arg = num;
  }
} // field_ulonglong::add


bool Query_result_analyse::send_data(List<Item> & /* field_list */)
{
  field_info **f = f_info;

  rows++;

  for (;f != f_end; f++)
  {
    (*f)->add();
  }
  return false;
}


bool Query_result_analyse::send_eof()
{
  field_info **f = f_info;
  char buff[MAX_FIELD_WIDTH];
  String *res, s_min(buff, sizeof(buff),&my_charset_bin), 
	 s_max(buff, sizeof(buff),&my_charset_bin),
	 ans(buff, sizeof(buff),&my_charset_bin);

  if (rows == 0) // for backward compatibility
    goto ok;

  for (; f != f_end; f++)
  {
    func_items[0]->set((*f)->item->full_name());
    if (!(*f)->found)
    {
      func_items[1]->null_value = 1;
      func_items[2]->null_value = 1;
    }
    else
    {
      func_items[1]->null_value = 0;
      res = (*f)->get_min_arg(&s_min);
      func_items[1]->set(res->ptr(), res->length(), res->charset());
      func_items[2]->null_value = 0;
      res = (*f)->get_max_arg(&s_max);
      func_items[2]->set(res->ptr(), res->length(), res->charset());
    }
    func_items[3]->set((longlong) (*f)->min_length);
    func_items[4]->set((longlong) (*f)->max_length);
    func_items[5]->set((longlong) (*f)->empty);
    func_items[6]->set((longlong) (*f)->nulls);
    res = (*f)->avg(&s_max, rows);
    func_items[7]->set(res->ptr(), res->length(), res->charset());
    func_items[8]->null_value = 0;
    res = (*f)->std(&s_max, rows);
    if (!res)
      func_items[8]->null_value = 1;
    else
      func_items[8]->set(res->ptr(), res->length(), res->charset());
    /*
      count the dots, quotas, etc. in (ENUM("a","b","c"...))
      If tree has been removed, don't suggest ENUM.
      treemem is used to measure the size of tree for strings,
      tree_elements is used to count the elements
      max_treemem tells how long the string starting from ENUM("... and
      ending to ..") shall at maximum be. If case is about numbers,
      max_tree_elements will tell the length of the above, now
      every number is considered as length 1
    */
    if (((*f)->treemem || (*f)->tree_elements) &&
	(*f)->tree.elements_in_tree &&
	(((*f)->treemem ? max_treemem : max_tree_elements) >
	 (((*f)->treemem ? (*f)->treemem : (*f)->tree_elements) +
	   ((*f)->tree.elements_in_tree * 3 - 1 + 6))))
    {
      char tmp[331]; //331, because one double prec. num. can be this long
      String tmp_str(tmp, sizeof(tmp),&my_charset_bin);
      TREE_INFO tree_info;

      tree_info.str = &tmp_str;
      tree_info.found = 0;
      tree_info.item = (*f)->item;

      tmp_str.set(STRING_WITH_LEN("ENUM("),&my_charset_bin);
      tree_walk(&(*f)->tree, (*f)->collect_enum(), (char*) &tree_info,
		left_root_right);
      tmp_str.append(')');

      if (!(*f)->nulls)
	tmp_str.append(STRING_WITH_LEN(" NOT NULL"));
      output_str_length = tmp_str.length();
      func_items[9]->set(tmp_str.ptr(), tmp_str.length(), tmp_str.charset());
      if (result->send_data(result_fields))
	goto error;
      continue;
    }

    ans.length(0);
    if (!(*f)->treemem && !(*f)->tree_elements)
      ans.append(STRING_WITH_LEN("CHAR(0)"));
    else if ((*f)->item->type() == Item::FIELD_ITEM)
    {
      switch (((Item_field*) (*f)->item)->field->real_type())
      {
      case MYSQL_TYPE_TIMESTAMP:
	ans.append(STRING_WITH_LEN("TIMESTAMP"));
	break;
      case MYSQL_TYPE_DATETIME:
	ans.append(STRING_WITH_LEN("DATETIME"));
	break;
      case MYSQL_TYPE_DATE:
      case MYSQL_TYPE_NEWDATE:
	ans.append(STRING_WITH_LEN("DATE"));
	break;
      case MYSQL_TYPE_SET:
	ans.append(STRING_WITH_LEN("SET"));
	break;
      case MYSQL_TYPE_YEAR:
	ans.append(STRING_WITH_LEN("YEAR"));
	break;
      case MYSQL_TYPE_TIME:
	ans.append(STRING_WITH_LEN("TIME"));
	break;
      case MYSQL_TYPE_DECIMAL:
	ans.append(STRING_WITH_LEN("DECIMAL"));
	// if item is FIELD_ITEM, it _must_be_ Field_num in this case
	if (((Field_num*) ((Item_field*) (*f)->item)->field)->zerofill)
	  ans.append(STRING_WITH_LEN(" ZEROFILL"));
	break;
      default:
	(*f)->get_opt_type(&ans, rows);
	break;
      }
    }
    if (!(*f)->nulls)
      ans.append(STRING_WITH_LEN(" NOT NULL"));
    func_items[9]->set(ans.ptr(), ans.length(), ans.charset());
    if (result->send_data(result_fields))
      goto error;
  }
ok:
  return result->send_eof();
error:
  abort_result_set();
  return true;
} // Query_result_analyse::send_eof


void field_str::get_opt_type(String *answer, ha_rows total_rows)
{
  char buff[MAX_FIELD_WIDTH];

  if (can_be_still_num)
  {
    if (num_info.is_float)
      sprintf(buff, "DOUBLE");	  // number was like 1e+50... TODO:
    else if (num_info.decimals) // DOUBLE(%d,%d) sometime
    {
      if (num_info.dval > -FLT_MAX && num_info.dval < FLT_MAX)
	sprintf(buff, "FLOAT(%d,%d)", (num_info.integers + num_info.decimals), num_info.decimals);
      else
	sprintf(buff, "DOUBLE(%d,%d)", (num_info.integers + num_info.decimals), num_info.decimals);
    }
    else if (ev_num_info.llval >= -128 &&
	     ev_num_info.ullval <=
	     (ulonglong) (ev_num_info.llval >= 0 ? 255 : 127))
      sprintf(buff, "TINYINT(%d)", num_info.integers);
    else if (ev_num_info.llval >= INT_MIN16 &&
	     ev_num_info.ullval <= (ulonglong) (ev_num_info.llval >= 0 ?
						UINT_MAX16 : INT_MAX16))
      sprintf(buff, "SMALLINT(%d)", num_info.integers);
    else if (ev_num_info.llval >= INT_MIN24 &&
	     ev_num_info.ullval <= (ulonglong) (ev_num_info.llval >= 0 ?
						UINT_MAX24 : INT_MAX24))
      sprintf(buff, "MEDIUMINT(%d)", num_info.integers);
    else if (ev_num_info.llval >= INT_MIN32 &&
	     ev_num_info.ullval <= (ulonglong) (ev_num_info.llval >= 0 ?
						UINT_MAX32 : INT_MAX32))
      sprintf(buff, "INT(%d)", num_info.integers);
    else
      sprintf(buff, "BIGINT(%d)", num_info.integers);
    answer->append(buff, strlen(buff));
    if (ev_num_info.llval >= 0 && ev_num_info.min_dval >= 0)
      answer->append(STRING_WITH_LEN(" UNSIGNED"));
    if (num_info.zerofill)
      answer->append(STRING_WITH_LEN(" ZEROFILL"));
  }
  else if (max_length < 256)
  {
    if (must_be_blob)
    {
      if (item->collation.collation == &my_charset_bin)
	answer->append(STRING_WITH_LEN("TINYBLOB"));
      else
	answer->append(STRING_WITH_LEN("TINYTEXT"));
    }
    else if ((max_length * (total_rows - nulls)) < (sum + total_rows))
    {
      sprintf(buff, "CHAR(%d)", (int) max_length);
      answer->append(buff, strlen(buff));
    }
    else
    {
      sprintf(buff, "VARCHAR(%d)", (int) max_length);
      answer->append(buff, strlen(buff));
    }
  }
  else if (max_length < (1L << 16))
  {
    if (item->collation.collation == &my_charset_bin)
      answer->append(STRING_WITH_LEN("BLOB"));
    else
      answer->append(STRING_WITH_LEN("TEXT"));
  }
  else if (max_length < (1L << 24))
  {
    if (item->collation.collation == &my_charset_bin)
      answer->append(STRING_WITH_LEN("MEDIUMBLOB"));
    else
      answer->append(STRING_WITH_LEN("MEDIUMTEXT"));
  }
  else
  {
    if (item->collation.collation == &my_charset_bin)
      answer->append(STRING_WITH_LEN("LONGBLOB"));
    else
      answer->append(STRING_WITH_LEN("LONGTEXT"));
  }
} // field_str::get_opt_type


void field_real::get_opt_type(String *answer,
			      ha_rows total_rows MY_ATTRIBUTE((unused)))
{
  char buff[MAX_FIELD_WIDTH];

  if (!max_notzero_dec_len)
  {
    int len= (int) max_length - ((item->decimals == NOT_FIXED_DEC) ?
				 0 : (item->decimals + 1));

    if (min_arg >= -128 && max_arg <= (min_arg >= 0 ? 255 : 127))
      sprintf(buff, "TINYINT(%d)", len);
    else if (min_arg >= INT_MIN16 && max_arg <= (min_arg >= 0 ?
						 UINT_MAX16 : INT_MAX16))
      sprintf(buff, "SMALLINT(%d)", len);
    else if (min_arg >= INT_MIN24 && max_arg <= (min_arg >= 0 ?
						 UINT_MAX24 : INT_MAX24))
      sprintf(buff, "MEDIUMINT(%d)", len);
    else if (min_arg >= INT_MIN32 && max_arg <= (min_arg >= 0 ?
						 UINT_MAX32 : INT_MAX32))
      sprintf(buff, "INT(%d)", len);
    else
      sprintf(buff, "BIGINT(%d)", len);
    answer->append(buff, strlen(buff));
    if (min_arg >= 0)
      answer->append(STRING_WITH_LEN(" UNSIGNED"));
  }
  else if (item->decimals == NOT_FIXED_DEC)
  {
    if (min_arg >= -FLT_MAX && max_arg <= FLT_MAX)
      answer->append(STRING_WITH_LEN("FLOAT"));
    else
      answer->append(STRING_WITH_LEN("DOUBLE"));
  }
  else
  {
    if (min_arg >= -FLT_MAX && max_arg <= FLT_MAX)
      sprintf(buff, "FLOAT(%d,%d)", (int) max_length - (item->decimals + 1) + max_notzero_dec_len,
	      max_notzero_dec_len);
    else
      sprintf(buff, "DOUBLE(%d,%d)", (int) max_length - (item->decimals + 1) + max_notzero_dec_len,
	      max_notzero_dec_len);
    answer->append(buff, strlen(buff));
  }
  // if item is FIELD_ITEM, it _must_be_ Field_num in this class
  if (item->type() == Item::FIELD_ITEM &&
      // a single number shouldn't be zerofill
      (max_length - (item->decimals + 1)) != 1 &&
      ((Field_num*) ((Item_field*) item)->field)->zerofill)
    answer->append(STRING_WITH_LEN(" ZEROFILL"));
} // field_real::get_opt_type


void field_longlong::get_opt_type(String *answer,
				  ha_rows total_rows MY_ATTRIBUTE((unused)))
{
  char buff[MAX_FIELD_WIDTH];

  if (min_arg >= -128 && max_arg <= (min_arg >= 0 ? 255 : 127))
    sprintf(buff, "TINYINT(%d)", (int) max_length);
  else if (min_arg >= INT_MIN16 && max_arg <= (min_arg >= 0 ?
					       UINT_MAX16 : INT_MAX16))
    sprintf(buff, "SMALLINT(%d)", (int) max_length);
  else if (min_arg >= INT_MIN24 && max_arg <= (min_arg >= 0 ?
					       UINT_MAX24 : INT_MAX24))
    sprintf(buff, "MEDIUMINT(%d)", (int) max_length);
  else if (min_arg >= INT_MIN32 && max_arg <= (min_arg >= 0 ?
					       UINT_MAX32 : INT_MAX32))
    sprintf(buff, "INT(%d)", (int) max_length);
  else
    sprintf(buff, "BIGINT(%d)", (int) max_length);
  answer->append(buff, strlen(buff));
  if (min_arg >= 0)
    answer->append(STRING_WITH_LEN(" UNSIGNED"));

  // if item is FIELD_ITEM, it _must_be_ Field_num in this class
  if ((item->type() == Item::FIELD_ITEM) &&
      // a single number shouldn't be zerofill
      max_length != 1 &&
      ((Field_num*) ((Item_field*) item)->field)->zerofill)
    answer->append(STRING_WITH_LEN(" ZEROFILL"));
} // field_longlong::get_opt_type


void field_ulonglong::get_opt_type(String *answer,
				   ha_rows total_rows MY_ATTRIBUTE((unused)))
{
  char buff[MAX_FIELD_WIDTH];

  if (max_arg < 256)
    sprintf(buff, "TINYINT(%d) UNSIGNED", (int) max_length);
   else if (max_arg <= ((2 * INT_MAX16) + 1))
     sprintf(buff, "SMALLINT(%d) UNSIGNED", (int) max_length);
  else if (max_arg <= ((2 * INT_MAX24) + 1))
    sprintf(buff, "MEDIUMINT(%d) UNSIGNED", (int) max_length);
  else if (max_arg < (((ulonglong) 1) << 32))
    sprintf(buff, "INT(%d) UNSIGNED", (int) max_length);
  else
    sprintf(buff, "BIGINT(%d) UNSIGNED", (int) max_length);
  // if item is FIELD_ITEM, it _must_be_ Field_num in this class
  answer->append(buff, strlen(buff));
  if (item->type() == Item::FIELD_ITEM &&
      // a single number shouldn't be zerofill
      max_length != 1 &&
      ((Field_num*) ((Item_field*) item)->field)->zerofill)
    answer->append(STRING_WITH_LEN(" ZEROFILL"));
} //field_ulonglong::get_opt_type


void field_decimal::get_opt_type(String *answer,
                                 ha_rows total_rows MY_ATTRIBUTE((unused)))
{
  my_decimal zero;
  char buff[MAX_FIELD_WIDTH];
  size_t length;

  my_decimal_set_zero(&zero);
  my_bool is_unsigned= (my_decimal_cmp(&zero, &min_arg) >= 0);

  length= my_snprintf(buff, sizeof(buff), "DECIMAL(%d, %d)",
                      static_cast<int>(max_length - (item->decimals ? 1 : 0)),
                      static_cast<int>(item->decimals));
  if (is_unsigned)
    length= (my_stpcpy(buff+length, " UNSIGNED")- buff);
  answer->append(buff, length);
}


String *field_decimal::get_min_arg(String *str)
{
  my_decimal2string(E_DEC_FATAL_ERROR, &min_arg, 0, 0, '0', str);
  return str;
}


String *field_decimal::get_max_arg(String *str)
{
  my_decimal2string(E_DEC_FATAL_ERROR, &max_arg, 0, 0, '0', str);
  return str;
}


String *field_decimal::avg(String *s, ha_rows rows)
{
  if (!(rows - nulls))
  {
    s->set_real(0.0, 1,my_thd_charset);
    return s;
  }
  my_decimal num, avg_val, rounded_avg;
  int prec_increment= current_thd->variables.div_precincrement;

  int2my_decimal(E_DEC_FATAL_ERROR, rows - nulls, FALSE, &num);
  my_decimal_div(E_DEC_FATAL_ERROR, &avg_val, sum+cur_sum, &num, prec_increment);
  /* TODO remove this after decimal_div returns proper frac */
  my_decimal_round(E_DEC_FATAL_ERROR, &avg_val,
                   min(sum[cur_sum].frac + prec_increment, DECIMAL_MAX_SCALE),
                   FALSE,&rounded_avg);
  my_decimal2string(E_DEC_FATAL_ERROR, &rounded_avg, 0, 0, '0', s);
  return s;
}


String *field_decimal::std(String *s, ha_rows rows)
{
  if (!(rows - nulls))
  {
    s->set_real(0.0, 1,my_thd_charset);
    return s;
  }
  my_decimal num, tmp, sum2, sum2d;
  double std_sqr;
  int prec_increment= current_thd->variables.div_precincrement;

  int2my_decimal(E_DEC_FATAL_ERROR, rows - nulls, FALSE, &num);
  my_decimal_mul(E_DEC_FATAL_ERROR, &sum2, sum+cur_sum, sum+cur_sum);
  my_decimal_div(E_DEC_FATAL_ERROR, &tmp, &sum2, &num, prec_increment);
  my_decimal_sub(E_DEC_FATAL_ERROR, &sum2, sum_sqr+cur_sum, &tmp);
  my_decimal_div(E_DEC_FATAL_ERROR, &tmp, &sum2, &num, prec_increment);
  my_decimal2double(E_DEC_FATAL_ERROR, &tmp, &std_sqr);
  s->set_real((std_sqr <= 0.0 ? 0.0 : sqrt(std_sqr)),
         min(item->decimals + prec_increment, NOT_FIXED_DEC), my_thd_charset);

  return s;
}


int collect_string(String *element,
		   element_count count MY_ATTRIBUTE((unused)),
		   TREE_INFO *info)
{
  if (info->found)
    info->str->append(',');
  else
    info->found = 1;
  info->str->append('\'');
  if (append_escaped(info->str, element))
    return 1;
  info->str->append('\'');
  return 0;
} // collect_string


int collect_real(double *element, element_count count MY_ATTRIBUTE((unused)),
		 TREE_INFO *info)
{
  char buff[MAX_FIELD_WIDTH];
  String s(buff, sizeof(buff),current_thd->charset());

  if (info->found)
    info->str->append(',');
  else
    info->found = 1;
  info->str->append('\'');
  s.set_real(*element, info->item->decimals, current_thd->charset());
  info->str->append(s);
  info->str->append('\'');
  return 0;
} // collect_real


int collect_decimal(uchar *element, element_count count,
                    TREE_INFO *info)
{
  char buff[DECIMAL_MAX_STR_LENGTH + 1];
  String s(buff, sizeof(buff),&my_charset_bin);

  if (info->found)
    info->str->append(',');
  else
    info->found = 1;
  my_decimal dec;
  binary2my_decimal(E_DEC_FATAL_ERROR, element, &dec,
                    info->item->max_length, info->item->decimals);
  
  info->str->append('\'');
  my_decimal2string(E_DEC_FATAL_ERROR, &dec, 0, 0, '0', &s);
  info->str->append(s);
  info->str->append('\'');
  return 0;
}


int collect_longlong(longlong *element,
		     element_count count MY_ATTRIBUTE((unused)),
		     TREE_INFO *info)
{
  char buff[MAX_FIELD_WIDTH];
  String s(buff, sizeof(buff),&my_charset_bin);

  if (info->found)
    info->str->append(',');
  else
    info->found = 1;
  info->str->append('\'');
  s.set(*element, current_thd->charset());
  info->str->append(s);
  info->str->append('\'');
  return 0;
} // collect_longlong


int collect_ulonglong(ulonglong *element,
		      element_count count MY_ATTRIBUTE((unused)),
		      TREE_INFO *info)
{
  char buff[MAX_FIELD_WIDTH];
  String s(buff, sizeof(buff),&my_charset_bin);

  if (info->found)
    info->str->append(',');
  else
    info->found = 1;
  info->str->append('\'');
  s.set(*element, current_thd->charset());
  info->str->append(s);
  info->str->append('\'');
  return 0;
} // collect_ulonglong


/**
  Create items for substituted output columns (both metadata and data)
*/
bool Query_result_analyse::change_columns()
{
  func_items[0] = new Item_proc_string("Field_name", 255);
  func_items[1] = new Item_proc_string("Min_value", 255);
  func_items[1]->maybe_null = 1;
  func_items[2] = new Item_proc_string("Max_value", 255);
  func_items[2]->maybe_null = 1;
  func_items[3] = new Item_proc_int("Min_length");
  func_items[4] = new Item_proc_int("Max_length");
  func_items[5] = new Item_proc_int("Empties_or_zeros");
  func_items[6] = new Item_proc_int("Nulls");
  func_items[7] = new Item_proc_string("Avg_value_or_avg_length", 255);
  func_items[8] = new Item_proc_string("Std", 255);
  func_items[8]->maybe_null = 1;
  func_items[9] = new Item_proc_string("Optimal_fieldtype",
				       max<size_t>(64U, output_str_length));
  result_fields.empty();
  for (uint i = 0; i < array_elements(func_items); i++)
  {
    if (func_items[i] == NULL)
      return true;
    result_fields.push_back(func_items[i]);
  }
  return false;
} // Query_result_analyse::change_columns


void Query_result_analyse::cleanup()
{
  if (f_info)
  {
    for (field_info **f= f_info; f != f_end; f++)
      delete (*f);
    f_info= f_end= NULL;
  }
  rows= 0;
  output_str_length= 0;
}


bool Query_result_analyse::send_result_set_metadata(List<Item> &fields,
                                                    uint flag)
{
  return (init(fields) || change_columns() ||
	  result->send_result_set_metadata(result_fields, flag));
}


void Query_result_analyse::abort_result_set()
{
  cleanup();
  return result->abort_result_set();
}


int compare_double(const double *s, const double *t)
{
  return ((*s < *t) ? -1 : *s > *t ? 1 : 0);
} /* compare_double */

int compare_longlong(const longlong *s, const longlong *t)
{
  return ((*s < *t) ? -1 : *s > *t ? 1 : 0);
} /* compare_longlong */

 int compare_ulonglong(const ulonglong *s, const ulonglong *t)
{
  return ((*s < *t) ? -1 : *s > *t ? 1 : 0);
} /* compare_ulonglong */


uint check_ulonglong(const char *str, uint length)
{
  const char *long_str = "2147483647", *ulonglong_str = "18446744073709551615";
  const uint long_len = 10, ulonglong_len = 20;

  while (*str == '0' && length)
  {
    str++; length--;
  }
  if (length < long_len)
    return NUM;

  uint smaller, bigger;
  const char *cmp;

  if (length == long_len)
  {
    cmp = long_str;
    smaller = NUM;
    bigger = LONG_NUM;
  }
  else if (length > ulonglong_len)
    return DECIMAL_NUM;
  else
  {
    cmp = ulonglong_str;
    smaller = LONG_NUM;
    bigger = DECIMAL_NUM;
  }
  while (*cmp && *cmp++ == *str++) ;
  return ((uchar) str[-1] <= (uchar) cmp[-1]) ? smaller : bigger;
} /* check_ulonlong */


/*
  Quote special characters in a string.

  SYNOPSIS
   append_escaped(to_str, from_str)
   to_str (in) A pointer to a String.
   from_str (to) A pointer to an allocated string

  DESCRIPTION
    append_escaped() takes a String type variable, where it appends
    escaped the second argument. Only characters that require escaping
    will be escaped.

  RETURN VALUES
    0 Success
    1 Out of memory
*/

bool append_escaped(String *to_str, String *from_str)
{
  char *from, *end, c;

  if (to_str->mem_realloc(to_str->length() + from_str->length()))
    return 1;

  from= (char*) from_str->ptr();
  end= from + from_str->length();
  for (; from < end; from++)
  {
    c= *from;
    switch (c) {
    case '\0':
      c= '0';
      break;
    case '\032':
      c= 'Z';
      break;
    case '\\':
    case '\'':
      break;
    default:
      goto normal_character;
    }
    if (to_str->append('\\'))
      return 1;

  normal_character:
    if (to_str->append(c))
      return 1;
  }
  return 0;
}

