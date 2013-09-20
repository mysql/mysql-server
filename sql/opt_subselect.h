/*
   Copyright (c) 2010, 2012, Monty Program Ab

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

/*
  Semi-join subquery optimization code definitions
*/

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

int check_and_do_in_subquery_rewrites(JOIN *join);
bool convert_join_subqueries_to_semijoins(JOIN *join);
int pull_out_semijoin_tables(JOIN *join);
bool optimize_semijoin_nests(JOIN *join, table_map all_table_map);
bool setup_jtbm_semi_joins(JOIN *join, List<TABLE_LIST> *join_list,  
                           Item **join_where);
void cleanup_empty_jtbm_semi_joins(JOIN *join);

// used by Loose_scan_opt
ulonglong get_bound_sj_equalities(TABLE_LIST *sj_nest, 
                                  table_map remaining_tables);

/*
  This is a class for considering possible loose index scan optimizations.
  It's usage pattern is as follows:
    best_access_path()
    {
       Loose_scan_opt opt;

       opt.init()
       for each index we can do ref access with
       {
         opt.next_ref_key();
         for each keyuse 
           opt.add_keyuse();
         opt.check_ref_access();
       }

       if (some criteria for range scans)
         opt.check_range_access();
       
       opt.get_best_option();
    }
*/

class Loose_scan_opt
{
  /* All methods must check this before doing anything else */
  bool try_loosescan;

  /*
    If we consider (oe1, .. oeN) IN (SELECT ie1, .. ieN) then ieK=oeK is
    called sj-equality. If oeK depends only on preceding tables then such
    equality is called 'bound'.
  */
  ulonglong bound_sj_equalities;
 
  /* Accumulated properties of ref access we're now considering: */
  ulonglong handled_sj_equalities;
  key_part_map loose_scan_keyparts;
  uint max_loose_keypart;
  bool part1_conds_met;

  /*
    Use of quick select is a special case. Some of its properties:
  */
  uint quick_uses_applicable_index;
  uint quick_max_loose_keypart;
  
  /* Best loose scan method so far */
  uint   best_loose_scan_key;
  double best_loose_scan_cost;
  double best_loose_scan_records;
  KEYUSE *best_loose_scan_start_key;

  uint best_max_loose_keypart;

public:
  Loose_scan_opt():
    try_loosescan(FALSE),
    bound_sj_equalities(0),
    quick_uses_applicable_index(FALSE)
  {
    UNINIT_VAR(quick_max_loose_keypart); /* Protected by quick_uses_applicable_index */
    /* The following are protected by best_loose_scan_cost!= DBL_MAX */
    UNINIT_VAR(best_loose_scan_key);
    UNINIT_VAR(best_loose_scan_records);
    UNINIT_VAR(best_max_loose_keypart);
    UNINIT_VAR(best_loose_scan_start_key);
  }
  
  void init(JOIN *join, JOIN_TAB *s, table_map remaining_tables)
  {
    /*
      Discover the bound equalities. We need to do this if
        1. The next table is an SJ-inner table, and
        2. It is the first table from that semijoin, and
        3. We're not within a semi-join range (i.e. all semi-joins either have
           all or none of their tables in join_table_map), except
           s->emb_sj_nest (which we've just entered, see #2).
        4. All non-IN-equality correlation references from this sj-nest are 
           bound
        5. But some of the IN-equalities aren't (so this can't be handled by 
           FirstMatch strategy)
    */
    best_loose_scan_cost= DBL_MAX;
    if (!join->emb_sjm_nest && s->emb_sj_nest &&                        // (1)
        s->emb_sj_nest->sj_in_exprs < 64 && 
        ((remaining_tables & s->emb_sj_nest->sj_inner_tables) ==        // (2)
         s->emb_sj_nest->sj_inner_tables) &&                            // (2)
        join->cur_sj_inner_tables == 0 &&                                  // (3)
        !(remaining_tables & 
          s->emb_sj_nest->nested_join->sj_corr_tables) &&               // (4)
        remaining_tables & s->emb_sj_nest->nested_join->sj_depends_on &&// (5)
        optimizer_flag(join->thd, OPTIMIZER_SWITCH_LOOSE_SCAN))
    {
      /* This table is an LooseScan scan candidate */
      bound_sj_equalities= get_bound_sj_equalities(s->emb_sj_nest, 
                                                   remaining_tables);
      try_loosescan= TRUE;
      DBUG_PRINT("info", ("Will try LooseScan scan, bound_map=%llx",
                          (longlong)bound_sj_equalities));
    }
  }

  void next_ref_key()
  {
    handled_sj_equalities=0;
    loose_scan_keyparts= 0;
    max_loose_keypart= 0;
    part1_conds_met= FALSE;
  }
  
  void add_keyuse(table_map remaining_tables, KEYUSE *keyuse)
  {
    if (try_loosescan && keyuse->sj_pred_no != UINT_MAX &&
        (keyuse->table->file->index_flags(keyuse->key, 0, 1 ) & HA_READ_ORDER))

    {
      if (!(remaining_tables & keyuse->used_tables))
      {
        /* 
          This allows to use equality propagation to infer that some 
          sj-equalities are bound.
        */
        bound_sj_equalities |= 1ULL << keyuse->sj_pred_no;
      }
      else
      {
        handled_sj_equalities |= 1ULL << keyuse->sj_pred_no;
        loose_scan_keyparts |= ((key_part_map)1) << keyuse->keypart;
        set_if_bigger(max_loose_keypart, keyuse->keypart);
      }
    }
  }

  bool have_a_case() { return test(handled_sj_equalities); }

  void check_ref_access_part1(JOIN_TAB *s, uint key, KEYUSE *start_key, 
                              table_map found_part)
  {
    /*
      Check if we can use LooseScan semi-join strategy. We can if
      1. This is the right table at right location
      2. All IN-equalities are either
         - "bound", ie. the outer_expr part refers to the preceding tables
         - "handled", ie. covered by the index we're considering
      3. Index order allows to enumerate subquery's duplicate groups in
         order. This happens when the index definition matches this
         pattern:

           (handled_col|bound_col)* (other_col|bound_col)

    */
    if (try_loosescan &&                                       // (1)
        (handled_sj_equalities | bound_sj_equalities) ==       // (2)
        PREV_BITS(ulonglong, s->emb_sj_nest->sj_in_exprs) &&   // (2)
        (PREV_BITS(key_part_map, max_loose_keypart+1) &        // (3)
         (found_part | loose_scan_keyparts)) ==                // (3)
        PREV_BITS(key_part_map, max_loose_keypart+1) &&        // (3)
        !key_uses_partial_cols(s->table->s, key))
    {
      /* Ok, can use the strategy */
      part1_conds_met= TRUE;
      if (s->quick && s->quick->index == key && 
          s->quick->get_type() == QUICK_SELECT_I::QS_TYPE_RANGE)
      {
        quick_uses_applicable_index= TRUE;
        quick_max_loose_keypart= max_loose_keypart;
      }
      DBUG_PRINT("info", ("Can use LooseScan scan"));

      /* 
        Check if this is a special case where there are no usable bound
        IN-equalities, i.e. we have

          outer_expr IN (SELECT innertbl.key FROM ...) 
        
        and outer_expr cannot be evaluated yet, so it's actually full
        index scan and not a ref access
      */
      if (!(found_part & 1 ) && /* no usable ref access for 1st key part */
          s->table->covering_keys.is_set(key))
      {
        DBUG_PRINT("info", ("Can use full index scan for LooseScan"));
        
        /* Calculate the cost of complete loose index scan.  */
        double records= rows2double(s->table->file->stats.records);

        /* The cost is entire index scan cost (divided by 2) */
        double read_time= s->table->file->keyread_time(key, 1,
                                                       (ha_rows) records);

        /*
          Now find out how many different keys we will get (for now we
          ignore the fact that we have "keypart_i=const" restriction for
          some key components, that may make us think think that loose
          scan will produce more distinct records than it actually will)
        */
        ulong rpc;
        if ((rpc= s->table->key_info[key].rec_per_key[max_loose_keypart]))
          records= records / rpc;

        // TODO: previous version also did /2
        if (read_time < best_loose_scan_cost)
        {
          best_loose_scan_key= key;
          best_loose_scan_cost= read_time;
          best_loose_scan_records= records;
          best_max_loose_keypart= max_loose_keypart;
          best_loose_scan_start_key= start_key;
        }
      }
    }
  }
  
  void check_ref_access_part2(uint key, KEYUSE *start_key, double records, 
                              double read_time)
  {
    if (part1_conds_met && read_time < best_loose_scan_cost)
    {
      /* TODO use rec-per-key-based fanout calculations */
      best_loose_scan_key= key;
      best_loose_scan_cost= read_time;
      best_loose_scan_records= records;
      best_max_loose_keypart= max_loose_keypart;
      best_loose_scan_start_key= start_key;
    }
  }

  void check_range_access(JOIN *join, uint idx, QUICK_SELECT_I *quick)
  {
    /* TODO: this the right part restriction: */
    if (quick_uses_applicable_index && idx == join->const_tables && 
        quick->read_time < best_loose_scan_cost)
    {
      best_loose_scan_key= quick->index;
      best_loose_scan_cost= quick->read_time;
      /* this is ok because idx == join->const_tables */
      best_loose_scan_records= rows2double(quick->records);
      best_max_loose_keypart= quick_max_loose_keypart;
      best_loose_scan_start_key= NULL;
    }
  }

  void save_to_position(JOIN_TAB *tab, POSITION *pos)
  {
    pos->read_time=       best_loose_scan_cost;
    if (best_loose_scan_cost != DBL_MAX)
    {
      pos->records_read=    best_loose_scan_records;
      pos->key=             best_loose_scan_start_key;
      pos->loosescan_picker.loosescan_key=   best_loose_scan_key;
      pos->loosescan_picker.loosescan_parts= best_max_loose_keypart + 1;
      pos->use_join_buffer= FALSE;
      pos->table=           tab;
      // todo need ref_depend_map ?
      DBUG_PRINT("info", ("Produced a LooseScan plan, key %s, %s",
                          tab->table->key_info[best_loose_scan_key].name,
                          best_loose_scan_start_key? "(ref access)":
                                                     "(range/index access)"));
    }
  }
};


extern void advance_sj_state(JOIN *join, table_map remaining_tables, uint idx,
                      double *current_record_count, double *current_read_time,
                      POSITION *loose_scan_pos);
void restore_prev_sj_state(const table_map remaining_tables, 
                                  const JOIN_TAB *tab, uint idx);

void fix_semijoin_strategies_for_picked_join_order(JOIN *join);

bool setup_sj_materialization_part1(JOIN_TAB *sjm_tab);
bool setup_sj_materialization_part2(JOIN_TAB *sjm_tab);


/*
  Temporary table used by semi-join DuplicateElimination strategy

  This consists of the temptable itself and data needed to put records
  into it. The table's DDL is as follows:

    CREATE TABLE tmptable (col VARCHAR(n) BINARY, PRIMARY KEY(col));

  where the primary key can be replaced with unique constraint if n exceeds
  the limit (as it is always done for query execution-time temptables).

  The record value is a concatenation of rowids of tables from the join we're
  executing. If a join table is on the inner side of the outer join, we
  assume that its rowid can be NULL and provide means to store this rowid in
  the tuple.
*/

class SJ_TMP_TABLE : public Sql_alloc
{
public:
  /*
    Array of pointers to tables whose rowids compose the temporary table
    record.
  */
  class TAB
  {
  public:
    JOIN_TAB *join_tab;
    uint rowid_offset;
    ushort null_byte;
    uchar null_bit;
  };
  TAB *tabs;
  TAB *tabs_end;
  
  /* 
    is_degenerate==TRUE means this is a special case where the temptable record
    has zero length (and presence of a unique key means that the temptable can
    have either 0 or 1 records). 
    In this case we don't create the physical temptable but instead record
    its state in SJ_TMP_TABLE::have_degenerate_row.
  */
  bool is_degenerate;

  /* 
    When is_degenerate==TRUE: the contents of the table (whether it has the
    record or not).
  */
  bool have_degenerate_row;
  
  /* table record parameters */
  uint null_bits;
  uint null_bytes;
  uint rowid_len;

  /* The temporary table itself (NULL means not created yet) */
  TABLE *tmp_table;
  
  /*
    These are the members we got from temptable creation code. We'll need
    them if we'll need to convert table from HEAP to MyISAM/Maria.
  */
  ENGINE_COLUMNDEF *start_recinfo;
  ENGINE_COLUMNDEF *recinfo;

  SJ_TMP_TABLE *next_flush_table; 

  int sj_weedout_delete_rows();
  int sj_weedout_check_row(THD *thd);
  bool create_sj_weedout_tmp_table(THD *thd);
};

int setup_semijoin_dups_elimination(JOIN *join, ulonglong options, 
                                    uint no_jbuf_after);
void destroy_sj_tmp_tables(JOIN *join);
int clear_sj_tmp_tables(JOIN *join);
int rewrite_to_index_subquery_engine(JOIN *join);


void get_delayed_table_estimates(TABLE *table,
                                 ha_rows *out_rows, 
                                 double *scan_time,
                                 double *startup_cost);

enum_nested_loop_state join_tab_execution_startup(JOIN_TAB *tab);

