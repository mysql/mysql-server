/* Copyright (C) 2007-2009 Arjen G Lentz & Antony T Curtis for Open Query
   Portions of this file copyright (C) 2000-2006 MySQL AB

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

/* ======================================================================
   Open Query Graph Computation Engine, based on a concept by Arjen Lentz
   Mk.II implementation by Antony Curtis & Arjen Lentz
   For more information, documentation, support, enhancement engineering,
   and non-GPL licensing, see http://openquery.com/graph
   or contact graph@openquery.com
   For packaged binaries, see http://ourdelta.org
   ======================================================================
*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#define MYSQL_SERVER	// to have THD
#include "mysql_priv.h"
#if MYSQL_VERSION_ID >= 50100
#include <mysql/plugin.h>
#endif

#ifdef HAVE_OQGRAPH

#include "ha_oqgraph.h"
#include "graphcore.h"

#define OQGRAPH_STATS_UPDATE_THRESHOLD 10

using namespace open_query;


struct oqgraph_info_st
{
  THR_LOCK lock;
  oqgraph_share *graph;
  uint use_count;
  uint key_stat_version;
  uint records;
  bool dropped;
  char name[FN_REFLEN+1];
};

static const char oqgraph_description[]=
  "Open Query Graph Computation Engine, stored in memory "
  "(http://openquery.com/graph)";

#if MYSQL_VERSION_ID < 50100
static bool oqgraph_init();

handlerton oqgraph_hton= {
  "OQGRAPH",
  SHOW_OPTION_YES,
  oqgraph_description,
  DB_TYPE_OQGRAPH,
  oqgraph_init,
  0,       /* slot */
  0,       /* savepoint size. */
  NULL,    /* close_connection */
  NULL,    /* savepoint */
  NULL,    /* rollback to savepoint */
  NULL,    /* release savepoint */
  NULL,    /* commit */
  NULL,    /* rollback */
  NULL,    /* prepare */
  NULL,    /* recover */
  NULL,    /* commit_by_xid */
  NULL,    /* rollback_by_xid */
  NULL,    /* create_cursor_read_view */
  NULL,    /* set_cursor_read_view */
  NULL,    /* close_cursor_read_view */
  HTON_NO_FLAGS
};

#define STATISTIC_INCREMENT(X) \
statistic_increment(table->in_use->status_var.X, &LOCK_status)
#define MOVE(X) move_field(X)
#define RECORDS records
#else
#define STATISTIC_INCREMENT(X) ha_statistic_increment(&SSV::X)
#define MOVE(X) move_field_offset(X)
#define RECORDS stats.records
#endif

static HASH oqgraph_open_tables;
static pthread_mutex_t LOCK_oqgraph;
static bool oqgraph_init_done= 0;

#if MYSQL_VERSION_ID >= 50130
#define HASH_KEY_LENGTH size_t
#else
#define HASH_KEY_LENGTH uint
#endif

static uchar* get_key(const uchar *ptr, HASH_KEY_LENGTH *length,
                      my_bool)
{
  const OQGRAPH_INFO *share= (const OQGRAPH_INFO*) ptr;
  *length= strlen(share->name);
  return (uchar*) share->name;
}

#if MYSQL_VERSION_ID >= 50100
static handler* oqgraph_create_handler(handlerton *hton, TABLE_SHARE *table,
                                       MEM_ROOT *mem_root)
{
  return new (mem_root) ha_oqgraph(hton, table);
}

static int oqgraph_init(handlerton *hton)
{
#else
static bool oqgraph_init()
{
  if (have_oqgraph == SHOW_OPTION_DISABLED)
    return 1;
#endif
  if (pthread_mutex_init(&LOCK_oqgraph, MY_MUTEX_INIT_FAST))
    goto error;
  if (hash_init(&oqgraph_open_tables, &my_charset_bin, 32, 0, 0,
                get_key, 0, 0))
  {
    pthread_mutex_destroy(&LOCK_oqgraph);
    goto error;
  }
#if MYSQL_VERSION_ID >= 50100
  hton->state= SHOW_OPTION_YES;
  hton->db_type= DB_TYPE_AUTOASSIGN;
  hton->create= oqgraph_create_handler;
  hton->flags= HTON_NO_FLAGS;
#endif
  oqgraph_init_done= TRUE;
  return 0;
error:
#if MYSQL_VERSION_ID < 50100
  have_oqgraph= SHOW_OPTION_DISABLED;
#endif
  return 1;
}

#if MYSQL_VERSION_ID >= 50100
static int oqgraph_fini(void *)
{
  hash_free(&oqgraph_open_tables);
  pthread_mutex_destroy(&LOCK_oqgraph);
  oqgraph_init_done= FALSE;
  return 0;
}
#endif

static OQGRAPH_INFO *get_share(const char *name, TABLE *table=0)
{
  OQGRAPH_INFO *share;
  uint length= strlen(name);

  safe_mutex_assert_owner(&LOCK_oqgraph);
  if (!(share= (OQGRAPH_INFO*) hash_search(&oqgraph_open_tables,
                                           (byte*) name, length)))
  {
    if (!table ||
        !(share= new OQGRAPH_INFO))
      return 0;
    share->use_count= share->key_stat_version= share->records= 0;
    share->dropped= 0;
    strmov(share->name, name);
    if (!(share->graph= oqgraph::create()))
    {
      delete share;
      return 0;
    }
    if (my_hash_insert(&oqgraph_open_tables, (byte*) share))
    {
      oqgraph::free(share->graph);
      delete share;
      return 0;
    }
    thr_lock_init(&share->lock);
  }
  share->use_count++;
  return share;
}

static int free_share(OQGRAPH_INFO *share, bool drop=0)
{
  safe_mutex_assert_owner(&LOCK_oqgraph);
  if (!share)
    return 0;
  if (drop)
  {
    share->dropped= true;
    hash_delete(&oqgraph_open_tables, (byte*) share);
  }
  if (!--share->use_count)
  {
    if (share->dropped)
    {
      thr_lock_delete(&share->lock);
      oqgraph::free(share->graph);
      delete share;
    }
  }
  return 0;
}

static int error_code(int res)
{
  switch (res)
  {
  case oqgraph::OK:
    return 0;
  case oqgraph::NO_MORE_DATA:
    return HA_ERR_END_OF_FILE;
  case oqgraph::EDGE_NOT_FOUND:
    return HA_ERR_KEY_NOT_FOUND;
  case oqgraph::INVALID_WEIGHT:
    return HA_ERR_AUTOINC_ERANGE;
  case oqgraph::DUPLICATE_EDGE:
    return HA_ERR_FOUND_DUPP_KEY;
  case oqgraph::CANNOT_ADD_VERTEX:
  case oqgraph::CANNOT_ADD_EDGE:
    return HA_ERR_RECORD_FILE_FULL;
  case oqgraph::MISC_FAIL:
  default:
    return HA_ERR_CRASHED_ON_USAGE;
  }
}

/**
 * Check if table complies with our designated structure
 *
 *    ColName    Type      Attributes
 *    =======    ========  =============
 *    latch     SMALLINT  UNSIGNED NULL
 *    origid    BIGINT    UNSIGNED NULL
 *    destid    BIGINT    UNSIGNED NULL
 *    weight    DOUBLE    NULL
 *    seq       BIGINT    UNSIGNED NULL
 *    linkid    BIGINT    UNSIGNED NULL
 *    =================================
 *
  CREATE TABLE foo (
    latch   SMALLINT  UNSIGNED NULL,
    origid  BIGINT    UNSIGNED NULL,
    destid  BIGINT    UNSIGNED NULL,
    weight  DOUBLE    NULL,
    seq     BIGINT    UNSIGNED NULL,
    linkid  BIGINT    UNSIGNED NULL,
    KEY (latch, origid, destid) USING HASH,
    KEY (latch, destid, origid) USING HASH
  ) ENGINE=OQGRAPH

 */
static int oqgraph_check_table_structure (TABLE *table_arg)
{
  int i;
  struct { const char *colname; int coltype; } skel[] = {
    { "latch" , MYSQL_TYPE_SHORT },
    { "origid", MYSQL_TYPE_LONGLONG },
    { "destid", MYSQL_TYPE_LONGLONG },
    { "weight", MYSQL_TYPE_DOUBLE },
    { "seq"   , MYSQL_TYPE_LONGLONG },
    { "linkid", MYSQL_TYPE_LONGLONG },
  { NULL    , 0}
  };

  DBUG_ENTER("ha_oqgraph::table_structure_ok");

  Field **field= table_arg->field;
  for (i= 0; *field && skel[i].colname; i++, field++) {
    /* Check Column Type */
    if ((*field)->type() != skel[i].coltype)
      DBUG_RETURN(-1);
    if (skel[i].coltype != MYSQL_TYPE_DOUBLE) {
      /* Check Is UNSIGNED */
      if (!((*field)->flags & UNSIGNED_FLAG ))
        DBUG_RETURN(-1);
    }
    /* Check THAT  NOT NULL isn't set */
    if ((*field)->flags & NOT_NULL_FLAG)
      DBUG_RETURN(-1);
    /* Check the column name */
    if (strcmp(skel[i].colname,(*field)->field_name))
      DBUG_RETURN(-1);
  }

  if (skel[i].colname || *field || !table_arg->key_info || !table_arg->s->keys)
    DBUG_RETURN(-1);

  KEY *key= table_arg->key_info;
  for (uint i= 0; i < table_arg->s->keys; ++i, ++key)
  {
    Field **field= table_arg->field;
    /* check that the first key part is the latch and it is a hash key */
    if (!(field[0] == key->key_part[0].field &&
          HA_KEY_ALG_HASH == key->algorithm))
      DBUG_RETURN(-1);
    if (key->key_parts == 3)
    {
      /* KEY (latch, origid, destid) USING HASH */
      /* KEY (latch, destid, origid) USING HASH */
      if (!(field[1] == key->key_part[1].field &&
            field[2] == key->key_part[2].field) &&
          !(field[1] == key->key_part[2].field &&
            field[2] == key->key_part[1].field))
        DBUG_RETURN(-1);
    }
    else
      DBUG_RETURN(-1);
  }

  DBUG_RETURN(0);
}

/*****************************************************************************
** OQGRAPH tables
*****************************************************************************/

#if MYSQL_VERSION_ID >= 50100
ha_oqgraph::ha_oqgraph(handlerton *hton, TABLE_SHARE *table_arg)
  : handler(hton, table_arg),
#else
ha_oqgraph::ha_oqgraph(TABLE *table_arg)
  : handler(&oqgraph_hton, table_arg),
#endif
    share(0), graph(0), records_changed(0), key_stat_version(0)
{ }


static const char *ha_oqgraph_exts[] =
{
  NullS
};

const char **ha_oqgraph::bas_ext() const
{
  return ha_oqgraph_exts;
}

#if MYSQL_VERSION_ID >= 50100
ulonglong ha_oqgraph::table_flags() const
#else
ulong ha_oqgraph::table_flags() const
#endif
{
  return (HA_NO_BLOBS | HA_NULL_IN_KEY |
          HA_REC_NOT_IN_SEQ | HA_CAN_INSERT_DELAYED |
          HA_BINLOG_STMT_CAPABLE | HA_BINLOG_ROW_CAPABLE);
}

ulong ha_oqgraph::index_flags(uint inx, uint part, bool all_parts) const
{
  return HA_ONLY_WHOLE_INDEX | HA_KEY_SCAN_NOT_ROR;
}

int ha_oqgraph::open(const char *name, int mode, uint test_if_locked)
{
  pthread_mutex_lock(&LOCK_oqgraph);
  if ((share = get_share(name, table)))
  {
    ref_length= oqgraph::sizeof_ref;
  }

  if (share)
  {
    /* Initialize variables for the opened table */
    thr_lock_data_init(&share->lock, &lock, NULL);

    graph= oqgraph::create(share->graph);

    /*
      We cannot run update_key_stats() here because we do not have a
      lock on the table. The 'records' count might just be changed
      temporarily at this moment and we might get wrong statistics (Bug
      #10178). Instead we request for update. This will be done in
      ha_oqgraph::info(), which is always called before key statistics are
      used.
    */
    key_stat_version= share->key_stat_version-1;
  }
  pthread_mutex_unlock(&LOCK_oqgraph);

  return (share ? 0 : 1);
}

int ha_oqgraph::close(void)
{
  pthread_mutex_lock(&LOCK_oqgraph);
  oqgraph::free(graph); graph= 0;
  int res= free_share(share);
  pthread_mutex_unlock(&LOCK_oqgraph);
  return error_code(res);
}

void ha_oqgraph::update_key_stats()
{
  for (uint i= 0; i < table->s->keys; i++)
  {
    KEY *key=table->key_info+i;
    if (!key->rec_per_key)
      continue;
    if (key->algorithm != HA_KEY_ALG_BTREE)
    {
      if (key->flags & HA_NOSAME)
        key->rec_per_key[key->key_parts-1]= 1;
      else
      {
        unsigned vertices= graph->vertices_count();
        unsigned edges= graph->edges_count();
        uint no_records= vertices ? 2 * (edges + vertices) / vertices : 2;
        if (no_records < 2)
          no_records= 2;
        key->rec_per_key[key->key_parts-1]= no_records;
      }
    }
  }
  records_changed= 0;
  /* At the end of update_key_stats() we can proudly claim they are OK. */
  key_stat_version= share->key_stat_version;
}


int ha_oqgraph::write_row(byte * buf)
{
  int res= oqgraph::MISC_FAIL;
  Field ** const field= table->field;
  STATISTIC_INCREMENT(ha_write_count);

#if MYSQL_VERSION_ID >= 50100
  my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->read_set);
#endif
  my_ptrdiff_t ptrdiff= buf - table->record[0];

  if (ptrdiff)
  {
    field[1]->MOVE(ptrdiff);
    field[2]->MOVE(ptrdiff);
    field[3]->MOVE(ptrdiff);
  }

  if (!field[1]->is_null() && !field[2]->is_null())
  {
    VertexID orig_id= (VertexID) field[1]->val_int();
    VertexID dest_id= (VertexID) field[2]->val_int();
    EdgeWeight weight= 1;

    if (!field[3]->is_null())
      weight= (EdgeWeight) field[3]->val_real();

    if (!(res= graph->insert_edge(orig_id, dest_id, weight, replace_dups)))
    {
      ++records_changed;
      share->records++;
    }
    if (res == oqgraph::DUPLICATE_EDGE && ignore_dups && !insert_dups)
      res= oqgraph::OK;
  }

  if (ptrdiff)
  {
    field[1]->MOVE(-ptrdiff);
    field[2]->MOVE(-ptrdiff);
    field[3]->MOVE(-ptrdiff);
  }
#if MYSQL_VERSION_ID >= 50100
  dbug_tmp_restore_column_map(table->read_set, old_map);
#endif

  if (!res && records_changed*OQGRAPH_STATS_UPDATE_THRESHOLD > share->records)
  {
    /*
       We can perform this safely since only one writer at the time is
       allowed on the table.
    */
    share->key_stat_version++;
  }

  return error_code(res);
}

int ha_oqgraph::update_row(const byte * old, byte * buf)
{
  int res= oqgraph::MISC_FAIL;
  VertexID orig_id, dest_id;
  EdgeWeight weight= 1;
  Field **field= table->field;
  STATISTIC_INCREMENT(ha_update_count);

#if MYSQL_VERSION_ID >= 50100
  my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->read_set);
#endif
  my_ptrdiff_t ptrdiff= buf - table->record[0];

  if (ptrdiff)
  {
    field[0]->MOVE(ptrdiff);
    field[1]->MOVE(ptrdiff);
    field[2]->MOVE(ptrdiff);
    field[3]->MOVE(ptrdiff);
  }

  if (inited == INDEX || inited == RND)
  {
    VertexID *origp= 0, *destp= 0;
    EdgeWeight *weightp= 0;
    if (!field[1]->is_null())
      *(origp= &orig_id)= (VertexID) field[1]->val_int();
    if (!field[2]->is_null())
      *(destp= &dest_id)= (VertexID) field[2]->val_int();
    if (!field[3]->is_null())
      *(weightp= &weight)= (EdgeWeight) field[3]->val_real();

    my_ptrdiff_t ptrdiff2= old - buf;

    field[0]->MOVE(ptrdiff2);
    field[1]->MOVE(ptrdiff2);
    field[2]->MOVE(ptrdiff2);
    field[3]->MOVE(ptrdiff2);

    if (field[0]->is_null())
    {
      if (!origp == field[1]->is_null() &&
          *origp == (VertexID) field[1]->val_int())
        origp= 0;
      if (!destp == field[2]->is_null() &&
          *destp == (VertexID) field[2]->val_int())
        origp= 0;
      if (!weightp == field[3]->is_null() &&
          *weightp == (VertexID) field[3]->val_real())
        weightp= 0;

      if (!(res= graph->modify_edge(oqgraph::current_row(),
                                    origp, destp, weightp, replace_dups)))
        ++records_changed;
      else if (ignore_dups && res == oqgraph::DUPLICATE_EDGE)
        res= oqgraph::OK;
    }

    field[0]->MOVE(-ptrdiff2);
    field[1]->MOVE(-ptrdiff2);
    field[2]->MOVE(-ptrdiff2);
    field[3]->MOVE(-ptrdiff2);
  }

  if (ptrdiff)
  {
    field[0]->MOVE(-ptrdiff);
    field[1]->MOVE(-ptrdiff);
    field[2]->MOVE(-ptrdiff);
    field[3]->MOVE(-ptrdiff);
  }
#if MYSQL_VERSION_ID >= 50100
  dbug_tmp_restore_column_map(table->read_set, old_map);
#endif

  if (!res && records_changed*OQGRAPH_STATS_UPDATE_THRESHOLD > share->records)
  {
    /*
       We can perform this safely since only one writer at the time is
       allowed on the table.
    */
    share->key_stat_version++;
  }
  return error_code(res);
}

int ha_oqgraph::delete_row(const byte * buf)
{
  int res= oqgraph::EDGE_NOT_FOUND;
  Field **field= table->field;
  STATISTIC_INCREMENT(ha_delete_count);

  if (inited == INDEX || inited == RND)
  {
    if ((res= graph->delete_edge(oqgraph::current_row())) == oqgraph::OK)
    {
      ++records_changed;
      share->records--;
    }
  }
  if (res != oqgraph::OK)
  {
#if MYSQL_VERSION_ID >= 50100
    my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->read_set);
#endif
    my_ptrdiff_t ptrdiff= buf - table->record[0];

    if (ptrdiff)
    {
      field[0]->MOVE(ptrdiff);
      field[1]->MOVE(ptrdiff);
      field[2]->MOVE(ptrdiff);
    }

    if (field[0]->is_null() && !field[1]->is_null() && !field[2]->is_null())
    {
      VertexID orig_id= (VertexID) field[1]->val_int();
      VertexID dest_id= (VertexID) field[2]->val_int();

      if ((res= graph->delete_edge(orig_id, dest_id)) == oqgraph::OK)
      {
        ++records_changed;
        share->records--;
      }
    }

    if (ptrdiff)
    {
      field[0]->MOVE(-ptrdiff);
      field[1]->MOVE(-ptrdiff);
      field[2]->MOVE(-ptrdiff);
    }
#if MYSQL_VERSION_ID >= 50100
    dbug_tmp_restore_column_map(table->read_set, old_map);
#endif
  }

  if (!res && table->s->tmp_table == NO_TMP_TABLE &&
      records_changed*OQGRAPH_STATS_UPDATE_THRESHOLD > share->records)
  {
    /*
       We can perform this safely since only one writer at the time is
       allowed on the table.
    */
    share->key_stat_version++;
  }
  return error_code(res);
}

int ha_oqgraph::index_read(byte * buf, const byte * key, uint key_len,
			enum ha_rkey_function find_flag)
{
  DBUG_ASSERT(inited==INDEX);
  return index_read_idx(buf, active_index, key, key_len, find_flag);
}

int ha_oqgraph::index_next_same(byte *buf, const byte *key, uint key_len)
{
  int res;
  open_query::row row;
  DBUG_ASSERT(inited==INDEX);
  STATISTIC_INCREMENT(ha_read_key_count);
  if (!(res= graph->fetch_row(row)))
    res= fill_record(buf, row);
  table->status= res ? STATUS_NOT_FOUND : 0;
  return error_code(res);
}

int ha_oqgraph::index_read_idx(byte * buf, uint index, const byte * key,
			    uint key_len, enum ha_rkey_function find_flag)
{
  Field **field= table->field;
  KEY *key_info= table->key_info + index;
  int res;
  VertexID orig_id, dest_id;
  int latch;
  VertexID *orig_idp=0, *dest_idp=0;
  int *latchp=0;
  open_query::row row;
  STATISTIC_INCREMENT(ha_read_key_count);

  bmove_align(buf, table->s->default_values, table->s->reclength);
  key_restore(buf, (byte*) key, key_info, key_len);

#if MYSQL_VERSION_ID >= 50100
  my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->read_set);
#endif
  my_ptrdiff_t ptrdiff= buf - table->record[0];

  if (ptrdiff)
  {
    field[0]->MOVE(ptrdiff);
    field[1]->MOVE(ptrdiff);
    field[2]->MOVE(ptrdiff);
  }

  if (!field[0]->is_null())
  {
    latch= (int) field[0]->val_int();
    latchp= &latch;
  }

  if (!field[1]->is_null())
  {
    orig_id= (VertexID) field[1]->val_int();
    orig_idp= &orig_id;
  }

  if (!field[2]->is_null())
  {
    dest_id= (VertexID) field[2]->val_int();
    dest_idp= &dest_id;
  }

  if (ptrdiff)
  {
    field[0]->MOVE(-ptrdiff);
    field[1]->MOVE(-ptrdiff);
    field[2]->MOVE(-ptrdiff);
  }
#if MYSQL_VERSION_ID >= 50100
  dbug_tmp_restore_column_map(table->read_set, old_map);
#endif

  res= graph->search(latchp, orig_idp, dest_idp);

  if (!res && !(res= graph->fetch_row(row)))
    res= fill_record(buf, row);
  table->status = res ? STATUS_NOT_FOUND : 0;
  return error_code(res);
}

int ha_oqgraph::fill_record(byte *record, const open_query::row &row)
{
  Field **field= table->field;

  bmove_align(record, table->s->default_values, table->s->reclength);

#if MYSQL_VERSION_ID >= 50100
  my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->write_set);
#endif
  my_ptrdiff_t ptrdiff= record - table->record[0];

  if (ptrdiff)
  {
    field[0]->MOVE(ptrdiff);
    field[1]->MOVE(ptrdiff);
    field[2]->MOVE(ptrdiff);
    field[3]->MOVE(ptrdiff);
    field[4]->MOVE(ptrdiff);
    field[5]->MOVE(ptrdiff);
  }

  // just each field specifically, no sense iterating
  if (row.latch_indicator)
  {
    field[0]->set_notnull();
    field[0]->store((longlong) row.latch, 0);
  }

  if (row.orig_indicator)
  {
    field[1]->set_notnull();
    field[1]->store((longlong) row.orig, 0);
  }

  if (row.dest_indicator)
  {
    field[2]->set_notnull();
    field[2]->store((longlong) row.dest, 0);
  }

  if (row.weight_indicator)
  {
    field[3]->set_notnull();
    field[3]->store((double) row.weight);
  }

  if (row.seq_indicator)
  {
    field[4]->set_notnull();
    field[4]->store((longlong) row.seq, 0);
  }

  if (row.link_indicator)
  {
    field[5]->set_notnull();
    field[5]->store((longlong) row.link, 0);
  }

  if (ptrdiff)
  {
    field[0]->MOVE(-ptrdiff);
    field[1]->MOVE(-ptrdiff);
    field[2]->MOVE(-ptrdiff);
    field[3]->MOVE(-ptrdiff);
    field[4]->MOVE(-ptrdiff);
    field[5]->MOVE(-ptrdiff);
  }
#if MYSQL_VERSION_ID >= 50100
  dbug_tmp_restore_column_map(table->write_set, old_map);
#endif

  return 0;
}

int ha_oqgraph::rnd_init(bool scan)
{
  return error_code(graph->random(scan));
}

int ha_oqgraph::rnd_next(byte *buf)
{
  int res;
  open_query::row row;
  STATISTIC_INCREMENT(ha_read_rnd_next_count);
  if (!(res= graph->fetch_row(row)))
    res= fill_record(buf, row);
  table->status= res ? STATUS_NOT_FOUND: 0;
  return error_code(res);
}

int ha_oqgraph::rnd_pos(byte * buf, byte *pos)
{
  int res;
  open_query::row row;
  STATISTIC_INCREMENT(ha_read_rnd_count);
  if (!(res= graph->fetch_row(row, pos)))
    res= fill_record(buf, row);
  table->status=res ? STATUS_NOT_FOUND: 0;
  return error_code(res);
}

void ha_oqgraph::position(const byte *record)
{
  graph->row_ref((void*) ref);	// Ref is aligned
}

int ha_oqgraph::cmp_ref(const byte *ref1, const byte *ref2)
{
  return memcmp(ref1, ref2, oqgraph::sizeof_ref);
}

int ha_oqgraph::info(uint flag)
{
  RECORDS= graph->vertices_count() + graph->edges_count();
#if 0
  records= hp_info.records;
  deleted= hp_info.deleted;
  errkey=  hp_info.errkey;
  mean_rec_length= hp_info.reclength;
  data_file_length= hp_info.data_length;
  index_file_length= hp_info.index_length;
  max_data_file_length= hp_info.max_records* hp_info.reclength;
  delete_length= hp_info.deleted * hp_info.reclength;
#endif
  /*
    If info() is called for the first time after open(), we will still
    have to update the key statistics. Hoping that a table lock is now
    in place.
  */
  if (key_stat_version != share->key_stat_version)
    update_key_stats();
  return 0;
}

int ha_oqgraph::extra(enum ha_extra_function operation)
{
  switch (operation)
  {
  case HA_EXTRA_IGNORE_DUP_KEY:
    ignore_dups= true;
    break;
  case HA_EXTRA_NO_IGNORE_DUP_KEY:
    ignore_dups= false;
    insert_dups= false;
    break;
  case HA_EXTRA_WRITE_CAN_REPLACE:
    replace_dups= true;
    break;
  case HA_EXTRA_WRITE_CANNOT_REPLACE:
    replace_dups= false;
    break;
  case HA_EXTRA_INSERT_WITH_UPDATE:
    insert_dups= true;
    break;
  default:
    break;
  }
  return 0;
}

int ha_oqgraph::delete_all_rows()
{
  int res;
  if (!(res= graph->delete_all()))
  {
    share->records= 0;
  }

  if (!res && table->s->tmp_table == NO_TMP_TABLE)
  {
    /*
       We can perform this safely since only one writer at the time is
       allowed on the table.
    */
    share->key_stat_version++;
  }
  return error_code(res);
}

int ha_oqgraph::external_lock(THD *thd, int lock_type)
{
  return 0;					// No external locking
}


THR_LOCK_DATA **ha_oqgraph::store_lock(THD *thd,
				       THR_LOCK_DATA **to,
				       enum thr_lock_type lock_type)
{
  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK)
    lock.type=lock_type;
  *to++= &lock;
  return to;
}

/*
  We have to ignore ENOENT entries as the HEAP table is created on open and
  not when doing a CREATE on the table.
*/

int ha_oqgraph::delete_table(const char *name)
{
  int res= 0;
  OQGRAPH_INFO *share;
  pthread_mutex_lock(&LOCK_oqgraph);
  if ((share= get_share(name)))
  {
    res= free_share(share, true);
  }
  pthread_mutex_unlock(&LOCK_oqgraph);
  return error_code(res);
}

int ha_oqgraph::rename_table(const char * from, const char * to)
{
  pthread_mutex_lock(&LOCK_oqgraph);
  if (OQGRAPH_INFO *share= get_share(from))
  {
    strmov(share->name, to);
    hash_update(&oqgraph_open_tables, (byte*) share,
                (byte*) from, strlen(from));
  }
  pthread_mutex_unlock(&LOCK_oqgraph);
  return 0;
}


ha_rows ha_oqgraph::records_in_range(uint inx, key_range *min_key,
                                  key_range *max_key)
{
  KEY *key=table->key_info+inx;
  //if (key->algorithm == HA_KEY_ALG_BTREE)
  //  return btree_records_in_range(file, inx, min_key, max_key);

  if (!min_key || !max_key ||
      min_key->length != max_key->length ||
      min_key->length < key->key_length - key->key_part[2].store_length ||
      min_key->flag != HA_READ_KEY_EXACT ||
      max_key->flag != HA_READ_AFTER_KEY)
  {
    if (min_key->length == key->key_part[0].store_length)
    {
      // If latch is not null and equals 0, return # nodes
      DBUG_ASSERT(key->key_part[0].store_length == 3);
      if (key->key_part[0].null_bit && !min_key->key[0] &&
          !min_key->key[1] && !min_key->key[2])
        return graph->vertices_count();
    }
    return HA_POS_ERROR;			// Can only use exact keys
  }

  if (RECORDS <= 1)
    return RECORDS;

  /* Assert that info() did run. We need current statistics here. */
  DBUG_ASSERT(key_stat_version == share->key_stat_version);
  ha_rows result= key->rec_per_key[key->key_parts-1];

  return result;
}


int ha_oqgraph::create(const char *name, TABLE *table_arg,
		    HA_CREATE_INFO *create_info)
{
  int res = -1;
  OQGRAPH_INFO *share;

  pthread_mutex_lock(&LOCK_oqgraph);
  if ((share= get_share(name)))
  {
    free_share(share);
  }
  else
  {
    if (!oqgraph_check_table_structure(table_arg))
      res= 0;;
  }
  pthread_mutex_unlock(&LOCK_oqgraph);

  if (this->share)
    info(HA_STATUS_NO_LOCK | HA_STATUS_CONST | HA_STATUS_VARIABLE);
  return error_code(res);
}


void ha_oqgraph::update_create_info(HA_CREATE_INFO *create_info)
{
  table->file->info(HA_STATUS_AUTO);
  //if (!(create_info->used_fields & HA_CREATE_USED_AUTO))
  //  create_info->auto_increment_value= auto_increment_value;
}

#if MYSQL_VERSION_ID >= 50100
struct st_mysql_storage_engine oqgraph_storage_engine=
{ MYSQL_HANDLERTON_INTERFACE_VERSION };

mysql_declare_plugin(oqgraph)
{
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &oqgraph_storage_engine,
  "OQGRAPH",
  "Arjen Lentz & Antony T Curtis, Open Query",
  oqgraph_description,
  PLUGIN_LICENSE_GPL,
  (int (*)(void*)) oqgraph_init, /* Plugin Init                  */
  oqgraph_fini,               /* Plugin Deinit                   */
  0x0200,                     /* Version: 2.0                    */
  NULL,                       /* status variables                */
  NULL,                       /* system variables                */
  NULL                        /* config options                  */
}
mysql_declare_plugin_end;
#endif

#endif
