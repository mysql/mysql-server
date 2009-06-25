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

    Table elimination is intended to be done on every PS re-execution.
*/

static int
eliminate_tables_for_join_list(JOIN *join, List<TABLE_LIST> *join_list,
                               table_map used_tables_elsewhere, 
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
   
   TODO fix comment
  
    SELECT * FROM t1 LEFT JOIN 
      (t2 JOIN t3) ON t3.primary_key=t1.col AND 
                      t4.primary_key= t2.col

  CRITERIA FOR REMOVING ONE OJ NEST
    we can't rely on sole presense of eq_refs. Because if we do, we'll miss
    things like this:

    SELECT * FROM flights LEFT JOIN 
      (pax as S1 JOIN pax as S2 ON S2.id=S1.spouse AND s1.id=s2.spouse)
  
   (no-polygamy schema/query but there can be many couples on the flight)
   ..
  
  REMOVAL PROCESS
    We can remove an inner side of an outer join if it there is a warranty
    that it will produce not more than one record:

     ... t1 LEFT JOIN t2 ON (t2.unique_key = expr) ...

    For nested outer joins:
    - The process naturally occurs bottom-up (in order to remove an
      outer-join we need to analyze its contents)
    - If we failed to remove an outer join nest, it makes no sense to 
      try removing its ancestors, as the 
         ot LEFT JOIN it ON cond
      pair may possibly produce two records (one record via match and
      another one as access-method record).

  Q: If we haven't removed an OUTER JOIN, does it make sense to attempt
  removing its ancestors? 
  A: No as the innermost outer join will produce two records => no ancestor 
  outer join nest will be able to provide the max_fanout==1 guarantee.
*/

void eliminate_tables(JOIN *join, uint *const_tbl_count, 
                      table_map *const_tables)
{
  Item *item;
  table_map used_tables;
  DBUG_ENTER("eliminate_tables");
  
  DBUG_ASSERT(join->eliminated_tables == 0);
  
  /* MWL#17 is only about outer join elimination, so: */
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
    /* There are some time tables that we probably could eliminate */
    eliminate_tables_for_join_list(join, join->join_list, used_tables,
                                   const_tbl_count, const_tables);
  }
  DBUG_VOID_RETURN;
}




/*
  Now on to traversal. There can be a situation like this:

    FROM t1 
         LEFT JOIN t2 ON cond(t1,t2)
         LEFT JOIN t3 ON cond(..., possibly-t2)  // <--(*)
         LEFT JOIN t4 ON cond(..., possibly-t2)

  Besides that, simplify_joins() may have created back references, so when
  we're e.g. looking at outer join (*) we need to look both forward and
  backward to check if there are any references in preceding/following
  outer joins'

  TODO would it create only following-sibling references or
  preceding-sibling as well?
    And if not, should we rely on that?
    
*/

static int
eliminate_tables_for_join_list(JOIN *join, List<TABLE_LIST> *join_list,
                               table_map used_tables_elsewhere, 
                               uint *const_tbl_count, table_map *const_tables)
{
  List_iterator<TABLE_LIST> it(*join_list);
  table_map used_tables_on_right[MAX_TABLES]; // todo change to alloca
  table_map used_tables_on_left;
  TABLE_LIST *tbl;
  int i, n_tables;
  int eliminated=0;

  /* Collect the reverse-bitmap-array */
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
  
  it.rewind();
  
  /* Walk through tables and join nests and see if we can eliminate them */
  used_tables_on_left= 0;
  i= 1;
  while ((tbl= it++))
  {
    table_map tables_used_outside= used_tables_on_left |
                                   used_tables_on_right[i] |
                                   used_tables_elsewhere;
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
      /* Check and remove */
      if (!(tbl->table->map & tables_used_outside) && 
          table_has_one_match(tbl->table, (table_map)-1))
      {
        mark_table_as_eliminated(join, tbl->table, const_tbl_count, 
                                 const_tables);
        tbl->on_expr->walk(&Item::mark_as_eliminated_processor, FALSE, NULL);
        eliminated += 1;
      }
    }

    /* Update bitmap of tables we've seen on the left */
    i++;
    used_tables_on_left |= cur_tables;
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
  Check the table will produce at most one matching record

  SYNOPSIS
    table_has_one_match()
      table          The [base] table being checked 
      bound_tables   Tables that should be considered bound.

  DESCRIPTION
    Check if the given table will produce at most one matching record for 
    each record combination of tables in bound_tables.

  RETURN 
    TRUE   Yes, at most one match
    FALSE  No
*/

static bool table_has_one_match(TABLE *table, table_map bound_tables)
{
  KEYUSE *keyuse= table->reginfo.join_tab->keyuse;
  if (keyuse)
  {
    /*
      Walk through all of the KEYUSE elements and
       - locate unique keys
       - check if we have eq_ref access for them
          TODO any other reqs?
      loops are constructed like in best_access_path
    */
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
  SYNOPSIS
    extra_keyuses_bind_all_keyparts()
      bound_tables   Tables which can be considered constants
      table          Table we're examining
      key_start      Start of KEYUSE array with elements describing the key
                     of interest
      key_end        End of the array + 1
      n_keyuses      Number
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
  if (n_keyuses && bound_parts)
  {
    KEY *keyinfo= table->key_info + key_start->key; 
    Keyuse_w_needed_reg *uses;
    if (!(uses= (Keyuse_w_needed_reg*)my_alloca(sizeof(Keyuse_w_needed_reg)*
                                                n_keyuses)))
      return FALSE;
    uint n_uses=0;
    /* First, collect an array<keyuse, key_parts_it_depends_on>*/
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

    /* Now compute transitive closure */
    uint n_bounded;
    do 
    {
      n_bounded= 0;
      for (uint i=0; i< n_uses; i++)
      {
        /* needed_parts is covered by what is already bound*/
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

