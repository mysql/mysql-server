/* Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file

  @brief
  mysql_select and join optimization


  @defgroup Query_Optimizer  Query Optimizer
  @{
*/

#include "sql_priv.h"
#include "sql_select.h"
#include "sql_table.h"                          // primary_key_name
#include "sql_derived.h"
#include "probes_mysql.h"
#include "opt_trace.h"
#include "key.h"                 // key_copy, key_cmp, key_cmp_if_same
#include "lock.h"                // mysql_unlock_some_tables,
                                 // mysql_unlock_read_tables
#include "sql_show.h"            // append_identifier
#include "sql_base.h"            // setup_wild, setup_fields, fill_record
#include "sql_acl.h"             // *_ACL
#include "sql_test.h"            // misc. debug printing utilities
#include "records.h"             // init_read_record, end_read_record
#include "filesort.h"            // filesort_free_buffers
#include "sql_union.h"           // mysql_union
#include "opt_explain.h"
#include "sql_join_buffer.h"     // JOIN_CACHE
#include "sql_optimizer.h"       // JOIN
#include "sql_tmp_table.h"       // tmp tables

#include <algorithm>
using std::max;
using std::min;

static store_key *get_store_key(THD *thd,
				Key_use *keyuse, table_map used_tables,
				KEY_PART_INFO *key_part, uchar *key_buff,
				uint maybe_null);
bool const_expression_in_where(Item *conds,Item *item, Item **comp_item);
uint find_shortest_key(TABLE *table, const key_map *usable_keys);
static bool test_if_cheaper_ordering(const JOIN_TAB *tab,
                                     ORDER *order, TABLE *table,
                                     key_map usable_keys, int key,
                                     ha_rows select_limit,
                                     int *new_key, int *new_key_direction,
                                     ha_rows *new_select_limit,
                                     uint *new_used_key_parts= NULL,
                                     uint *saved_best_key_parts= NULL);
static uint join_buffer_alg(const THD *thd);
static void push_index_cond(JOIN_TAB *tab, uint keyno, bool other_tbls_ok,
                            Opt_trace_object *trace_obj);

/**
  This handles SELECT with and without UNION
*/

bool handle_select(THD *thd, select_result *result,
                   ulong setup_tables_done_option)
{
  bool res;
  LEX *lex= thd->lex;
  register SELECT_LEX *select_lex = &lex->select_lex;
  DBUG_ENTER("handle_select");
  MYSQL_SELECT_START(thd->query());

  if (lex->proc_analyse && lex->sql_command != SQLCOM_SELECT)
  {
    my_error(ER_WRONG_USAGE, MYF(0), "PROCEDURE", "non-SELECT");
    DBUG_RETURN(true);
  }

  if (select_lex->master_unit()->is_union() || 
      select_lex->master_unit()->fake_select_lex)
    res= mysql_union(thd, lex, result, &lex->unit, setup_tables_done_option);
  else
  {
    SELECT_LEX_UNIT *unit= &lex->unit;
    unit->set_limit(unit->global_parameters);
    /*
      'options' of mysql_select will be set in JOIN, as far as JOIN for
      every PS/SP execution new, we will not need reset this flag if 
      setup_tables_done_option changed for next rexecution
    */
    res= mysql_select(thd,
		      select_lex->table_list.first,
		      select_lex->with_wild, select_lex->item_list,
		      select_lex->where,
		      &select_lex->order_list,
		      &select_lex->group_list,
		      select_lex->having,
		      select_lex->options | thd->variables.option_bits |
                      setup_tables_done_option,
		      result, unit, select_lex);
  }
  DBUG_PRINT("info",("res: %d  report_error: %d", res,
		     thd->is_error()));
  res|= thd->is_error();
  if (unlikely(res))
    result->abort_result_set();

  MYSQL_SELECT_DONE((int) res, (ulong) thd->limit_found_rows);
  DBUG_RETURN(res);
}


/*****************************************************************************
  Check fields, find best join, do the select and output fields.
  All tables must be opened.
*****************************************************************************/

/**
  @brief Check if two items are compatible wrt. materialization.

  @param outer Expression from outer query
  @param inner Expression from inner query

  @retval TRUE   If subquery types allow materialization.
  @retval FALSE  Otherwise.
*/

bool types_allow_materialization(Item *outer, Item *inner)

{
  if (outer->result_type() != inner->result_type())
    return FALSE;
  switch (outer->result_type()) {
  case STRING_RESULT:
    if (outer->is_temporal_with_date() != inner->is_temporal_with_date())
      return FALSE;
    if (!(outer->collation.collation == inner->collation.collation
        /*&& outer->max_length <= inner->max_length */))
      return FALSE;
  /*case INT_RESULT:
    if (!(outer->unsigned_flag ^ inner->unsigned_flag))
      return FALSE; */
  default:
    ;                 /* suitable for materialization */
  }
  return TRUE;
}


/*
  Check if the table's rowid is included in the temptable

  SYNOPSIS
    sj_table_is_included()
      join      The join
      join_tab  The table to be checked

  DESCRIPTION
    SemiJoinDuplicateElimination: check the table's rowid should be included
    in the temptable. This is so if

    1. The table is not embedded within some semi-join nest
    2. The has been pulled out of a semi-join nest, or

    3. The table is functionally dependent on some previous table

    [4. This is also true for constant tables that can't be
        NULL-complemented but this function is not called for such tables]

  RETURN
    TRUE  - Include table's rowid
    FALSE - Don't
*/

static bool sj_table_is_included(JOIN *join, JOIN_TAB *join_tab)
{
  if (join_tab->emb_sj_nest)
    return FALSE;
  
  /* Check if this table is functionally dependent on the tables that
     are within the same outer join nest
  */
  TABLE_LIST *embedding= join_tab->table->pos_in_table_list->embedding;
  if (join_tab->type == JT_EQ_REF)
  {
    table_map depends_on= 0;
    uint idx;
    
    for (uint kp= 0; kp < join_tab->ref.key_parts; kp++)
      depends_on |= join_tab->ref.items[kp]->used_tables();

    Table_map_iterator it(depends_on & ~PSEUDO_TABLE_BITS);
    while ((idx= it.next_bit())!=Table_map_iterator::BITMAP_END)
    {
      JOIN_TAB *ref_tab= join->map2table[idx];
      if (embedding != ref_tab->table->pos_in_table_list->embedding)
        return TRUE;
    }
    /* Ok, functionally dependent */
    return FALSE;
  }
  /* Not functionally dependent => need to include*/
  return TRUE;
}

/**
   Check if the optimizer might choose to use join buffering for this
   join. If that is the case, and if duplicate weedout semijoin
   strategy is used, the duplicate generating range must be extended
   to the first non-const table. 

   This function is called from setup_semijoin_dups_elimination()
   before the final decision is made on whether or not buffering is
   used. It is therefore only a rough test that covers all cases where
   join buffering might be used, but potentially also some cases where
   join buffering will not be used.

   @param join_buffer_alg      Bitmap with possible join buffer algorithms
   @param sj_tab               Table that might be joined by BNL/BKA

   @return                     
      true if join buffering might be used, false otherwise

 */
static bool might_do_join_buffering(uint join_buffer_alg, 
                                    const JOIN_TAB *sj_tab) 
{
  /* 
     (1) sj_tab is not a const table
  */
  int sj_tabno= sj_tab - sj_tab->join->join_tab;
  return (sj_tabno >= (int)sj_tab->join->const_tables && // (1)
          sj_tab->use_quick != QS_DYNAMIC_RANGE && 
          (((join_buffer_alg & JOIN_CACHE::ALG_BNL) && 
            sj_tab->type == JT_ALL) ||
           ((join_buffer_alg & 
             (JOIN_CACHE::ALG_BKA | JOIN_CACHE::ALG_BKA_UNIQUE)) && 
            (sj_tab->type == JT_REF || 
             sj_tab->type == JT_EQ_REF ||
             sj_tab->type == JT_CONST))));
}

/**
  Setup the strategies to eliminate semi-join duplicates.
  
  @param join           Join to process
  @param options        Join options (needed to see if join buffering will be 
                        used or not)
  @param no_jbuf_after  Do not use join buffering after the table with this 
                        number

  @retval FALSE  OK 
  @retval TRUE   Out of memory error

  @details
    Setup the strategies to eliminate semi-join duplicates.
    At the moment there are 5 strategies:

    1. DuplicateWeedout (use of temptable to remove duplicates based on rowids
                         of row combinations)
    2. FirstMatch (pick only the 1st matching row combination of inner tables)
    3. LooseScan (scanning the sj-inner table in a way that groups duplicates
                  together and picking the 1st one)
    4. MaterializeLookup (Materialize inner tables, then setup a scan over
                          outer correlated tables, lookup in materialized table)
    5. MaterializeScan (Materialize inner tables, then setup a scan over
                        materialized tables, perform lookup in outer tables)
    
    The join order has "duplicate-generating ranges", and every range is
    served by one strategy or a combination of FirstMatch with with some
    other strategy.
    
    "Duplicate-generating range" is defined as a range within the join order
    that contains all of the inner tables of a semi-join. All ranges must be
    disjoint, if tables of several semi-joins are interleaved, then the ranges
    are joined together, which is equivalent to converting
      SELECT ... WHERE oe1 IN (SELECT ie1 ...) AND oe2 IN (SELECT ie2 )
    to
      SELECT ... WHERE (oe1, oe2) IN (SELECT ie1, ie2 ... ...)
    .

    Applicability conditions are as follows:

    DuplicateWeedout strategy
    ~~~~~~~~~~~~~~~~~~~~~~~~~

      (ot|nt)*  [ it ((it|ot|nt)* (it|ot))]  (nt)*
      +------+  +=========================+  +---+
        (1)                 (2)               (3)

       (1) - Prefix of OuterTables (those that participate in 
             IN-equality and/or are correlated with subquery) and outer 
             Non-correlated tables.
       (2) - The handled range. The range starts with the first sj-inner
             table, and covers all sj-inner and outer tables 
             Within the range,  Inner, Outer, outer non-correlated tables
             may follow in any order.
       (3) - The suffix of outer non-correlated tables.
    
    FirstMatch strategy
    ~~~~~~~~~~~~~~~~~~~

      (ot|nt)*  [ it ((it|nt)* it) ]  (nt)*
      +------+  +==================+  +---+
        (1)             (2)          (3)

      (1) - Prefix of outer correlated and non-correlated tables
      (2) - The handled range, which may contain only inner and
            non-correlated tables.
      (3) - The suffix of outer non-correlated tables.

    LooseScan strategy 
    ~~~~~~~~~~~~~~~~~~

     (ot|ct|nt) [ loosescan_tbl (ot|nt|it)* it ]  (ot|nt)*
     +--------+   +===========+ +=============+   +------+
        (1)           (2)          (3)              (4)
     
      (1) - Prefix that may contain any outer tables. The prefix must contain
            all the non-trivially correlated outer tables. (non-trivially means
            that the correlation is not just through the IN-equality).
      
      (2) - Inner table for which the LooseScan scan is performed.
            Notice that special requirements for existence of certain indexes
            apply to this table, @see class Loose_scan_opt.

      (3) - The remainder of the duplicate-generating range. It is served by 
            application of FirstMatch strategy. Outer IN-correlated tables
            must be correlated to the LooseScan table but not to the inner
            tables in this range. (Currently, there can be no outer tables
            in this range because of implementation restrictions,
            @see Optimize_table_order::advance_sj_state()).

      (4) - The suffix of outer correlated and non-correlated tables.

    MaterializeLookup strategy
    ~~~~~~~~~~~~~~~~~~~~~~~~~~

     (ot|nt)*  [ it (it)* ]  (nt)*
     +------+  +==========+  +---+
        (1)         (2)        (3)

      (1) - Prefix of outer correlated and non-correlated tables.

      (2) - The handled range, which may contain only inner tables.
            The inner tables are materialized in a temporary table that is
            later used as a lookup structure for the outer correlated tables.

      (3) - The suffix of outer non-correlated tables.

    MaterializeScan strategy
    ~~~~~~~~~~~~~~~~~~~~~~~~~~

     (ot|nt)*  [ it (it)* ]  (ot|nt)*
     +------+  +==========+  +-----+
        (1)         (2)         (3)

      (1) - Prefix of outer correlated and non-correlated tables.

      (2) - The handled range, which may contain only inner tables.
            The inner tables are materialized in a temporary table which is
            later used to setup a scan.

      (3) - The suffix of outer correlated and non-correlated tables.

  Note that MaterializeLookup and MaterializeScan has overlap in their patterns.
  It may be possible to consolidate the materialization strategies into one.
  
  The choice between the strategies is made by the join optimizer (see
  advance_sj_state() and fix_semijoin_strategies()).
  This function sets up all fields/structures/etc needed for execution except
  for setup/initialization of semi-join materialization which is done in 
  setup_materialized_table().
*/

static bool setup_semijoin_dups_elimination(JOIN *join, ulonglong options,
                                            uint no_jbuf_after)
{
  uint tableno;
  THD *thd= join->thd;
  DBUG_ENTER("setup_semijoin_dups_elimination");

  if (join->select_lex->sj_nests.is_empty())
    DBUG_RETURN(FALSE);

  for (tableno= join->const_tables; tableno < join->primary_tables; )
  {
    JOIN_TAB *const tab= join->join_tab + tableno;
    POSITION *const pos= tab->position;
    uint keylen, keyno;
    if (pos->sj_strategy == SJ_OPT_NONE)
    {
      tableno++;  // nothing to do
      continue;
    }
    JOIN_TAB *last_sj_tab= tab + pos->n_sj_tables - 1;
    switch (pos->sj_strategy) {
      case SJ_OPT_MATERIALIZE_LOOKUP:
      case SJ_OPT_MATERIALIZE_SCAN:
        DBUG_ASSERT(false); // Should not occur among "primary" tables
        // Do nothing
        tableno+= pos->n_sj_tables;
        break;
      case SJ_OPT_LOOSE_SCAN:
      {
        DBUG_ASSERT(tab->emb_sj_nest != NULL); // First table must be inner
        /* We jump from the last table to the first one */
        tab->match_tab= last_sj_tab;

        /* For LooseScan, duplicate elimination is based on rows being sorted 
           on key. We need to make sure that range select keeps the sorted index
           order. (When using MRR it may not.)  

           Note: need_sorted_output() implementations for range select classes 
           that do not support sorted output, will trigger an assert. This 
           should not happen since LooseScan strategy is only picked if sorted 
           output is supported.
        */
        tab->sorted= true;
        if (tab->select && tab->select->quick)
        {
          if (tab->select->quick->index == pos->loosescan_key)
            tab->select->quick->need_sorted_output(true);
          else
            tab->select->set_quick(NULL);
        }
        /* Calculate key length */
        keylen= 0;
        keyno= pos->loosescan_key;
        for (uint kp=0; kp < pos->loosescan_parts; kp++)
          keylen += tab->table->key_info[keyno].key_part[kp].store_length;

        tab->loosescan_key_len= keylen;
        if (pos->n_sj_tables > 1)
        {
          last_sj_tab->firstmatch_return= tab;
          last_sj_tab->match_tab= last_sj_tab;
        }
        tableno+= pos->n_sj_tables;
        break;
      }
      case SJ_OPT_DUPS_WEEDOUT:
      {
        DBUG_ASSERT(tab->emb_sj_nest != NULL); // First table must be inner
        /*
          Consider a semijoin of one outer and one inner table, both
          with two rows. The inner table is assumed to be confluent
          (See sj_opt_materialize_lookup)

          If normal nested loop execution is used, we do not need to
          include semi-join outer table rowids in the duplicate
          weedout temp table since NL guarantees that outer table rows
          are encountered only consecutively and because all rows in
          the temp table are deleted for every new outer table
          combination (example is with a confluent inner table):

            ot1.row1|it1.row1 
                 '-> temp table's have_confluent_row == FALSE 
                   |-> output ot1.row1
                   '-> set have_confluent_row= TRUE
            ot1.row1|it1.row2
                 |-> temp table's have_confluent_row == TRUE
                 | '-> do not output ot1.row1
                 '-> no more join matches - set have_confluent_row= FALSE
            ot1.row2|it1.row1 
                 '-> temp table's have_confluent_row == FALSE 
                   |-> output ot1.row2
                   '-> set have_confluent_row= TRUE
              ...                 

          Note: not having outer table rowids in the temp table and
          then emptying the temp table when a new outer table row
          combinition is encountered is an optimization. Including
          outer table rowids in the temp table is not harmful but
          wastes memory.

          Now consider the join buffering algorithms (BNL/BKA). These
          algorithms join each inner row with outer rows in "reverse"
          order compared to NL. Effectively, this means that outer
          table rows may be encountered multiple times in a
          non-consecutive manner:

            NL:                 BNL/BKA:
            ot1.row1|it1.row1   ot1.row1|it1.row1
            ot1.row1|it1.row2   ot1.row2|it1.row1
            ot1.row2|it1.row1   ot1.row1|it1.row2
            ot1.row2|it1.row2   ot1.row2|it1.row2

          It is clear from the above that there is no place we can
          empty the temp table like we do in NL to avoid storing outer
          table rowids. 

          Below we check if join buffering might be used. If so, set
          first_table to the first non-constant table so that outer
          table rowids are included in the temp table. Do not destroy
          other duplicate elimination methods. 
        */
        uint first_table= tableno;
        for (uint sj_tableno= tableno; 
             sj_tableno < tableno + pos->n_sj_tables; 
             sj_tableno++)
        {
          /*
            The final decision on whether or not join buffering will
            be used is taken in setup_join_buffering(), which is
            called from make_join_readinfo()'s main loop.
            setup_join_buffering() needs to know if duplicate weedout is used,
            so moving setup_semijoin_dups_elimination() from before the main
            loop to after it is not possible. I.e.,
            join->join_tab[sj_tableno]->position->use_join_buffer is not
            trustworthy at this point.
          */
          /**
            @todo: merge make_join_readinfo() and
            setup_semijoin_dups_elimination() loops and change the
            following 'if' to

            "if (join->join_tab[sj_tableno]->position->use_join_buffer && 
                 sj_tableno <= no_jbuf_after)".

            For now, use a rough criteria:
          */

          if (sj_tableno <= no_jbuf_after &&
              might_do_join_buffering(join_buffer_alg(thd), 
                                      join->join_tab + sj_tableno))

          {
            /* Join buffering will probably be used */
            first_table= join->const_tables;
            break;
          }
        }

        JOIN_TAB *const first_sj_tab= join->join_tab + first_table;
        if (last_sj_tab->first_inner != NULL &&
            first_sj_tab->first_inner != last_sj_tab->first_inner)
        {
          /*
            The first duplicate weedout table is an outer table of an outer join
            and the last duplicate weedout table is one of the inner tables of
            the outer join.
            In this case, we must assure that all the inner tables of the
            outer join are part of the duplicate weedout operation.
            This is to assure that NULL-extension for inner tables of an
            outer join is performed before duplicate elimination is performed,
            otherwise we will have extra NULL-extended rows being output, which
            should have been eliminated as duplicates.
          */
          JOIN_TAB *tab= last_sj_tab->first_inner;
          /*
            First, locate the table that is the first inner table of the
            outer join operation that first_sj_tab is outer for.
          */
          while (tab->first_upper != NULL &&
                 tab->first_upper != first_sj_tab->first_inner)
            tab= tab->first_upper;
          // Then, extend the range with all inner tables of the join nest:
          if (tab->first_inner->last_inner > last_sj_tab)
            last_sj_tab= tab->first_inner->last_inner;
        }

        SJ_TMP_TABLE::TAB sjtabs[MAX_TABLES];
        SJ_TMP_TABLE::TAB *last_tab= sjtabs;
        uint jt_rowid_offset= 0; // # tuple bytes are already occupied (w/o NULL bytes)
        uint jt_null_bits= 0;    // # null bits in tuple bytes
        /*
          Walk through the range and remember
           - tables that need their rowids to be put into temptable
           - the last outer table
        */
        for (JOIN_TAB *tab_in_range= join->join_tab + first_table; 
             tab_in_range <= last_sj_tab; 
             tab_in_range++)
        {
          if (sj_table_is_included(join, tab_in_range))
          {
            last_tab->join_tab= tab_in_range;
            last_tab->rowid_offset= jt_rowid_offset;
            jt_rowid_offset += tab_in_range->table->file->ref_length;
            if (tab_in_range->table->maybe_null)
            {
              last_tab->null_byte= jt_null_bits / 8;
              last_tab->null_bit= jt_null_bits++;
            }
            last_tab++;
            tab_in_range->table->prepare_for_position();
            tab_in_range->keep_current_rowid= TRUE;
          }
        }

        SJ_TMP_TABLE *sjtbl;
        if (jt_rowid_offset) /* Temptable has at least one rowid */
        {
          uint tabs_size= (last_tab - sjtabs) * sizeof(SJ_TMP_TABLE::TAB);
          if (!(sjtbl= new (thd->mem_root) SJ_TMP_TABLE) ||
              !(sjtbl->tabs= (SJ_TMP_TABLE::TAB*) thd->alloc(tabs_size)))
            DBUG_RETURN(TRUE); /* purecov: inspected */
          memcpy(sjtbl->tabs, sjtabs, tabs_size);
          sjtbl->is_confluent= FALSE;
          sjtbl->tabs_end= sjtbl->tabs + (last_tab - sjtabs);
          sjtbl->rowid_len= jt_rowid_offset;
          sjtbl->null_bits= jt_null_bits;
          sjtbl->null_bytes= (jt_null_bits + 7)/8;
          sjtbl->tmp_table= 
            create_duplicate_weedout_tmp_table(thd, 
                                               sjtbl->rowid_len + 
                                               sjtbl->null_bytes,
                                               sjtbl);
          join->sj_tmp_tables.push_back(sjtbl->tmp_table);
        }
        else
        {
          /* 
            This is confluent case where the entire subquery predicate does 
            not depend on anything at all, ie this is 
              WHERE const IN (uncorrelated select)
          */
          if (!(sjtbl= new (thd->mem_root) SJ_TMP_TABLE))
            DBUG_RETURN(TRUE); /* purecov: inspected */
          sjtbl->tmp_table= NULL;
          sjtbl->is_confluent= TRUE;
          sjtbl->have_confluent_row= FALSE;
        }
        join->join_tab[first_table].flush_weedout_table= sjtbl;
        last_sj_tab->check_weed_out_table= sjtbl;

        tableno+= pos->n_sj_tables;
        break;
      }
      case SJ_OPT_FIRST_MATCH:
      {
        /*
          Setup a "jump" from the last table in the range of inner tables
          to the last outer table before the inner tables.
          If there are outer tables inbetween the inner tables, we have to
          setup a "split jump": Jump from the last inner table to the last
          outer table within the range, then from the last inner table
          before the outer table(s), jump to the last outer table before
          this range of inner tables, etc.
        */
        JOIN_TAB *jump_to= tab - 1;
        DBUG_ASSERT(tab->emb_sj_nest != NULL); // First table must be inner
        for (JOIN_TAB *tab_in_range= tab; 
             tab_in_range <= last_sj_tab; 
             tab_in_range++)
        {
          if (!tab_in_range->emb_sj_nest)
          {
            /*
              Let last non-correlated table be jump target for
              subsequent inner tables.
            */
            jump_to= tab_in_range;
          }
          else
          {
            /*
              Assign jump target for last table in a consecutive range of 
              inner tables.
            */
            if (tab_in_range == last_sj_tab || !(tab_in_range+1)->emb_sj_nest)
            {
              tab_in_range->firstmatch_return= jump_to;
              tab_in_range->match_tab= last_sj_tab;
            }
          }
        }
        tableno+= pos->n_sj_tables;
        break;
      }
    }
  }
  DBUG_RETURN(FALSE);
}


/*
  Destroy all temporary tables created by NL-semijoin runtime
*/

static void destroy_sj_tmp_tables(JOIN *join)
{
  List_iterator<TABLE> it(join->sj_tmp_tables);
  TABLE *table;
  while ((table= it++))
  {
    /* 
      SJ-Materialization tables are initialized for either sequential reading 
      or index lookup, DuplicateWeedout tables are not initialized for read 
      (we only write to them), so need to call ha_index_or_rnd_end.
    */
    table->file->ha_index_or_rnd_end();
    free_tmp_table(join->thd, table);
  }
  join->sj_tmp_tables.empty();
}


/**
  Remove all rows from all temp tables used by NL-semijoin runtime

  @param join  The join to remove tables for

  All rows must be removed from all temporary tables before every join
  re-execution.
*/

static int clear_sj_tmp_tables(JOIN *join)
{
  int res;
  List_iterator<TABLE> it(join->sj_tmp_tables);
  TABLE *table;
  while ((table= it++))
  {
    if ((res= table->file->ha_delete_all_rows()))
      return res; /* purecov: inspected */
  }
  Semijoin_mat_exec *sjm;
  List_iterator<Semijoin_mat_exec> it2(join->sjm_exec_list);
  while ((sjm= it2++))
  {
    JOIN_TAB *const tab= join->join_tab + sjm->mat_table_index;
    DBUG_ASSERT(tab->materialize_table);
    tab->materialized= false;
    // The materialized table must be re-read on next evaluation:
    tab->table->status= STATUS_GARBAGE | STATUS_NOT_FOUND;
  }
  return 0;
}


/**
  Reset the state of this join object so that it is ready for a
  new execution.
*/

void JOIN::reset()
{
  DBUG_ENTER("JOIN::reset");

  unit->offset_limit_cnt= (ha_rows)(select_lex->offset_limit ?
                                    select_lex->offset_limit->val_uint() :
                                    ULL(0));

  first_record= false;
  group_sent= false;

  if (tmp_tables)
  {
    for (uint tmp= primary_tables; tmp < primary_tables + tmp_tables; tmp++)
    {
      TABLE *tmp_table= join_tab[tmp].table;
      if (!tmp_table->is_created())
        continue;
      tmp_table->file->extra(HA_EXTRA_RESET_STATE);
      tmp_table->file->ha_delete_all_rows();
      free_io_cache(tmp_table);
      filesort_free_buffers(tmp_table,0);
    }
  }
  clear_sj_tmp_tables(this);
  if (current_ref_ptrs != items0)
  {
    set_items_ref_array(items0);
    set_group_rpa= false;
  }

  /* need to reset ref access state (see join_read_key) */
  if (join_tab)
    for (uint i= 0; i < tables; i++)
      join_tab[i].ref.key_err= TRUE;

  /* Reset of sum functions */
  if (sum_funcs)
  {
    Item_sum *func, **func_ptr= sum_funcs;
    while ((func= *(func_ptr++)))
      func->clear();
  }

  if (!(select_options & SELECT_DESCRIBE))
    init_ftfuncs(thd, select_lex, test(order));

  DBUG_VOID_RETURN;
}


/**
  Prepare join result.

  @details Prepare join result prior to join execution or describing.
  Instantiate derived tables and get schema tables result if necessary.

  @return
    TRUE  An error during derived or schema tables instantiation.
    FALSE Ok
*/

bool JOIN::prepare_result(List<Item> **columns_list)
{
  DBUG_ENTER("JOIN::prepare_result");

  error= 0;
  /* Create result tables for materialized views. */
  if (!zero_result_cause &&
      select_lex->handle_derived(thd->lex, &mysql_derived_create))
    goto err;

  if (result->prepare2())
    goto err;

  if ((select_lex->options & OPTION_SCHEMA_TABLE) &&
      get_schema_tables_result(this, PROCESSED_BY_JOIN_EXEC))
    goto err;

  DBUG_RETURN(FALSE);

err:
  error= 1;
  DBUG_RETURN(TRUE);
}


/**
  Explain join.
*/

void
JOIN::explain()
{
  Opt_trace_context * const trace= &thd->opt_trace;
  Opt_trace_object trace_wrapper(trace);
  Opt_trace_object trace_exec(trace, "join_explain");
  trace_exec.add_select_number(select_lex->select_number);
  Opt_trace_array trace_steps(trace, "steps");
  List<Item> *columns_list= &fields_list;
  DBUG_ENTER("JOIN::explain");

  THD_STAGE_INFO(thd, stage_explaining);

  if (prepare_result(&columns_list))
    DBUG_VOID_RETURN;

  if (!tables_list && (tables || !select_lex->with_sum_func))
  {                                           // Only test of functions
    explain_no_table(thd, this, zero_result_cause ? zero_result_cause 
                                                  : "No tables used");
    /* Single select (without union) always returns 0 or 1 row */
    thd->limit_found_rows= send_records;
    thd->set_examined_row_count(0);
    DBUG_VOID_RETURN;
  }
  /*
    Don't reset the found rows count if there're no tables as
    FOUND_ROWS() may be called. Never reset the examined row count here.
    It must be accumulated from all join iterations of all join parts.
  */
  if (tables)
    thd->limit_found_rows= 0;

  if (zero_result_cause)
  {
    explain_no_table(thd, this, zero_result_cause);
    DBUG_VOID_RETURN;
  }

  if (tables)
    explain_query_specification(thd, this);
  else
    explain_no_table(thd, this, "No tables used");

  DBUG_VOID_RETURN;
}


/**
  Clean up and destroy join object.

  @return false if previous execution was successful, and true otherwise
*/

bool JOIN::destroy()
{
  DBUG_ENTER("JOIN::destroy");
  select_lex->join= 0;

  cond_equal= 0;

  cleanup(1);

  if (join_tab) // We should not have tables > 0 and join_tab != NULL
  for (uint i= 0; i < tables; i++)
  {
    JOIN_TAB *const tab= join_tab + i;

    DBUG_ASSERT(!tab->table || !tab->table->sort.record_pointers);
    if (tab->op)
    {
      if (tab->op->type() == QEP_operation::OT_TMP_TABLE)
      {
        free_tmp_table(thd, tab->table);
        delete tab->tmp_table_param;
        tab->tmp_table_param= NULL;
      }
      tab->op->free();
      tab->op= NULL;
    }

    tab->table= NULL;
  }
 /* Cleanup items referencing temporary table columns */
  cleanup_item_list(tmp_all_fields1);
  cleanup_item_list(tmp_all_fields3);
  destroy_sj_tmp_tables(this);

  List_iterator<Semijoin_mat_exec> sjm_list_it(sjm_exec_list);
  Semijoin_mat_exec *sjm;
  while ((sjm= sjm_list_it++))
    delete sjm;
  sjm_exec_list.empty();

  keyuse.clear();
  DBUG_RETURN(test(error));
}


void JOIN::cleanup_item_list(List<Item> &items) const
{
  if (!items.is_empty())
  {
    List_iterator_fast<Item> it(items);
    Item *item;
    while ((item= it++))
      item->cleanup();
  }
}


/**
  Prepare stage of mysql_select.

  @param thd                  thread handler
                              the top-level select_lex for this query
  @param tables               list of all tables used in this query.
                              The tables have been pre-opened.
  @param wild_num             number of wildcards used in the top level 
                              select of this query.
                              For example statement
                              SELECT *, t1.*, catalog.t2.* FROM t0, t1, t2;
                              has 3 wildcards.
  @param fields               list of items in SELECT list of the top-level
                              select
                              e.g. SELECT a, b, c FROM t1 will have Item_field
                              for a, b and c in this list.
  @param conds                top level item of an expression representing
                              WHERE clause of the top level select
  @param og_num               total number of ORDER BY and GROUP BY clauses
                              arguments
  @param order                linked list of ORDER BY agruments
  @param group                linked list of GROUP BY arguments
  @param having               top level item of HAVING expression
  @param select_options       select options (BIG_RESULT, etc)
  @param result               an instance of result set handling class.
                              This object is responsible for send result
                              set rows to the client or inserting them
                              into a table.
  @param unit                 top-level UNIT of this query
                              UNIT is an artificial object created by the
                              parser for every SELECT clause.
                              e.g.
                              SELECT * FROM t1 WHERE a1 IN (SELECT * FROM t2)
                              has 2 unions.
  @param select_lex           the only SELECT_LEX of this query
  @param[out] free_join       Will be set to false if select_lex->join does
                              not need to be freed.

  @retval
    false  success
  @retval
    true   an error

  @note tables must be opened before calling mysql_prepare_select.
*/

static bool
mysql_prepare_select(THD *thd,
                     TABLE_LIST *tables, uint wild_num, List<Item> &fields,
                     Item *conds, uint og_num,  ORDER *order, ORDER *group,
                     Item *having, ulonglong select_options,
                     select_result *result, SELECT_LEX_UNIT *unit,
                     SELECT_LEX *select_lex, bool *free_join)
{
  bool err= false;
  JOIN *join;

  DBUG_ENTER("mysql_prepare_select");
  select_lex->context.resolve_in_select_list= TRUE;
  if (select_lex->join != 0)
  {
    join= select_lex->join;
    /*
      is it single SELECT in derived table, called in derived table
      creation
    */
    if (select_lex->linkage != DERIVED_TABLE_TYPE ||
	(select_options & SELECT_DESCRIBE))
    {
      if (select_lex->linkage != GLOBAL_OPTIONS_TYPE)
      {
	//here is EXPLAIN of subselect or derived table
	if (join->change_result(result))
	{
	  DBUG_RETURN(TRUE);
	}
        /*
          Original join tabs might be overwritten at first
          subselect execution. So we need to restore them.
        */
        Item_subselect *subselect= select_lex->master_unit()->item;
        if (subselect && subselect->is_uncacheable())
          join->reset();
      }
      else
      {
        err= join->prepare(tables, wild_num,
                           conds, og_num, order, group, having,
                           select_lex, unit);
        if (err)
          DBUG_RETURN(true);
      }
    }
    *free_join= false;
    join->select_options= select_options;
  }
  else
  {
    if (!(join= new JOIN(thd, fields, select_options, result)))
	DBUG_RETURN(TRUE); /* purecov: inspected */
    THD_STAGE_INFO(thd, stage_init);
    thd->lex->used_tables=0;                         // Updated by setup_fields
    err= join->prepare(tables, wild_num,
                       conds, og_num, order, group, having,
                       select_lex, unit);
    if (err)
      DBUG_RETURN(true);
  }

  DBUG_RETURN(err);
}


/**
  Execute stage of mysql_select.

  @param thd                  thread handler
  @param select_lex           the only SELECT_LEX of this query
  @param free_join            if join should be freed

  @return Operation status
    @retval false  success
    @retval true   an error

  @note tables must be opened and locked before calling mysql_execute_select.
*/

static bool
mysql_execute_select(THD *thd, SELECT_LEX *select_lex, bool free_join)
{
  bool err;
  JOIN* join= select_lex->join;

  DBUG_ENTER("mysql_execute_select");
  DBUG_ASSERT(join);

  if ((err= join->optimize()))
  {
    goto err;					// 1
  }

  if (thd->is_error())
    goto err;

  if (join->select_options & SELECT_DESCRIBE)
  {
    join->explain();
    free_join= false;
  }
  else
    join->exec();

err:
  if (free_join)
  {
    THD_STAGE_INFO(thd, stage_end);
    err|= select_lex->cleanup();
    DBUG_RETURN(err || thd->is_error());
  }
  DBUG_RETURN(join->error);
}


/**
  An entry point to single-unit select (a select without UNION).

  @param thd                  thread handler
  @param tables               list of all tables used in this query.
                              The tables have been pre-opened.
  @param wild_num             number of wildcards used in the top level 
                              select of this query.
                              For example statement
                              SELECT *, t1.*, catalog.t2.* FROM t0, t1, t2;
                              has 3 wildcards.
  @param fields               list of items in SELECT list of the top-level
                              select
                              e.g. SELECT a, b, c FROM t1 will have Item_field
                              for a, b and c in this list.
  @param conds                top level item of an expression representing
                              WHERE clause of the top level select
  @param order                linked list of ORDER BY agruments
  @param group                linked list of GROUP BY arguments
  @param having               top level item of HAVING expression
  @param select_options       select options (BIG_RESULT, etc)
  @param result               an instance of result set handling class.
                              This object is responsible for send result
                              set rows to the client or inserting them
                              into a table.
  @param unit                 top-level UNIT of this query
                              UNIT is an artificial object created by the
                              parser for every SELECT clause.
                              e.g.
                              SELECT * FROM t1 WHERE a1 IN (SELECT * FROM t2)
                              has 2 unions.
  @param select_lex           the only SELECT_LEX of this query

  @retval
    false  success
  @retval
    true   an error
*/

bool
mysql_select(THD *thd,
             TABLE_LIST *tables, uint wild_num, List<Item> &fields,
             Item *conds, SQL_I_List<ORDER> *order, SQL_I_List<ORDER> *group,
             Item *having, ulonglong select_options,
             select_result *result, SELECT_LEX_UNIT *unit,
             SELECT_LEX *select_lex)
{
  bool free_join= true;
  uint og_num= 0;
  ORDER *first_order= NULL;
  ORDER *first_group= NULL;
  DBUG_ENTER("mysql_select");

  if (order)
  {
    og_num= order->elements;
    first_order= order->first;
  }
  if (group)
  {
    og_num+= group->elements;
    first_group= group->first;
  }

  if (mysql_prepare_select(thd, tables, wild_num, fields,
                           conds, og_num, first_order, first_group, having,
                           select_options, result, unit,
                           select_lex, &free_join))
  {
    if (free_join)
    {
      THD_STAGE_INFO(thd, stage_end);
      (void) select_lex->cleanup();
    }
    DBUG_RETURN(true);
  }

  if (! thd->lex->is_query_tables_locked())
  {
    /*
      If tables are not locked at this point, it means that we have delayed
      this step until after prepare stage (i.e. this moment). This allows to
      do better partition pruning and avoid locking unused partitions.
      As a consequence, in such a case, prepare stage can rely only on
      metadata about tables used and not data from them.
      We need to lock tables now in order to proceed with the remaning
      stages of query optimization and execution.
    */
    if (lock_tables(thd, thd->lex->query_tables, thd->lex->table_count, 0))
    {
      if (free_join)
      {
        THD_STAGE_INFO(thd, stage_end);
        (void) select_lex->cleanup();
      }
      DBUG_RETURN(true);
    }

    /*
      Only register query in cache if it tables were locked above.

      Tables must be locked before storing the query in the query cache.
      Transactional engines must been signalled that the statement started,
      which external_lock signals.
    */
    query_cache_store_query(thd, thd->lex->query_tables);
  }

  DBUG_RETURN(mysql_execute_select(thd, select_lex, free_join));
}

/*****************************************************************************
  Go through all combinations of not marked tables and find the one
  which uses least records
*****************************************************************************/

/**
   Returns which join buffer algorithms are enabled for this session.

   @param thd the @c THD for this session

   @return bitmap with available join buffer algorithms
*/

static uint join_buffer_alg(const THD *thd)
{
  uint alg= JOIN_CACHE::ALG_NONE;

  if (thd->optimizer_switch_flag(OPTIMIZER_SWITCH_BNL))
    alg|= JOIN_CACHE::ALG_BNL;

  if (thd->optimizer_switch_flag(OPTIMIZER_SWITCH_BKA))
  {
    bool use_bka_unique= false;
    DBUG_EXECUTE_IF("test_bka_unique", use_bka_unique= true;);
    
    if (use_bka_unique)
      alg|= JOIN_CACHE::ALG_BKA_UNIQUE;
    else
      alg|= JOIN_CACHE::ALG_BKA;
  }

  return alg;
}


/**
  Find how much space the prevous read not const tables takes in cache.
*/

void calc_used_field_length(THD *thd, JOIN_TAB *join_tab)
{
  uint null_fields,blobs,fields,rec_length;
  Field **f_ptr,*field;
  uint uneven_bit_fields;
  MY_BITMAP *read_set= join_tab->table->read_set;

  uneven_bit_fields= null_fields= blobs= fields= rec_length=0;
  for (f_ptr=join_tab->table->field ; (field= *f_ptr) ; f_ptr++)
  {
    if (bitmap_is_set(read_set, field->field_index))
    {
      uint flags=field->flags;
      fields++;
      rec_length+=field->pack_length();
      if (flags & BLOB_FLAG)
	blobs++;
      if (!(flags & NOT_NULL_FLAG))
        null_fields++;
      if (field->type() == MYSQL_TYPE_BIT &&
          ((Field_bit*)field)->bit_len)
        uneven_bit_fields++;
    }
  }
  if (null_fields || uneven_bit_fields)
    rec_length+=(join_tab->table->s->null_fields+7)/8;
  if (join_tab->table->maybe_null)
    rec_length+=sizeof(my_bool);
  if (blobs)
  {
    uint blob_length=(uint) (join_tab->table->file->stats.mean_rec_length-
			     (join_tab->table->s->reclength- rec_length));
    rec_length+= max<uint>(4U, blob_length);
  }
  /**
    @todo why don't we count the rowids that we might need to store
    when using DuplicateElimination?
  */
  join_tab->used_fields=fields;
  join_tab->used_fieldlength=rec_length;
  join_tab->used_blobs=blobs;
  join_tab->used_null_fields= null_fields;
  join_tab->used_uneven_bit_fields= uneven_bit_fields;
}


/**
  Set up JOIN_TAB structs according to the picked join order in best_positions.
  This allocates execution structures so may be called only after we have the
  very final plan. It must be called after
  Optimize_table_order::fix_semijoin_strategies().

  @return False if success, True if error

  @details
    - create join->join_tab array and copy from existing JOIN_TABs in join order
    - create helper structs for materialized semi-join handling
    - finalize semi-join strategy choices
    - Number of intermediate tables "tmp_tables" is calculated.
    - "tables" and "primary_tables" are recalculated.

   Notice that intermediate tables will not have a POSITION reference; and they
   will not have a TABLE reference before the final stages of code generation.
*/

bool JOIN::get_best_combination()
{
  DBUG_ENTER("JOIN::get_best_combination");

  // At this point "tables" and "primary"tables" represent the same:
  DBUG_ASSERT(tables == primary_tables);

  /*
    Allocate additional space for tmp tables.
    Number of plan nodes:
      # of regular input tables (including semi-joined ones) +
      # of semi-join nests for materialization +
      1? + // For GROUP BY
      1? + // For DISTINCT
      1? + // For ORDER BY
      1?   // buffer result
    Up to 2 tmp tables are actually used, but it's hard to tell exact number
    at this stage.
  */
  uint tmp_tables= (group_list ? 1 : 0) +
                   (select_distinct ? 1 : 0) +
                   (order ? 1 : 0) +
       (select_options & (SELECT_BIG_RESULT | OPTION_BUFFER_RESULT) ? 1 : 0) ;
  if (tmp_tables > 2)
    tmp_tables= 2;

  /*
    Rearrange queries with materialized semi-join nests so that the semi-join
    nest is replaced with a reference to a materialized temporary table and all
    materialized subquery tables are placed after the intermediate tables.
    After the following loop, "inner_target" is the position of the first
    subquery table (if any). "outer_target" is the position of first outer
    table, and will later be used to track the position of any materialized
    temporary tables. 
  */
  uint outer_target= 0;                   
  uint inner_target= primary_tables + tmp_tables;
  uint sjm_nests= 0;

  for (uint tableno= 0; tableno < primary_tables; )
  {
    if (sj_is_materialize_strategy(best_positions[tableno].sj_strategy))
    {
      sjm_nests++;
      inner_target-= (best_positions[tableno].n_sj_tables - 1);
      tableno+= best_positions[tableno].n_sj_tables;
    }
    else
      tableno++;
  }

  if (!(join_tab= new(thd->mem_root) JOIN_TAB[tables + sjm_nests + tmp_tables]))
    DBUG_RETURN(true);

  int sjm_index= tables;  // Number assigned to materialized temporary table
  int remaining_sjm_inner= 0;
  for (uint tableno= 0; tableno < tables; tableno++)
  {
    if (sj_is_materialize_strategy(best_positions[tableno].sj_strategy))
    {
      DBUG_ASSERT(outer_target < inner_target);

      POSITION *const pos_table= best_positions + tableno;
      TABLE_LIST *const sj_nest= pos_table->table->emb_sj_nest;

      // Handle this many inner tables of materialized semi-join
      remaining_sjm_inner= pos_table->n_sj_tables;

      Semijoin_mat_exec *const sjm_exec=
        new (thd->mem_root)
        Semijoin_mat_exec(sj_nest,
                          (pos_table->sj_strategy == SJ_OPT_MATERIALIZE_SCAN),
                          remaining_sjm_inner, outer_target, inner_target);
      if (!sjm_exec)
        DBUG_RETURN(true);

      (join_tab + outer_target)->sj_mat_exec= sjm_exec;

      if (setup_materialized_table(join_tab + outer_target, sjm_index,
                                   pos_table, best_positions + sjm_index))
        DBUG_RETURN(true);

      map2table[sjm_exec->table->tablenr]= join_tab + outer_target;

      outer_target++;
      sjm_index++;
    }
    /*
      Locate join_tab target for the table we are considering.
      (remaining_sjm_inner becomes negative for non-SJM tables, this can be
       safely ignored).
    */
    const uint target=
      (remaining_sjm_inner--) > 0 ? inner_target++ : outer_target++;
    JOIN_TAB *const tab= join_tab + target;

    // Copy data from existing join_tab
    *tab= *best_positions[tableno].table;

    tab->position= best_positions + tableno;

    TABLE *const table= tab->table;
    table->reginfo.join_tab= tab;
    if (!*tab->on_expr_ref)
      table->reginfo.not_exists_optimize= false;     // Only with LEFT JOIN
    map2table[table->tablenr]= tab;
  }

  // Count the materialized semi-join tables as regular input tables
  tables+= sjm_nests + tmp_tables;
  // Set the number of non-materialized tables:
  primary_tables= outer_target;

  set_semijoin_info();

  // Update equalities and keyuses after having added semi-join materialization
  if (update_equalities_for_sjm())
    DBUG_RETURN(true);

  // sjm is no longer needed, trash it. To reuse it, reset its members!
  List_iterator<TABLE_LIST> sj_list_it(select_lex->sj_nests);
  TABLE_LIST *sj_nest;
  while ((sj_nest= sj_list_it++))
    TRASH(&sj_nest->nested_join->sjm, sizeof(sj_nest->nested_join->sjm));

  DBUG_RETURN(false);
}


/**
  Set access methods for the tables of a query plan.

  @return False if success, True if error

  @details
    We need to fill in data for the case where
     - There is no key selected (use JT_ALL)
     - Loose scan semi-join strategy is selected (use JT_ALL)
     - A ref key can be used (use JT_REF, JT_REF_OR_NULL, JT_EQ_REF or JT_FT)

  @note We cannot setup fields used for ref access before we have sorted
        the items within multiple equalities according to the final order of
        the tables involved in the join operation. Currently, this occurs in
        @see substitute_for_best_equal_field().
*/
bool JOIN::set_access_methods()
{
  DBUG_ENTER("JOIN::set_access_methods");

  full_join= false;

  for (uint tableno= 0; tableno < tables; tableno++)
  {
    JOIN_TAB *const tab= join_tab + tableno;

    if (!tab->position)
      continue;

    DBUG_PRINT("info",("type: %d", tab->type));

    // Set preliminary join cache setting based on decision from greedy search
    tab->use_join_cache= tab->position->use_join_buffer ?
                           JOIN_CACHE::ALG_BNL : JOIN_CACHE::ALG_NONE;

    if (tab->type == JT_CONST || tab->type == JT_SYSTEM)
      continue;                      // Handled in make_join_statistics()

    Key_use *const keyuse= tab->position->key;
    if (!keyuse)
    {
      tab->type= JT_ALL;
      if (tableno > const_tables)
       full_join= true;
     }
    else if (tab->position->sj_strategy == SJ_OPT_LOOSE_SCAN)
    {
      DBUG_ASSERT(tab->keys.is_set(tab->position->loosescan_key));
      tab->type= JT_ALL; // @todo is this consistent for a LooseScan table ?
      tab->index= tab->position->loosescan_key;
     }
    else
    {
      if (create_ref_for_key(this, tab, keyuse, tab->prefix_tables()))
        DBUG_RETURN(true);
    }
   }

  DBUG_RETURN(false);
}


/**
  Set the first_sj_inner_tab and last_sj_inner_tab fields for all tables
  inside the semijoin nests of the query.
*/
void JOIN::set_semijoin_info()
{
  if (select_lex->sj_nests.is_empty())
    return;

  for (uint tableno= const_tables; tableno < tables; )
  {
    JOIN_TAB *const tab= join_tab + tableno;
    const POSITION *const pos= tab->position;

    if (!pos)
    {
      tableno++;
      continue;
    }
    switch (pos->sj_strategy)
    {
    case SJ_OPT_NONE:
      tableno++;
      break;
    case SJ_OPT_MATERIALIZE_LOOKUP:
    case SJ_OPT_MATERIALIZE_SCAN:
    case SJ_OPT_LOOSE_SCAN:
    case SJ_OPT_DUPS_WEEDOUT:
    case SJ_OPT_FIRST_MATCH:
      /*
        Remember the first and last semijoin inner tables; this serves to tell
        a JOIN_TAB's semijoin strategy (like in setup_join_buffering()).
      */
      JOIN_TAB *last_sj_tab= tab + pos->n_sj_tables - 1;
      JOIN_TAB *last_sj_inner=
        (pos->sj_strategy == SJ_OPT_DUPS_WEEDOUT) ?
        /* Range may end with non-inner table so cannot set last_sj_inner_tab */
        NULL : last_sj_tab;
      for (JOIN_TAB *tab_in_range= tab;
           tab_in_range <= last_sj_tab;
           tab_in_range++)
      {
        tab_in_range->first_sj_inner_tab= tab;
        tab_in_range->last_sj_inner_tab=  last_sj_inner;
      }
      tableno+= pos->n_sj_tables;
      break;
    }
  }
}


/**
  Setup a ref access for looking up rows via an index (a key).

  @param join          The join object being handled
  @param j             The join_tab which will have the ref access populated
  @param first_keyuse  First key part of (possibly multi-part) key
  @param used_tables   Bitmap of available tables

  @return False if success, True if error

  @details
    This function will set up a ref access using the best key found
    during access path analysis and cost analysis.

  @note We cannot setup fields used for ref access before we have sorted
        the items within multiple equalities according to the final order of
        the tables involved in the join operation. Currently, this occurs in
        @see substitute_for_best_equal_field().
        The exception is ref access for const tables, which are fixed
        before the greedy search planner is invoked.  
*/

bool create_ref_for_key(JOIN *join, JOIN_TAB *j, Key_use *org_keyuse,
                        table_map used_tables)
{
  DBUG_ENTER("create_ref_for_key");

  Key_use *keyuse= org_keyuse;
  const uint key= keyuse->key;
  const bool ftkey= (keyuse->keypart == FT_KEYPART);
  THD  *const thd= join->thd;
  uint keyparts, length;
  TABLE *const table= j->table;
  KEY   *const keyinfo= table->key_info+key;
  Key_use *chosen_keyuses[MAX_REF_PARTS];

  DBUG_ASSERT(j->keys.is_set(org_keyuse->key));

  if (ftkey)
  {
    Item_func_match *ifm=(Item_func_match *)keyuse->val;

    length=0;
    keyparts=1;
    ifm->join_key=1;
  }
  else
  {
    keyparts=length=0;
    uint found_part_ref_or_null= 0;
    // Calculate length for the used key. Remember chosen Key_use-s.
    do
    {
      /*
        This Key_use is chosen if:
        - it involves a key part at the right place (if index is (a,b) we
        can have a search criterion on 'b' only if we also have a criterion
        on 'a'),
        - it references only tables earlier in the plan.
        Moreover, the execution layer is limited to maximum one ref_or_null
        keypart, as TABLE_REF::null_ref_key is only one byte.
      */
      if (!(~used_tables & keyuse->used_tables) &&
          keyparts == keyuse->keypart &&
          !(found_part_ref_or_null & keyuse->optimize))
      {
        DBUG_ASSERT(keyparts <= MAX_REF_PARTS);
        chosen_keyuses[keyparts]= keyuse;
        keyparts++;
        length+= keyinfo->key_part[keyuse->keypart].store_length;
        found_part_ref_or_null|= keyuse->optimize;
      }
      keyuse++;
    } while (keyuse->table == table && keyuse->key == key);
    DBUG_ASSERT(length > 0 && keyparts != 0);
  } /* not ftkey */

  DBUG_ASSERT(keyparts > 0);

  /* set up fieldref */
  j->ref.key_parts=keyparts;
  j->ref.key_length=length;
  j->ref.key=(int) key;
  if (!(j->ref.key_buff= (uchar*) thd->calloc(ALIGN_SIZE(length)*2)) ||
      !(j->ref.key_copy= (store_key**) thd->alloc((sizeof(store_key*) *
                                                   (keyparts)))) ||
      !(j->ref.items=    (Item**) thd->alloc(sizeof(Item*)*keyparts)) ||
      !(j->ref.cond_guards= (bool**) thd->alloc(sizeof(uint*)*keyparts)))
  {
    DBUG_RETURN(TRUE);
  }
  j->ref.key_buff2=j->ref.key_buff+ALIGN_SIZE(length);
  j->ref.key_err=1;
  j->ref.has_record= FALSE;
  j->ref.null_rejecting= 0;
  j->ref.use_count= 0;
  j->ref.disable_cache= FALSE;
  keyuse=org_keyuse;

  uchar *key_buff= j->ref.key_buff;
  uchar *null_ref_key= NULL;
  bool keyuse_uses_no_tables= true;
  if (ftkey)
  {
    j->ref.items[0]=((Item_func*)(keyuse->val))->key_item();
    /* Predicates pushed down into subquery can't be used FT access */
    j->ref.cond_guards[0]= NULL;
    if (keyuse->used_tables)
      DBUG_RETURN(TRUE);                        // not supported yet. SerG

    j->type=JT_FT;
    memset(j->ref.key_copy, 0, sizeof(j->ref.key_copy[0]) * keyparts);
  }
  else
  {
    // Set up TABLE_REF based on chosen Key_use-s.
    for (uint part_no= 0 ; part_no < keyparts ; part_no++)
    {
      keyuse= chosen_keyuses[part_no];
      uint maybe_null= test(keyinfo->key_part[part_no].null_bit);

      if (keyuse->val->type() == Item::FIELD_ITEM)
      {
        // Look up the most appropriate field to base the ref access on.
        keyuse->val= get_best_field(static_cast<Item_field *>(keyuse->val),
                                    join->cond_equal);
        keyuse->used_tables= keyuse->val->used_tables();
      }
      j->ref.items[part_no]=keyuse->val;        // Save for cond removal
      j->ref.cond_guards[part_no]= keyuse->cond_guard;
      if (keyuse->null_rejecting) 
        j->ref.null_rejecting|= (key_part_map)1 << part_no;
      keyuse_uses_no_tables= keyuse_uses_no_tables && !keyuse->used_tables;

      store_key* key= get_store_key(thd,
                                    keyuse,join->const_table_map,
                                    &keyinfo->key_part[part_no],
                                    key_buff, maybe_null);
      if (unlikely(!key || thd->is_fatal_error))
        DBUG_RETURN(TRUE);

      if (keyuse->used_tables || thd->lex->describe)
        /* 
          Comparing against a non-constant or executing an EXPLAIN
          query (which refers to this info when printing the 'ref'
          column of the query plan)
        */
        j->ref.key_copy[part_no]= key;
      else
      {
        /*
          key is const, copy value now and possibly skip it while ::exec().

          Note:
            Result check of store_key::copy() is unnecessary,
            it could be an error returned by store_key::copy() method
            but stored value is not null and default value could be used
            in this case. Methods which used for storing the value
            should be responsible for proper null value setting
            in case of an error. Thus it's enough to check key->null_key
            value only.
        */
        (void) key->copy();
        /*
          It should be reevaluated in ::exec() if
          constant evaluated to NULL value which we might need to 
          handle as a special case during JOIN::exec()
          (As in : 'Full scan on NULL key')
        */
        if (key->null_key)
          j->ref.key_copy[part_no]= key; // Reevaluate in JOIN::exec()
        else
          j->ref.key_copy[part_no]= NULL;
      }
      /*
	Remember if we are going to use REF_OR_NULL
	But only if field _really_ can be null i.e. we force JT_REF
	instead of JT_REF_OR_NULL in case if field can't be null
      */
      if ((keyuse->optimize & KEY_OPTIMIZE_REF_OR_NULL) && maybe_null)
      {
        DBUG_ASSERT(null_ref_key == NULL); // or we would overwrite it below
        null_ref_key= key_buff;
      }
      key_buff+=keyinfo->key_part[part_no].store_length;
    }
  } /* not ftkey */
  if (j->type == JT_FT)
    DBUG_RETURN(false);
  if (j->type == JT_CONST)
    j->table->const_table= 1;
  else if (((actual_key_flags(keyinfo) & 
             (HA_NOSAME | HA_NULL_PART_KEY)) != HA_NOSAME) ||
	   keyparts != actual_key_parts(keyinfo) || null_ref_key)
  {
    /* Must read with repeat */
    j->type= null_ref_key ? JT_REF_OR_NULL : JT_REF;
    j->ref.null_ref_key= null_ref_key;
  }
  else if (keyuse_uses_no_tables &&
           !(table->file->ha_table_flags() & HA_BLOCK_CONST_TABLE))
  {
    /*
      This happen if we are using a constant expression in the ON part
      of an LEFT JOIN.
      SELECT * FROM a LEFT JOIN b ON b.key=30
      Here we should not mark the table as a 'const' as a field may
      have a 'normal' value or a NULL value.
    */
    j->type=JT_CONST;
  }
  else
    j->type=JT_EQ_REF;
  DBUG_RETURN(false);
}



static store_key *
get_store_key(THD *thd, Key_use *keyuse, table_map used_tables,
	      KEY_PART_INFO *key_part, uchar *key_buff, uint maybe_null)
{
  if (!((~used_tables) & keyuse->used_tables))		// if const item
  {
    return new store_key_const_item(thd,
				    key_part->field,
				    key_buff + maybe_null,
				    maybe_null ? key_buff : 0,
				    key_part->length,
				    keyuse->val);
  }

  Item_field *field_item= NULL;
  if (keyuse->val->type() == Item::FIELD_ITEM)  
    field_item= static_cast<Item_field*>(keyuse->val->real_item());
  else if (keyuse->val->type() == Item::REF_ITEM)
  {
    Item_ref *item_ref= static_cast<Item_ref*>(keyuse->val);
    if (item_ref->ref_type() == Item_ref::OUTER_REF)
    {
      if ((*item_ref->ref)->type() == Item::FIELD_ITEM)
        field_item= static_cast<Item_field*>(item_ref->real_item());
      else if ((*(Item_ref**)(item_ref)->ref)->ref_type()
               == Item_ref::DIRECT_REF
               && 
               item_ref->real_item()->type() == Item::FIELD_ITEM)
        field_item= static_cast<Item_field*>(item_ref->real_item());
    }
  }
  if (field_item)
    return new store_key_field(thd,
                               key_part->field,
                               key_buff + maybe_null,
                               maybe_null ? key_buff : 0,
                               key_part->length,
                               field_item->field,
                               keyuse->val->full_name());

  return new store_key_item(thd,
			    key_part->field,
			    key_buff + maybe_null,
			    maybe_null ? key_buff : 0,
			    key_part->length,
			    keyuse->val);
}


/**
  Extend e1 by AND'ing e2 to the condition e1 points to. The resulting
  condition is fixed. Requirement: the input Items must already have
  been fixed.

  @param[in,out]   e1 Pointer to condition that will be extended with e2
  @param           e2 Condition that will extend e1

  @retval true   if there was a memory allocation error, in which case
                 e1 remains unchanged
  @retval false  otherwise
*/

bool and_conditions(Item **e1, Item *e2)
{
  DBUG_ASSERT(!(*e1) || (*e1)->fixed);
  DBUG_ASSERT(!e2 || e2->fixed);
  if (*e1)
  {
    if (!e2)
      return false;
    Item *res= new Item_cond_and(*e1, e2);
    if (unlikely(!res))
      return true;

    *e1= res;
    res->quick_fix_field();
    res->update_used_tables();

  }
  else
    *e1= e2;
  return false;
}


#define ICP_COND_USES_INDEX_ONLY 10

/*
  Get a part of the condition that can be checked using only index fields

  SYNOPSIS
    make_cond_for_index()
      cond           The source condition
      table          The table that is partially available
      keyno          The index in the above table. Only fields covered by the index
                     are available
      other_tbls_ok  TRUE <=> Fields of other non-const tables are allowed

  DESCRIPTION
    Get a part of the condition that can be checked when for the given table 
    we have values only of fields covered by some index. The condition may
    refer to other tables, it is assumed that we have values of all of their 
    fields.

    Example:
      make_cond_for_index(
         "cond(t1.field) AND cond(t2.key1) AND cond(t2.non_key) AND cond(t2.key2)",
          t2, keyno(t2.key1)) 
      will return
        "cond(t1.field) AND cond(t2.key2)"

  RETURN
    Index condition, or NULL if no condition could be inferred.
*/

static Item *make_cond_for_index(Item *cond, TABLE *table, uint keyno,
                                 bool other_tbls_ok)
{
  DBUG_ASSERT(cond != NULL);

  if (cond->type() == Item::COND_ITEM)
  {
    uint n_marked= 0;
    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      table_map used_tables= 0;
      Item_cond_and *new_cond=new Item_cond_and;
      if (!new_cond)
	return NULL;
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item=li++))
      {
	Item *fix= make_cond_for_index(item, table, keyno, other_tbls_ok);
	if (fix)
        {
	  new_cond->argument_list()->push_back(fix);
          used_tables|= fix->used_tables();
        }
        n_marked += test(item->marker == ICP_COND_USES_INDEX_ONLY);
      }
      if (n_marked ==((Item_cond*)cond)->argument_list()->elements)
        cond->marker= ICP_COND_USES_INDEX_ONLY;
      switch (new_cond->argument_list()->elements) {
      case 0:
	return NULL;
      case 1:
        new_cond->set_used_tables(used_tables);
	return new_cond->argument_list()->head();
      default:
	new_cond->quick_fix_field();
        new_cond->set_used_tables(used_tables);
	return new_cond;
      }
    }
    else /* It's OR */
    {
      Item_cond_or *new_cond=new Item_cond_or;
      if (!new_cond)
	return NULL;
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item=li++))
      {
	Item *fix= make_cond_for_index(item, table, keyno, other_tbls_ok);
	if (!fix)
	  return NULL;
	new_cond->argument_list()->push_back(fix);
        n_marked += test(item->marker == ICP_COND_USES_INDEX_ONLY);
      }
      if (n_marked ==((Item_cond*)cond)->argument_list()->elements)
        cond->marker= ICP_COND_USES_INDEX_ONLY;
      new_cond->quick_fix_field();
      new_cond->set_used_tables(cond->used_tables());
      new_cond->top_level_item();
      return new_cond;
    }
  }

  if (!uses_index_fields_only(cond, table, keyno, other_tbls_ok))
  {
    /* 
      Reset marker since it might have the value
      ICP_COND_USES_INDEX_ONLY if this condition is part of the select
      condition for multiple tables.
    */
    cond->marker= 0;
    return NULL;
  }
  cond->marker= ICP_COND_USES_INDEX_ONLY;
  return cond;
}


static Item *make_cond_remainder(Item *cond, bool exclude_index)
{
  if (exclude_index && cond->marker == ICP_COND_USES_INDEX_ONLY)
    return 0; /* Already checked */

  if (cond->type() == Item::COND_ITEM)
  {
    table_map tbl_map= 0;
    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      /* Create new top level AND item */
      Item_cond_and *new_cond=new Item_cond_and;
      if (!new_cond)
	return (Item*) 0;
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item=li++))
      {
	Item *fix= make_cond_remainder(item, exclude_index);
	if (fix)
        {
	  new_cond->argument_list()->push_back(fix);
          tbl_map |= fix->used_tables();
        }
      }
      switch (new_cond->argument_list()->elements) {
      case 0:
	return (Item*) 0;
      case 1:
	return new_cond->argument_list()->head();
      default:
	new_cond->quick_fix_field();
        new_cond->set_used_tables(tbl_map);
	return new_cond;
      }
    }
    else /* It's OR */
    {
      Item_cond_or *new_cond=new Item_cond_or;
      if (!new_cond)
	return (Item*) 0;
      List_iterator<Item> li(*((Item_cond*) cond)->argument_list());
      Item *item;
      while ((item=li++))
      {
	Item *fix= make_cond_remainder(item, FALSE);
	if (!fix)
	  return (Item*) 0;
	new_cond->argument_list()->push_back(fix);
        tbl_map |= fix->used_tables();
      }
      new_cond->quick_fix_field();
      new_cond->set_used_tables(tbl_map);
      new_cond->top_level_item();
      return new_cond;
    }
  }
  return cond;
}


/**
  Try to extract and push the index condition down to table handler

  @param  tab            A join tab that has tab->table->file and its
                         condition in tab->m_condition
  @param  keyno          Index for which extract and push the condition
  @param  other_tbls_ok  TRUE <=> Fields of other non-const tables are allowed
  @param  trace_obj      trace object where information is to be added
*/

static void push_index_cond(JOIN_TAB *tab, uint keyno, bool other_tbls_ok,
                            Opt_trace_object *trace_obj)
{
  DBUG_ENTER("push_index_cond");

  /*
    We will only attempt to push down an index condition when the
    following criteria are true:
    0. The table has a select condition
    1. The storage engine supports ICP.
    2. The system variable for enabling ICP is ON.
    3. The query is not a multi-table update or delete statement. The reason
       for this requirement is that the same handler will be used 
       both for doing the select/join and the update. The pushed index
       condition might then also be applied by the storage engine
       when doing the update part and result in either not finding
       the record to update or updating the wrong record.
    4. The JOIN_TAB is not part of a subquery that has guarded conditions
       that can be turned on or off during execution of a 'Full scan on NULL 
       key'.
       @see Item_in_optimizer::val_int()
       @see subselect_single_select_engine::exec()
       @see TABLE_REF::cond_guards
       @see setup_join_buffering
    5. The join type is not CONST or SYSTEM. The reason for excluding
       these join types, is that these are optimized to only read the
       record once from the storage engine and later re-use it. In a
       join where a pushed index condition evaluates fields from
       tables earlier in the join sequence, the pushed condition would
       only be evaluated the first time the record value was needed.
    6. The index is not a clustered index. The performance improvement
       of pushing an index condition on a clustered key is much lower 
       than on a non-clustered key. This restriction should be 
       re-evaluated when WL#6061 is implemented.
  */
  if (tab->condition() &&
      tab->table->file->index_flags(keyno, 0, 1) &
      HA_DO_INDEX_COND_PUSHDOWN &&
      tab->join->thd->optimizer_switch_flag(OPTIMIZER_SWITCH_INDEX_CONDITION_PUSHDOWN) &&
      tab->join->thd->lex->sql_command != SQLCOM_UPDATE_MULTI &&
      tab->join->thd->lex->sql_command != SQLCOM_DELETE_MULTI &&
      !tab->has_guarded_conds() &&
      tab->type != JT_CONST && tab->type != JT_SYSTEM &&
      !(keyno == tab->table->s->primary_key &&
        tab->table->file->primary_key_is_clustered()))
  {
    DBUG_EXECUTE("where", print_where(tab->condition(), "full cond",
                 QT_ORDINARY););
    Item *idx_cond= make_cond_for_index(tab->condition(), tab->table,
                                        keyno, other_tbls_ok);
    DBUG_EXECUTE("where", print_where(idx_cond, "idx cond", QT_ORDINARY););
    if (idx_cond)
    {
      Item *idx_remainder_cond= 0;
      tab->pre_idx_push_cond= tab->condition();

      /*
        For BKA cache we store condition to special BKA cache field
        because evaluation of the condition requires additional operations
        before the evaluation. This condition is used in 
        JOIN_CACHE_BKA[_UNIQUE]::skip_index_tuple() functions.
      */
      if (tab->use_join_cache &&
          /*
            if cache is used then the value is TRUE only 
            for BKA[_UNIQUE] cache (see setup_join_buffering() func).
            In this case other_tbls_ok is an equivalent of
            cache->is_key_access().
          */
          other_tbls_ok &&
          (idx_cond->used_tables() &
           ~(tab->table->map | tab->join->const_table_map)))
      {
        tab->cache_idx_cond= idx_cond;
        trace_obj->add("pushed_to_BKA", true);
      }
      else
      {
        idx_remainder_cond= tab->table->file->idx_cond_push(keyno, idx_cond);
        tab->select->icp_cond= idx_cond;
        DBUG_EXECUTE("where",
                     print_where(tab->select->icp_cond, "icp cond", 
                                 QT_ORDINARY););
      }
      /*
        Disable eq_ref's "lookup cache" if we've pushed down an index
        condition. 
        TODO: This check happens to work on current ICP implementations, but
        there may exist a compliant implementation that will not work 
        correctly with it. Sort this out when we stabilize the condition
        pushdown APIs.
      */
      if (idx_remainder_cond != idx_cond)
      {
        tab->ref.disable_cache= TRUE;
        trace_obj->add("pushed_index_condition", idx_cond);
      }

      Item *row_cond= make_cond_remainder(tab->condition(), TRUE);
      DBUG_EXECUTE("where", print_where(row_cond, "remainder cond",
                   QT_ORDINARY););
      
      if (row_cond)
      {
        if (!idx_remainder_cond)
          tab->set_condition(row_cond, __LINE__);
        else
        {
          and_conditions(&row_cond, idx_remainder_cond);
          tab->set_condition(row_cond, __LINE__);
        }
      }
      else
        tab->set_condition(idx_remainder_cond, __LINE__);
      trace_obj->add("table_condition_attached", tab->condition());
      if (tab->select)
      {
        DBUG_EXECUTE("where", print_where(tab->select->cond, "cond",
                     QT_ORDINARY););
        tab->select->cond= tab->condition();
      }
    }
  }
  DBUG_VOID_RETURN;
}


/*
  Deny usage of join buffer for the specified table

  SYNOPSIS
    set_join_cache_denial()
      tab    join table for which join buffer usage is to be denied  
     
  DESCRIPTION
    The function denies usage of join buffer when joining the table 'tab'.
    The table is marked as not employing any join buffer. If a join cache
    object has been already allocated for the table this object is destroyed.

  RETURN
    none    
*/

static
void set_join_cache_denial(JOIN_TAB *join_tab)
{
  if (join_tab->op)
  {
    join_tab->op->free();
    join_tab->op= 0;
  }
  if (join_tab->use_join_cache)
  {
    join_tab->use_join_cache= JOIN_CACHE::ALG_NONE;
    /*
      It could be only sub_select(). It could not be sub_seject_sjm because we
      don't do join buffering for the first table in sjm nest. 
    */
    join_tab[-1].next_select= sub_select;
  }
}


/* 
  Revise usage of join buffer for the specified table and the whole nest   

  SYNOPSIS
    revise_cache_usage()
      tab    join table for which join buffer usage is to be revised  

  DESCRIPTION
    The function revise the decision to use a join buffer for the table 'tab'.
    If this table happened to be among the inner tables of a nested outer join/
    semi-join the functions denies usage of join buffers for all of them

  RETURN
    none    
*/

static
void revise_cache_usage(JOIN_TAB *join_tab)
{
  JOIN_TAB *tab;
  JOIN_TAB *first_inner;

  if (join_tab->first_inner)
  {
    JOIN_TAB *end_tab= join_tab;
    for (first_inner= join_tab->first_inner; 
         first_inner;
         first_inner= first_inner->first_upper)           
    {
      for (tab= end_tab-1; tab >= first_inner; tab--)
        set_join_cache_denial(tab);
      end_tab= first_inner;
    }
  }
  else if (join_tab->get_sj_strategy() == SJ_OPT_FIRST_MATCH)
  {
    first_inner= join_tab->first_sj_inner_tab;
    for (tab= join_tab-1; tab >= first_inner; tab--)
    {
      if (tab->first_sj_inner_tab == first_inner)
        set_join_cache_denial(tab);
    }
  }
  else set_join_cache_denial(join_tab);
}


/**
  Set up join buffering for a specified table, if possible.

  @param tab             joined table to check join buffer usage for
  @param join            join for which the check is performed
  @param options         options of the join
  @param no_jbuf_after   don't use join buffering after table with this number
  @param icp_other_tables_ok[out] TRUE if condition pushdown supports
                                  other tables presence

  @return false if successful, true if error.
          Currently, allocation errors for join cache objects are ignored,
          and regular execution is chosen silently.

  @details
    The function finds out whether the table 'tab' can be joined using a join
    buffer. This check is performed after the best execution plan for 'join'
    has been chosen. If the function decides that a join buffer can be employed
    then it selects the most appropriate join cache object that contains this
    join buffer.
    If it has already been decided to not use join buffering for this table,
    no action is taken.

    Often it is already decided that join buffering will be used earlier in
    the optimization process, and this will also ensure that the most correct
    cost for the operation is calculated, and hence the probability of
    choosing an optimal join plan is higher. However, some join buffering
    decisions cannot currently be taken before this stage, hence we need this
    function to decide the most accurate join buffering strategy.

    @todo Long-term it is the goal that join buffering strategy is decided
    when the plan is selected.

    The result of the check and the type of the the join buffer to be used
    depend on:
      - the access method to access rows of the joined table
      - whether the join table is an inner table of an outer join or semi-join
      - the optimizer_switch settings for join buffering
      - the join 'options'.
    In any case join buffer is not used if the number of the joined table is
    greater than 'no_jbuf_after'. 

    If block_nested_loop is turned on, and if all other criteria for using
    join buffering is fulfilled (see below), then join buffer is used 
    for any join operation (inner join, outer join, semi-join) with 'JT_ALL' 
    access method.  In that case, a JOIN_CACHE_BNL object is always employed.

    If an index is used to access rows of the joined table and batched_key_access
    is on, then a JOIN_CACHE_BKA object is employed. (Unless debug flag,
    test_bka unique, is set, then a JOIN_CACHE_BKA_UNIQUE object is employed
    instead.) 

    If the function decides that a join buffer can be used to join the table
    'tab' then it sets @c tab->use_join_cache to reflect the chosen algorithm 
    and assigns the selected join cache object to the field 'cache' of the 
    previous join table.  After creating a join cache object, it will be 
    initialized. Failure to do so, will cause the decision to use join
    buffering to be reverted.
 
  @note
    For a nested outer join/semi-join, currently, we either use join buffers for
    all inner tables or for none of them. 
   
  @todo
    Support BKA inside SJ-Materialization nests. When doing this, we'll need
    to only store sj-inner tables in the join buffer.
#if 0
        JOIN_TAB *first_tab= join->join_tab+join->const_tables;
        uint n_tables= i-join->const_tables;
        / *
          We normally put all preceding tables into the join buffer, except
          for the constant tables.
          If we're inside a semi-join materialization nest, e.g.

             outer_tbl1  outer_tbl2  ( inner_tbl1, inner_tbl2 ) ...
                                                       ^-- we're here

          then we need to put into the join buffer only the tables from
          within the nest.
        * /
        if (i >= first_sjm_table && i < last_sjm_table)
        {
          n_tables= i - first_sjm_table; // will be >0 if we got here
          first_tab= join->join_tab + first_sjm_table;
        }
#endif

*/

static bool setup_join_buffering(JOIN_TAB *tab, JOIN *join,
                                 ulonglong options, uint no_jbuf_after,
                                 bool *icp_other_tables_ok)
{
  uint flags;
  Cost_estimate cost;
  ha_rows rows;
  uint bufsz= 4096;
  JOIN_CACHE *prev_cache;
  const bool bnl_on= join->thd->optimizer_switch_flag(OPTIMIZER_SWITCH_BNL);
  const bool bka_on= join->thd->optimizer_switch_flag(OPTIMIZER_SWITCH_BKA);
  const uint tableno= tab - join->join_tab;
  const uint tab_sj_strategy= tab->get_sj_strategy();
  bool use_bka_unique= false;
  DBUG_EXECUTE_IF("test_bka_unique", use_bka_unique= true;);
  *icp_other_tables_ok= TRUE;

  if (!(bnl_on || bka_on) || tableno == join->const_tables)
  {
    DBUG_ASSERT(tab->use_join_cache == JOIN_CACHE::ALG_NONE);
    return false;
  }
  if (options & SELECT_NO_JOIN_CACHE)
    goto no_join_cache;
  /* 
    psergey-todo: why the below when execution code seems to handle the
    "range checked for each record" case?
  */
  if (tab->use_quick == QS_DYNAMIC_RANGE)
    goto no_join_cache;

  /* No join buffering if prevented by no_jbuf_after */
  if (tableno > no_jbuf_after)
    goto no_join_cache;

  /*
    An inner table of an outer join nest must not use join buffering if
    the first inner table of that outer join nest does not use join buffering.
    This condition is not handled by earlier optimizer stages.
  */
  if (tab->first_inner != NULL &&
      tab->first_inner != tab &&
      !tab->first_inner->use_join_cache)
    goto no_join_cache;
  /*
    The first inner table of an outer join nest must not use join buffering
    if the tables in the embedding outer join nest do not use join buffering.
    This condition is not handled by earlier optimizer stages.
  */
  if (tab->first_upper != NULL &&
      !tab->first_upper->use_join_cache)
    goto no_join_cache;

  switch (tab_sj_strategy)
  {
  case SJ_OPT_FIRST_MATCH:
    /*
      Use join cache with FirstMatch semi-join strategy only when semi-join
      contains only one table.
    */
    if (!tab->is_single_inner_of_semi_join())
    {
      DBUG_ASSERT(tab->use_join_cache == JOIN_CACHE::ALG_NONE);
      goto no_join_cache;
    }
    break;

  case SJ_OPT_LOOSE_SCAN:
    /* No join buffering if this semijoin nest is handled by loosescan */
    DBUG_ASSERT(tab->use_join_cache == JOIN_CACHE::ALG_NONE);
    goto no_join_cache;

  case SJ_OPT_MATERIALIZE_LOOKUP:
  case SJ_OPT_MATERIALIZE_SCAN:
    /*
      The Materialize strategies reuse the join_tab belonging to the
      first table that was materialized. Neither table can use join buffering:
      - The first table in a join never uses join buffering.
      - The join_tab used for looking up a row in the materialized table, or
        scanning the rows of a materialized table, cannot use join buffering.
      We allow join buffering for the remaining tables of the materialized
      semi-join nest.
    */
    if (tab->first_sj_inner_tab == tab)
    {
      DBUG_ASSERT(tab->use_join_cache == JOIN_CACHE::ALG_NONE);
      goto no_join_cache;
    }
    break;

  case SJ_OPT_DUPS_WEEDOUT:
    // This strategy allows the same join buffering as a regular join would.
  case SJ_OPT_NONE:
    break;
  }

  /*
    Link with the previous join cache, but make sure that we do not link
    join caches of two different operations when the previous operation was
    MaterializeLookup or MaterializeScan, ie if:
     1. the previous join_tab has join buffering enabled, and
     2. the previous join_tab belongs to a materialized semi-join nest, and
     3. this join_tab represents a regular table, or is part of a different
        semi-join interval than the previous join_tab.
  */
  prev_cache= (JOIN_CACHE*)(tab-1)->op;
  if (prev_cache != NULL &&                                       // 1
      sj_is_materialize_strategy((tab-1)->get_sj_strategy()) &&   // 2
      tab->first_sj_inner_tab != (tab-1)->first_sj_inner_tab)     // 3
    prev_cache= NULL;

  /*
    The following code prevents use of join buffering when there is an
    outer join operation and first match semi-join strategy is used, because:

    Outer join needs a "match flag" to track that a row should be
    NULL-complemented, such flag being attached to first inner table's cache
    (tracks whether the cached row from outer table got a match, in which case
    no NULL-complemented row is needed).

    FirstMatch also needs a "match flag", such flag is attached to sj inner
    table's cache (tracks whether the cached row from outer table already got
    a first match in the sj-inner table, in which case we don't need to join
    this cached row again)
     - but a row in a cache has only one "match flag"
     - so if "sj inner table"=="first inner", there is a problem. 
  */
  if (tab_sj_strategy == SJ_OPT_FIRST_MATCH &&
      tab->is_inner_table_of_outer_join())
    goto no_join_cache;

  switch (tab->type) {
  case JT_ALL:
    if (!bnl_on)
    {
      DBUG_ASSERT(tab->use_join_cache == JOIN_CACHE::ALG_NONE);
      goto no_join_cache;
    }

    if ((options & SELECT_DESCRIBE) ||
        ((tab->op= new JOIN_CACHE_BNL(join, tab, prev_cache)) &&
         !tab->op->init()))
    {
      *icp_other_tables_ok= FALSE;
      DBUG_ASSERT(might_do_join_buffering(join_buffer_alg(join->thd), tab));
      tab->use_join_cache= JOIN_CACHE::ALG_BNL;
      return false;
    }
    goto no_join_cache;
  case JT_SYSTEM:
  case JT_CONST:
  case JT_REF:
  case JT_EQ_REF:
    if (!bka_on)
    {
      DBUG_ASSERT(tab->use_join_cache == JOIN_CACHE::ALG_NONE);
      goto no_join_cache;
    }

    /*
      Disable BKA for materializable derived tables/views as they aren't
      instantiated yet.
    */
    if (tab->table->pos_in_table_list->uses_materialization())
      goto no_join_cache;

    /*
      Can't use BKA for subquery if dealing with a subquery that can
      turn a ref access into a "full scan on NULL key" table scan.

      @see Item_in_optimizer::val_int()
      @see subselect_single_select_engine::exec()
      @see TABLE_REF::cond_guards
      @see push_index_cond()

      @todo: This choice to not use BKA should be done before making
      cost estimates, e.g. in set_join_buffer_properties(). That
      happens before cond guards are set up, so instead of doing the
      check below, BKA should be disabled if
       - We are in an IN subquery, and
       - The IN predicate is not a top_level_item, and
       - The left_expr of the IN predicate may contain NULL values 
         (left_expr->maybe_null)
    */
    if (tab->has_guarded_conds())
      goto no_join_cache;

    flags= HA_MRR_NO_NULL_ENDPOINTS;
    if (tab->table->covering_keys.is_set(tab->ref.key))
      flags|= HA_MRR_INDEX_ONLY;
    rows= tab->table->file->multi_range_read_info(tab->ref.key, 10, 20,
                                                  &bufsz, &flags, &cost);
    /*
      Cannot use BKA/BKA_UNIQUE if
      1. MRR scan cannot be performed, or
      2. MRR default implementation is used
      Cannot use BKA if
      3. HA_MRR_NO_ASSOCIATION flag is set
    */
    if ((rows == HA_POS_ERROR) ||                               // 1
        (flags & HA_MRR_USE_DEFAULT_IMPL) ||                    // 2
        ((flags & HA_MRR_NO_ASSOCIATION) && !use_bka_unique))   // 3
      goto no_join_cache;

    if (!(options & SELECT_DESCRIBE))
    {
      if (use_bka_unique)
        tab->op= new JOIN_CACHE_BKA_UNIQUE(join, tab, flags, prev_cache);
      else
        tab->op= new JOIN_CACHE_BKA(join, tab, flags, prev_cache);

      if (!tab->op || tab->op->init())
        goto no_join_cache;
    }
     
    DBUG_ASSERT(might_do_join_buffering(join_buffer_alg(join->thd), tab));
    if (use_bka_unique)
      tab->use_join_cache= JOIN_CACHE::ALG_BKA_UNIQUE;
    else
      tab->use_join_cache= JOIN_CACHE::ALG_BKA;

    return false;
  default : ;
  }

no_join_cache:
  if (bnl_on || bka_on)
    revise_cache_usage(tab);
  tab->use_join_cache= JOIN_CACHE::ALG_NONE;
  return false;
}


/**
  Setup the materialized table for a semi-join nest

  @param tab       join_tab for the materialized semi-join table
  @param tableno   table number of materialized table
  @param inner_pos information about the first inner table of the subquery
  @param sjm_pos   information about the materialized semi-join table,
                   to be filled with data.

  @details
    Setup execution structures for one semi-join materialization nest:
    - Create the materialization temporary table, including TABLE_LIST object.
    - Create a list of Item_field objects per column in the temporary table.
    - Create a keyuse array describing index lookups into the table
      (for MaterializeLookup)

  @return False if OK, True if error
*/

bool JOIN::setup_materialized_table(JOIN_TAB *tab, uint tableno,
                                    const POSITION *inner_pos,
                                    POSITION *sjm_pos)
{
  DBUG_ENTER("JOIN::setup_materialized_table");
  const TABLE_LIST *const emb_sj_nest= inner_pos->table->emb_sj_nest;
  Semijoin_mat_optimize *const sjm_opt= &emb_sj_nest->nested_join->sjm;
  Semijoin_mat_exec *const sjm_exec= tab->sj_mat_exec;
  const uint field_count= emb_sj_nest->nested_join->sj_inner_exprs.elements;

  DBUG_ASSERT(inner_pos->sj_strategy == SJ_OPT_MATERIALIZE_LOOKUP ||
              inner_pos->sj_strategy == SJ_OPT_MATERIALIZE_SCAN);

  /* 
    Set up the table to write to, do as select_union::create_result_table does
  */
  sjm_exec->table_param.init();
  sjm_exec->table_param.field_count= field_count;
  sjm_exec->table_param.bit_fields_as_long= true;

  char buffer[NAME_LEN];
  const size_t len= my_snprintf(buffer, sizeof(buffer) - 1, "<subquery%u>",
                                emb_sj_nest->nested_join->query_block_id);
  char *name= (char *)alloc_root(thd->mem_root, len + 1);
  if (name == NULL)
    DBUG_RETURN(true); /* purecov: inspected */

  memcpy(name, buffer, len);
  name[len] = '\0';
  TABLE *table;
  if (!(table= create_tmp_table(thd, &sjm_exec->table_param, 
                                emb_sj_nest->nested_join->sj_inner_exprs,
                                NULL, 
                                true /* distinct */, 
                                true /* save_sum_fields */,
                                thd->variables.option_bits |
                                TMP_TABLE_ALL_COLUMNS, 
                                HA_POS_ERROR /* rows_limit */, 
                                name)))
    DBUG_RETURN(true); /* purecov: inspected */
  sjm_exec->table= table;
  table->tablenr= tableno;
  table->map= (table_map)1 << tableno;
  table->file->extra(HA_EXTRA_WRITE_CACHE);
  table->file->extra(HA_EXTRA_IGNORE_DUP_KEY);
  table->reginfo.join_tab= tab;
  sj_tmp_tables.push_back(table);
  sjm_exec_list.push_back(sjm_exec);

  if (!(sjm_opt->mat_fields=
    (Item_field **) alloc_root(thd->mem_root,
                               field_count * sizeof(Item_field **))))
    DBUG_RETURN(true);

  for (uint fieldno= 0; fieldno < field_count; fieldno++)
  {
    if (!(sjm_opt->mat_fields[fieldno]= new Item_field(table->field[fieldno])))
      DBUG_RETURN(true);
  }

  TABLE_LIST *tl;
  if (!(tl= (TABLE_LIST *) alloc_root(thd->mem_root, sizeof(TABLE_LIST))))
    DBUG_RETURN(true);
  // TODO: May have to setup outer-join info for this TABLE_LIST !!!

  tl->init_one_table("", 0, name, strlen(name), name, TL_IGNORE);

  tl->table= table;

  tab->table= table;  
  tab->position= sjm_pos;
  tab->join= this;

  tab->worst_seeks= 1.0;
  tab->records= (ha_rows)emb_sj_nest->nested_join->sjm.expected_rowcount;
  tab->found_records= tab->records;
  tab->read_time= (ha_rows)emb_sj_nest->nested_join->sjm.scan_cost.total_cost();

  tab->on_expr_ref= tl->join_cond_ref();

  tab->materialize_table= join_materialize_semijoin;

  table->pos_in_table_list= tl;
  table->keys_in_use_for_query.set_all();
  sjm_pos->table= tab;
  sjm_pos->sj_strategy= SJ_OPT_NONE;

  sjm_pos->use_join_buffer= false;

  /*
    Key_use objects are required so that create_ref_for_key() can set up
    a proper ref access for this table.
  */
  Key_use_array *keyuse=
   create_keyuse_for_table(thd, table, field_count, sjm_opt->mat_fields,
                           emb_sj_nest->nested_join->sj_outer_exprs);
  if (!keyuse)
    DBUG_RETURN(true);

  double fanout= (tab == join_tab + tab->join->const_tables) ?
                 1.0 : (tab-1)->position->prefix_record_count;
  if (!sjm_exec->is_scan)
  {
    sjm_pos->key= keyuse->begin(); // MaterializeLookup will use the index
    tab->keyuse= keyuse->begin();
    tab->keys.set_bit(0);          // There is one index - use it always
    tab->index= 0;
    sjm_pos->set_prefix_costs(1.0, fanout);
    sjm_pos->records_read= 1.0;   
    sjm_pos->read_time= 1.0;      
  }
  else
  {
    sjm_pos->key= NULL; // No index use for MaterializeScan
    sjm_pos->set_prefix_costs(tab->read_time, tab->records * fanout);
    sjm_pos->records_read= tab->records;
    sjm_pos->read_time= tab->read_time;
  }

  DBUG_RETURN(false);
}


/**
  Plan refinement stage: do various setup things for the executor

  @param join          Join being processed
  @param options       Join's options (checking for SELECT_DESCRIBE, 
                       SELECT_NO_JOIN_CACHE)
  @param no_jbuf_after Don't use join buffering after table with this number.

  @return false if successful, true if error (Out of memory)

  @details
    Plan refinement stage: do various set ups for the executioner
      - setup join buffering use
      - push index conditions
      - increment relevant counters
      - etc
*/

bool
make_join_readinfo(JOIN *join, ulonglong options, uint no_jbuf_after)
{
  const bool statistics= test(!(join->select_options & SELECT_DESCRIBE));

  /* First table sorted if ORDER or GROUP BY was specified */
  bool sorted= (join->order || join->group_list);

  DBUG_ENTER("make_join_readinfo");

  Opt_trace_context * const trace= &join->thd->opt_trace;
  Opt_trace_object wrapper(trace);
  Opt_trace_array trace_refine_plan(trace, "refine_plan");

  if (setup_semijoin_dups_elimination(join, options, no_jbuf_after))
    DBUG_RETURN(TRUE); /* purecov: inspected */

  for (uint i= join->const_tables; i < join->tables; i++)
  {
    JOIN_TAB *const tab= join->join_tab+i;
    TABLE    *const table= tab->table;
    if (!tab->position)
      continue;

    bool icp_other_tables_ok;
    tab->read_record.table= table;
    tab->next_select=sub_select;		/* normal select */
    tab->cache_idx_cond= 0;
    /*
      For eq_ref there is at most one join match for each row from
      previous tables so ordering is not useful.
      NOTE: setup_semijoin_dups_elimination() might have requested 
            'sorted', thus a '|=' is required to preserve that.
    */
    tab->sorted|= (sorted && tab->type != JT_EQ_REF);
    sorted= false;                              // only first must be sorted
    table->status= STATUS_GARBAGE | STATUS_NOT_FOUND;
    tab->read_first_record= NULL; // Access methods not set yet
    tab->read_record.read_record= NULL;
    tab->read_record.unlock_row= rr_unlock_row;

    Opt_trace_object trace_refine_table(trace);
    trace_refine_table.add_utf8_table(table);

    if (tab->do_loosescan())
    {
      if (!(tab->loosescan_buf= (uchar*)join->thd->alloc(tab->
                                                         loosescan_key_len)))
        DBUG_RETURN(TRUE); /* purecov: inspected */
    }
    switch (tab->type) {
    case JT_EQ_REF:
    case JT_REF_OR_NULL:
    case JT_REF:
      if (tab->select)
        tab->select->set_quick(NULL);
      delete tab->quick;
      tab->quick=0;
      /* fall through */
    case JT_SYSTEM: 
    case JT_CONST:
      /* Only happens with outer joins */
      if (setup_join_buffering(tab, join, options, no_jbuf_after,
                               &icp_other_tables_ok))
        DBUG_RETURN(true);
      if (tab->use_join_cache != JOIN_CACHE::ALG_NONE)
        tab[-1].next_select= sub_select_op;

      if (table->covering_keys.is_set(tab->ref.key) &&
          !table->no_keyread)
        table->set_keyread(TRUE);
      else
        push_index_cond(tab, tab->ref.key, icp_other_tables_ok,
                        &trace_refine_table);
      break;
    case JT_ALL:
      if (setup_join_buffering(tab, join, options, no_jbuf_after,
                               &icp_other_tables_ok))
        DBUG_RETURN(true);
      if (tab->use_join_cache != JOIN_CACHE::ALG_NONE)
        tab[-1].next_select=sub_select_op;

      /* These init changes read_record */
      if (tab->use_quick == QS_DYNAMIC_RANGE)
      {
	join->thd->set_status_no_good_index_used();
	tab->read_first_record= join_init_quick_read_record;
	if (statistics)
	  join->thd->inc_status_select_range_check();
        trace_refine_table.add_alnum("access_type", "dynamic_range");
      }
      else
      {
	tab->read_first_record= join_init_read_record;
	if (i == join->const_tables)
	{
	  if (tab->select && tab->select->quick)
	  {
	    if (statistics)
	      join->thd->inc_status_select_range();
	  }
	  else
	  {
	    join->thd->set_status_no_index_used();
	    if (statistics)
	      join->thd->inc_status_select_scan();
	  }
	}
	else
	{
	  if (tab->select && tab->select->quick)
	  {
	    if (statistics)
	      join->thd->inc_status_select_full_range_join();
	  }
	  else
	  {
	    join->thd->set_status_no_index_used();
	    if (statistics)
	      join->thd->inc_status_select_full_join();
	  }
	}
	if (!table->no_keyread)
	{
          if (tab->select && tab->select->quick &&
              tab->select->quick->index != MAX_KEY && //not index_merge
              table->covering_keys.is_set(tab->select->quick->index))
            table->set_keyread(TRUE);
          else if (!table->covering_keys.is_clear_all() &&
                   !(tab->select && tab->select->quick))
	  {					// Only read index tree
	    /*
            It has turned out that the below change, while speeding things
            up for disk-bound loads, slows them down for cases when the data
            is in disk cache (see BUG#35850):
	    //  See bug #26447: "Using the clustered index for a table scan
	    //  is always faster than using a secondary index".
            if (table->s->primary_key != MAX_KEY &&
                table->file->primary_key_is_clustered())
              tab->index= table->s->primary_key;
            else
              tab->index=find_shortest_key(table, & table->covering_keys);
	    */
            if (!tab->do_loosescan())
              tab->index=find_shortest_key(table, & table->covering_keys);
	    tab->read_first_record= join_read_first;
            tab->type=JT_INDEX_SCAN;      // Read with index_first / index_next
	  }
	}
        if (tab->select && tab->select->quick &&
            tab->select->quick->index != MAX_KEY && ! tab->table->key_read)
          push_index_cond(tab, tab->select->quick->index, icp_other_tables_ok,
                          &trace_refine_table);
        trace_refine_table.add_alnum("access_type",
                                     tab->type == JT_INDEX_SCAN ?
                                     "index_scan" :
                                     (tab->select && tab->select->quick) ?
                                     "range" : "table_scan");
      }
      break;
    case JT_FT:
      break;
    default:
      DBUG_PRINT("error",("Table type %d found",tab->type)); /* purecov: deadcode */
      break;					/* purecov: deadcode */
    case JT_UNKNOWN:
      abort();					/* purecov: deadcode */
    }
    // Materialize derived tables prior to accessing them.
    if (tab->table->pos_in_table_list->uses_materialization())
      tab->materialize_table= join_materialize_derived;
  }

  for (uint i= join->const_tables; i < join->primary_tables; i++)
  {
    if (join->join_tab[i].use_join_cache != JOIN_CACHE::ALG_NONE)
    {
      /*
        A join buffer is used for this table. We here inform the optimizer
        that it should not rely on rows of the first non-const table being in
        order thanks to an index scan; indeed join buffering of the present
        table subsequently changes the order of rows.
      */
      if (join->order != NULL)
        join->simple_order= false;
      if (join->group_list != NULL)
        join->simple_group= false;
      break;
    }
  }

  DBUG_RETURN(FALSE);
}


/**
  Give error if we some tables are done with a full join.

  This is used by multi_table_update and multi_table_delete when running
  in safe mode.

  @param join		Join condition

  @retval
    0	ok
  @retval
    1	Error (full join used)
*/

bool error_if_full_join(JOIN *join)
{
  for (uint i= 0; i < join->primary_tables; i++)
  {
    JOIN_TAB *const tab= join->join_tab + i;

    if (tab->type == JT_ALL && (!tab->select || !tab->select->quick))
    {
      /* This error should not be ignored. */
      join->select_lex->no_error= FALSE;
      my_message(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE,
                 ER(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE), MYF(0));
      return true;
    }
  }
  return false;
}


/**
  Cleanup table of join operation.

  @note
    Notice that this is not a complete cleanup. In some situations, the
    object may be reused after a cleanup operation, hence we cannot set
    the table pointer to NULL in this function.
*/

void JOIN_TAB::cleanup()
{
  delete select;
  select= 0;
  delete quick;
  quick= 0;
  limit= 0;

  // Free select that was created for filesort outside of create_sort_index
  if (filesort && filesort->select && !filesort->own_select)
    delete filesort->select;
  delete filesort;
  filesort= NULL;
  /* Skip non-existing derived tables/views result tables */
  if (table &&
      (table->s->tmp_table != INTERNAL_TMP_TABLE || table->is_created()))
  {
    table->set_keyread(FALSE);
    table->file->ha_index_or_rnd_end();

    free_io_cache(table);
    filesort_free_buffers(table, true);
    /*
      We need to reset this for next select
      (Tested in part_of_refkey)
    */
    table->reginfo.join_tab= NULL;
  }
  end_read_record(&read_record);
}

uint JOIN_TAB::sjm_query_block_id() const
{
  return sj_is_materialize_strategy(get_sj_strategy()) ?
    first_sj_inner_tab->emb_sj_nest->nested_join->query_block_id : 0;
}


/**
  Extend join_tab->m_condition and join_tab->select->cond by AND'ing
  add_cond to them

  @param add_cond   The condition to AND with the existing conditions
  @param line       Code line this method was called from

  @retval true   if there was a memory allocation error
  @retval false  otherwise

*/
bool JOIN_TAB::and_with_jt_and_sel_condition(Item *add_cond, uint line)
{
  if (and_with_condition(add_cond, line))
    return true;

  if (select)
  {
    DBUG_PRINT("info", 
               ("select::cond extended. Change %p -> %p "
                "at line %u tab %p select %p",
                select->cond, m_condition, line, this, select));
    select->cond= m_condition;
  }
  return false;
}

/**
  Extend join_tab->cond by AND'ing add_cond to it

  @param add_cond    The condition to AND with the existing cond
                     for this JOIN_TAB
  @param line        Code line this method was called from

  @retval true   if there was a memory allocation error
  @retval false  otherwise
*/
bool JOIN_TAB::and_with_condition(Item *add_cond, uint line)
{
  Item *old_cond __attribute__((unused))= m_condition;
  if (and_conditions(&m_condition, add_cond))
    return true;
  DBUG_PRINT("info", ("JOIN_TAB::m_condition extended. Change %p -> %p "
                      "at line %u tab %p",
                      old_cond, m_condition, line, this));
  return false;
}


/**
  Partially cleanup JOIN after it has executed: close index or rnd read
  (table cursors), free quick selects.

    This function is called in the end of execution of a JOIN, before the used
    tables are unlocked and closed.

    For a join that is resolved using a temporary table, the first sweep is
    performed against actual tables and an intermediate result is inserted
    into the temprorary table.
    The last sweep is performed against the temporary table. Therefore,
    the base tables and associated buffers used to fill the temporary table
    are no longer needed, and this function is called to free them.

    For a join that is performed without a temporary table, this function
    is called after all rows are sent, but before EOF packet is sent.

    For a simple SELECT with no subqueries this function performs a full
    cleanup of the JOIN and calls mysql_unlock_read_tables to free used base
    tables.

    If a JOIN is executed for a subquery or if it has a subquery, we can't
    do the full cleanup and need to do a partial cleanup only.
    - If a JOIN is not the top level join, we must not unlock the tables
    because the outer select may not have been evaluated yet, and we
    can't unlock only selected tables of a query.
    - Additionally, if this JOIN corresponds to a correlated subquery, we
    should not free quick selects and join buffers because they will be
    needed for the next execution of the correlated subquery.
    - However, if this is a JOIN for a [sub]select, which is not
    a correlated subquery itself, but has subqueries, we can free it
    fully and also free JOINs of all its subqueries. The exception
    is a subquery in SELECT list, e.g: @n
    SELECT a, (select max(b) from t1) group by c @n
    This subquery will not be evaluated at first sweep and its value will
    not be inserted into the temporary table. Instead, it's evaluated
    when selecting from the temporary table. Therefore, it can't be freed
    here even though it's not correlated.

  @todo
    Unlock tables even if the join isn't top level select in the tree
*/

void JOIN::join_free()
{
  SELECT_LEX_UNIT *tmp_unit;
  SELECT_LEX *sl;
  /*
    Optimization: if not EXPLAIN and we are done with the JOIN,
    free all tables.
  */
  bool full= (!select_lex->uncacheable && !thd->lex->describe);
  bool can_unlock= full;
  DBUG_ENTER("JOIN::join_free");

  cleanup(full);

  for (tmp_unit= select_lex->first_inner_unit();
       tmp_unit;
       tmp_unit= tmp_unit->next_unit())
    for (sl= tmp_unit->first_select(); sl; sl= sl->next_select())
    {
      Item_subselect *subselect= sl->master_unit()->item;
      bool full_local= full && (!subselect || subselect->is_evaluated());
      /*
        If this join is evaluated, we can fully clean it up and clean up all
        its underlying joins even if they are correlated -- they will not be
        used any more anyway.
        If this join is not yet evaluated, we still must clean it up to
        close its table cursors -- it may never get evaluated, as in case of
        ... HAVING FALSE OR a IN (SELECT ...))
        but all table cursors must be closed before the unlock.
      */
      sl->cleanup_all_joins(full_local);
      /* Can't unlock if at least one JOIN is still needed */
      can_unlock= can_unlock && full_local;
    }

  /*
    We are not using tables anymore
    Unlock all tables. We may be in an INSERT .... SELECT statement.
  */
  if (can_unlock && lock && thd->lock && ! thd->locked_tables_mode &&
      !(select_options & SELECT_NO_UNLOCK) &&
      !select_lex->subquery_in_having &&
      (select_lex == (thd->lex->unit.fake_select_lex ?
                      thd->lex->unit.fake_select_lex : &thd->lex->select_lex)))
  {
    /*
      TODO: unlock tables even if the join isn't top level select in the
      tree.
    */
    mysql_unlock_read_tables(thd, lock);           // Don't free join->lock
    lock= 0;
  }

  DBUG_VOID_RETURN;
}


/**
  Free resources of given join.

  @param fill   true if we should free all resources, call with full==1
                should be last, before it this function can be called with
                full==0

  @note
    With subquery this function definitely will be called several times,
    but even for simple query it can be called several times.
*/

void JOIN::cleanup(bool full)
{
  DBUG_ENTER("JOIN::cleanup");

  DBUG_ASSERT(const_tables <= primary_tables &&
              primary_tables <= tables);

  if (join_tab)
  {
    JOIN_TAB *tab,*end;

    if (full)
    {
      for (tab= join_tab, end= tab + tables; tab < end; tab++)
	tab->cleanup();
    }
    else
    {
      for (tab= join_tab, end= tab + tables; tab < end; tab++)
      {
        if (!tab->table)
          continue;
	if (tab->table->is_created())
        {
          tab->table->file->ha_index_or_rnd_end();
          if (tab->op &&
              tab->op->type() == QEP_operation::OT_TMP_TABLE)
          {
            int tmp= 0;
            if ((tmp= tab->table->file->extra(HA_EXTRA_NO_CACHE)))
              tab->table->file->print_error(tmp, MYF(0));
          }
        }
        free_io_cache(tab->table);
        filesort_free_buffers(tab->table, full);
      }
    }
  }
  /*
    We are not using tables anymore
    Unlock all tables. We may be in an INSERT .... SELECT statement.
  */
  if (full)
  {
    // Run Cached_item DTORs!
    group_fields.delete_elements();

    /*
      We can't call delete_elements() on copy_funcs as this will cause
      problems in free_elements() as some of the elements are then deleted.
    */
    tmp_table_param.copy_funcs.empty();
    tmp_table_param.cleanup();
  }
  /* Restore ref array to original state */
  if (current_ref_ptrs != items0)
  {
    set_items_ref_array(items0);
    set_group_rpa= false;
  }
  DBUG_VOID_RETURN;
}


/**
  Filter out ORDER items those are equal to constants in WHERE

  This function is a limited version of remove_const() for use
  with non-JOIN statements (i.e. single-table UPDATE and DELETE).


  @param order            Linked list of ORDER BY arguments
  @param cond             WHERE expression

  @return pointer to new filtered ORDER list or NULL if whole list eliminated

  @note
    This function overwrites input order list.
*/

ORDER *simple_remove_const(ORDER *order, Item *where)
{
  if (!order || !where)
    return order;

  ORDER *first= NULL, *prev= NULL;
  for (; order; order= order->next)
  {
    DBUG_ASSERT(!order->item[0]->with_sum_func); // should never happen
    if (!const_expression_in_where(where, order->item[0]))
    {
      if (!first)
        first= order;
      if (prev)
        prev->next= order;
      prev= order;
    }
  }
  if (prev)
    prev->next= NULL;
  return first;
}




/* 
  Check if equality can be used in removing components of GROUP BY/DISTINCT
  
  SYNOPSIS
    test_if_equality_guarantees_uniqueness()
      l          the left comparison argument (a field if any)
      r          the right comparison argument (a const of any)
  
  DESCRIPTION    
    Checks if an equality predicate can be used to take away 
    DISTINCT/GROUP BY because it is known to be true for exactly one 
    distinct value (e.g. <expr> == <const>).
    Arguments must be of the same type because e.g. 
    <string_field> = <int_const> may match more than 1 distinct value from 
    the column. 
    We must take into consideration and the optimization done for various 
    string constants when compared to dates etc (see Item_int_with_ref) as
    well as the collation of the arguments.
  
  RETURN VALUE  
    TRUE    can be used
    FALSE   cannot be used
*/
static bool
test_if_equality_guarantees_uniqueness(Item *l, Item *r)
{
  return r->const_item() &&
    /* elements must be compared as dates */
     (Arg_comparator::can_compare_as_dates(l, r, 0) ||
      /* or of the same result type */
      (r->result_type() == l->result_type() &&
       /* and must have the same collation if compared as strings */
       (l->result_type() != STRING_RESULT ||
        l->collation.collation == r->collation.collation)));
}


/*
  Return TRUE if i1 and i2 (if any) are equal items,
  or if i1 is a wrapper item around the f2 field.
*/

static bool equal(Item *i1, Item *i2, Field *f2)
{
  DBUG_ASSERT((i2 == NULL) ^ (f2 == NULL));

  if (i2 != NULL)
    return i1->eq(i2, 1);
  else if (i1->type() == Item::FIELD_ITEM)
    return f2->eq(((Item_field *) i1)->field);
  else
    return FALSE;
}


/**
  Test if a field or an item is equal to a constant value in WHERE

  @param        cond            WHERE clause expression
  @param        comp_item       Item to find in WHERE expression
                                (if comp_field != NULL)
  @param        comp_field      Field to find in WHERE expression
                                (if comp_item != NULL)
  @param[out]   const_item      intermediate arg, set to Item pointer to NULL 

  @return TRUE if the field is a constant value in WHERE

  @note
    comp_item and comp_field parameters are mutually exclusive.
*/
bool
const_expression_in_where(Item *cond, Item *comp_item, Field *comp_field,
                          Item **const_item)
{
  DBUG_ASSERT((comp_item == NULL) ^ (comp_field == NULL));

  Item *intermediate= NULL;
  if (const_item == NULL)
    const_item= &intermediate;

  if (cond->type() == Item::COND_ITEM)
  {
    bool and_level= (((Item_cond*) cond)->functype()
		     == Item_func::COND_AND_FUNC);
    List_iterator_fast<Item> li(*((Item_cond*) cond)->argument_list());
    Item *item;
    while ((item=li++))
    {
      bool res=const_expression_in_where(item, comp_item, comp_field,
                                         const_item);
      if (res)					// Is a const value
      {
	if (and_level)
	  return 1;
      }
      else if (!and_level)
	return 0;
    }
    return and_level ? 0 : 1;
  }
  else if (cond->eq_cmp_result() != Item::COND_OK)
  {						// boolean compare function
    Item_func* func= (Item_func*) cond;
    if (func->functype() != Item_func::EQUAL_FUNC &&
	func->functype() != Item_func::EQ_FUNC)
      return 0;
    Item *left_item=	((Item_func*) cond)->arguments()[0];
    Item *right_item= ((Item_func*) cond)->arguments()[1];
    if (equal(left_item, comp_item, comp_field))
    {
      if (test_if_equality_guarantees_uniqueness (left_item, right_item))
      {
	if (*const_item)
	  return right_item->eq(*const_item, 1);
	*const_item=right_item;
	return 1;
      }
    }
    else if (equal(right_item, comp_item, comp_field))
    {
      if (test_if_equality_guarantees_uniqueness (right_item, left_item))
      {
	if (*const_item)
	  return left_item->eq(*const_item, 1);
	*const_item=left_item;
	return 1;
      }
    }
  }
  return 0;
}


/**
  Test if one can use the key to resolve ORDER BY.

  @param order                 Sort order
  @param table                 Table to sort
  @param idx                   Index to check
  @param used_key_parts [out]  NULL by default, otherwise return value for
                               used key parts.


  @note
    used_key_parts is set to correct key parts used if return value != 0
    (On other cases, used_key_part may be changed)
    Note that the value may actually be greater than the number of index 
    key parts. This can happen for storage engines that have the primary 
    key parts as a suffix for every secondary key.

  @retval
    1   key is ok.
  @retval
    0   Key can't be used
  @retval
    -1   Reverse key can be used
*/

static int test_if_order_by_key(ORDER *order, TABLE *table, uint idx,
				uint *used_key_parts= NULL)
{
  KEY_PART_INFO *key_part,*key_part_end;
  key_part=table->key_info[idx].key_part;
  key_part_end=key_part+table->key_info[idx].user_defined_key_parts;
  key_part_map const_key_parts=table->const_key_parts[idx];
  int reverse=0;
  uint key_parts;
  my_bool on_pk_suffix= FALSE;
  DBUG_ENTER("test_if_order_by_key");

  for (; order ; order=order->next, const_key_parts>>=1)
  {
    Field *field=((Item_field*) (*order->item)->real_item())->field;
    int flag;

    /*
      Skip key parts that are constants in the WHERE clause.
      These are already skipped in the ORDER BY by const_expression_in_where()
    */
    for (; const_key_parts & 1 ; const_key_parts>>= 1)
      key_part++; 

    if (key_part == key_part_end)
    {
      /* 
        We are at the end of the key. Check if the engine has the primary
        key as a suffix to the secondary keys. If it has continue to check
        the primary key as a suffix.
      */
      if (!on_pk_suffix &&
          (table->file->ha_table_flags() & HA_PRIMARY_KEY_IN_READ_INDEX) &&
          table->s->primary_key != MAX_KEY &&
          table->s->primary_key != idx)
      {
        on_pk_suffix= TRUE;
        key_part= table->key_info[table->s->primary_key].key_part;
        key_part_end=key_part +
          table->key_info[table->s->primary_key].user_defined_key_parts;
        const_key_parts=table->const_key_parts[table->s->primary_key];

        for (; const_key_parts & 1 ; const_key_parts>>= 1)
          key_part++; 
        /*
         The primary and secondary key parts were all const (i.e. there's
         one row).  The sorting doesn't matter.
        */
        if (key_part == key_part_end && reverse == 0)
        {
          key_parts= 0;
          reverse= 1;
          goto ok;
        }
      }
      else
        DBUG_RETURN(0);
    }

    if (key_part->field != field || !field->part_of_sortkey.is_set(idx))
      DBUG_RETURN(0);

    const ORDER::enum_order keypart_order= 
      (key_part->key_part_flag & HA_REVERSE_SORT) ? 
      ORDER::ORDER_DESC : ORDER::ORDER_ASC;
    /* set flag to 1 if we can use read-next on key, else to -1 */
    flag= (order->direction == keypart_order) ? 1 : -1;
    if (reverse && flag != reverse)
      DBUG_RETURN(0);
    reverse=flag;				// Remember if reverse
    key_part++;
  }
  if (on_pk_suffix)
  {
    uint used_key_parts_secondary= table->key_info[idx].user_defined_key_parts;
    uint used_key_parts_pk=
      (uint) (key_part - table->key_info[table->s->primary_key].key_part);
    key_parts= used_key_parts_pk + used_key_parts_secondary;

    if (reverse == -1 &&
        (!(table->file->index_flags(idx, used_key_parts_secondary - 1, 1) &
           HA_READ_PREV) ||
         !(table->file->index_flags(table->s->primary_key,
                                    used_key_parts_pk - 1, 1) & HA_READ_PREV)))
      reverse= 0;                               // Index can't be used
  }
  else
  {
    key_parts= (uint) (key_part - table->key_info[idx].key_part);
    if (reverse == -1 && 
        !(table->file->index_flags(idx, key_parts-1, 1) & HA_READ_PREV))
      reverse= 0;                               // Index can't be used
  }
ok:
  if (used_key_parts != NULL)
    *used_key_parts= key_parts;
  DBUG_RETURN(reverse);
}


/**
  Find shortest key suitable for full table scan.

  @param table                 Table to scan
  @param usable_keys           Allowed keys

  @note
     As far as 
     1) clustered primary key entry data set is a set of all record
        fields (key fields and not key fields) and
     2) secondary index entry data is a union of its key fields and
        primary key fields (at least InnoDB and its derivatives don't
        duplicate primary key fields there, even if the primary and
        the secondary keys have a common subset of key fields),
     then secondary index entry data is always a subset of primary key entry.
     Unfortunately, key_info[nr].key_length doesn't show the length
     of key/pointer pair but a sum of key field lengths only, thus
     we can't estimate index IO volume comparing only this key_length
     value of secondary keys and clustered PK.
     So, try secondary keys first, and choose PK only if there are no
     usable secondary covering keys or found best secondary key include
     all table fields (i.e. same as PK):

  @return
    MAX_KEY     no suitable key found
    key index   otherwise
*/

uint find_shortest_key(TABLE *table, const key_map *usable_keys)
{
  uint best= MAX_KEY;
  uint usable_clustered_pk= (table->file->primary_key_is_clustered() &&
                             table->s->primary_key != MAX_KEY &&
                             usable_keys->is_set(table->s->primary_key)) ?
                            table->s->primary_key : MAX_KEY;
  if (!usable_keys->is_clear_all())
  {
    uint min_length= (uint) ~0;
    for (uint nr=0; nr < table->s->keys ; nr++)
    {
      if (nr == usable_clustered_pk)
        continue;
      if (usable_keys->is_set(nr))
      {
        if (table->key_info[nr].key_length < min_length)
        {
          min_length=table->key_info[nr].key_length;
          best=nr;
        }
      }
    }
  }
  if (usable_clustered_pk != MAX_KEY)
  {
    /*
     If the primary key is clustered and found shorter key covers all table
     fields then primary key scan normally would be faster because amount of
     data to scan is the same but PK is clustered.
     It's safe to compare key parts with table fields since duplicate key
     parts aren't allowed.
     */
    if (best == MAX_KEY ||
        table->key_info[best].user_defined_key_parts >= table->s->fields)
      best= usable_clustered_pk;
  }
  return best;
}

/**
  Test if a second key is the subkey of the first one.

  @param key_part              First key parts
  @param ref_key_part          Second key parts
  @param ref_key_part_end      Last+1 part of the second key

  @note
    Second key MUST be shorter than the first one.

  @retval
    1	is a subkey
  @retval
    0	no sub key
*/

inline bool 
is_subkey(KEY_PART_INFO *key_part, KEY_PART_INFO *ref_key_part,
	  KEY_PART_INFO *ref_key_part_end)
{
  for (; ref_key_part < ref_key_part_end; key_part++, ref_key_part++)
    if (!key_part->field->eq(ref_key_part->field))
      return 0;
  return 1;
}

/**
  Test if REF_OR_NULL optimization will be used if the specified
  ref_key is used for REF-access to 'tab'

  @retval
    true	JT_REF_OR_NULL will be used
  @retval
    false	no JT_REF_OR_NULL access
*/
bool
is_ref_or_null_optimized(const JOIN_TAB *tab, uint ref_key)
{
  if (tab->keyuse)
  {
    const Key_use *keyuse= tab->keyuse;
    while (keyuse->key != ref_key && keyuse->table == tab->table)
      keyuse++;

    const table_map const_tables= tab->join->const_table_map;
    while (keyuse->key == ref_key && keyuse->table == tab->table)
    {
      if (!(keyuse->used_tables & ~const_tables))
      {
        if (keyuse->optimize & KEY_OPTIMIZE_REF_OR_NULL)
          return true;
      }
      keyuse++;
    }
  }
  return false;
}

/**
  Test if we can use one of the 'usable_keys' instead of 'ref' key
  for sorting.

  @param ref			Number of key, used for WHERE clause
  @param usable_keys		Keys for testing

  @return
    - MAX_KEY			If we can't use other key
    - the number of found key	Otherwise
*/

static uint
test_if_subkey(ORDER *order, JOIN_TAB *tab, uint ref, uint ref_key_parts,
	       const key_map *usable_keys)
{
  uint nr;
  uint min_length= (uint) ~0;
  uint best= MAX_KEY;
  TABLE *table= tab->table;
  KEY_PART_INFO *ref_key_part= table->key_info[ref].key_part;
  KEY_PART_INFO *ref_key_part_end= ref_key_part + ref_key_parts;

  for (nr= 0 ; nr < table->s->keys ; nr++)
  {
    if (usable_keys->is_set(nr) &&
	table->key_info[nr].key_length < min_length &&
	table->key_info[nr].user_defined_key_parts >= ref_key_parts &&
	is_subkey(table->key_info[nr].key_part, ref_key_part,
		  ref_key_part_end) &&
        !is_ref_or_null_optimized(tab, nr) &&
	test_if_order_by_key(order, table, nr))
    {
      min_length= table->key_info[nr].key_length;
      best= nr;
    }
  }
  return best;
}


/**
  It is not obvious to see that test_if_skip_sort_order() never changes the
  plan if no_changes is true. So we double-check: creating an instance of this
  class saves some important access-path-related information of the current
  table; when the instance is destroyed, the latest access-path information is
  compared with saved data.
*/
class Plan_change_watchdog
{
#ifndef DBUG_OFF
public:
  /**
    @param tab_arg     table whose access path is being determined
    @param no_changes  whether a change to the access path is allowed
  */
  Plan_change_watchdog(const JOIN_TAB *tab_arg, const bool no_changes_arg)
  {
    // Only to keep gcc 4.1.2-44 silent about uninitialized variables
    quick= NULL;
    quick_index= 0;
    if (no_changes_arg)
    {
      tab= tab_arg;
      type= tab->type;
      if ((select= tab->select))
        if ((quick= tab->select->quick))
          quick_index= quick->index;
      use_quick= tab->use_quick;
      ref_key= tab->ref.key;
      ref_key_parts= tab->ref.key_parts;
      index= tab->index;
    }
    else
    {
      tab= NULL;
      // Only to keep gcc 4.1.2-44 silent about uninitialized variables
      type= JT_UNKNOWN;
      select= NULL;
      ref_key= ref_key_parts= index= 0;
      use_quick= QS_NONE;
    }
  }
  ~Plan_change_watchdog()
  {
    if (tab == NULL)
      return;
    // changes are not allowed, we verify:
    DBUG_ASSERT(tab->type == type);
    DBUG_ASSERT(tab->select == select);
    if (select != NULL)
    {
      DBUG_ASSERT(tab->select->quick == quick);
      if (quick != NULL)
        DBUG_ASSERT(tab->select->quick->index == quick_index);
    }
    DBUG_ASSERT(tab->use_quick == use_quick);
    DBUG_ASSERT(tab->ref.key == ref_key);
    DBUG_ASSERT(tab->ref.key_parts == ref_key_parts);
    DBUG_ASSERT(tab->index == index);
  }
private:
  const JOIN_TAB *tab;            ///< table, or NULL if changes are allowed
  enum join_type type;            ///< copy of tab->type
  // "Range / index merge" info
  const SQL_SELECT *select;       ///< copy of tab->select
  const QUICK_SELECT_I *quick;    ///< copy of tab->select->quick
  uint quick_index;               ///< copy of tab->select->quick->index
  enum quick_type use_quick;      ///< copy of tab->use_quick
  // "ref access" info
  int ref_key;                    ///< copy of tab->ref.key
  uint ref_key_parts;/// copy of tab->ref.key_parts
  // Other index-related info
  uint index;                     ///< copy of tab->index
#else // in non-debug build, empty class
public:
  Plan_change_watchdog(const JOIN_TAB *tab_arg, const bool no_changes_arg) {}
#endif
};


/**
  Test if we can skip the ORDER BY by using an index.

  SYNOPSIS
    test_if_skip_sort_order()
      tab
      order
      select_limit
      no_changes
      map

  If we can use an index, the JOIN_TAB / tab->select struct
  is changed to use the index.

  The index must cover all fields in <order>, or it will not be considered.

  @param tab           NULL or JOIN_TAB of the accessed table
  @param order         Linked list of ORDER BY arguments
  @param select_limit  LIMIT value, or HA_POS_ERROR if no limit
  @param no_changes    No changes will be made to the query plan.
  @param map           key_map of applicable indexes.
  @param clause_type   "ORDER BY" etc for printing in optimizer trace

  @todo
    - sergeyp: Results of all index merge selects actually are ordered 
    by clustered PK values.

  @retval
    0    We have to use filesort to do the sorting
  @retval
    1    We can use an index.
*/

bool
test_if_skip_sort_order(JOIN_TAB *tab, ORDER *order, ha_rows select_limit,
                        const bool no_changes, const key_map *map,
                        const char *clause_type)
{
  int ref_key;
  uint ref_key_parts;
  int order_direction= 0;
  uint used_key_parts;
  TABLE *table=tab->table;
  SQL_SELECT *select=tab->select;
  QUICK_SELECT_I *save_quick= select ? select->quick : NULL;
  int best_key= -1;
  Item *orig_cond;
  bool orig_cond_saved= false, set_up_ref_access_to_key= false;
  bool can_skip_sorting= false;                  // used as return value
  int changed_key= -1;
  DBUG_ENTER("test_if_skip_sort_order");
  LINT_INIT(ref_key_parts);
  LINT_INIT(orig_cond);

  /* Check that we are always called with first non-const table */
  DBUG_ASSERT(tab == tab->join->join_tab + tab->join->const_tables); 

  Plan_change_watchdog watchdog(tab, no_changes);

  /* Sorting a single row can always be skipped */
  if (tab->type == JT_EQ_REF ||
      tab->type == JT_CONST  ||
      tab->type == JT_SYSTEM)
  {
    DBUG_RETURN(1);
  }

  /*
    Keys disabled by ALTER TABLE ... DISABLE KEYS should have already
    been taken into account.
  */
  key_map usable_keys= *map;

  for (ORDER *tmp_order=order; tmp_order ; tmp_order=tmp_order->next)
  {
    Item *item= (*tmp_order->item)->real_item();
    if (item->type() != Item::FIELD_ITEM)
    {
      usable_keys.clear_all();
      DBUG_RETURN(0);
    }
    usable_keys.intersect(((Item_field*) item)->field->part_of_sortkey);
    if (usable_keys.is_clear_all())
      DBUG_RETURN(0);					// No usable keys
  }

  ref_key= -1;
  /* Test if constant range in WHERE */
  if (tab->ref.key >= 0 && tab->ref.key_parts)
  {
    if (tab->type == JT_REF_OR_NULL || tab->type == JT_FT)
      DBUG_RETURN(0);
    ref_key=	   tab->ref.key;
    ref_key_parts= tab->ref.key_parts;
  }
  else if (select && select->quick)		// Range found by opt_range
  {
    int quick_type= select->quick->get_type();
    /* 
      assume results are not ordered when index merge is used 
      TODO: sergeyp: Results of all index merge selects actually are ordered 
      by clustered PK values.
    */
  
    if (quick_type == QUICK_SELECT_I::QS_TYPE_INDEX_MERGE || 
        quick_type == QUICK_SELECT_I::QS_TYPE_ROR_UNION || 
        quick_type == QUICK_SELECT_I::QS_TYPE_ROR_INTERSECT)
      DBUG_RETURN(0);
    ref_key=	   select->quick->index;
    ref_key_parts= select->quick->used_key_parts;
  }

  /*
    If part of the select condition has been pushed we use the
    select condition as it was before pushing. The original
    select condition is saved so that it can be restored when
    exiting this function (if we have not changed index).
  */
  if (tab->pre_idx_push_cond)
  {
    orig_cond=
      tab->set_jt_and_sel_condition(tab->pre_idx_push_cond, __LINE__);
    orig_cond_saved= true;
  }

  Opt_trace_context * const trace= &tab->join->thd->opt_trace;
  Opt_trace_object trace_wrapper(trace);
  Opt_trace_object
    trace_skip_sort_order(trace, "reconsidering_access_paths_for_index_ordering");
  trace_skip_sort_order.add_alnum("clause", clause_type);

  if (ref_key >= 0)
  {
    /*
      We come here when there is a {ref or or ordered range access} key.
    */
    if (!usable_keys.is_set(ref_key))
    {
      /*
        We come here when ref_key is not among usable_keys, try to find a
        usable prefix key of that key.
      */
      uint new_ref_key;
      /*
	If using index only read, only consider other possible index only
	keys
      */
      if (table->covering_keys.is_set(ref_key))
	usable_keys.intersect(table->covering_keys);

      if ((new_ref_key= test_if_subkey(order, tab, ref_key, ref_key_parts,
				       &usable_keys)) < MAX_KEY)
      {
	/* Found key that can be used to retrieve data in sorted order */
	if (tab->ref.key >= 0)
        {
          /*
            We'll use ref access method on key new_ref_key. The actual change
            is done further down in this function where we update the plan.
          */
          set_up_ref_access_to_key= true;
        }
	else if (!no_changes)
	{
          /*
            The range optimizer constructed QUICK_RANGE for ref_key, and
            we want to use instead new_ref_key as the index. We can't
            just change the index of the quick select, because this may
            result in an incosistent QUICK_SELECT object. Below we
            create a new QUICK_SELECT from scratch so that all its
            parameres are set correctly by the range optimizer.

            Note that the range optimizer is NOT called if
            no_changes==true. This reason is that the range optimizer
            cannot find a QUICK that can return ordered result unless
            index access (ref or index scan) is also able to do so
            (which test_if_order_by_key () will tell).
            Admittedly, range access may be much more efficient than
            e.g. index scan, but the only thing that matters when
            no_change==true is the answer to the question: "Is it
            possible to avoid sorting if an index is used to access
            this table?". The answer does not depend on the outcome of
            the range optimizer.
          */
          key_map new_ref_key_map;  // Force the creation of quick select
          new_ref_key_map.set_bit(new_ref_key); // only for new_ref_key.

          Opt_trace_object
            trace_recest(trace, "rows_estimation");
          trace_recest.add_utf8_table(tab->table).
          add_utf8("index", table->key_info[new_ref_key].name);
          select->quick= 0;
          if (select->test_quick_select(tab->join->thd, 
                                        new_ref_key_map, 
                                        0,       // empty table_map
                                        (tab->join->select_options &
                                         OPTION_FOUND_ROWS) ?
                                        HA_POS_ERROR :
                                        tab->join->unit->select_limit_cnt,
                                        false,   // don't force quick range
                                        order->direction) <= 0)
          {
            can_skip_sorting= false;
            goto fix_ICP;
          }
	}
        ref_key= new_ref_key;
        changed_key= new_ref_key;
      }
    }
    /* Check if we get the rows in requested sorted order by using the key */
    if (usable_keys.is_set(ref_key) &&
        (order_direction= test_if_order_by_key(order,table,ref_key,
					       &used_key_parts)))
      goto check_reverse_order;
  }
  {
    /*
      There was no {ref or or ordered range access} key, or it was not
      satisfying, neither was any prefix of it. Do a cost-based search on all
      keys:
    */
    uint best_key_parts= 0;
    uint saved_best_key_parts= 0;
    int best_key_direction= 0;
    JOIN *join= tab->join;
    ha_rows table_records= table->file->stats.records;

    test_if_cheaper_ordering(tab, order, table, usable_keys,
                             ref_key, select_limit,
                             &best_key, &best_key_direction,
                             &select_limit, &best_key_parts,
                             &saved_best_key_parts);

    if (best_key < 0)
    {
      // No usable key has been found
      can_skip_sorting= false;
      goto fix_ICP;
    }

    /*
      filesort() and join cache are usually faster than reading in 
      index order and not using join cache, except in case that chosen
      index is clustered primary key.
    */
    if ((select_limit >= table_records) &&
        (tab->type == JT_ALL &&
         tab->join->primary_tables > tab->join->const_tables + 1) &&
         ((unsigned) best_key != table->s->primary_key ||
          !table->file->primary_key_is_clustered()))
    {
      can_skip_sorting= false;
      goto fix_ICP;
    }

    if (select &&
        table->quick_keys.is_set(best_key) &&
        !tab->quick_order_tested.is_set(best_key) &&
        best_key != ref_key)
    {
      tab->quick_order_tested.set_bit(best_key);
      Opt_trace_object
        trace_recest(trace, "rows_estimation");
      trace_recest.add_utf8_table(tab->table).
        add_utf8("index", table->key_info[best_key].name);

      key_map map;           // Force the creation of quick select
      map.set_bit(best_key); // only best_key.
      select->quick= 0;
      select->test_quick_select(join->thd, 
                                map, 
                                0,        // empty table_map
                                join->select_options & OPTION_FOUND_ROWS ?
                                HA_POS_ERROR :
                                join->unit->select_limit_cnt,
                                true,     // force quick range
                                order->direction);
    }
    order_direction= best_key_direction;
    /*
      saved_best_key_parts is actual number of used keyparts found by the
      test_if_order_by_key function. It could differ from keyinfo->key_parts,
      thus we have to restore it in case of desc order as it affects
      QUICK_SELECT_DESC behaviour.
    */
    used_key_parts= (order_direction == -1) ?
      saved_best_key_parts :  best_key_parts;
    changed_key= best_key;
    // We will use index scan or range scan:
    set_up_ref_access_to_key= false;
  }

check_reverse_order:                  
  DBUG_ASSERT(order_direction != 0);

  if (order_direction == -1)		// If ORDER BY ... DESC
  {
    if (select && select->quick)
    {
      /*
	Don't reverse the sort order, if it's already done.
        (In some cases test_if_order_by_key() can be called multiple times
      */
      if (select->quick->reverse_sorted())
      {
        can_skip_sorting= true;
        goto fix_ICP;
      }

      if (select->quick->reverse_sort_possible())
        can_skip_sorting= true;
      else
      {
        can_skip_sorting= false;
        goto fix_ICP;
      }

      /*
        test_quick_select() should not create a quick that cannot do
        reverse ordering
      */
      DBUG_ASSERT((select->quick == save_quick) || can_skip_sorting);
    }
    else
    {
      // Other index access (ref or scan) poses no problem
      can_skip_sorting= true;
    }
  }
  else
  {
    // ORDER BY ASC poses no problem
    can_skip_sorting= true;
  }

  DBUG_ASSERT(can_skip_sorting);

  /*
    Update query plan with access pattern for doing 
    ordered access according to what we have decided
    above.
  */
  if (!no_changes) // We are allowed to update QEP
  {
    if (set_up_ref_access_to_key)
    {
      /*
        We'll use ref access method on key changed_key. In general case 
        the index search tuple for changed_ref_key will be different (e.g.
        when one index is defined as (part1, part2, ...) and another as
        (part1, part2(N), ...) and the WHERE clause contains 
        "part1 = const1 AND part2=const2". 
        So we build tab->ref from scratch here.
      */
      Key_use *keyuse= tab->keyuse;
      while (keyuse->key != (uint)changed_key && keyuse->table == tab->table)
        keyuse++;

      if (create_ref_for_key(tab->join, tab, keyuse, tab->prefix_tables()))
      {
        can_skip_sorting= false;
        goto fix_ICP;
      }

      DBUG_ASSERT(tab->type != JT_REF_OR_NULL && tab->type != JT_FT);
    }
    else if (best_key >= 0)
    {
      bool quick_created= 
        (select && select->quick && select->quick!=save_quick);

      /*
        If ref_key used index tree reading only ('Using index' in EXPLAIN),
        and best_key doesn't, then revert the decision.
      */
      if(!table->covering_keys.is_set(best_key))
        table->set_keyread(false);
      if (!quick_created)
      {
        if (select)                  // Throw any existing quick select
          select->quick= 0;          // Cleanup either reset to save_quick,
                                     // or 'delete save_quick'
        tab->index= best_key;
        tab->read_first_record= order_direction > 0 ?
                                join_read_first:join_read_last;
        tab->type=JT_INDEX_SCAN;       // Read with index_first(), index_next()

        table->file->ha_index_or_rnd_end();
        if (tab->join->select_options & SELECT_DESCRIBE)
        {
          /*
            @todo this neutralizes add_ref_to_table_cond(); as a result
            EXPLAIN shows no "using where" though real SELECT has one.
          */
          tab->ref.key= -1;
          tab->ref.key_parts= 0;
          if (select_limit < table->file->stats.records) 
            tab->limit= select_limit;
        }
      }
      else if (tab->type != JT_ALL)
      {
        /*
          We're about to use a quick access to the table.
          We need to change the access method so as the quick access
          method is actually used.
        */
        DBUG_ASSERT(tab->select->quick);
        DBUG_ASSERT(tab->select->quick->index==(uint)best_key);
        tab->type=JT_ALL;
        tab->use_quick=QS_RANGE;
        tab->ref.key= -1;
        tab->ref.key_parts=0;		// Don't use ref key.
        tab->read_first_record= join_init_read_record;
        if (tab->is_using_loose_index_scan())
          tab->join->tmp_table_param.precomputed_group_by= TRUE;
        /*
          TODO: update the number of records in tab->position
        */
      }
    } // best_key >= 0

    if (order_direction == -1)		// If ORDER BY ... DESC
    {
      if (select && select->quick)
      {
        /* ORDER BY range_key DESC */
        QUICK_SELECT_I *tmp= select->quick->make_reverse(used_key_parts);
        if (!tmp)
        {
          tab->limit= 0;
          can_skip_sorting= false;      // Reverse sort failed -> filesort
          goto fix_ICP;
        }
        if (select->quick == save_quick)
          save_quick= 0;                // Because set_quick(tmp) frees it
        select->set_quick(tmp);
      }
      else if (tab->type != JT_INDEX_SCAN && tab->type != JT_REF_OR_NULL &&
               tab->ref.key >= 0 && tab->ref.key_parts <= used_key_parts)
      {
        /*
          SELECT * FROM t1 WHERE a=1 ORDER BY a DESC,b DESC

          Use a traversal function that starts by reading the last row
          with key part (A) and then traverse the index backwards.
        */
        tab->read_first_record= join_read_last_key;
        tab->read_record.read_record= join_read_prev_same;
        tab->read_record.unlock_row= rr_unlock_row;

        /*
          The current implementation of join_read_prev_same() does not
          work well in combination with ICP and can lead to increased
          execution time. Setting changed_key to the current key
          (based on that we change the access order for the key) will
          ensure that a pushed index condition will be cancelled.
        */
        changed_key= tab->ref.key;
      }
    }
    else if (select && select->quick)
      select->quick->need_sorted_output(true);
  } // QEP has been modified

fix_ICP:
  /*
    Cleanup:
    We may have both a 'select->quick' and 'save_quick' (original)
    at this point. Delete the one that we won't use.
  */
  if (can_skip_sorting && !no_changes)
  {
    // Keep current (ordered) select->quick
    if (select && save_quick != select->quick)
      delete save_quick;
  }
  else
  {
    // Restore original save_quick
    if (select && select->quick != save_quick)
      select->set_quick(save_quick);
  }

  Opt_trace_object
    trace_change_index(trace, "index_order_summary");
  trace_change_index.add_utf8_table(tab->table)
    .add("index_provides_order", can_skip_sorting)
    .add_alnum("order_direction", order_direction == 1 ? "asc" :
               ((order_direction == -1) ? "desc" :
                "undefined"));

  if (changed_key >= 0)
  {
    bool cancelled_ICP= false;
    // switching to another index, makes pushed index condition obsolete
    if (!no_changes && table->file->pushed_idx_cond)
    {
      table->file->cancel_pushed_idx_cond();
      // and thus tab's m_condition must be how it was before ICP
      orig_cond_saved= false;
      cancelled_ICP= true;
    }
    if (unlikely(trace->is_started()))
    {
      if (cancelled_ICP)
        trace_change_index.add("disabled_pushed_condition_on_old_index", true);
      trace_change_index.add_utf8("index", table->key_info[changed_key].name);
      trace_change_index.add("plan_changed", !no_changes);
      if (!no_changes)
      {
        const char *new_type= tab->type == JT_INDEX_SCAN ? "index_scan" :
          (tab->select && tab->select->quick) ?
          "range" : join_type_str[tab->type];
        trace_change_index.add_alnum("access_type", new_type);
      }
    }
  }
  else if (unlikely(trace->is_started()))
  {
    trace_change_index.add_utf8("index",
                                ref_key >= 0 ?
                                table->key_info[ref_key].name : "unknown");
    trace_change_index.add("plan_changed", false);
  }
  if (orig_cond_saved)
  {
    // ICP set up prior to the call, is still valid:
    tab->set_jt_and_sel_condition(orig_cond, __LINE__);
  }
  DBUG_RETURN(can_skip_sorting);
}



/**
  Update join with count of the different type of fields.
*/

void
count_field_types(SELECT_LEX *select_lex, TMP_TABLE_PARAM *param, 
                  List<Item> &fields, bool reset_with_sum_func)
{
  List_iterator<Item> li(fields);
  Item *field;

  param->field_count=param->sum_func_count=param->func_count=
    param->hidden_field_count=0;
  param->quick_group=1;
  while ((field=li++))
  {
    Item::Type real_type= field->real_item()->type();
    if (real_type == Item::FIELD_ITEM)
      param->field_count++;
    else if (real_type == Item::SUM_FUNC_ITEM)
    {
      if (! field->const_item())
      {
	Item_sum *sum_item=(Item_sum*) field->real_item();
        if (!sum_item->depended_from() ||
            sum_item->depended_from() == select_lex)
        {
          if (!sum_item->quick_group)
            param->quick_group=0;			// UDF SUM function
          param->sum_func_count++;

          for (uint i=0 ; i < sum_item->get_arg_count() ; i++)
          {
            if (sum_item->get_arg(i)->real_item()->type() == Item::FIELD_ITEM)
              param->field_count++;
            else
              param->func_count++;
          }
        }
        param->func_count++;
      }
    }
    else
    {
      param->func_count++;
      if (reset_with_sum_func)
	field->with_sum_func=0;
    }
  }
}


/**
  Return 1 if second is a subpart of first argument.

  If first parts has different direction, change it to second part
  (group is sorted like order)
*/

bool
test_if_subpart(ORDER *a,ORDER *b)
{
  for (; a && b; a=a->next,b=b->next)
  {
    if ((*a->item)->eq(*b->item,1))
      a->direction= b->direction;
    else
      return 0;
  }
  return test(!b);
}

/**
  calc how big buffer we need for comparing group entries.
*/

void
calc_group_buffer(JOIN *join,ORDER *group)
{
  uint key_length=0, parts=0, null_parts=0;

  if (group)
    join->group= 1;
  for (; group ; group=group->next)
  {
    Item *group_item= *group->item;
    Field *field= group_item->get_tmp_table_field();
    if (field)
    {
      enum_field_types type;
      if ((type= field->type()) == MYSQL_TYPE_BLOB)
	key_length+=MAX_BLOB_WIDTH;		// Can't be used as a key
      else if (type == MYSQL_TYPE_VARCHAR || type == MYSQL_TYPE_VAR_STRING)
        key_length+= field->field_length + HA_KEY_BLOB_LENGTH;
      else if (type == MYSQL_TYPE_BIT)
      {
        /* Bit is usually stored as a longlong key for group fields */
        key_length+= 8;                         // Big enough
      }
      else
	key_length+= field->pack_length();
    }
    else
    { 
      switch (group_item->result_type()) {
      case REAL_RESULT:
        key_length+= sizeof(double);
        break;
      case INT_RESULT:
        key_length+= sizeof(longlong);
        break;
      case DECIMAL_RESULT:
        key_length+= my_decimal_get_binary_size(group_item->max_length - 
                                                (group_item->decimals ? 1 : 0),
                                                group_item->decimals);
        break;
      case STRING_RESULT:
      {
        /*
          As items represented as DATE/TIME fields in the group buffer
          have STRING_RESULT result type, we increase the length 
          by 8 as maximum pack length of such fields.
        */
        if (group_item->is_temporal())
        {
          key_length+= 8;
        }
        else if (group_item->field_type() == MYSQL_TYPE_BLOB)
          key_length+= MAX_BLOB_WIDTH;		// Can't be used as a key
        else
        {
          /*
            Group strings are taken as varstrings and require an length field.
            A field is not yet created by create_tmp_field()
            and the sizes should match up.
          */
          key_length+= group_item->max_length + HA_KEY_BLOB_LENGTH;
        }
        break;
      }
      default:
        /* This case should never be choosen */
        DBUG_ASSERT(0);
        my_error(ER_OUT_OF_RESOURCES, MYF(ME_FATALERROR));
      }
    }
    parts++;
    if (group_item->maybe_null)
      null_parts++;
  }
  join->tmp_table_param.group_length=key_length+null_parts;
  join->tmp_table_param.group_parts=parts;
  join->tmp_table_param.group_null_parts=null_parts;
}



/**
  Make an array of pointers to sum_functions to speed up
  sum_func calculation.

  @retval
    0	ok
  @retval
    1	Error
*/

bool JOIN::alloc_func_list()
{
  uint func_count, group_parts;
  DBUG_ENTER("alloc_func_list");

  func_count= tmp_table_param.sum_func_count;
  /*
    If we are using rollup, we need a copy of the summary functions for
    each level
  */
  if (rollup.state != ROLLUP::STATE_NONE)
    func_count*= (send_group_parts+1);

  group_parts= send_group_parts;
  /*
    If distinct, reserve memory for possible
    disctinct->group_by optimization
  */
  if (select_distinct)
  {
    group_parts+= fields_list.elements;
    /*
      If the ORDER clause is specified then it's possible that
      it also will be optimized, so reserve space for it too
    */
    if (order)
    {
      ORDER *ord;
      for (ord= order; ord; ord= ord->next)
        group_parts++;
    }
  }

  /* This must use calloc() as rollup_make_fields depends on this */
  sum_funcs= (Item_sum**) thd->calloc(sizeof(Item_sum**) * (func_count+1) +
				           sizeof(Item_sum***) * (group_parts+1));
  sum_funcs_end= (Item_sum***) (sum_funcs + func_count + 1);
  DBUG_RETURN(sum_funcs == 0);
}


/**
  Initialize 'sum_funcs' array with all Item_sum objects.

  @param field_list        All items
  @param send_result_set_metadata       Items in select list
  @param before_group_by   Set to 1 if this is called before GROUP BY handling
  @param recompute         Set to TRUE if sum_funcs must be recomputed

  @retval
    0  ok
  @retval
    1  error
*/

bool JOIN::make_sum_func_list(List<Item> &field_list,
                              List<Item> &send_result_set_metadata,
			      bool before_group_by, bool recompute)
{
  List_iterator_fast<Item> it(field_list);
  Item_sum **func;
  Item *item;
  DBUG_ENTER("make_sum_func_list");

  if (*sum_funcs && !recompute)
    DBUG_RETURN(FALSE); /* We have already initialized sum_funcs. */

  func= sum_funcs;
  while ((item=it++))
  {
    if (item->type() == Item::SUM_FUNC_ITEM && !item->const_item() &&
        (!((Item_sum*) item)->depended_from() ||
         ((Item_sum *)item)->depended_from() == select_lex))
      *func++= (Item_sum*) item;
  }
  if (before_group_by && rollup.state == ROLLUP::STATE_INITED)
  {
    rollup.state= ROLLUP::STATE_READY;
    if (rollup_make_fields(field_list, send_result_set_metadata, &func))
      DBUG_RETURN(TRUE);			// Should never happen
  }
  else if (rollup.state == ROLLUP::STATE_NONE)
  {
    for (uint i=0 ; i <= send_group_parts ;i++)
      sum_funcs_end[i]= func;
  }
  else if (rollup.state == ROLLUP::STATE_READY)
    DBUG_RETURN(FALSE);                         // Don't put end marker
  *func=0;					// End marker
  DBUG_RETURN(FALSE);
}


/**
  Free joins of subselect of this select.

  @param thd      THD pointer
  @param select   pointer to st_select_lex which subselects joins we will free
*/

void free_underlaid_joins(THD *thd, SELECT_LEX *select)
{
  for (SELECT_LEX_UNIT *unit= select->first_inner_unit();
       unit;
       unit= unit->next_unit())
    unit->cleanup();
}

/****************************************************************************
  ROLLUP handling
****************************************************************************/

/**
   Wrap all constant Items in GROUP BY list.

   For ROLLUP queries each constant item referenced in GROUP BY list
   is wrapped up into an Item_func object yielding the same value
   as the constant item. The objects of the wrapper class are never
   considered as constant items and besides they inherit all
   properties of the Item_result_field class.
   This wrapping allows us to ensure writing constant items
   into temporary tables whenever the result of the ROLLUP
   operation has to be written into a temporary table, e.g. when
   ROLLUP is used together with DISTINCT in the SELECT list.
   Usually when creating temporary tables for a intermidiate
   result we do not include fields for constant expressions.

   @retval
     0  if ok
   @retval
     1  on error
*/

bool JOIN::rollup_process_const_fields()
{
  ORDER *group_tmp;
  Item *item;
  List_iterator<Item> it(all_fields);

  for (group_tmp= group_list; group_tmp; group_tmp= group_tmp->next)
  {
    if (!(*group_tmp->item)->const_item())
      continue;
    while ((item= it++))
    {
      if (*group_tmp->item == item)
      {
        Item* new_item= new Item_func_rollup_const(item);
        if (!new_item)
          return 1;
        new_item->fix_fields(thd, (Item **) 0);
        thd->change_item_tree(it.ref(), new_item);
        for (ORDER *tmp= group_tmp; tmp; tmp= tmp->next)
        {
          if (*tmp->item == item)
            thd->change_item_tree(tmp->item, new_item);
        }
        break;
      }
    }
    it.rewind();
  }
  return 0;
}
  

/**
  Fill up rollup structures with pointers to fields to use.

  Creates copies of item_sum items for each sum level.

  @param fields_arg		List of all fields (hidden and real ones)
  @param sel_fields		Pointer to selected fields
  @param func			Store here a pointer to all fields

  @retval
    0	if ok;
    In this case func is pointing to next not used element.
  @retval
    1    on error
*/

bool JOIN::rollup_make_fields(List<Item> &fields_arg, List<Item> &sel_fields,
			      Item_sum ***func)
{
  List_iterator_fast<Item> it(fields_arg);
  Item *first_field= sel_fields.head();
  uint level;

  /*
    Create field lists for the different levels

    The idea here is to have a separate field list for each rollup level to
    avoid all runtime checks of which columns should be NULL.

    The list is stored in reverse order to get sum function in such an order
    in func that it makes it easy to reset them with init_sum_functions()

    Assuming:  SELECT a, b, c SUM(b) FROM t1 GROUP BY a,b WITH ROLLUP

    rollup.fields[0] will contain list where a,b,c is NULL
    rollup.fields[1] will contain list where b,c is NULL
    ...
    rollup.ref_pointer_array[#] points to fields for rollup.fields[#]
    ...
    sum_funcs_end[0] points to all sum functions
    sum_funcs_end[1] points to all sum functions, except grand totals
    ...
  */

  for (level=0 ; level < send_group_parts ; level++)
  {
    uint i;
    uint pos= send_group_parts - level -1;
    bool real_fields= 0;
    Item *item;
    List_iterator<Item> new_it(rollup.fields[pos]);
    Ref_ptr_array ref_array_start= rollup.ref_pointer_arrays[pos];
    ORDER *start_group;

    /* Point to first hidden field */
    uint ref_array_ix= fields_arg.elements-1;

    /* Remember where the sum functions ends for the previous level */
    sum_funcs_end[pos+1]= *func;

    /* Find the start of the group for this level */
    for (i= 0, start_group= group_list ;
	 i++ < pos ;
	 start_group= start_group->next)
      ;

    it.rewind();
    while ((item= it++))
    {
      if (item == first_field)
      {
	real_fields= 1;				// End of hidden fields
        ref_array_ix= 0;
      }

      if (item->type() == Item::SUM_FUNC_ITEM && !item->const_item() &&
          (!((Item_sum*) item)->depended_from() ||
           ((Item_sum *)item)->depended_from() == select_lex))
          
      {
	/*
	  This is a top level summary function that must be replaced with
	  a sum function that is reset for this level.

	  NOTE: This code creates an object which is not that nice in a
	  sub select.  Fortunately it's not common to have rollup in
	  sub selects.
	*/
	item= item->copy_or_same(thd);
	((Item_sum*) item)->make_unique();
	*(*func)= (Item_sum*) item;
	(*func)++;
      }
      else 
      {
	/* Check if this is something that is part of this group by */
	ORDER *group_tmp;
	for (group_tmp= start_group, i= pos ;
             group_tmp ; group_tmp= group_tmp->next, i++)
	{
          if (*group_tmp->item == item)
	  {
	    /*
	      This is an element that is used by the GROUP BY and should be
	      set to NULL in this level
	    */
            Item_null_result *null_item=
              new (thd->mem_root) Item_null_result(item->field_type(),
                                                   item->result_type());
            if (!null_item)
              return 1;
	    item->maybe_null= 1;		// Value will be null sometimes
            null_item->result_field= item->get_tmp_table_field();
            item= null_item;
	    break;
	  }
	}
      }
      ref_array_start[ref_array_ix]= item;
      if (real_fields)
      {
	(void) new_it++;			// Point to next item
	new_it.replace(item);			// Replace previous
	ref_array_ix++;
      }
      else
	ref_array_ix--;
    }
  }
  sum_funcs_end[0]= *func;			// Point to last function
  return 0;
}


/**
  clear results if there are not rows found for group
  (end_send_group/end_write_group)
*/

void JOIN::clear()
{
  for (uint tableno= 0; tableno < primary_tables; tableno++)
  {
    TABLE *const table= (join_tab+tableno)->table;
    if (table)
      mark_as_null_row(table);
  }

  copy_fields(&tmp_table_param);

  if (sum_funcs)
  {
    Item_sum *func, **func_ptr= sum_funcs;
    while ((func= *(func_ptr++)))
      func->clear();
  }
}


/**
  change select_result object of JOIN.

  @param res		new select_result object

  @retval
    FALSE   OK
  @retval
    TRUE    error
*/

bool JOIN::change_result(select_result *res)
{
  DBUG_ENTER("JOIN::change_result");
  result= res;
  if (result->prepare(fields_list, select_lex->master_unit()) ||
      result->prepare2())
  {
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}


/**
  Init tmp tables usage info.

  @details
  This function finalizes execution plan by taking following actions:
    .) tmp tables are created, but not instantiated (this is done during
       execution). JOIN_TABs dedicated to tmp tables are filled appropriately.
       see JOIN::create_intermediate_table.
    .) prepare fields lists (fields, all_fields, ref_pointer_array slices) for
       each required stage of execution. These fields lists are set for
       tmp tables' tabs and for the tab of last table in the join.
    .) fill info for sorting/grouping/dups removal is prepared and saved to
       appropriate tabs. Here is an example:
        SELECT * from t1,t2 WHERE ... GROUP BY t1.f1 ORDER BY t2.f2, t1.f2
        and lets assume that the table order in the plan is t1,t2.
       In this case optimizer will sort for group only the first table as the
       second one isn't mentioned in GROUP BY. The result will be materialized
       in tmp table.  As filesort can't sort join optimizer will sort tmp table
       also. The first sorting (for group) is called simple as is doesn't
       require tmp table.  The Filesort object for it is created here - in
       JOIN::create_intermediate_table.  Filesort for the second case is
       created here, in JOIN::make_tmp_tables_info.
  @returns
  false - Ok
  true  - Error
*/

bool JOIN::make_tmp_tables_info()
{
  List<Item> *curr_all_fields= &all_fields;
  List<Item> *curr_fields_list= &fields_list;
  bool materialize_join= false;
  uint curr_tmp_table= const_tables;
  TABLE *exec_tmp_table= NULL;
  DBUG_ENTER("JOIN::make_tmp_tables_info");
  having_for_explain= having;

  const bool has_group_by= this->group;
  /*
    Setup last table to provide fields and all_fields lists to the next
    node in the plan.
  */
  if (join_tab)
  {
    join_tab[primary_tables - 1].fields= &fields_list;
    join_tab[primary_tables - 1].all_fields= &all_fields;
  }
  /*
    The loose index scan access method guarantees that all grouping or
    duplicate row elimination (for distinct) is already performed
    during data retrieval, and that all MIN/MAX functions are already
    computed for each group. Thus all MIN/MAX functions should be
    treated as regular functions, and there is no need to perform
    grouping in the main execution loop.
    Notice that currently loose index scan is applicable only for
    single table queries, thus it is sufficient to test only the first
    join_tab element of the plan for its access method.
  */
  if (join_tab && join_tab->is_using_loose_index_scan())
    tmp_table_param.precomputed_group_by=
      !join_tab->is_using_agg_loose_index_scan();

  /* Create a tmp table if distinct or if the sort is too complicated */
  if (need_tmp)
  {
    curr_tmp_table= primary_tables;
    tmp_tables++;
    if (plan_is_const())
      first_select= sub_select_op;

    /*
      Create temporary table on first execution of this join.
      (Will be reused if this is a subquery that is executed several times.)
    */
    init_items_ref_array();

    ORDER_with_src tmp_group;
    if (!simple_group && !(test_flags & TEST_NO_KEY_GROUP))
      tmp_group= group_list;
      
    tmp_table_param.hidden_field_count= 
      all_fields.elements - fields_list.elements;

    if (create_intermediate_table(&join_tab[curr_tmp_table],
                                  &all_fields, tmp_group, 
                                  group_list && simple_group))
      DBUG_RETURN(true);
    exec_tmp_table= join_tab[curr_tmp_table].table;

    if (exec_tmp_table->distinct)
      optimize_distinct();

    /*
      If there is no sorting or grouping, one may turn off
      requirement that access method should deliver rows in sorted
      order.  Exception: LooseScan strategy for semijoin requires
      sorted access even if final result is not to be sorted.
    */
    if (!sort_and_group &&
        !plan_is_const() && 
        join_tab[const_tables].position->sj_strategy != SJ_OPT_LOOSE_SCAN)
      disable_sorted_access(&join_tab[const_tables]);
    /*
      We don't have to store rows in temp table that doesn't match HAVING if:
      - we are sorting the table and writing complete group rows to the
        temp table.
      - We are using DISTINCT without resolving the distinct as a GROUP BY
        on all columns.

      If having is not handled here, it will be checked before the row
      is sent to the client.
    */
    if (having &&
        (sort_and_group || (exec_tmp_table->distinct && !group_list)))
    {
      // Attach HAVING to tmp table's condition
      join_tab[curr_tmp_table].having= having;
      having= NULL; // Already done
    }

    /* Change sum_fields reference to calculated fields in tmp_table */
    DBUG_ASSERT(items1.is_null());
    items1= ref_ptr_array_slice(2);
    if (sort_and_group || join_tab[curr_tmp_table].table->group ||
        tmp_table_param.precomputed_group_by)
    {
      if (change_to_use_tmp_fields(thd, items1,
                                   tmp_fields_list1, tmp_all_fields1,
                                   fields_list.elements, all_fields))
        DBUG_RETURN(true);
    }
    else
    {
      if (change_refs_to_tmp_fields(thd, items1,
                                    tmp_fields_list1, tmp_all_fields1,
                                    fields_list.elements, all_fields))
        DBUG_RETURN(true);
    }
    curr_all_fields= &tmp_all_fields1;
    curr_fields_list= &tmp_fields_list1;
    // Need to set them now for correct group_fields setup, reset at the end.
    set_items_ref_array(items1);
    join_tab[curr_tmp_table].ref_array= &items1;
    join_tab[curr_tmp_table].all_fields= &tmp_all_fields1;
    join_tab[curr_tmp_table].fields= &tmp_fields_list1;
    setup_tmptable_write_func(&join_tab[curr_tmp_table]);
 
    tmp_table_param.func_count= 0;
    tmp_table_param.field_count+= tmp_table_param.func_count;
    if (sort_and_group || join_tab[curr_tmp_table].table->group)
    {
      tmp_table_param.field_count+= tmp_table_param.sum_func_count;
      tmp_table_param.sum_func_count= 0;
    }

    if (exec_tmp_table->group)
    {						// Already grouped
      if (!order && !no_order && !skip_sort_order)
        order= group_list;  /* order by group */
      group_list= NULL;
    }
    /*
      If we have different sort & group then we must sort the data by group
      and copy it to another tmp table
      This code is also used if we are using distinct something
      we haven't been able to store in the temporary table yet
      like SEC_TO_TIME(SUM(...)).
    */

    if ((group_list && 
         (!test_if_subpart(group_list, order) || select_distinct)) ||
        (select_distinct && tmp_table_param.using_indirect_summary_function))
    {					/* Must copy to another table */
      DBUG_PRINT("info",("Creating group table"));
      
      calc_group_buffer(this, group_list);
      count_field_types(select_lex, &tmp_table_param, tmp_all_fields1,
                        select_distinct && !group_list);
      tmp_table_param.hidden_field_count= 
        tmp_all_fields1.elements - tmp_fields_list1.elements;
      
      if (!exec_tmp_table->group && !exec_tmp_table->distinct)
      {
        // 1st tmp table were materializing join result
        materialize_join= true;
        explain_flags.set(ESC_BUFFER_RESULT, ESP_USING_TMPTABLE);
      }
      curr_tmp_table++;
      tmp_tables++;

      /* group data to new table */
      /*
        If the access method is loose index scan then all MIN/MAX
        functions are precomputed, and should be treated as regular
        functions. See extended comment above.
      */
      if (join_tab->is_using_loose_index_scan())
        tmp_table_param.precomputed_group_by= TRUE;

      tmp_table_param.hidden_field_count= 
        curr_all_fields->elements - curr_fields_list->elements;
      ORDER_with_src dummy= NULL; //TODO can use table->group here also

      if (create_intermediate_table(&join_tab[curr_tmp_table],
                                    curr_all_fields, dummy, true))
	DBUG_RETURN(true);

      if (group_list)
      {
        explain_flags.set(group_list.src, ESP_USING_TMPTABLE);
        if (!plan_is_const())        // No need to sort a single row
        {
          JOIN_TAB *sort_tab= &join_tab[curr_tmp_table - 1];
          if (add_sorting_to_table(sort_tab, &group_list))
            DBUG_RETURN(true);
        }

        if (make_group_fields(this, this))
          DBUG_RETURN(true);
      }

      if (!sort_and_group && !plan_is_const())
        disable_sorted_access(&join_tab[const_tables]);
      // Setup sum funcs only when necessary, otherwise we might break info
      // for the first table
      if (group_list || tmp_table_param.sum_func_count)
      {
        if (make_sum_func_list(*curr_all_fields, *curr_fields_list, true, true))
          DBUG_RETURN(true);
        if (prepare_sum_aggregators(sum_funcs,
                                    !join_tab->is_using_agg_loose_index_scan()))
          DBUG_RETURN(true);
        group_list= NULL;
        if (setup_sum_funcs(thd, sum_funcs))
          DBUG_RETURN(true);
      }
      // No sum funcs anymore
      DBUG_ASSERT(items2.is_null());

      items2= ref_ptr_array_slice(3);
      if (change_to_use_tmp_fields(thd, items2,
                                   tmp_fields_list2, tmp_all_fields2, 
                                   fields_list.elements, tmp_all_fields1))
        DBUG_RETURN(true);

      curr_fields_list= &tmp_fields_list2;
      curr_all_fields= &tmp_all_fields2;
      set_items_ref_array(items2);
      join_tab[curr_tmp_table].ref_array= &items2;
      join_tab[curr_tmp_table].all_fields= &tmp_all_fields2;
      join_tab[curr_tmp_table].fields= &tmp_fields_list2;
      setup_tmptable_write_func(&join_tab[curr_tmp_table]);

      tmp_table_param.field_count+= tmp_table_param.sum_func_count;
      tmp_table_param.sum_func_count= 0;
    }
    if (join_tab[curr_tmp_table].table->distinct)
      select_distinct= false;               /* Each row is unique */

    if (select_distinct && !group_list)
    {
      if (having)
      {
        join_tab[curr_tmp_table].having= having;
        having->update_used_tables();
      }
      join_tab[curr_tmp_table].distinct= true;
      explain_flags.set(ESC_DISTINCT, ESP_DUPS_REMOVAL);
      having= NULL;
      select_distinct= false;
    }
    /* Clean tmp_table_param for the next tmp table. */
    tmp_table_param.field_count= tmp_table_param.sum_func_count=
      tmp_table_param.func_count= 0;

    tmp_table_param.copy_field= tmp_table_param.copy_field_end=0;
    first_record= sort_and_group=0;

    if (!group_optimized_away)
    {
      group= false;
    }
    else
    {
      /*
        If grouping has been optimized away, a temporary table is
        normally not needed unless we're explicitly requested to create
        one (e.g. due to a SQL_BUFFER_RESULT hint or INSERT ... SELECT).

        In this case (grouping was optimized away), temp_table was
        created without a grouping expression and JOIN::exec() will not
        perform the necessary grouping (by the use of end_send_group()
        or end_write_group()) if JOIN::group is set to false.
      */
      // the temporary table was explicitly requested
      DBUG_ASSERT(test(select_options & OPTION_BUFFER_RESULT));
      // the temporary table does not have a grouping expression
      DBUG_ASSERT(!join_tab[curr_tmp_table].table->group); 
    }
    calc_group_buffer(this, group_list);
    count_field_types(select_lex, &tmp_table_param, *curr_all_fields, false);
  }

  if (group || implicit_grouping || tmp_table_param.sum_func_count)
  {
    if (make_group_fields(this, this))
      DBUG_RETURN(true);

    DBUG_ASSERT(items3.is_null());

    if (items0.is_null())
      init_items_ref_array();
    items3= ref_ptr_array_slice(4);
    setup_copy_fields(thd, &tmp_table_param,
                      items3, tmp_fields_list3, tmp_all_fields3,
                      curr_fields_list->elements, *curr_all_fields);

    curr_fields_list= &tmp_fields_list3;
    curr_all_fields= &tmp_all_fields3;
    set_items_ref_array(items3);
    if (join_tab)
    {
      // Set grouped fields on the last table
      join_tab[primary_tables + tmp_tables - 1].ref_array= &items3;
      join_tab[primary_tables + tmp_tables - 1].all_fields= &tmp_all_fields3;
      join_tab[primary_tables + tmp_tables - 1].fields= &tmp_fields_list3;
    }
    if (make_sum_func_list(*curr_all_fields, *curr_fields_list, true, true))
      DBUG_RETURN(true);
    if (prepare_sum_aggregators(sum_funcs,
                                !join_tab ||
                                !join_tab-> is_using_agg_loose_index_scan()))
      DBUG_RETURN(true);
    if (setup_sum_funcs(thd, sum_funcs) || thd->is_fatal_error)
      DBUG_RETURN(true);
  }
  if (group_list || order)
  {
    DBUG_PRINT("info",("Sorting for send_result_set_metadata"));
    THD_STAGE_INFO(thd, stage_sorting_result);
    /* If we have already done the group, add HAVING to sorted table */
    if (having && !group_list && !sort_and_group)
    {
      // Some tables may have been const
      having->update_used_tables();
      JOIN_TAB *curr_table= &join_tab[curr_tmp_table];
      table_map used_tables= (const_table_map | curr_table->table->map);

      Item* sort_table_cond= make_cond_for_table(having, used_tables,
                                                 (table_map) 0, false);
      if (sort_table_cond)
      {
	if (!curr_table->select)
	  if (!(curr_table->select= new SQL_SELECT))
	    DBUG_RETURN(true);
	if (!curr_table->select->cond)
	  curr_table->select->cond= sort_table_cond;
	else
	{
	  if (!(curr_table->select->cond=
		new Item_cond_and(curr_table->select->cond,
				  sort_table_cond)))
	    DBUG_RETURN(true);
	  curr_table->select->cond->fix_fields(thd, 0);
	}
        curr_table->set_condition(curr_table->select->cond, __LINE__);
        curr_table->condition()->top_level_item();
	DBUG_EXECUTE("where",print_where(curr_table->select->cond,
					 "select and having",
                                         QT_ORDINARY););

        having= make_cond_for_table(having, ~ (table_map) 0,
                                    ~used_tables, false);
        DBUG_EXECUTE("where",
                     print_where(having, "having after sort", QT_ORDINARY););
      }
    }

    if (group)
      m_select_limit= HA_POS_ERROR;
    else if (!need_tmp)
    {
      /*
        We can abort sorting after thd->select_limit rows if there are no
        filter conditions for any tables after the sorted one.
        Filter conditions come in several forms:
         1. as a condition item attached to the join_tab, or
         2. as a keyuse attached to the join_tab (ref access).
      */
      for (uint i= const_tables + 1; i < primary_tables; i++)
      {
        JOIN_TAB *const tab= join_tab + i;
        if (tab->condition() ||                                // 1
            (tab->keyuse && !tab->first_inner))                // 2
        {
          /* We have to sort all rows */
          m_select_limit= HA_POS_ERROR;
          break;
        }
      }
    }
    /*
      Here we add sorting stage for ORDER BY/GROUP BY clause, if the
      optimiser chose FILESORT to be faster than INDEX SCAN or there is
      no suitable index present.
      OPTION_FOUND_ROWS supersedes LIMIT and is taken into account.
    */
    DBUG_PRINT("info",("Sorting for order by/group by"));
    ORDER_with_src order_arg= group_list ?  group_list : order;
    if (join_tab &&
        ordered_index_usage !=
        (group_list ? ordered_index_group_by : ordered_index_order_by) &&
        join_tab[curr_tmp_table].type != JT_CONST &&
        join_tab[curr_tmp_table].type != JT_EQ_REF) // Don't sort 1 row
    {
      // Sort either first non-const table or the last tmp table
      JOIN_TAB *sort_tab= &join_tab[curr_tmp_table];
      if (need_tmp && !materialize_join && !exec_tmp_table->group)
        explain_flags.set(order_arg.src, ESP_USING_TMPTABLE);

      if (add_sorting_to_table(sort_tab, &order_arg))
        DBUG_RETURN(true);
      /*
        filesort_limit:	 Return only this many rows from filesort().
        We can use select_limit_cnt only if we have no group_by and 1 table.
        This allows us to use Bounded_queue for queries like:
          "select SQL_CALC_FOUND_ROWS * from t1 order by b desc limit 1;"
        m_select_limit == HA_POS_ERROR (we need a full table scan)
        unit->select_limit_cnt == 1 (we only need one row in the result set)
      */
      sort_tab->filesort->limit=
        (has_group_by || (primary_tables > curr_tmp_table + 1)) ?
         m_select_limit : unit->select_limit_cnt;
    }
    if (!plan_is_const() &&
        !join_tab[const_tables].table->sort.io_cache)
    {
      /*
        If no IO cache exists for the first table then we are using an
        INDEX SCAN and no filesort. Thus we should not remove the sorted
        attribute on the INDEX SCAN.
      */
      skip_sort_order= true;
    }
  }
  fields= curr_fields_list;
  // Reset before execution
  set_items_ref_array(items0);
  if (join_tab)
    join_tab[primary_tables + tmp_tables - 1].next_select=
      setup_end_select_func(this, NULL);
  group= has_group_by;

  DBUG_RETURN(false);
}


/**
  @brief Add Filesort object to the given table to sort if with filesort

  @param tab   the JOIN_TAB object to attach created Filesort object to
  @param order List of expressions to sort the table by

  @note This function moves tab->select, if any, to filesort->select

  @return false on success, true on OOM
*/

bool
JOIN::add_sorting_to_table(JOIN_TAB *tab, ORDER_with_src *order)
{
  explain_flags.set(order->src, ESP_USING_FILESORT);
  tab->filesort= new (thd->mem_root) Filesort(*order, HA_POS_ERROR, tab->select);
  if (!tab->filesort)
    return true;
  /*
    Select was moved to filesort->select to force join_init_read_record to use
    sorted result instead of reading table through select.
  */
  if (tab->select)
  {
    tab->select= NULL;
    tab->set_condition(NULL, __LINE__);
  }
  tab->read_first_record= join_init_read_record;
  return false;
}

/**
  Find a cheaper access key than a given @a key

  @param          tab                 NULL or JOIN_TAB of the accessed table
  @param          order               Linked list of ORDER BY arguments
  @param          table               Table if tab == NULL or tab->table
  @param          usable_keys         Key map to find a cheaper key in
  @param          ref_key             
                * 0 <= key < MAX_KEY   - key number (hint) to start the search
                * -1                   - no key number provided
  @param          select_limit        LIMIT value, or HA_POS_ERROR if no limit
  @param [out]    new_key             Key number if success, otherwise undefined
  @param [out]    new_key_direction   Return -1 (reverse) or +1 if success,
                                      otherwise undefined
  @param [out]    new_select_limit    Return adjusted LIMIT
  @param [out]    new_used_key_parts  NULL by default, otherwise return number
                                      of new_key prefix columns if success
                                      or undefined if the function fails
  @param [out]  saved_best_key_parts  NULL by default, otherwise preserve the
                                      value for further use in QUICK_SELECT_DESC

  @note
    This function takes into account table->quick_condition_rows statistic
    (that is calculated by the make_join_statistics function).
    However, single table procedures such as mysql_update() and mysql_delete()
    never call make_join_statistics, so they have to update it manually
    (@see get_index_for_order()).
*/

static bool
test_if_cheaper_ordering(const JOIN_TAB *tab, ORDER *order, TABLE *table,
                         key_map usable_keys,  int ref_key,
                         ha_rows select_limit,
                         int *new_key, int *new_key_direction,
                         ha_rows *new_select_limit, uint *new_used_key_parts,
                         uint *saved_best_key_parts)
{
  DBUG_ENTER("test_if_cheaper_ordering");
  /*
    Check whether there is an index compatible with the given order
    usage of which is cheaper than usage of the ref_key index (ref_key>=0)
    or a table scan.
    It may be the case if ORDER/GROUP BY is used with LIMIT.
  */
  ha_rows best_select_limit= HA_POS_ERROR;
  JOIN *join= tab ? tab->join : NULL;
  uint nr;
  key_map keys;
  uint best_key_parts= 0;
  int best_key_direction= 0;
  ha_rows best_records= 0;
  double read_time;
  int best_key= -1;
  bool is_best_covering= FALSE;
  double fanout= 1;
  ha_rows table_records= table->file->stats.records;
  bool group= join && join->group && order == join->group_list;
  ha_rows refkey_rows_estimate= table->quick_condition_rows;
  const bool has_limit= (select_limit != HA_POS_ERROR);

  /*
    If not used with LIMIT, only use keys if the whole query can be
    resolved with a key;  This is because filesort() is usually faster than
    retrieving all rows through an index.
  */
  if (select_limit >= table_records)
  {
    keys= *table->file->keys_to_use_for_scanning();
    keys.merge(table->covering_keys);

    /*
      We are adding here also the index specified in FORCE INDEX clause, 
      if any.
      This is to allow users to use index in ORDER BY.
    */
    if (table->force_index) 
      keys.merge(group ? table->keys_in_use_for_group_by :
                         table->keys_in_use_for_order_by);
    keys.intersect(usable_keys);
  }
  else
    keys= usable_keys;

  if (join)
  {
    read_time= tab->position->read_time;
    for (const JOIN_TAB *jt= tab + 1;
         jt < join->join_tab + join->primary_tables; jt++)
      fanout*= jt->position->records_read; // fanout is always >= 1
  }
  else
    read_time= table->file->scan_time();

  /*
    Calculate the selectivity of the ref_key for REF_ACCESS. For
    RANGE_ACCESS we use table->quick_condition_rows.
  */
  if (ref_key >= 0 && tab->type == JT_REF)
  {
    if (table->quick_keys.is_set(ref_key))
      refkey_rows_estimate= table->quick_rows[ref_key];
    else
    {
      const KEY *ref_keyinfo= table->key_info + ref_key;
      refkey_rows_estimate= ref_keyinfo->rec_per_key[tab->ref.key_parts - 1];
    }
    set_if_bigger(refkey_rows_estimate, 1);
  }
  for (nr=0; nr < table->s->keys ; nr++)
  {
    int direction;
    uint used_key_parts;

    if (keys.is_set(nr) &&
        (direction= test_if_order_by_key(order, table, nr, &used_key_parts)))
    {
      /*
        At this point we are sure that ref_key is a non-ordering
        key (where "ordering key" is a key that will return rows
        in the order required by ORDER BY).
      */
      DBUG_ASSERT (ref_key != (int) nr);

      bool is_covering= table->covering_keys.is_set(nr) ||
                        (nr == table->s->primary_key &&
                        table->file->primary_key_is_clustered());
      
      /* 
        Don't use an index scan with ORDER BY without limit.
        For GROUP BY without limit always use index scan
        if there is a suitable index. 
        Why we hold to this asymmetry hardly can be explained
        rationally. It's easy to demonstrate that using
        temporary table + filesort could be cheaper for grouping
        queries too.
      */ 
      if (is_covering ||
          select_limit != HA_POS_ERROR || 
          (ref_key < 0 && (group || table->force_index)))
      { 
        double rec_per_key;
        double index_scan_time;
        KEY *keyinfo= table->key_info+nr;
        if (select_limit == HA_POS_ERROR)
          select_limit= table_records;
        if (group)
        {
          /* 
            Used_key_parts can be larger than keyinfo->key_parts
            when using a secondary index clustered with a primary 
            key (e.g. as in Innodb). 
            See Bug #28591 for details.
          */  
          rec_per_key= used_key_parts &&
                       used_key_parts <= actual_key_parts(keyinfo) ?
                       keyinfo->rec_per_key[used_key_parts-1] : 1;
          set_if_bigger(rec_per_key, 1);
          /*
            With a grouping query each group containing on average
            rec_per_key records produces only one row that will
            be included into the result set.
          */  
          if (select_limit > table_records/rec_per_key)
            select_limit= table_records;
          else
            select_limit= (ha_rows) (select_limit*rec_per_key);
        }
        /* 
          If tab=tk is not the last joined table tn then to get first
          L records from the result set we can expect to retrieve
          only L/fanout(tk,tn) where fanout(tk,tn) says how many
          rows in the record set on average will match each row tk.
          Usually our estimates for fanouts are too pessimistic.
          So the estimate for L/fanout(tk,tn) will be too optimistic
          and as result we'll choose an index scan when using ref/range
          access + filesort will be cheaper.
        */
        select_limit= (ha_rows) (select_limit < fanout ?
                                 1 : select_limit/fanout);
        /*
          We assume that each of the tested indexes is not correlated
          with ref_key. Thus, to select first N records we have to scan
          N/selectivity(ref_key) index entries. 
          selectivity(ref_key) = #scanned_records/#table_records =
          refkey_rows_estimate/table_records.
          In any case we can't select more than #table_records.
          N/(refkey_rows_estimate/table_records) > table_records
          <=> N > refkey_rows_estimate.
         */
        if (select_limit > refkey_rows_estimate)
          select_limit= table_records;
        else
          select_limit= (ha_rows) (select_limit *
                                   (double) table_records /
                                    refkey_rows_estimate);
        rec_per_key= keyinfo->rec_per_key[keyinfo->user_defined_key_parts - 1];
        set_if_bigger(rec_per_key, 1);
        /*
          Here we take into account the fact that rows are
          accessed in sequences rec_per_key records in each.
          Rows in such a sequence are supposed to be ordered
          by rowid/primary key. When reading the data
          in a sequence we'll touch not more pages than the
          table file contains.
          TODO. Use the formula for a disk sweep sequential access
          to calculate the cost of accessing data rows for one 
          index entry.
        */
        index_scan_time= select_limit/rec_per_key *
                         min(rec_per_key, table->file->scan_time());
        if ((ref_key < 0 && is_covering) || 
            (ref_key < 0 && (group || table->force_index)) ||
            index_scan_time < read_time)
        {
          ha_rows quick_records= table_records;
          ha_rows refkey_select_limit= (ref_key >= 0 &&
                                        table->covering_keys.is_set(ref_key)) ?
                                        refkey_rows_estimate :
                                        HA_POS_ERROR;
          if ((is_best_covering && !is_covering) ||
              (is_covering && refkey_select_limit < select_limit))
            continue;
          if (table->quick_keys.is_set(nr))
            quick_records= table->quick_rows[nr];
          if (best_key < 0 ||
              (select_limit <= min(quick_records,best_records) ?
               keyinfo->user_defined_key_parts < best_key_parts :
               quick_records < best_records))
          {
            best_key= nr;
            best_key_parts= keyinfo->user_defined_key_parts;
            if (saved_best_key_parts)
              *saved_best_key_parts= used_key_parts;
            best_records= quick_records;
            is_best_covering= is_covering;
            best_key_direction= direction; 
            best_select_limit= select_limit;
          }
        }   
      }      
    }
  }

  if (best_key < 0 || best_key == ref_key)
    DBUG_RETURN(FALSE);
  
  *new_key= best_key;
  *new_key_direction= best_key_direction;
  *new_select_limit= has_limit ? best_select_limit : table_records;
  if (new_used_key_parts != NULL)
    *new_used_key_parts= best_key_parts;

  DBUG_RETURN(TRUE);
}


/**
  Find a key to apply single table UPDATE/DELETE by a given ORDER

  @param       order           Linked list of ORDER BY arguments
  @param       table           Table to find a key
  @param       select          Pointer to access/update select->quick (if any)
  @param       limit           LIMIT clause parameter 
  @param [out] need_sort       TRUE if filesort needed
  @param [out] reverse
    TRUE if the key is reversed again given ORDER (undefined if key == MAX_KEY)

  @return
    - MAX_KEY if no key found                        (need_sort == TRUE)
    - MAX_KEY if quick select result order is OK     (need_sort == FALSE)
    - key number (either index scan or quick select) (need_sort == FALSE)

  @note
    Side effects:
    - may deallocate or deallocate and replace select->quick;
    - may set table->quick_condition_rows and table->quick_rows[...]
      to table->file->stats.records. 
*/

uint get_index_for_order(ORDER *order, TABLE *table, SQL_SELECT *select,
                         ha_rows limit, bool *need_sort, bool *reverse)
{
  if (select && select->quick && select->quick->unique_key_range())
  { // Single row select (always "ordered"): Ok to use with key field UPDATE
    *need_sort= FALSE;
    /*
      Returning of MAX_KEY here prevents updating of used_key_is_modified
      in mysql_update(). Use quick select "as is".
    */
    return MAX_KEY;
  }

  if (!order)
  {
    *need_sort= FALSE;
    if (select && select->quick)
      return select->quick->index; // index or MAX_KEY, use quick select as is
    else
      return table->file->key_used_on_scan; // MAX_KEY or index for some engines
  }

  if (!is_simple_order(order)) // just to cut further expensive checks
  {
    *need_sort= TRUE;
    return MAX_KEY;
  }

  if (select && select->quick)
  {
    if (select->quick->index == MAX_KEY)
    {
      *need_sort= TRUE;
      return MAX_KEY;
    }

    uint used_key_parts;
    switch (test_if_order_by_key(order, table, select->quick->index,
                                 &used_key_parts)) {
    case 1: // desired order
      *need_sort= FALSE;
      return select->quick->index;
    case 0: // unacceptable order
      *need_sort= TRUE;
      return MAX_KEY;
    case -1: // desired order, but opposite direction
      {
        QUICK_SELECT_I *reverse_quick;
        if ((reverse_quick=
               select->quick->make_reverse(used_key_parts)))
        {
          select->set_quick(reverse_quick);
          *need_sort= FALSE;
          return select->quick->index;
        }
        else
        {
          *need_sort= TRUE;
          return MAX_KEY;
        }
      }
    }
    DBUG_ASSERT(0);
  }
  else if (limit != HA_POS_ERROR)
  { // check if some index scan & LIMIT is more efficient than filesort
    
    /*
      Update quick_condition_rows since single table UPDATE/DELETE procedures
      don't call make_join_statistics() and leave this variable uninitialized.
    */
    table->quick_condition_rows= table->file->stats.records;
    
    int key, direction;
    if (test_if_cheaper_ordering(NULL, order, table,
                                 table->keys_in_use_for_order_by, -1,
                                 limit,
                                 &key, &direction, &limit) &&
        !is_key_used(table, key, table->write_set))
    {
      *need_sort= FALSE;
      *reverse= (direction < 0);
      return key;
    }
  }
  *need_sort= TRUE;
  return MAX_KEY;
}


/**
  Returns number of key parts depending on
  OPTIMIZER_SWITCH_USE_INDEX_EXTENSIONS flag.

  @param  key_info  pointer to KEY structure

  @return number of key parts.
*/

uint actual_key_parts(KEY *key_info)
{
  return key_info->table->in_use->
    optimizer_switch_flag(OPTIMIZER_SWITCH_USE_INDEX_EXTENSIONS) ?
    key_info->actual_key_parts : key_info->user_defined_key_parts;
}


/**
  Returns key flags depending on
  OPTIMIZER_SWITCH_USE_INDEX_EXTENSIONS flag.

  @param  key_info  pointer to KEY structure

  @return key flags.
*/

uint actual_key_flags(KEY *key_info)
{
  return key_info->table->in_use->
    optimizer_switch_flag(OPTIMIZER_SWITCH_USE_INDEX_EXTENSIONS) ?
    key_info->actual_flags : key_info->flags;
}

/**
  @} (end of group Query_Optimizer)
*/
