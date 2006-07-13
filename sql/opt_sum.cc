/* Copyright (C) 2000-2003 MySQL AB

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


/*
  Optimising of MIN(), MAX() and COUNT(*) queries without 'group by' clause
  by replacing the aggregate expression with a constant.  

  Given a table with a compound key on columns (a,b,c), the following
  types of queries are optimised (assuming the table handler supports
  the required methods)

  SELECT COUNT(*) FROM t1[,t2,t3,...]
  SELECT MIN(b) FROM t1 WHERE a=const
  SELECT MAX(c) FROM t1 WHERE a=const AND b=const
  SELECT MAX(b) FROM t1 WHERE a=const AND b<const
  SELECT MIN(b) FROM t1 WHERE a=const AND b>const
  SELECT MIN(b) FROM t1 WHERE a=const AND b BETWEEN const AND const
  SELECT MAX(b) FROM t1 WHERE a=const AND b BETWEEN const AND const

  Instead of '<' one can use '<=', '>', '>=' and '=' as well.
  Instead of 'a=const' the condition 'a IS NULL' can be used.

  If all selected fields are replaced then we will also remove all
  involved tables and return the answer without any join. Thus, the
  following query will be replaced with a row of two constants:
  SELECT MAX(b), MIN(d) FROM t1,t2 
    WHERE a=const AND b<const AND d>const
  (assuming a index for column d of table t2 is defined)

*/

#include "mysql_priv.h"
#include "sql_select.h"

static bool find_key_for_maxmin(bool max_fl, TABLE_REF *ref, Field* field,
                                COND *cond, uint *range_fl,
                                uint *key_prefix_length);
static int reckey_in_range(bool max_fl, TABLE_REF *ref, Field* field,
                            COND *cond, uint range_fl, uint prefix_len);
static int maxmin_in_range(bool max_fl, Field* field, COND *cond);


/*
  Substitutes constants for some COUNT(), MIN() and MAX() functions.

  SYNOPSIS
    opt_sum_query()
    tables               Tables in query
    all_fields           All fields to be returned
    conds                WHERE clause

  NOTE:
    This function is only called for queries with sum functions and no
    GROUP BY part.

  RETURN VALUES
    0 No errors
    1 if all items were resolved
   -1 on impossible conditions
    OR an error number from my_base.h HA_ERR_... if a deadlock or a lock
       wait timeout happens, for example
*/

int opt_sum_query(TABLE_LIST *tables, List<Item> &all_fields,COND *conds)
{
  List_iterator_fast<Item> it(all_fields);
  int const_result= 1;
  bool recalc_const_item= 0;
  longlong count= 1;
  bool is_exact_count= TRUE;
  table_map removed_tables= 0, outer_tables= 0, used_tables= 0;
  table_map where_tables= 0;
  Item *item;
  int error;

  if (conds)
    where_tables= conds->used_tables();

  /*
    Analyze outer join dependencies, and, if possible, compute the number
    of returned rows.
  */
  for (TABLE_LIST *tl=tables; tl ; tl= tl->next)
  {
    /* Don't replace expression on a table that is part of an outer join */
    if (tl->on_expr)
    {
      outer_tables|= tl->table->map;

      /*
        We can't optimise LEFT JOIN in cases where the WHERE condition
        restricts the table that is used, like in:
          SELECT MAX(t1.a) FROM t1 LEFT JOIN t2 join-condition
          WHERE t2.field IS NULL;
      */
      if (tl->table->map & where_tables)
        return 0;
    }
    else
      used_tables|= tl->table->map;

    /*
      If the storage manager of 'tl' gives exact row count, compute the total
      number of rows. If there are no outer table dependencies, this count
      may be used as the real count.
    */
    if (tl->table->file->table_flags() & HA_NOT_EXACT_COUNT)
    {
      is_exact_count= FALSE;
      count= 1;                                 // ensure count != 0
    }
    else
    {
      tl->table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK);
      count*= tl->table->file->records;
    }
  }

  /*
    Iterate through all items in the SELECT clause and replace
    COUNT(), MIN() and MAX() with constants (if possible).
  */

  while ((item= it++))
  {
    if (item->type() == Item::SUM_FUNC_ITEM)
    {
      Item_sum *item_sum= (((Item_sum*) item));
      switch (item_sum->sum_func()) {
      case Item_sum::COUNT_FUNC:
        /*
          If the expr in count(expr) can never be null we can change this
          to the number of rows in the tables if this number is exact and
          there are no outer joins.
        */
        if (!conds && !((Item_sum_count*) item)->args[0]->maybe_null &&
            !outer_tables && is_exact_count)
        {
          ((Item_sum_count*) item)->make_const(count);
          recalc_const_item= 1;
        }
        else
          const_result= 0;
        break;
      case Item_sum::MIN_FUNC:
      {
        /*
          If MIN(expr) is the first part of a key or if all previous
          parts of the key is found in the COND, then we can use
          indexes to find the key.
        */
        Item *expr=item_sum->args[0];
        if (expr->type() == Item::FIELD_ITEM)
        {
          byte key_buff[MAX_KEY_LENGTH];
          TABLE_REF ref;
          uint range_fl, prefix_len;

          ref.key_buff= key_buff;
          Item_field *item_field= ((Item_field*) expr);
          TABLE *table= item_field->field->table;

          /* 
            Look for a partial key that can be used for optimization.
            If we succeed, ref.key_length will contain the length of
            this key, while prefix_len will contain the length of 
            the beginning of this key without field used in MIN(). 
            Type of range for the key part for this field will be
            returned in range_fl.
          */
          if ((outer_tables & table->map) ||
              !find_key_for_maxmin(0, &ref, item_field->field, conds,
                                   &range_fl, &prefix_len))
          {
            const_result= 0;
            break;
          }
          error= table->file->ha_index_init((uint) ref.key);

          if (!ref.key_length)
            error= table->file->index_first(table->record[0]);
          else
	    error= table->file->index_read(table->record[0],key_buff,
					   ref.key_length,
					   range_fl & NEAR_MIN ?
					   HA_READ_AFTER_KEY :
					   HA_READ_KEY_OR_NEXT);
	  if (!error && reckey_in_range(0, &ref, item_field->field, 
			                conds, range_fl, prefix_len))
	    error= HA_ERR_KEY_NOT_FOUND;
          if (table->key_read)
          {
            table->key_read= 0;
            table->file->extra(HA_EXTRA_NO_KEYREAD);
          }
          table->file->ha_index_end();
          if (error)
	  {
	    if (error == HA_ERR_KEY_NOT_FOUND || error == HA_ERR_END_OF_FILE)
	      return -1;		       // No rows matching WHERE
	    /* HA_ERR_LOCK_DEADLOCK or some other error */
 	    table->file->print_error(error, MYF(0));
            return(error);
	  }
          removed_tables|= table->map;
        }
        else if (!expr->const_item() || !is_exact_count)
        {
          /*
            The optimization is not applicable in both cases:
            (a) 'expr' is a non-constant expression. Then we can't
            replace 'expr' by a constant.
            (b) 'expr' is a costant. According to ANSI, MIN/MAX must return
            NULL if the query does not return any rows. Thus, if we are not
            able to determine if the query returns any rows, we can't apply
            the optimization and replace MIN/MAX with a constant.
          */
          const_result= 0;
          break;
        }
        if (!count)
        {
          /* If count == 0, then we know that is_exact_count == TRUE. */
          ((Item_sum_min*) item_sum)->clear(); /* Set to NULL. */
        }
        else
          ((Item_sum_min*) item_sum)->reset(); /* Set to the constant value. */
        ((Item_sum_min*) item_sum)->make_const();
        recalc_const_item= 1;
        break;
      }
      case Item_sum::MAX_FUNC:
      {
        /*
          If MAX(expr) is the first part of a key or if all previous
          parts of the key is found in the COND, then we can use
          indexes to find the key.
        */
        Item *expr=item_sum->args[0];
        if (expr->type() == Item::FIELD_ITEM)
        {
          byte key_buff[MAX_KEY_LENGTH];
          TABLE_REF ref;
	      uint range_fl, prefix_len;

          ref.key_buff= key_buff;
	      Item_field *item_field= ((Item_field*) expr);
          TABLE *table= item_field->field->table;

          /* 
            Look for a partial key that can be used for optimization.
            If we succeed, ref.key_length will contain the length of
            this key, while prefix_len will contain the length of 
            the beginning of this key without field used in MAX().
            Type of range for the key part for this field will be
            returned in range_fl.
          */
          if ((outer_tables & table->map) ||
	          !find_key_for_maxmin(1, &ref, item_field->field, conds,
				                   &range_fl, &prefix_len))
          {
            const_result= 0;
            break;
          }
          error= table->file->ha_index_init((uint) ref.key);

          if (!ref.key_length)
            error= table->file->index_last(table->record[0]);
          else
	    error= table->file->index_read(table->record[0], key_buff,
					   ref.key_length,
					   range_fl & NEAR_MAX ?
					   HA_READ_BEFORE_KEY :
					   HA_READ_PREFIX_LAST_OR_PREV);
	  if (!error && reckey_in_range(1, &ref, item_field->field, 
			                conds, range_fl, prefix_len))
	    error= HA_ERR_KEY_NOT_FOUND;
          if (table->key_read)
          {
            table->key_read=0;
            table->file->extra(HA_EXTRA_NO_KEYREAD);
          }
          table->file->ha_index_end();
          if (error)
          {
	    if (error == HA_ERR_KEY_NOT_FOUND || error == HA_ERR_END_OF_FILE)
	      return -1;		       // No rows matching WHERE
	    /* HA_ERR_LOCK_DEADLOCK or some other error */
 	    table->file->print_error(error, MYF(0));
            return(error);
	  }
          removed_tables|= table->map;
        }
        else if (!expr->const_item() || !is_exact_count)
        {
          /*
            The optimization is not applicable in both cases:
            (a) 'expr' is a non-constant expression. Then we can't
            replace 'expr' by a constant.
            (b) 'expr' is a costant. According to ANSI, MIN/MAX must return
            NULL if the query does not return any rows. Thus, if we are not
            able to determine if the query returns any rows, we can't apply
            the optimization and replace MIN/MAX with a constant.
          */
          const_result= 0;
          break;
        }
        if (!count)
        {
          /* If count != 1, then we know that is_exact_count == TRUE. */
          ((Item_sum_max*) item_sum)->clear(); /* Set to NULL. */
        }
        else
          ((Item_sum_max*) item_sum)->reset(); /* Set to the constant value. */
        ((Item_sum_max*) item_sum)->make_const();
        recalc_const_item= 1;
        break;
      }
      default:
        const_result= 0;
        break;
      }
    }
    else if (const_result)
    {
      if (recalc_const_item)
        item->update_used_tables();
      if (!item->const_item())
        const_result= 0;
    }
  }
  /*
    If we have a where clause, we can only ignore searching in the
    tables if MIN/MAX optimisation replaced all used tables
    We do not use replaced values in case of:
    SELECT MIN(key) FROM table_1, empty_table
    removed_tables is != 0 if we have used MIN() or MAX().
  */
  if (removed_tables && used_tables != removed_tables)
    const_result= 0;                                // We didn't remove all tables
  return const_result;
}


/*
  Test if the predicate compares a field with constants

  SYNOPSIS
    simple_pred()
    func_item   in:  Predicate item
    args        out: Here we store the field followed by constants
    inv_order   out: Is set to 1 if the predicate is of the form 'const op field' 

  RETURN
    0        func_item is a simple predicate: a field is compared with constants
    1        Otherwise
*/

static bool simple_pred(Item_func *func_item, Item **args, bool *inv_order)
{
  Item *item;
  *inv_order= 0;
  switch (func_item->argument_count()) {
  case 1:
    /* field IS NULL */
    item= func_item->arguments()[0];
    if (item->type() != Item::FIELD_ITEM)
      return 0;
    args[0]= item;
    break;
  case 2:
    /* 'field op const' or 'const op field' */
    item= func_item->arguments()[0];
    if (item->type() == Item::FIELD_ITEM)
    {
      args[0]= item;
      item= func_item->arguments()[1];
      if (!item->const_item())
        return 0;
      args[1]= item;
    }
    else if (item->const_item())
    {
      args[1]= item;
      item= func_item->arguments()[1];
      if (item->type() != Item::FIELD_ITEM)
        return 0;
      args[0]= item;
      *inv_order= 1;
    }
    else
      return 0;
    break;
  case 3:
    /* field BETWEEN const AND const */
    item= func_item->arguments()[0];
    if (item->type() == Item::FIELD_ITEM)
    {
      args[0]= item;
      for (int i= 1 ; i <= 2; i++)
      {
        item= func_item->arguments()[i];
        if (!item->const_item())
          return 0;
        args[i]= item;
      }
    }
    else
      return 0;
  }
  return 1;
}


/* 
   Check whether a condition matches a key to get {MAX|MIN}(field):

   SYNOPSIS
     matching_cond()
     max_fl         in:     Set to 1 if we are optimising MAX()              
     ref            in/out: Reference to the structure we store the key value
     keyinfo        in      Reference to the key info
     field_part     in:     Pointer to the key part for the field
     cond           in      WHERE condition
     key_part_used  in/out: Map of matchings parts
     range_fl       in/out: Says whether including key will be used
     prefix_len     out:    Length of common key part for the range
                            where MAX/MIN is searched for

   DESCRIPTION
     For the index specified by the keyinfo parameter, index that
     contains field as its component (field_part), the function
     checks whether the condition cond is a conjunction and all its
     conjuncts referring to the columns of the same table as column
     field are one of the following forms:
     - f_i= const_i or const_i= f_i or f_i is null,
       where f_i is part of the index
     - field {<|<=|>=|>|=} const or const {<|<=|>=|>|=} field
     - field between const1 and const2

  RETURN
    0        Index can't be used.
    1        We can use index to get MIN/MAX value
*/

static bool matching_cond(bool max_fl, TABLE_REF *ref, KEY *keyinfo, 
                          KEY_PART_INFO *field_part, COND *cond,
                          key_part_map *key_part_used, uint *range_fl,
                          uint *prefix_len)
{
  if (!cond)
    return 1;
  Field *field= field_part->field;
  if (!(cond->used_tables() & field->table->map))
  {
    /* Condition doesn't restrict the used table */
    return 1;
  }
  if (cond->type() == Item::COND_ITEM)
  {
    if (((Item_cond*) cond)->functype() == Item_func::COND_OR_FUNC)
      return 0;

    /* AND */
    List_iterator_fast<Item> li(*((Item_cond*) cond)->argument_list());
    Item *item;
    while ((item= li++))
    {
      if (!matching_cond(max_fl, ref, keyinfo, field_part, item,
                         key_part_used, range_fl, prefix_len))
        return 0;
    }
    return 1;
  }

  if (cond->type() != Item::FUNC_ITEM)
    return 0;                                 // Not operator, can't optimize

  bool eq_type= 0;                            // =, <=> or IS NULL
  bool noeq_type= 0;                          // < or >  
  bool less_fl= 0;                            // < or <= 
  bool is_null= 0;
  bool between= 0;

  switch (((Item_func*) cond)->functype()) {
  case Item_func::ISNULL_FUNC:
    is_null= 1;     /* fall through */
  case Item_func::EQ_FUNC:
  case Item_func::EQUAL_FUNC:
    eq_type= 1;
    break;
  case Item_func::LT_FUNC:
    noeq_type= 1;   /* fall through */
  case Item_func::LE_FUNC:
    less_fl= 1;      
    break;
  case Item_func::GT_FUNC:
    noeq_type= 1;   /* fall through */
  case Item_func::GE_FUNC:
    break;
  case Item_func::BETWEEN:
    between= 1;
    break;
  default:
    return 0;                                        // Can't optimize function
  }
  
  Item *args[3];
  bool inv;

  /* Test if this is a comparison of a field and constant */
  if (!simple_pred((Item_func*) cond, args, &inv))
    return 0;

  if (inv && !eq_type)
    less_fl= 1-less_fl;                         // Convert '<' -> '>' (etc)

  /* Check if field is part of the tested partial key */
  byte *key_ptr= ref->key_buff;
  KEY_PART_INFO *part;
  for (part= keyinfo->key_part;
       ;
       key_ptr+= part++->store_length)

  {
    if (part > field_part)
      return 0;                     // Field is beyond the tested parts
    if (part->field->eq(((Item_field*) args[0])->field))
      break;                        // Found a part od the key for the field
  }

  bool is_field_part= part == field_part;
  if (!(is_field_part || eq_type))
    return 0;

  key_part_map org_key_part_used= *key_part_used;
  if (eq_type || between || max_fl == less_fl)
  {
    uint length= (key_ptr-ref->key_buff)+part->store_length;
    if (ref->key_length < length)
    /* Ultimately ref->key_length will contain the length of the search key */
      ref->key_length= length;      
    if (!*prefix_len && part+1 == field_part)       
      *prefix_len= length;
    if (is_field_part && eq_type)
      *prefix_len= ref->key_length;
  
    *key_part_used|= (key_part_map) 1 << (part - keyinfo->key_part);
  }

  if (org_key_part_used != *key_part_used ||
      (is_field_part && 
       (between || eq_type || max_fl == less_fl) && !cond->val_int()))
  {
    /*
      It's the first predicate for this part or a predicate of the
      following form  that moves upper/lower bounds for max/min values:
      - field BETWEEN const AND const
      - field = const 
      - field {<|<=} const, when searching for MAX
      - field {>|>=} const, when searching for MIN
    */

    if (is_null)
    {
      part->field->set_null();
      *key_ptr= (byte) 1;
    }
    else
    {
      store_val_in_field(part->field, args[between && max_fl ? 2 : 1],
                         CHECK_FIELD_IGNORE);
      if (part->null_bit) 
        *key_ptr++= (byte) test(part->field->is_null());
      part->field->get_key_image((char*) key_ptr, part->length,
                                 part->field->charset(), Field::itRAW);
    }
    if (is_field_part)
    {
      if (between || eq_type)
        *range_fl&= ~(NO_MAX_RANGE | NO_MIN_RANGE);
      else
      {
        *range_fl&= ~(max_fl ? NO_MAX_RANGE : NO_MIN_RANGE);
        if (noeq_type)
          *range_fl|=  (max_fl ? NEAR_MAX : NEAR_MIN);
        else
          *range_fl&= ~(max_fl ? NEAR_MAX : NEAR_MIN);
      }
    }
  }
  else if (eq_type)
  {
    if (!is_null && !cond->val_int() ||
        is_null && !test(part->field->is_null()))  
     return 0;                       // Impossible test
  }
  else if (is_field_part)
    *range_fl&= ~(max_fl ? NO_MIN_RANGE : NO_MAX_RANGE);
  return 1;  
}


/*
  Check whether we can get value for {max|min}(field) by using a key.

  SYNOPSIS
    find_key_for_maxmin()
    max_fl      in:     0 for MIN(field) / 1 for MAX(field)
    ref         in/out  Reference to the structure we store the key value
    field       in:     Field used inside MIN() / MAX()
    cond        in:     WHERE condition
    range_fl    out:    Bit flags for how to search if key is ok
    prefix_len  out:    Length of prefix for the search range

  DESCRIPTION
    If where condition is not a conjunction of 0 or more conjuct the
    function returns false, otherwise it checks whether there is an
    index including field as its k-th component/part such that:

     1. for each previous component f_i there is one and only one conjunct
        of the form: f_i= const_i or const_i= f_i or f_i is null
     2. references to field occur only in conjucts of the form:
        field {<|<=|>=|>|=} const or const {<|<=|>=|>|=} field or 
        field BETWEEN const1 AND const2
     3. all references to the columns from the same table as column field
        occur only in conjucts mentioned above.
     4. each of k first components the index is not partial, i.e. is not
        defined on a fixed length proper prefix of the field.

     If such an index exists the function through the ref parameter
     returns the key value to find max/min for the field using the index,
     the length of first (k-1) components of the key and flags saying
     how to apply the key for the search max/min value.
     (if we have a condition field = const, prefix_len contains the length
      of the whole search key)

  NOTE
    This function may set table->key_read to 1, which must be reset after
    index is used! (This can only happen when function returns 1)

  RETURN
    0   Index can not be used to optimize MIN(field)/MAX(field)
    1   Can use key to optimize MIN()/MAX()
        In this case ref, range_fl and prefix_len are updated
*/ 
      
static bool find_key_for_maxmin(bool max_fl, TABLE_REF *ref,
                                Field* field, COND *cond,
                                uint *range_fl, uint *prefix_len)
{
  if (!(field->flags & PART_KEY_FLAG))
    return 0;                                        // Not key field

  TABLE *table= field->table;
  uint idx= 0;

  KEY *keyinfo,*keyinfo_end;
  for (keyinfo= table->key_info, keyinfo_end= keyinfo+table->keys ;
       keyinfo != keyinfo_end;
       keyinfo++,idx++)
  {
    KEY_PART_INFO *part,*part_end;
    key_part_map key_part_to_use= 0;
    /*
      Perform a check if index is not disabled by ALTER TABLE
      or IGNORE INDEX.
    */
    if (!table->keys_in_use_for_query.is_set(idx))
      continue;
    uint jdx= 0;
    *prefix_len= 0;
    for (part= keyinfo->key_part, part_end= part+keyinfo->key_parts ;
         part != part_end ;
         part++, jdx++, key_part_to_use= (key_part_to_use << 1) | 1)
    {
      if (!(table->file->index_flags(idx, jdx, 0) & HA_READ_ORDER))
        return 0;

        /* Check whether the index component is partial */
      if (part->length < table->field[part->fieldnr-1]->pack_length())
        break;

      if (field->eq(part->field))
      {
        ref->key= idx;
        ref->key_length= 0;
        key_part_map key_part_used= 0;
        *range_fl= NO_MIN_RANGE | NO_MAX_RANGE;
        if (matching_cond(max_fl, ref, keyinfo, part, cond,
                          &key_part_used, range_fl, prefix_len) &&
            !(key_part_to_use & ~key_part_used))
        {
          if (!max_fl && key_part_used == key_part_to_use && part->null_bit)
          {
            /*
              SELECT MIN(key_part2) FROM t1 WHERE key_part1=const
              If key_part2 may be NULL, then we want to find the first row
              that is not null
            */
            ref->key_buff[ref->key_length]= 1;
            ref->key_length+= part->store_length;
            *range_fl&= ~NO_MIN_RANGE;
            *range_fl|= NEAR_MIN;                // > NULL
          }
          /*
            The following test is false when the key in the key tree is
            converted (for example to upper case)
          */
          if (field->part_of_key.is_set(idx))
          {
            table->key_read= 1;
            table->file->extra(HA_EXTRA_KEYREAD);
          }
          return 1;
        }
      }
    }
  }
  return 0;
}


/*
  Check whether found key is in range specified by conditions

  SYNOPSIS
    reckey_in_range()
    max_fl      in:     0 for MIN(field) / 1 for MAX(field)
    ref         in:     Reference to the key value and info
    field       in:     Field used the MIN/MAX expression
    cond        in:     WHERE condition
    range_fl    in:     Says whether there is a condition to to be checked
    prefix_len  in:     Length of the constant part of the key

  RETURN
    0        ok
    1        WHERE was not true for the found row
*/

static int reckey_in_range(bool max_fl, TABLE_REF *ref, Field* field,
                            COND *cond, uint range_fl, uint prefix_len)
{
  if (key_cmp_if_same(field->table, ref->key_buff, ref->key, prefix_len))
    return 1;
  if (!cond || (range_fl & (max_fl ? NO_MIN_RANGE : NO_MAX_RANGE)))
    return 0;
  return maxmin_in_range(max_fl, field, cond);
}


/*
  Check whether {MAX|MIN}(field) is in range specified by conditions
  SYNOPSIS
    maxmin_in_range()
    max_fl      in:     0 for MIN(field) / 1 for MAX(field)
    field       in:     Field used the MIN/MAX expression
    cond        in:     WHERE condition

  RETURN
    0        ok
    1        WHERE was not true for the found row
*/

static int maxmin_in_range(bool max_fl, Field* field, COND *cond)
{
  /* If AND/OR condition */
  if (cond->type() == Item::COND_ITEM)
  {
    List_iterator_fast<Item> li(*((Item_cond*) cond)->argument_list());
    Item *item;
    while ((item= li++))
    {
      if (maxmin_in_range(max_fl, field, item))
        return 1;
    }
    return 0;
  }

  if (cond->used_tables() != field->table->map)
    return 0;
  bool less_fl= 0;
  switch (((Item_func*) cond)->functype()) {
  case Item_func::BETWEEN:
    return cond->val_int() == 0;                // Return 1 if WHERE is false
  case Item_func::LT_FUNC:
  case Item_func::LE_FUNC:
    less_fl= 1;
  case Item_func::GT_FUNC:
  case Item_func::GE_FUNC:
  {
    Item *item= ((Item_func*) cond)->arguments()[1];
    /* In case of 'const op item' we have to swap the operator */
    if (!item->const_item())
      less_fl= 1-less_fl;
    /*
      We only have to check the expression if we are using an expression like
      SELECT MAX(b) FROM t1 WHERE a=const AND b>const
      not for
      SELECT MAX(b) FROM t1 WHERE a=const AND b<const
    */
    if (max_fl != less_fl)
      return cond->val_int() == 0;                // Return 1 if WHERE is false
    return 0;
  }
  case Item_func::EQ_FUNC:
  case Item_func::EQUAL_FUNC:
    break;
  default:                                        // Keep compiler happy
    DBUG_ASSERT(1);                               // Impossible
    break;
  }
  return 0;
}

