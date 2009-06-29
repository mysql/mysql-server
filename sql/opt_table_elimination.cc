/**
  @file

  @brief
    Table Elimination Module

  @defgroup Table_Elimination Table Elimination Module
  @{
*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "sql_select.h"

/*
  OVERVIEW

  The module has one entry point - eliminate_tables() function, which one 
  needs to call (once) sometime after update_ref_and_keys() but before the
  join optimization.  
  eliminate_tables() operates over the JOIN structures. Logically, it
  removes the right sides of outer join nests. Physically, it changes the
  following members:

  * Eliminated tables are marked as constant and moved to the front of the
    join order.
  * In addition to this, they are recorded in JOIN::eliminated_tables bitmap.

  * All join nests have their NESTED_JOIN::n_tables updated to discount
    the eliminated tables

  * Items that became disused because they were in the ON expression of an 
    eliminated outer join are notified by means of the Item tree walk which 
    calls Item::mark_as_eliminated_processor for every item
    - At the moment the only Item that cares is Item_subselect with its 
      Item_subselect::eliminated flag which is used by EXPLAIN code to
      check if the subquery should be shown in EXPLAIN.

  Table elimination is redone on every PS re-execution.
*/

static void mark_as_eliminated(JOIN *join, TABLE_LIST *tbl);
static bool table_has_one_match(TABLE *table, table_map bound_tables, 
                                bool *multiple_matches);
static uint
eliminate_tables_for_list(JOIN *join, TABLE **leaves_arr,
                          List<TABLE_LIST> *join_list,
                          bool its_outer_join,
                          table_map tables_in_list,
                          table_map tables_used_elsewhere,
                          bool *multiple_matches);
static bool 
extra_keyuses_bind_all_keyparts(table_map bound_tables, TABLE *table, 
                                KEYUSE *key_start, KEYUSE *key_end, 
                                uint n_keyuses, table_map bound_parts);

/*
  Perform table elimination

  SYNOPSIS
    eliminate_tables()
      join                   Join to work on
      const_tbl_count INOUT  Number of constant tables (this includes
                             eliminated tables)
      const_tables    INOUT  Bitmap of constant tables

  DESCRIPTION
    This function is the entry point for table elimination. 
    The idea behind table elimination is that if we have an outer join:
   
      SELECT * FROM t1 LEFT JOIN 
        (t2 JOIN t3) ON t3.primary_key=t1.col AND 
                        t4.primary_key=t2.col
    such that

    1. columns of the inner tables are not used anywhere ouside the outer
       join (not in WHERE, not in GROUP/ORDER BY clause, not in select list 
       etc etc), and
    2. inner side of the outer join is guaranteed to produce at most one
       record combination for each record combination of outer tables.
    
    then the inner side of the outer join can be removed from the query.
    This is because it will always produce one matching record (either a
    real match or a NULL-complemented record combination), and since there
    are no references to columns of the inner tables anywhere, it doesn't
    matter which record combination it was.

    This function primary handles checking #1. It collects a bitmap of
    tables that are not used in select list/GROUP BY/ORDER BY/HAVING/etc and
    thus can possibly be eliminated.
  
  SIDE EFFECTS
    See the OVERVIEW section at the top of this file.

*/

void eliminate_tables(JOIN *join)
{
  Item *item;
  table_map used_tables;
  DBUG_ENTER("eliminate_tables");
  
  DBUG_ASSERT(join->eliminated_tables == 0);
  
  /* If there are no outer joins, we have nothing to eliminate: */
  if (!join->outer_join)
    DBUG_VOID_RETURN;

  /* Find the tables that are referred to from WHERE/HAVING */
  used_tables= (join->conds?  join->conds->used_tables() : 0) | 
               (join->having? join->having->used_tables() : 0);
  
  /* Add tables referred to from the select list */
  List_iterator<Item> it(join->fields_list);
  while ((item= it++))
    used_tables |= item->used_tables();
 
  /* Add tables referred to from ORDER BY and GROUP BY lists */
  ORDER *all_lists[]= { join->order, join->group_list};
  for (int i=0; i < 2; i++)
  {
    for (ORDER *cur_list= all_lists[i]; cur_list; cur_list= cur_list->next)
      used_tables |= (*(cur_list->item))->used_tables();
  }
  
  THD* thd= join->thd;
  if (join->select_lex == &thd->lex->select_lex)
  {
    /* Multi-table UPDATE and DELETE: don't eliminate the tables we modify: */
    used_tables |= thd->table_map_for_update;

    /* Multi-table UPDATE: don't eliminate tables referred from SET statement */
    if (thd->lex->sql_command == SQLCOM_UPDATE_MULTI)
    {
      List_iterator<Item> it2(thd->lex->value_list);
      while ((item= it2++))
        used_tables |= item->used_tables();
    }
  }
  
  table_map all_tables= join->all_tables_map();
  if (all_tables & ~used_tables)
  {
    /* There are some tables that we probably could eliminate. Try it. */
    TABLE *leaves_array[MAX_TABLES];
    bool multiple_matches= FALSE;
    eliminate_tables_for_list(join, leaves_array, join->join_list, FALSE,
                              all_tables, used_tables, &multiple_matches);
  }
  DBUG_VOID_RETURN;
}

/*
  Perform table elimination in a given join list

  SYNOPSIS
    eliminate_tables_for_list()
      join                    The join
      join_list               Join list to work on 
      tables_used_elsewhere   Bitmap of tables that are referred to from
                              somewhere outside of the join list (e.g.
                              select list, HAVING, etc).
  DESCRIPTION

  RETURN
    Number of base tables left after elimination. 0 means everything was
    eliminated. Tables that belong to the
    children of this join nest are also counted.

//    TRUE   The entire join list can be eliminated (caller should remove)
//    FALSE  Otherwise
   number of tables that were eliminated (compare this with total number of
   tables in the join_list to tell if the entire join was eliminated)
*/
static uint
eliminate_tables_for_list(JOIN *join, TABLE **leaves_arr,
                          List<TABLE_LIST> *join_list,
                          bool its_outer_join,
                          table_map tables_in_list,
                          table_map tables_used_elsewhere,
                          bool *multiple_matches)
{
  TABLE_LIST *tbl;
  List_iterator<TABLE_LIST> it(*join_list);
  table_map tables_used_on_left= 0;
  TABLE **cur_table= leaves_arr;
  bool children_have_multiple_matches= FALSE;
  uint base_tables= 0;

  while ((tbl= it++))
  {
    if (tbl->on_expr)
    {
      table_map outside_used_tables= tables_used_elsewhere | 
                                     tables_used_on_left;
      bool multiple_matches= FALSE;
      if (tbl->nested_join)
      {
        /* This is  "... LEFT JOIN (join_nest) ON cond" */
        uint n;
        if (!(n= eliminate_tables_for_list(join, cur_table,
                                           &tbl->nested_join->join_list, TRUE,
                                           tbl->nested_join->used_tables, 
                                           outside_used_tables, 
                                           &multiple_matches)))
        {
          mark_as_eliminated(join, tbl);
        }
        tbl->nested_join->n_tables= n;
        base_tables += n;
      }
      else
      {
        /* This is  "... LEFT JOIN tbl ON cond" */
        if (!(tbl->table->map & outside_used_tables) && 
            table_has_one_match(tbl->table, join->all_tables_map(),
                                &multiple_matches))
        {
          mark_as_eliminated(join, tbl);
        }
        else 
          base_tables++;
      }
      tables_used_on_left |= tbl->on_expr->used_tables();
      children_have_multiple_matches= children_have_multiple_matches ||  
                                      multiple_matches;
    }
    else
    {
      DBUG_ASSERT(!tbl->nested_join);
      base_tables++;
    }

    if (tbl->table)
      *(cur_table++)= tbl->table;
  }

  *multiple_matches |= children_have_multiple_matches;
  
  /* Try eliminating the nest we're called for */
  if (its_outer_join && !children_have_multiple_matches &&
      !(tables_in_list & tables_used_elsewhere))
  {
    table_map bound_tables= join->const_table_map | (join->all_tables_map() & 
                                                     ~tables_in_list);
    table_map old_bound_tables;
    TABLE **leaves_end= cur_table;
    /*
      Do the same as const table search table: try to expand the set of bound 
      tables until it covers all tables in the join_list
    */
    do
    {
      old_bound_tables= bound_tables;
      for (cur_table= leaves_arr; cur_table != leaves_end; cur_table++)
      {
        if (!((*cur_table)->map & join->eliminated_tables) && 
            table_has_one_match(*cur_table, bound_tables, multiple_matches))
        {
          bound_tables |= (*cur_table)->map;
        }
      }
    } while (old_bound_tables != bound_tables);
    
    if (!(tables_in_list & ~bound_tables))
    {
      /* 
        This join_list can be eliminated. Signal about this to the caller by
        returning number of tables.
      */
      base_tables= 0;
    }
  }
  return base_tables;
}


/*
  Check if the table will produce at most one matching record

  SYNOPSIS
    table_has_one_match()
      table                 The [base] table being checked 
      bound_tables          Tables that should be considered bound.
      multiple_matches OUT  Set to TRUE when there is no way we could 
                            find find a limitation that would give us one-match
                            property.

  DESCRIPTION
    Check if table will produce at most one matching record for each record 
    combination of tables in bound_tables bitmap.

    The check is based on ref analysis data, KEYUSE structures. We're
    handling two cases:

    1. Table has a UNIQUE KEY(uk_col_1, ... uk_col_N), and for each uk_col_i
       there is a KEYUSE that represents a limitation in form

         table.uk_col_i = func(bound_tables)                           (X)

    2. Same as above but we also handle limitations in form 

         table.uk_col_i = func(bound_tables, uk_col_j1, ... uk_col_j2) (XX)

       where values of uk_col_jN are known to be bound because for them we
       have an equality of form (X) or (XX).

  RETURN 
    TRUE   Yes, at most one match
    FALSE  No
*/

static bool table_has_one_match(TABLE *table, table_map bound_tables, 
                                bool *multiple_matches)
{
  KEYUSE *keyuse= table->reginfo.join_tab->keyuse;
  if (keyuse)
  {
    while (keyuse->table == table)
    {
      uint key= keyuse->key;
      key_part_map bound_parts=0;
      uint n_unusable=0;
      bool ft_key= test(keyuse->keypart == FT_KEYPART);
      KEY *keyinfo= table->key_info + key; 
      KEYUSE *key_start = keyuse;
      
      do  /* For each keypart and each way to read it */
      {
        if (keyuse->usable == 1)
        {
          if(!(keyuse->used_tables & ~bound_tables) &&
             !(keyuse->optimize & KEY_OPTIMIZE_REF_OR_NULL))
          {
            bound_parts |= keyuse->keypart_map;
          }
        }
        else
          n_unusable++;
        keyuse++;
      } while (keyuse->table == table && keyuse->key == key);
      
      if (ft_key || ((keyinfo->flags & (HA_NOSAME | HA_NULL_PART_KEY)) 
                     != HA_NOSAME))
      {
        continue; 
      }

      if (bound_parts == PREV_BITS(key_part_map, keyinfo->key_parts) ||
          extra_keyuses_bind_all_keyparts(bound_tables, table, key_start,
                                          keyuse, n_unusable, bound_parts))
      {
        return TRUE;
      }
    }
  }
  return FALSE;
}


typedef struct st_keyuse_w_needed_reg
{
  KEYUSE *keyuse;
  key_part_map dependency_parts;
} Keyuse_w_needed_reg;


/*
  Check if KEYUSE elemements with unusable==TRUE bind all parts of the key

  SYNOPSIS

    extra_keyuses_bind_all_keyparts()
      bound_tables   Tables which can be considered constants
      table          Table we're examining
      key_start      Start of KEYUSE array with elements describing the key
                     of interest
      key_end        End of the array + 1
      n_keyuses      Number of elements in the array that have unusable==TRUE
      bound_parts    Key parts whose values are known to be bound.

  DESCRIPTION
    Check if unusable KEYUSE elements cause all parts of key to be bound. An 
    unusable keyuse element makes a keypart bound when it
    represents the following:

      keyXpartY=func(bound_columns, preceding_tables)

  RETURN 
    TRUE   Yes, at most one match
    FALSE  No
*/

static bool 
extra_keyuses_bind_all_keyparts(table_map bound_tables, TABLE *table, 
                                KEYUSE *key_start, KEYUSE *key_end, 
                                uint n_keyuses, table_map bound_parts)
{
  /*
    Current implementation needs some keyparts to be already bound to start
    inferences: 
  */
  if (n_keyuses && bound_parts)
  {
    KEY *keyinfo= table->key_info + key_start->key; 
    Keyuse_w_needed_reg *uses;
    if (!(uses= (Keyuse_w_needed_reg*)my_alloca(sizeof(Keyuse_w_needed_reg)*
                                                n_keyuses)))
      return FALSE;
    uint n_uses=0;

    /* First, collect an array<keyuse, key_parts_it_depends_on> */
    for (KEYUSE *k= key_start; k!=key_end; k++)
    {
      if (!k->usable && !(k->used_tables & ~bound_tables))
      {
        Field_processor_info fp= {bound_tables, table, k->key, 0};
        if (!k->val->walk(&Item::check_column_usage_processor, FALSE, 
                          (uchar*)&fp))
        {
          uses[n_uses].keyuse= k;
          uses[n_uses].dependency_parts= fp.needed_key_parts;
          n_uses++;
        }
      }
    }

    /* 
      Now, repeatedly walk through the <keyuse, key_parts_it_depends_on> and
      see if we can find an elements that depend only on bound parts and
      hence make one more part bound.
    */
    uint n_bounded;
    do 
    {
      n_bounded= 0;
      for (uint i=0; i< n_uses; i++)
      {
        if (!(uses[i].dependency_parts & ~bound_parts))
        {
          table_map old= bound_parts;
          bound_parts|= key_part_map(1) << uses[i].keyuse->keypart;
          if (old != bound_parts)
            n_bounded++;
        }
        if (bound_parts == PREV_BITS(key_part_map, keyinfo->key_parts))
          return TRUE;
      }
    } while (n_bounded != 0);
  }
  return FALSE;
}


/* 
  Mark one table or the whole join nest as eliminated.
*/
static void mark_as_eliminated(JOIN *join, TABLE_LIST *tbl)
{
  TABLE *table;
  /*
    NOTE: there are TABLE_LIST object that have
    tbl->table!= NULL && tbl->nested_join!=NULL and 
    tbl->table == tbl->nested_join->join_list->element(..)->table
  */
  if (tbl->nested_join)
  {
    TABLE_LIST *child;
    List_iterator<TABLE_LIST> it(tbl->nested_join->join_list);
    while ((child= it++))
      mark_as_eliminated(join, child);
  }
  else if ((table= tbl->table))
  {
    JOIN_TAB *tab= tbl->table->reginfo.join_tab;
    if (!(join->const_table_map & tab->table->map))
    {
      DBUG_PRINT("info", ("Eliminated table %s", table->alias));
      tab->type= JT_CONST;
      join->eliminated_tables |= table->map;
      join->const_table_map|= table->map;
      set_position(join, join->const_tables++, tab, (KEYUSE*)0);
    }
  }

  if (tbl->on_expr)
    tbl->on_expr->walk(&Item::mark_as_eliminated_processor, FALSE, NULL);
}

/**
  @} (end of group Table_Elimination)
*/

