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

static int
eliminate_tables_for_join_list(JOIN *join, List<TABLE_LIST> *join_list,
                               table_map tables_used_elsewhere, 
                               uint *const_tbl_count, table_map *const_tables);
static bool table_has_one_match(TABLE *table, table_map bound_tables);
static void 
mark_table_as_eliminated(JOIN *join, TABLE *table, uint *const_tbl_count,
                         table_map *const_tables);
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

void eliminate_tables(JOIN *join, uint *const_tbl_count, 
                      table_map *const_tables)
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

  if (((1 << join->tables) - 1) & ~used_tables)
  {
    /* There are some tables that we probably could eliminate. Try it. */
    eliminate_tables_for_join_list(join, join->join_list, used_tables,
                                   const_tbl_count, const_tables);
  }
  DBUG_VOID_RETURN;
}




/*
  Perform table elimination in a given join list

  SYNOPSIS
    eliminate_tables_for_join_list()
      join                    The join
      join_list               Join list to work on 
      tables_used_elsewhere   Bitmap of tables that are referred to from
                              somewhere outside of the join list (e.g.
                              select list, HAVING, etc).
      const_tbl_count  INOUT  Number of constant tables (eliminated tables 
                              are considered constant)
      const_tables     INOUT  Bitmap of constant tables.

  DESCRIPTION
  Try eliminating members of the given join list (and its children,
  recursively).

  Search for tables to be eliminated is performed on recursive descent,
  while the elimination is done on ascent. 
  
  DESCENT AND NO-REFERENCES CHECK
  The descent part is needed because of the following: consider a join list
  
   t0 LEFT JOIN 
    (t1 
     LEFT JOIN t2 ON cond1(t1,t2)   
     LEFT JOIN t3 ON cond2(..., possibly-t2)                 (*)
     LEFT JOIN t4 ON cond3(..., possibly-t2, possibly-t3)
    ) ON cond4

  Suppose we're looking at whether we can eliminate outer join marked with
  (*), in other words, table t3. Before we can do that, we need to 
  1. Check that there are no references to table t3 in cond4 (in general:
     all ON expressions of embedding outer joins, this explains the need for
     descent)
  2. Check that there are no references to table t3 in its following-siblings,
     in this example, in cond3.
  3. Although SQL language doesn't allow referring to table t3 from cond1, 
     simplify_joins() may create such back-references, so we'll also need to 
     check if t3's preceding-siblings have ON expressions with references
     from t3.
  
  ASCENT AND THE ELIMINATION
  The removal is done in a bottom-up way because we can consider an outer
  join nest for elimination only after we have successfully eliminated all 
  of its children outer joins. 

  RETURN
    Number of tables that have been eliminated
*/

static int
eliminate_tables_for_join_list(JOIN *join, List<TABLE_LIST> *join_list,
                               table_map tables_used_elsewhere, 
                               uint *const_tbl_count, table_map *const_tables)
{
  List_iterator<TABLE_LIST> it(*join_list);
  table_map used_tables_on_right[MAX_TABLES];
  table_map tables_used_on_left;
  TABLE_LIST *tbl;
  int i, n_tables;
  int eliminated=0;

  /* Collect used_tables_on_right array */
  for (i=0; (tbl= it++); i++)
  {
    used_tables_on_right[i]= 0;
    if (tbl->on_expr)
      used_tables_on_right[i]= tbl->on_expr->used_tables();
    if (tbl->nested_join)
      used_tables_on_right[i]= tbl->nested_join->used_tables;
  }
  n_tables= i;
  for (i= n_tables - 2; i > 0; i--)
    used_tables_on_right[i] |= used_tables_on_right[i+1];
  
  i= 1;
  it.rewind();
  tables_used_on_left= 0;
  /* For each member of the join list, check if we can eliminate it */
  while ((tbl= it++))
  {
    table_map tables_used_outside= tables_used_on_left |
                                   used_tables_on_right[i] |
                                   tables_used_elsewhere;
    table_map cur_tables= 0;

    if (tbl->nested_join)
    {
      DBUG_ASSERT(tbl->on_expr); 
      /*
        There can be cases where table removal is applicable for tables
        within the outer join but not for the outer join itself. Ask to
        remove the children first.

        TODO: NoHopelessEliminationAttempts: the below call can return 
        information about whether it would make any sense to try removing 
        this entire outer join nest.
      */
      int eliminated_in_children= 
        eliminate_tables_for_join_list(join, &tbl->nested_join->join_list,
                                       tables_used_outside,
                                       const_tbl_count, const_tables);
      tbl->nested_join->n_tables -=eliminated_in_children; 
      cur_tables= tbl->nested_join->used_tables;
      if (!(cur_tables & tables_used_outside))
      {
        /* 
          Check if all embedded tables together can produce at most one
          record combination. This is true when
           - each of them has one_match(outer-tables) property
             (this is a stronger condition than all of them together having
              this property but that's irrelevant here)
           - there are no outer joins among them
             (except for the case  of outer join which has all inner tables
              to be constant and is guaranteed to produce only one record.
              that record will be null-complemented)
        */
        bool one_match= TRUE;
        List_iterator<TABLE_LIST> it2(tbl->nested_join->join_list);
        TABLE_LIST *inner;
        while ((inner= it2++))
        {
          /*
            Bail out if we see an outer join (TODO: handle the above
            null-complemntated-rows-only case)
          */
          if (inner->on_expr)
          {
            one_match= FALSE;
            break;
          }

          if (inner->table && // <-- to be removed after NoHopelessEliminationAttempts
              !table_has_one_match(inner->table,
                                   ~tbl->nested_join->used_tables))
          {
            one_match= FALSE;
            break;
          }
        }
        if (one_match)
        {
          it2.rewind();
          while ((inner= it2++))
          {
            mark_table_as_eliminated(join, inner->table, const_tbl_count, 
                                     const_tables);
          }
          eliminated += tbl->nested_join->join_list.elements;
          //psergey-todo: do we need to do anything about removing the join
          //nest?
          tbl->on_expr->walk(&Item::mark_as_eliminated_processor, FALSE, NULL);
        }
        else
        {
          eliminated += eliminated_in_children;
        }
      }
    }
    else if (tbl->on_expr)
    {
      cur_tables= tbl->on_expr->used_tables();
      if (!(tbl->table->map & tables_used_outside) && 
          table_has_one_match(tbl->table, (table_map)-1))
      {
        mark_table_as_eliminated(join, tbl->table, const_tbl_count, 
                                 const_tables);
        tbl->on_expr->walk(&Item::mark_as_eliminated_processor, FALSE, NULL);
        eliminated += 1;
      }
    }

    i++;
    tables_used_on_left |= cur_tables;
  }
  return eliminated;
}


/*
  Mark table as eliminated:
   - Mark it as constant table
   - Move it to the front of join order
   - Record it in join->eliminated_tables
*/

static 
void mark_table_as_eliminated(JOIN *join, TABLE *table, uint *const_tbl_count,
                              table_map *const_tables)
{
  JOIN_TAB *tab= table->reginfo.join_tab;
  if (!(*const_tables & tab->table->map))
  {
    DBUG_PRINT("info", ("Eliminated table %s", table->alias));
    tab->type= JT_CONST;
    join->eliminated_tables |= table->map;
    *const_tables |= table->map;
    join->const_table_map|= table->map;
    set_position(join, (*const_tbl_count)++, tab, (KEYUSE*)0);
  }
}


/*
  Check if the table will produce at most one matching record

  SYNOPSIS
    table_has_one_match()
      table          The [base] table being checked 
      bound_tables   Tables that should be considered bound.

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

static bool table_has_one_match(TABLE *table, table_map bound_tables)
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
        if (keyuse->usable)
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
          bound_parts|= key_part_map(1) << uses[i].keyuse->keypart;
          n_bounded++;
        }
        if (bound_parts == PREV_BITS(key_part_map, keyinfo->key_parts))
          return TRUE;
      }
    } while (n_bounded != 0);
  }
  return FALSE;
}

/**
  @} (end of group Table_Elimination)
*/

