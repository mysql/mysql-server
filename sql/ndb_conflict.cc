/*
   Copyright (c) 2012, 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
#include <my_global.h> /* For config defines */

#include "ha_ndbcluster_glue.h"
#include "ndb_conflict.h"
#include "ndb_binlog_extra_row_info.h"

#ifdef HAVE_NDB_BINLOG
#include "ndb_mi.h"
extern st_ndb_slave_state g_ndb_slave_state;

#define NDBTAB NdbDictionary::Table
#define NDBCOL NdbDictionary::Column

/*
  Return true if a column has a specific prefix.
*/
bool
ExceptionsTableWriter::has_prefix_ci(const char *col_name,
                                     const char *prefix,
                                     CHARSET_INFO *cs)
{
  uint col_len= strlen(col_name);
  uint prefix_len= strlen(prefix);
  if (col_len < prefix_len)
    return false;
  char col_name_prefix[FN_HEADLEN];
  strncpy(col_name_prefix, col_name, prefix_len);
  col_name_prefix[prefix_len]= '\0';
  return (my_strcasecmp(cs,
                        col_name_prefix,
                        prefix) == 0);
}

/*
  Return true if a column has a specific suffix
  and sets the column_real_name to the column name
  without the suffix.
*/
bool
ExceptionsTableWriter::has_suffix_ci(const char *col_name,
                                     const char *suffix,
                                     CHARSET_INFO *cs,
                                     char *col_name_real)
{
  uint col_len= strlen(col_name);
  uint suffix_len= strlen(suffix);
  const char *col_name_endp= col_name + col_len;
  strcpy(col_name_real, col_name);
  if (col_len > suffix_len &&
      my_strcasecmp(cs,
                    col_name_endp - suffix_len,
                    suffix) == 0)
  {
    col_name_real[col_len - suffix_len]= '\0';
    return true;
  }
  return false;
}

/*
  Search for column_name in table and
  return true if found. Also return what
  position column was found in pos and possible
  position in the primary key in key_pos.
 */
bool
ExceptionsTableWriter::find_column_name_ci(CHARSET_INFO *cs,
                                           const char *col_name,
                                           const NdbDictionary::Table* table,
                                           int *pos,
                                           int *key_pos)
{
  int ncol= table->getNoOfColumns();
  for(int m= 0; m < ncol; m++)
  {
    const NdbDictionary::Column* col= table->getColumn(m);
    const char *tcol_name= col->getName();
    if (col->getPrimaryKey())
      (*key_pos)++;
    if (my_strcasecmp(cs, col_name, tcol_name) == 0)
    {
      *pos= m;
      return true;
    }
  }
  return false;
}


bool
ExceptionsTableWriter::check_mandatory_columns(const NdbDictionary::Table* exceptionsTable)
{
  DBUG_ENTER("ExceptionsTableWriter::check_mandatory_columns");
  if (/* server id */
      exceptionsTable->getColumn(0)->getType() == NDBCOL::Unsigned &&
      exceptionsTable->getColumn(0)->getPrimaryKey() &&
      /* master_server_id */
      exceptionsTable->getColumn(1)->getType() == NDBCOL::Unsigned &&
      exceptionsTable->getColumn(1)->getPrimaryKey() &&
      /* master_epoch */
      exceptionsTable->getColumn(2)->getType() == NDBCOL::Bigunsigned &&
      exceptionsTable->getColumn(2)->getPrimaryKey() &&
      /* count */
      exceptionsTable->getColumn(3)->getType() == NDBCOL::Unsigned &&
      exceptionsTable->getColumn(3)->getPrimaryKey()
      )
    DBUG_RETURN(true);
  else
    DBUG_RETURN(false);    
}

bool
ExceptionsTableWriter::check_pk_columns(const NdbDictionary::Table* mainTable,
                                        const NdbDictionary::Table* exceptionsTable,
                                        int &k)
{
  DBUG_ENTER("ExceptionsTableWriter::check_pk_columns");
  const int fixed_cols= 4;
  int ncol= mainTable->getNoOfColumns();
  int nkey= mainTable->getNoOfPrimaryKeys();
  /* Check columns that are part of the primary key */
  for (int i= k= 0; i < ncol && k < nkey; i++)
  {
      const NdbDictionary::Column* col= mainTable->getColumn(i);
      if (col->getPrimaryKey())
      {
        const NdbDictionary::Column* ex_col=
          exceptionsTable->getColumn(fixed_cols + k);
        if(!(ex_col != NULL &&
             col->getType() == ex_col->getType() &&
             col->getLength() == ex_col->getLength() &&
             col->getNullable() == ex_col->getNullable()))
         {
           /* 
              Primary key type of the original table doesn't match
              the primary key column of the execption table.
              Assume that the table format has been extended and
              check more below.
           */
           DBUG_PRINT("info", ("Primary key column columns don't match, assume extended table"));
           m_extended= true;
           break;
         }
        /*
          Store mapping of Exception table key# to
          orig table attrid
        */
        DBUG_PRINT("info", ("%u: Setting m_key_attrids[%i]= %i", __LINE__, k, i));
        m_key_attrids[k]= i;
        k++;
      }
    }
  DBUG_RETURN(true);
}

bool
ExceptionsTableWriter::check_optional_columns(const NdbDictionary::Table* mainTable,
                                              const NdbDictionary::Table* exceptionsTable,
                                              char* msg_buf,
                                              uint msg_buf_len,
                                              const char** msg,
                                              int &k,
                                              char *error_details,
                                              uint error_details_len)
{
  DBUG_ENTER("ExceptionsTableWriter::check_optional_columns");
  /*
    Check optional columns. 
    Check if table has been extended by looking for
    the NDB$ prefix. By looking at the columns in
    reverse order we can determine if table has been
    extended and then double check that the original
    mandatory columns also have the NDB$ prefix.
    If an incomplete primary key has been found or
    additional non-primary key attributes from the
    original table then table is also assumed to be
    extended.
  */
  const char* ex_tab_name= exceptionsTable->getName();
  const int fixed_cols= 4;
  bool ok= true;
  int xncol= exceptionsTable->getNoOfColumns();
  int i;
  for (i= xncol - 1; i >= 0; i--)
  {
    const NdbDictionary::Column* col= exceptionsTable->getColumn(i);
    const char* col_name= col->getName();
    /*
      We really need the CHARSET_INFO from when the table was
      created but NdbDictionary::Table doesn't save this. This
      means we cannot handle tables and execption tables defined
      with a charset different than the system charset.
    */
    CHARSET_INFO *cs= system_charset_info;
    bool has_prefix= false;
    
    if (has_prefix_ci(col_name, NDB_EXCEPTIONS_TABLE_COLUMN_PREFIX, cs))
    {
      has_prefix= true;
      m_extended= true;
      DBUG_PRINT("info",
                 ("Exceptions table %s is extended with column %s",
                  ex_tab_name, col_name));
    }
    /* Check that mandatory columns have NDB$ prefix */
    if (i < 4)
    {
      if (m_extended && !has_prefix)
      {
        my_snprintf(msg_buf, msg_buf_len,
                    "Exceptions table %s is extended, but mandatory column %s  doesn't have the \'%s\' prefix",
                    ex_tab_name,
                    col_name,
                    NDB_EXCEPTIONS_TABLE_COLUMN_PREFIX);
        *msg= msg_buf;
        DBUG_RETURN(false);
      }
    }
    k= i - fixed_cols;
    /* Check for extended columns */
    if (my_strcasecmp(cs,
                      col_name,
                      NDB_EXCEPTIONS_TABLE_OP_TYPE) == 0)
    {
      /* Check if ENUM or INT UNSIGNED */
      if (exceptionsTable->getColumn(i)->getType() != NDBCOL::Char &&
          exceptionsTable->getColumn(i)->getType() != NDBCOL::Unsigned)
      {
        my_snprintf(error_details, error_details_len,
                    "Table %s has incorrect type %u for NDB$OP_TYPE",
                    exceptionsTable->getName(),
                    exceptionsTable->getColumn(i)->getType());
        DBUG_PRINT("info", ("%s", error_details));
        ok= false;
        break;
      }
      m_extended= true;
      m_op_type_pos= i;
      continue;
    }
    if (my_strcasecmp(cs,
                      col_name,
                      NDB_EXCEPTIONS_TABLE_CONFLICT_CAUSE) == 0)
    {
      /* Check if ENUM or INT UNSIGNED */
      if (exceptionsTable->getColumn(i)->getType() != NDBCOL::Char &&
          exceptionsTable->getColumn(i)->getType() != NDBCOL::Unsigned)
      {
        my_snprintf(error_details, error_details_len,
                    "Table %s has incorrect type %u for NDB$CFT_CAUSE",
                    exceptionsTable->getName(),
                    exceptionsTable->getColumn(i)->getType());
        DBUG_PRINT("info", ("%s", error_details));
        ok= false;
        break;
      }
      m_extended= true;
      m_conflict_cause_pos= i;
      continue;
    }
    if (my_strcasecmp(cs,
                      col_name,
                      NDB_EXCEPTIONS_TABLE_ORIG_TRANSID) == 0)
    {
      if (exceptionsTable->getColumn(i)->getType() != NDBCOL::Bigunsigned)
      {
        my_snprintf(error_details, error_details_len,
                    "Table %s has incorrect type %u for NDB$ORIG_TRANSID",
                    exceptionsTable->getName(),
                    exceptionsTable->getColumn(i)->getType());
        DBUG_PRINT("info", ("%s", error_details));
        ok= false;
        break;
      }
      m_extended= true;
      m_orig_transid_pos= i;
      continue;
    }
    /*
      Check for any optional columns from the original table in the extended
      table. Compare column types of columns with names matching a column in
      the original table. If a non-primary key column is found we assume that
      the table is extended.
    */
    if (i >= fixed_cols) 
    {
      int match= -1;
      int match_k= -1;
      COLUMN_VERSION column_version= DEFAULT;
      char col_name_real[FN_HEADLEN];
      /* Check for old or new column reference */
      if (has_suffix_ci(col_name,
                        NDB_EXCEPTIONS_TABLE_COLUMN_OLD_SUFFIX,
                        cs,
                        col_name_real))
      {
        DBUG_PRINT("info", ("Found reference to old column %s", col_name));
        column_version= OLD;
      }
      else if (has_suffix_ci(col_name,
                             NDB_EXCEPTIONS_TABLE_COLUMN_NEW_SUFFIX,
                             cs,
                             col_name_real))
      {
        DBUG_PRINT("info", ("Found reference to new column %s", col_name));
        column_version= NEW;
      }
      DBUG_PRINT("info", ("Checking for original column %s", col_name_real));
      /*
        We really need the CHARSET_INFO from when the table was
        created but NdbDictionary::Table doesn't save this. This
        means we cannot handle tables end execption tables defined
        with a charset different than the system charset.
      */
      CHARSET_INFO *mcs= system_charset_info;
      if (! find_column_name_ci(mcs, col_name_real, mainTable, &match, &match_k))
      {
        if (! strcmp(col_name, col_name_real))
        {
          /*
            Column did have $OLD or $NEW suffix, but it didn't
            match. Check if that is the real name of the column.
          */
          match_k= -1;
          if (find_column_name_ci(mcs, col_name, mainTable, &match, &match_k))
          {
            DBUG_PRINT("info", ("Column %s in main table %s has an unfortunate name",
                                col_name, mainTable->getName()));
          }
        }
      }
      /*
        Check that old or new references are nullable
        or have a default value.
      */
      if (column_version != DEFAULT &&
          match_k != -1)
      {
        if ((! col->getNullable()) &&
            col->getDefaultValue() == NULL)
        {
          my_snprintf(error_details, error_details_len,
                      "Old or new column reference %s in table %s is not nullable and doesn't have a default value",
                      col->getName(), exceptionsTable->getName());
          DBUG_PRINT("info", ("%s", error_details));
          ok= false;
          break;
        }
      }
      
      if (match == -1)
      {
        /* 
           Column do not have the same name, could be allowed
           if column is nullable or has a default value,
           continue checking, but give a warning to user
        */
        if ((! col->getNullable()) &&
            col->getDefaultValue() == NULL)
        {
          my_snprintf(error_details, error_details_len,
                      "Extra column %s in table %s is not nullable and doesn't have a default value",
                      col->getName(), exceptionsTable->getName());
          DBUG_PRINT("info", ("%s", error_details));
          ok= false;
          break;
        }
        my_snprintf(error_details, error_details_len,
                    "Column %s in extension table %s not found in %s",
                    col->getName(), exceptionsTable->getName(),
                    mainTable->getName());
        DBUG_PRINT("info", ("%s", error_details));
        my_snprintf(msg_buf, msg_buf_len,
                    "NDB Slave: exceptions table %s has suspicious "
                    "definition ((column %d): %s",
                    ex_tab_name, fixed_cols + k, error_details);
        continue;
      }
      /* We have a matching name */
      const NdbDictionary::Column* mcol= mainTable->getColumn(match);
      if (col->getType() == mcol->getType())
      {
        DBUG_PRINT("info", ("Comparing column %s in exceptions table with column %s in main table", col->getName(), mcol->getName()));
        /* We have matching type */
        if (!mcol->getPrimaryKey())
        {
          /*
            Matching non-key column found.
            Check that column is nullable
            or has a default value.
          */
          if (col->getNullable() ||
              col->getDefaultValue() != NULL)
          {
            DBUG_PRINT("info", ("Mapping column %s %s(%i) to %s(%i)",
                                col->getName(),
                                mainTable->getName(), match,
                                exceptionsTable->getName(), i));
            /* Save position */
            m_data_pos[i]= match;
            m_column_version[i]= column_version;
          }
          else
          {
            my_snprintf(error_details, error_details_len,
                        "Data column %s in table %s is not nullable and doesn't have a default value",
                        col->getName(), exceptionsTable->getName());
            DBUG_PRINT("info", ("%s", error_details));
            ok= false;
            break;
          }
        }
        else
        {
          /* Column is part of the primary key */
          if (column_version != DEFAULT)
          {
            my_snprintf(error_details, error_details_len,
                        "Old or new values of primary key columns cannot be referenced since primary keys cannot be updated, column %s in table %s",
                        col->getName(), exceptionsTable->getName());
            DBUG_PRINT("info", ("%s", error_details));
            ok= false;
            break;
          }
          if (col->getNullable() == mcol->getNullable())
          {
            /*
              Columns are both nullable or not nullable.
              Save position.
            */
            if (m_key_data_pos[match_k] != -1)
            {
              my_snprintf(error_details, error_details_len,
                          "Multiple references to the same key column %s in table %s",
                          col->getName(), exceptionsTable->getName());
              DBUG_PRINT("info", ("%s", error_details));
              ok= false;
              break;
            }
            DBUG_PRINT("info", ("Setting m_key_data_pos[%i]= %i", match_k, i));
            m_key_data_pos[match_k]= i;
            
            if (i == fixed_cols + match_k)
            {
              /* Found key column in correct position */
              if (!m_extended)
                continue;
            }
            /*
              Store mapping of Exception table key# to
              orig table attrid
            */
            DBUG_PRINT("info", ("%u: Setting m_key_attrids[%i]= %i", __LINE__, match_k, match));
            m_key_attrids[match_k]= match;
            m_extended= true;
          }
          else if (column_version == DEFAULT)
          {
            /* 
               Columns have same name and same type
               Column with this name is part of primary key,
               but both columns are not declared not null
            */ 
            my_snprintf(error_details, error_details_len,
                        "Pk column %s not declared not null in both tables",
                        col->getName());
            DBUG_PRINT("info", ("%s", error_details));
            ok= false;
            break;
          }
        }
      }
      else
      {
        /* 
           Columns have same name, but not the same type
        */ 
        my_snprintf(error_details, error_details_len,
                    "Column %s has matching name to column %s for table %s, but wrong type, %u versus %u",
                    col->getName(), mcol->getName(),
                    mainTable->getName(),
                    col->getType(), mcol->getType());
        DBUG_PRINT("info", ("%s", error_details));
        ok= false;
        break;
      }
    }
  }
  
  DBUG_RETURN(ok);
}

int
ExceptionsTableWriter::init(const NdbDictionary::Table* mainTable,
                            const NdbDictionary::Table* exceptionsTable,
                            char* msg_buf,
                            uint msg_buf_len,
                            const char** msg)
{
  DBUG_ENTER("ExceptionsTableWriter::init");
  const char* ex_tab_name= exceptionsTable->getName();
  const int fixed_cols= 4;
  *msg= NULL;
  *msg_buf= '\0';

  DBUG_PRINT("info", ("Checking definition of exceptions table %s",
                      ex_tab_name));
  /*
    Check that the table have the corrct number of columns
    and the mandatory columns.
   */

  bool ok=
    exceptionsTable->getNoOfColumns() >= fixed_cols &&
    exceptionsTable->getNoOfPrimaryKeys() == 4 &&
    check_mandatory_columns(exceptionsTable);

  if (ok)
  {
    char error_details[ FN_REFLEN ];
    uint error_details_len= sizeof(error_details);
    error_details[0]= '\0';
    int ncol= mainTable->getNoOfColumns();
    int nkey= mainTable->getNoOfPrimaryKeys();
    int xncol= exceptionsTable->getNoOfColumns();
    int i, k;
    /* Initialize position arrays */
    for(k=0; k < nkey; k++)
      m_key_data_pos[k]= -1;
    for(i=0; i < xncol; i++)
      m_data_pos[i]= -1;
    /* Initialize nullability information */
    for(i=0; i < ncol; i++)
    {
      const NdbDictionary::Column* col= mainTable->getColumn(i);
      m_col_nullable[i]= col->getNullable();
    }

    /*
      Check that the primary key columns in the main table
      are referenced correctly.
      Then check if the table is extended with optional
      columns.
     */
    ok=
      check_pk_columns(mainTable, exceptionsTable, k) &&
      check_optional_columns(mainTable,
                             exceptionsTable,
                             msg_buf,
                             msg_buf_len,
                             msg,
                             k,
                             error_details,
                             error_details_len);
    if (ok)
    {
      m_ex_tab= exceptionsTable;
      m_pk_cols= nkey;
      m_cols= ncol;
      m_xcols= xncol;
      if (m_extended && strlen(msg_buf) > 0)
        *msg= msg_buf;
      DBUG_RETURN(0);
    }
    else
      my_snprintf(msg_buf, msg_buf_len,
                  "NDB Slave: exceptions table %s has wrong "
                  "definition (column %d): %s",
                  ex_tab_name, fixed_cols + k, error_details);
  }
  else
    my_snprintf(msg_buf, msg_buf_len,
                "NDB Slave: exceptions table %s has wrong "
                "definition (initial %d columns)",
                ex_tab_name, fixed_cols);

  *msg= msg_buf;
  DBUG_RETURN(-1);
}

void
ExceptionsTableWriter::free(Ndb* ndb)
{
  if (m_ex_tab)
  {
    NdbDictionary::Dictionary* dict = ndb->getDictionary();
    dict->removeTableGlobal(*m_ex_tab, 0);
    m_ex_tab= 0;
  }
}

int
ExceptionsTableWriter::writeRow(NdbTransaction* trans,
                                const NdbRecord* keyRecord,
                                const NdbRecord* dataRecord,
                                uint32 server_id,
                                uint32 master_server_id,
                                uint64 master_epoch,
                                const uchar* oldRowPtr,
                                const uchar* newRowPtr,
                                enum_conflicting_op_type op_type,
                                enum_conflict_cause conflict_cause,
                                uint64 orig_transid,
                                const MY_BITMAP *write_set,
                                NdbError& err)
{
  DBUG_ENTER("ExceptionsTableWriter::writeRow");
  DBUG_PRINT("info", ("op_type(pos):%u(%u), conflict_cause(pos):%u(%u), orig_transid:%llu(%u)",
                      op_type, m_op_type_pos,
                      conflict_cause, m_conflict_cause_pos,
                      orig_transid, m_orig_transid_pos));
  DBUG_ASSERT(write_set != NULL);
  assert(err.code == 0);
  const uchar* rowPtr= (op_type == DELETE_ROW)? oldRowPtr : newRowPtr;

  do
  {
    /* Have exceptions table, add row to it */
    const NDBTAB *ex_tab= m_ex_tab;

    /* get insert op */
    NdbOperation *ex_op= trans->getNdbOperation(ex_tab);
    if (ex_op == NULL)
    {
      err= trans->getNdbError();
      break;
    }
    if (ex_op->insertTuple() == -1)
    {
      err= ex_op->getNdbError();
      break;
    }
    {
      uint32 count= (uint32)++m_count;
      /* Set mandatory columns */
      if (ex_op->setValue((Uint32)0, (const char *)&(server_id)) ||
          ex_op->setValue((Uint32)1, (const char *)&(master_server_id)) ||
          ex_op->setValue((Uint32)2, (const char *)&(master_epoch)) ||
          ex_op->setValue((Uint32)3, (const char *)&(count)))
      {
        err= ex_op->getNdbError();
        break;
      }
      /* Set optional columns */
      if (m_extended)
      {
        if (m_op_type_pos)
        {
          if (m_ex_tab->getColumn(m_op_type_pos)->getType()
              == NDBCOL::Char)
          {
            /* Defined as ENUM */
            char op_type_val= (char)op_type;
            if (ex_op->setValue((Uint32)m_op_type_pos,
                                (const char *)&(op_type_val)))
            {
              err= ex_op->getNdbError();
              break;
            }
          }
          else
          {
            uint32 op_type_val= op_type;
            if (ex_op->setValue((Uint32)m_op_type_pos,
                                (const char *)&(op_type_val)))
            {
              err= ex_op->getNdbError();
              break;
            }
          }
        }
        if (m_conflict_cause_pos)
        {
          if (m_ex_tab->getColumn(m_conflict_cause_pos)->getType()
              == NDBCOL::Char)
          {
            /* Defined as ENUM */
            char conflict_cause_val= (char)conflict_cause;
            if (ex_op->setValue((Uint32)m_conflict_cause_pos,
                                (const char *)&(conflict_cause_val)))
            {
              err= ex_op->getNdbError();
              break;
            }
          }
          else
          {
            uint32 conflict_cause_val= conflict_cause;
            if (ex_op->setValue((Uint32)m_conflict_cause_pos,
                                (const char *)&(conflict_cause_val)))
            {
              err= ex_op->getNdbError();
              break;
            }
          }
        }
        if (m_orig_transid_pos != 0)
        {
          const NdbDictionary::Column* col= m_ex_tab->getColumn(m_orig_transid_pos);
          if (orig_transid == Ndb_binlog_extra_row_info::InvalidTransactionId
              &&
              col->getNullable())
          {
            if (ex_op->setValue((Uint32) m_orig_transid_pos, (char*)NULL))
            {
              err= ex_op->getNdbError();
              break;
            }
          }
          else
          {
            DBUG_PRINT("info", ("Setting orig_transid (%u) for table %s", m_orig_transid_pos, ex_tab->getName()));
            uint64 orig_transid_val= orig_transid;
            if (ex_op->setValue((Uint32)m_orig_transid_pos,
                                (const char *)&(orig_transid_val)))
            {
              err= ex_op->getNdbError();
              break;
            }
          }
        }
      }
    }
    /* copy primary keys */
    {
      int nkey= m_pk_cols;
      int k;
      for (k= 0; k < nkey; k++)
      {
        DBUG_ASSERT(rowPtr != NULL);
        if (m_key_data_pos[k] != -1)
        {
          const uchar* data=
            (const uchar*) NdbDictionary::getValuePtr(keyRecord,
                                                      (const char*) rowPtr,
                                                      m_key_attrids[k]);
          if (ex_op->setValue((Uint32) m_key_data_pos[k], (const char*)data) == -1)
          {
            err= ex_op->getNdbError();
            break;
          }
        }
      }
    }
    /* Copy additional data */
    if (m_extended)
    {
      int xncol= m_xcols;
      int i;
      for (i= 0; i < xncol; i++)
      {
        const NdbDictionary::Column* col= m_ex_tab->getColumn(i);
        const uchar* default_value=  (const uchar*) col->getDefaultValue();
        DBUG_PRINT("info", ("Checking column %s(%i)%s", col->getName(), i,
                            (default_value)?", has default value":""));
        DBUG_ASSERT(rowPtr != NULL);
        if (m_data_pos[i] != -1)
        {
          const uchar* row_vPtr= NULL;
          switch (m_column_version[i]) {
          case DEFAULT:
            row_vPtr= rowPtr;
            break;
          case OLD:
            if (op_type != WRITE_ROW)
              row_vPtr= oldRowPtr;
            break;
          case NEW:
            if (op_type != DELETE_ROW)
              row_vPtr= newRowPtr;
          }
          if (row_vPtr == NULL ||
              (m_col_nullable[m_data_pos[i]] &&
               NdbDictionary::isNull(dataRecord,
                                     (const char*) row_vPtr,
                                     m_data_pos[i])))
          {
            DBUG_PRINT("info", ("Column %s is set to NULL because it is NULL", col->getName()));
            if (ex_op->setValue((Uint32) i, (char*)NULL))
            {
              err= ex_op->getNdbError();
              break;
            }
          }
          else if (write_set != NULL && bitmap_is_set(write_set, m_data_pos[i]))
          {
            DBUG_PRINT("info", ("Column %s is set", col->getName()));
            const uchar* data=
              (const uchar*) NdbDictionary::getValuePtr(dataRecord,
                                                        (const char*) row_vPtr,
                                                        m_data_pos[i]);
            if (ex_op->setValue((Uint32) i, (const char*)data) == -1)
            {
              err= ex_op->getNdbError();
              break;
            }
          }
          else if (default_value != NULL)
          {
            DBUG_PRINT("info", ("Column %s is not set to NULL because it has a default value", col->getName()));
            /*
             * Column has a default value
             * Since no value was set in write_set
             * we let the default value be set from
             * Ndb instead.
             */
          }
          else
          {
            DBUG_PRINT("info", ("Column %s is set to NULL because it not in write_set", col->getName()));
            if (ex_op->setValue((Uint32) i, (char*)NULL))
            {
              err= ex_op->getNdbError();
              break;
            }
          }
        }
      }
    }
  } while (0);

  if (err.code != 0)
  {
    if (err.classification == NdbError::SchemaError)
    {
      /* 
       * Something up with Exceptions table schema, forget it.
       * No further exceptions will be recorded.
       * Caller will log this and slave will stop.
       */
      NdbDictionary::Dictionary* dict= trans->getNdb()->getDictionary();
      dict->removeTableGlobal(*m_ex_tab, false);
      m_ex_tab= NULL;
      DBUG_RETURN(0);
    }
    DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}

/* HAVE_NDB_BINLOG */
#endif

/**
   st_ndb_slave_state constructor

   Initialise Ndb Slave state object
*/
st_ndb_slave_state::st_ndb_slave_state()
  : current_master_server_epoch(0),
    current_master_server_epoch_committed(false),
    current_max_rep_epoch(0),
    conflict_flags(0),
    retry_trans_count(0),
    current_trans_row_conflict_count(0),
    current_trans_row_reject_count(0),
    current_trans_in_conflict_count(0),
    last_conflicted_epoch(0),
    max_rep_epoch(0),
    sql_run_id(~Uint32(0)),
    trans_row_conflict_count(0),
    trans_row_reject_count(0),
    trans_detect_iter_count(0),
    trans_in_conflict_count(0),
    trans_conflict_commit_count(0),
    trans_conflict_apply_state(SAS_NORMAL),
    trans_dependency_tracker(NULL)
{
  memset(current_violation_count, 0, sizeof(current_violation_count));
  memset(total_violation_count, 0, sizeof(total_violation_count));

  /* Init conflict handling state memroot */
  const size_t CONFLICT_MEMROOT_BLOCK_SIZE = 32768;
  init_alloc_root(PSI_INSTRUMENT_ME,
                  &conflict_mem_root, CONFLICT_MEMROOT_BLOCK_SIZE, 0);
}

/**
   resetPerAttemptCounters

   Reset the per-epoch-transaction-application-attempt counters
*/
void
st_ndb_slave_state::resetPerAttemptCounters()
{
  memset(current_violation_count, 0, sizeof(current_violation_count));
  current_trans_row_conflict_count = 0;
  current_trans_row_reject_count = 0;
  current_trans_in_conflict_count = 0;

  conflict_flags = 0;
  current_max_rep_epoch = 0;
}

/**
   atTransactionAbort()

   Called by Slave SQL thread during transaction abort.
*/
void
st_ndb_slave_state::atTransactionAbort()
{
#ifdef HAVE_NDB_BINLOG
  /* Reset any gathered transaction dependency information */
  atEndTransConflictHandling();
  trans_conflict_apply_state = SAS_NORMAL;
#endif

  /* Reset current-transaction counters + state */
  resetPerAttemptCounters();
}



/**
   atTransactionCommit()

   Called by Slave SQL thread after transaction commit
*/
void
st_ndb_slave_state::atTransactionCommit(Uint64 epoch)
{
  assert( ((trans_dependency_tracker == NULL) &&
           (trans_conflict_apply_state == SAS_NORMAL)) ||
          ((trans_dependency_tracker != NULL) &&
           (trans_conflict_apply_state == SAS_TRACK_TRANS_DEPENDENCIES)) );
  assert( trans_conflict_apply_state != SAS_APPLY_TRANS_DEPENDENCIES );

  /* Merge committed transaction counters into total state
   * Then reset current transaction counters
   */
  Uint32 total_conflicts = 0;
  for (int i=0; i < CFT_NUMBER_OF_CFTS; i++)
  {
    total_conflicts+= current_violation_count[i];
    total_violation_count[i]+= current_violation_count[i];
  }
  trans_row_conflict_count+= current_trans_row_conflict_count;
  trans_row_reject_count+= current_trans_row_reject_count;
  trans_in_conflict_count+= current_trans_in_conflict_count;

  if (current_trans_in_conflict_count)
    trans_conflict_commit_count++;

  if (current_max_rep_epoch > max_rep_epoch)
  {
    DBUG_PRINT("info", ("Max replicated epoch increases from %llu to %llu",
                        max_rep_epoch,
                        current_max_rep_epoch));
    max_rep_epoch = current_max_rep_epoch;
  }

  if (total_conflicts > 0)
  {
    DBUG_PRINT("info", ("Last conflicted epoch increases from %llu to %llu",
                        last_conflicted_epoch,
                        epoch));
    last_conflicted_epoch = epoch;
  }

  resetPerAttemptCounters();

  /* Clear per-epoch-transaction retry_trans_count */
  retry_trans_count = 0;

  current_master_server_epoch_committed = true;

  DBUG_EXECUTE_IF("ndb_slave_fail_marking_epoch_committed",
                  {
                    fprintf(stderr, 
                            "Slave clearing epoch committed flag "
                            "for epoch %llu/%llu (%llu)\n",
                            current_master_server_epoch >> 32,
                            current_master_server_epoch & 0xffffffff,
                            current_master_server_epoch);
                    current_master_server_epoch_committed = false;
                  });
}

/**
   verifyNextEpoch

   Check that a new incoming epoch from the relay log
   is expected given the current slave state, previous
   epoch etc.
   This is checking Generic replication errors, with
   a user warning thrown in too.
*/
bool
st_ndb_slave_state::verifyNextEpoch(Uint64 next_epoch,
                                    Uint32 master_server_id) const
{
  DBUG_ENTER("verifyNextEpoch");
#ifdef HAVE_NDB_BINLOG
  /**
    WRITE_ROW to ndb_apply_status injected by MySQLD
    immediately upstream of us.

    Now we do some validation of the incoming epoch transaction's
    epoch - to make sure that we are getting a sensible sequence
    of epochs.
  */
  bool first_epoch_since_slave_start = (ndb_mi_get_slave_run_id() != sql_run_id);
  
  DBUG_PRINT("info", ("ndb_apply_status write from upstream master."
                      "ServerId %u, Epoch %llu/%llu (%llu) "
                      "Current master server epoch %llu/%llu (%llu)"
                      "Current master server epoch committed? %u",
                      master_server_id,
                      next_epoch >> 32,
                      next_epoch & 0xffffffff,
                      next_epoch,
                      current_master_server_epoch >> 32,
                      current_master_server_epoch & 0xffffffff,
                      current_master_server_epoch,
                      current_master_server_epoch_committed));
  DBUG_PRINT("info", ("mi_slave_run_id=%u, ndb_slave_state_run_id=%u",
                      ndb_mi_get_slave_run_id(), sql_run_id));
  DBUG_PRINT("info", ("First epoch since slave start : %u",
                      first_epoch_since_slave_start));
             
  /* Analysis of nextEpoch generally depends on whether it's the first or not */
  if (first_epoch_since_slave_start)
  {
    /**
       First epoch since slave start - might've had a CHANGE MASTER command,
       since we were last running, so we are not too strict about epoch
       changes, but we will warn.
    */
    if (next_epoch < current_master_server_epoch)
    {
      sql_print_warning("NDB Slave : At SQL thread start "
                        "applying epoch %llu/%llu "
                        "(%llu) from Master ServerId %u which is lower than previously "
                        "applied epoch %llu/%llu (%llu).  "
                        "Group Master Log : %s  Group Master Log Pos : %llu.  "
                        "Check slave positioning.",
                        next_epoch >> 32,
                        next_epoch & 0xffffffff,
                        next_epoch,
                        master_server_id,
                        current_master_server_epoch >> 32,
                        current_master_server_epoch & 0xffffffff,
                        current_master_server_epoch,
                        ndb_mi_get_group_master_log_name(),
                        ndb_mi_get_group_master_log_pos());
      /* Slave not stopped */
    }
    else if (next_epoch == current_master_server_epoch)
    {
      /**
         Could warn that started on already applied epoch,
         but this is often harmless.
      */
    }
    else
    {
      /* next_epoch > current_master_server_epoch - fine. */
    }
  }
  else
  {
    /**
       ! first_epoch_since_slave_start
       
       Slave has already applied some epoch in this run, so we expect
       either :
        a) previous epoch committed ok and next epoch is higher
                                  or
        b) previous epoch not committed and next epoch is the same
           (Retry case)
    */
    if (next_epoch < current_master_server_epoch)
    {
      /* Should never happen */
      sql_print_error("NDB Slave : SQL thread stopped as "
                      "applying epoch %llu/%llu "
                      "(%llu) from Master ServerId %u which is lower than previously "
                      "applied epoch %llu/%llu (%llu).  "
                      "Group Master Log : %s  Group Master Log Pos : %llu",
                      next_epoch >> 32,
                      next_epoch & 0xffffffff,
                      next_epoch,
                      master_server_id,
                      current_master_server_epoch >> 32,
                      current_master_server_epoch & 0xffffffff,
                      current_master_server_epoch,
                      ndb_mi_get_group_master_log_name(),
                      ndb_mi_get_group_master_log_pos());
      /* Stop the slave */
      DBUG_RETURN(false);
    }
    else if (next_epoch == current_master_server_epoch)
    {
      /**
         This is ok if we are retrying - e.g. the 
         last epoch was not committed
      */
      if (current_master_server_epoch_committed)
      {
        /* This epoch is committed already, why are we replaying it? */
        sql_print_error("NDB Slave : SQL thread stopped as attempted "
                        "to reapply already committed epoch %llu/%llu (%llu) "
                        "from server id %u.  "
                        "Group Master Log : %s  Group Master Log Pos : %llu.",
                        current_master_server_epoch >> 32,
                        current_master_server_epoch & 0xffffffff,
                        current_master_server_epoch,
                        master_server_id,
                        ndb_mi_get_group_master_log_name(),
                        ndb_mi_get_group_master_log_pos());
        /* Stop the slave */
        DBUG_RETURN(false);
      }
      else
      {
        /* Probably a retry, no problem. */
      }
    }
    else
    {
      /**
         next_epoch > current_master_server_epoch
      
         This is the normal case, *unless* the previous epoch
         did not commit - in which case it may be a bug in 
         transaction retry.
      */
      if (!current_master_server_epoch_committed)
      {
        /**
           We've moved onto a new epoch without committing
           the last - probably a bug in transaction retry
        */
        sql_print_error("NDB Slave : SQL thread stopped as attempting to "
                        "apply new epoch %llu/%llu (%llu) while lower "
                        "received epoch %llu/%llu (%llu) has not been "
                        "committed.  Master server id : %u.  "
                        "Group Master Log : %s  Group Master Log Pos : %llu.",
                        next_epoch >> 32,
                        next_epoch & 0xffffffff,
                        next_epoch,
                        current_master_server_epoch >> 32,
                        current_master_server_epoch & 0xffffffff,
                        current_master_server_epoch,
                        master_server_id,
                        ndb_mi_get_group_master_log_name(),
                        ndb_mi_get_group_master_log_pos());
        /* Stop the slave */
        DBUG_RETURN(false);
      }
      else
      {
        /* Normal case of next epoch after committing last */
      }
    }
  }
#endif

  /* Epoch looks ok */
  DBUG_RETURN(true);
}

/**
   atApplyStatusWrite

   Called by Slave SQL thread when applying an event to the
   ndb_apply_status table
*/
int
st_ndb_slave_state::atApplyStatusWrite(Uint32 master_server_id,
                                       Uint32 row_server_id,
                                       Uint64 row_epoch,
                                       bool is_row_server_id_local)
{
  DBUG_ENTER("atApplyStatusWrite");
  if (row_server_id == master_server_id)
  {
    /* This is an apply status write from the immediate master */

    if (!verifyNextEpoch(row_epoch,
                         master_server_id))
    {
      /* Problem with the next epoch, stop the slave SQL thread */
      DBUG_RETURN(HA_ERR_ROWS_EVENT_APPLY);
    }

    /* Epoch ok, record that we're working on it now... */

    current_master_server_epoch = row_epoch;
    current_master_server_epoch_committed = false;
    assert(! is_row_server_id_local);
  }
  else if (is_row_server_id_local)
  {
    DBUG_PRINT("info", ("Recording application of local server %u epoch %llu "
                        " which is %s.",
                        row_server_id, row_epoch,
                        (row_epoch > current_max_rep_epoch)?
                        " new highest." : " older than previously applied"));
    if (row_epoch > current_max_rep_epoch)
    {
      /*
        Store new highest epoch in thdvar.  If we commit successfully
        then this can become the new global max
      */
      current_max_rep_epoch = row_epoch;
    }
  }
  DBUG_RETURN(0);
}

/**
   atResetSlave()

   Called when RESET SLAVE command issued - in context of command client.
*/
void
st_ndb_slave_state::atResetSlave()
{
  /* Reset the Maximum replicated epoch vars
   * on slave reset
   * No need to touch the sql_run_id as that
   * will increment if the slave is started
   * again.
   */
  resetPerAttemptCounters();

  retry_trans_count = 0;
  max_rep_epoch = 0;
  last_conflicted_epoch = 0;

  /* Reset current master server epoch
   * This avoids warnings when replaying a lower
   * epoch number after a RESET SLAVE - in this
   * case we assume the user knows best.
   */
  current_master_server_epoch = 0;
  current_master_server_epoch_committed = false;
}


/**
   atStartSlave()

   Called by Slave SQL thread when first applying a row to Ndb after
   a START SLAVE command.
*/
void
st_ndb_slave_state::atStartSlave()
{
#ifdef HAVE_NDB_BINLOG
  if (trans_conflict_apply_state != SAS_NORMAL)
  {
    /*
      Remove conflict handling state on a SQL thread
      restart
    */
    atEndTransConflictHandling();
    trans_conflict_apply_state = SAS_NORMAL;
  }
#endif
}

bool
st_ndb_slave_state::checkSlaveConflictRoleChange(enum_slave_conflict_role old_role,
                                                 enum_slave_conflict_role new_role,
                                                 const char** failure_cause)
{
  if (old_role == new_role)
    return true;
  
  /**
   * Initial role is SCR_NONE
   * Allowed transitions :
   *   SCR_NONE -> SCR_PASS
   *   SCR_NONE -> SCR_PRIMARY
   *   SCR_NONE -> SCR_SECONDARY
   *   SCR_PRIMARY -> SCR_NONE
   *   SCR_PRIMARY -> SCR_SECONDARY
   *   SCR_SECONDARY -> SCR_NONE
   *   SCR_SECONDARY -> SCR_PRIMARY
   *   SCR_PASS -> SCR_NONE
   *
   * Disallowed transitions
   *   SCR_PASS -> SCR_PRIMARY
   *   SCR_PASS -> SCR_SECONDARY
   *   SCR_PRIMARY -> SCR_PASS
   *   SCR_SECONDARY -> SCR_PASS
   */
  bool bad_transition = false;
  *failure_cause = "Internal error";

  switch (old_role)
  {
  case SCR_NONE:
    break;
  case SCR_PRIMARY:
  case SCR_SECONDARY:
    bad_transition = (new_role == SCR_PASS);
    break;
  case SCR_PASS:
    bad_transition = ((new_role == SCR_PRIMARY) ||
                      (new_role == SCR_SECONDARY));
    break;
  default:
    assert(false);
    return false;
  }

  if (bad_transition)
  {
    *failure_cause = "Invalid role change.";
    return false;
  }
  
#ifdef HAVE_NDB_BINLOG
  /* Check that Slave SQL thread is not running */
  if (ndb_mi_get_slave_sql_running())
  {
    *failure_cause = "Cannot change role while Slave SQL "
      "thread is running.  Use STOP SLAVE first.";
    return false;
  }
#endif

  return true;
}


#ifdef HAVE_NDB_BINLOG

/**
   atEndTransConflictHandling

   Called when transactional conflict handling has completed.
*/
void
st_ndb_slave_state::atEndTransConflictHandling()
{
  DBUG_ENTER("atEndTransConflictHandling");
  /* Release any conflict handling state */
  if (trans_dependency_tracker)
  {
    current_trans_in_conflict_count =
      trans_dependency_tracker->get_conflict_count();
    trans_dependency_tracker = NULL;
    free_root(&conflict_mem_root, MY_MARK_BLOCKS_FREE);
  }
  DBUG_VOID_RETURN;
}

/**
   atBeginTransConflictHandling()

   Called by Slave SQL thread when it determines that Transactional
   Conflict handling is required
*/
void
st_ndb_slave_state::atBeginTransConflictHandling()
{
  DBUG_ENTER("atBeginTransConflictHandling");
  /*
     Allocate and initialise Transactional Conflict
     Resolution Handling Structures
  */
  assert(trans_dependency_tracker == NULL);
  trans_dependency_tracker = DependencyTracker::newDependencyTracker(&conflict_mem_root);
  DBUG_VOID_RETURN;
}

/**
   atPrepareConflictDetection

   Called by Slave SQL thread prior to defining an operation on
   a table with conflict detection defined.
*/
int
st_ndb_slave_state::atPrepareConflictDetection(const NdbDictionary::Table* table,
                                               const NdbRecord* key_rec,
                                               const uchar* row_data,
                                               Uint64 transaction_id,
                                               bool& handle_conflict_now)
{
  DBUG_ENTER("atPrepareConflictDetection");
  /*
    Slave is preparing to apply an operation with conflict detection.
    If we're performing Transactional Conflict Resolution, take
    extra steps
  */
  switch( trans_conflict_apply_state )
  {
  case SAS_NORMAL:
    DBUG_PRINT("info", ("SAS_NORMAL : No special handling"));
    /* No special handling */
    break;
  case SAS_TRACK_TRANS_DEPENDENCIES:
  {
    DBUG_PRINT("info", ("SAS_TRACK_TRANS_DEPENDENCIES : Tracking operation"));
    /*
      Track this operation and its transaction id, to determine
      inter-transaction dependencies by {table, primary key}
    */
    assert( trans_dependency_tracker );

    int res = trans_dependency_tracker
      ->track_operation(table,
                        key_rec,
                        row_data,
                        transaction_id);
    if (res != 0)
    {
      sql_print_error("%s", trans_dependency_tracker->get_error_text());
      DBUG_RETURN(res);
    }
    /* Proceed as normal */
    break;
  }
  case SAS_APPLY_TRANS_DEPENDENCIES:
  {
    DBUG_PRINT("info", ("SAS_APPLY_TRANS_DEPENDENCIES : Deciding whether to apply"));
    /*
       Check if this operation's transaction id is marked in-conflict.
       If it is, we tell the caller to perform conflict resolution now instead
       of attempting to apply the operation.
    */
    assert( trans_dependency_tracker );

    if (trans_dependency_tracker->in_conflict(transaction_id))
    {
      DBUG_PRINT("info", ("Event for transaction %llu is conflicting.  Handling.",
                          transaction_id));
      current_trans_row_reject_count++;
      handle_conflict_now = true;
      DBUG_RETURN(0);
    }

    /*
       This transaction is not marked in-conflict, so continue with normal
       processing.
       Note that normal processing may subsequently detect a conflict which
       didn't exist at the time of the previous TRACK_DEPENDENCIES pass.
       In this case, we will rollback and repeat the TRACK_DEPENDENCIES
       stage.
    */
    DBUG_PRINT("info", ("Event for transaction %llu is OK, applying",
                        transaction_id));
    break;
  }
  }
  DBUG_RETURN(0);
}

/**
   atTransConflictDetected

   Called by the Slave SQL thread when a conflict is detected on
   an executed operation.
*/
int
st_ndb_slave_state::atTransConflictDetected(Uint64 transaction_id)
{
  DBUG_ENTER("atTransConflictDetected");

  /*
     The Slave has detected a conflict on an operation applied
     to a table with Transactional Conflict Resolution defined.
     Handle according to current state.
  */
  conflict_flags |= SCS_TRANS_CONFLICT_DETECTED_THIS_PASS;
  current_trans_row_conflict_count++;

  switch (trans_conflict_apply_state)
  {
  case SAS_NORMAL:
  {
    DBUG_PRINT("info", ("SAS_NORMAL : Conflict on op on table with trans detection."
                        "Requires multi-pass resolution.  Will transition to "
                        "SAS_TRACK_TRANS_DEPENDENCIES at Commit."));
    /*
      Conflict on table with transactional conflict resolution
      defined.
      This is the trigger that we will do transactional conflict
      resolution.
      Record that we need to do multiple passes to correctly
      perform resolution.
      TODO : Early exit from applying epoch?
    */
    break;
  }
  case SAS_TRACK_TRANS_DEPENDENCIES:
  {
    DBUG_PRINT("info", ("SAS_TRACK_TRANS_DEPENDENCIES : Operation in transaction %llu "
                        "had conflict",
                        transaction_id));
    /*
       Conflict on table with transactional conflict resolution
       defined.
       We will mark the operation's transaction_id as in-conflict,
       so that any other operations on the transaction are also
       considered in-conflict, and any dependent transactions are also
       considered in-conflict.
    */
    assert(trans_dependency_tracker != NULL);
    int res = trans_dependency_tracker
      ->mark_conflict(transaction_id);

    if (res != 0)
    {
      sql_print_error("%s", trans_dependency_tracker->get_error_text());
      DBUG_RETURN(res);
    }
    break;
  }
  case SAS_APPLY_TRANS_DEPENDENCIES:
  {
    /*
       This must be a new conflict, not noticed on the previous
       pass.
    */
    DBUG_PRINT("info", ("SAS_APPLY_TRANS_DEPENDENCIES : Conflict detected.  "
                        "Must be further conflict.  Will return to "
                        "SAS_TRACK_TRANS_DEPENDENCIES state at commit."));
    // TODO : Early exit from applying epoch
    break;
  }
  default:
    break;
  }

  DBUG_RETURN(0);
}

/**
   atConflictPreCommit

   Called by the Slave SQL thread prior to committing a Slave transaction.
   This method can request that the Slave transaction is retried.


   State transitions :

                       START SLAVE /
                       RESET SLAVE /
                        STARTUP
                            |
                            |
                            v
                    ****************
                    *  SAS_NORMAL  *
                    ****************
                       ^       |
    No transactional   |       | Conflict on transactional table
       conflicts       |       | (Rollback)
       (Commit)        |       |
                       |       v
            **********************************
            *  SAS_TRACK_TRANS_DEPENDENCIES  *
            **********************************
               ^          I              ^
     More      I          I Dependencies |
    conflicts  I          I determined   | No new conflicts
     found     I          I (Rollback)   | (Commit)
    (Rollback) I          I              |
               I          v              |
           **********************************
           *  SAS_APPLY_TRANS_DEPENDENCIES  *
           **********************************


   Operation
     The initial state is SAS_NORMAL.

     On detecting a conflict on a transactional conflict detetecing table,
     SAS_TRACK_TRANS_DEPENDENCIES is entered, and the epoch transaction is
     rolled back and reapplied.

     In SAS_TRACK_TRANS_DEPENDENCIES state, transaction dependencies and
     conflicts are tracked as the epoch transaction is applied.

     Then the Slave transitions to SAS_APPLY_TRANS_DEPENDENCIES state, and
     the epoch transaction is rolled back and reapplied.

     In the SAS_APPLY_TRANS_DEPENDENCIES state, operations for transactions
     marked as in-conflict are not applied.

     If this results in no new conflicts, the epoch transaction is committed,
     and the SAS_TRACK_TRANS_DEPENDENCIES state is re-entered for processing
     the next replicated epch transaction.
     If it results in new conflicts, the epoch transactions is rolled back, and
     the SAS_TRACK_TRANS_DEPENDENCIES state is re-entered again, to determine
     the new set of dependencies.

     If no conflicts are found in the SAS_TRACK_TRANS_DEPENDENCIES state, then
     the epoch transaction is committed, and the Slave transitions to SAS_NORMAL
     state.


   Properties
     1) Normally, there is no transaction dependency tracking overhead paid by
        the slave.

     2) On first detecting a transactional conflict, the epoch transaction must be
        applied at least three times, with two rollbacks.

     3) Transactional conflicts detected in subsequent epochs require the epoch
        transaction to be applied two times, with one rollback.

     4) A loop between states SAS_TRACK_TRANS_DEPENDENCIES and SAS_APPLY_TRANS_
        DEPENDENCIES occurs when further transactional conflicts are discovered
        in SAS_APPLY_TRANS_DEPENDENCIES state.  This implies that the  conflicts
        discovered in the SAS_TRACK_TRANS_DEPENDENCIES state must not be complete,
        so we revisit that state to get a more complete picture.

     5) The number of iterations of this loop is fixed to a hard coded limit, after
        which the Slave will stop with an error.  This should be an unlikely
        occurrence, as it requires not just n conflicts, but at least 1 new conflict
        appearing between the transactions in the epoch transaction and the
        database between the two states, n times in a row.

     6) Where conflicts are occasional, as expected, the post-commit transition to
        SAS_TRACK_TRANS_DEPENDENCIES rather than SAS_NORMAL results in one epoch
        transaction having its transaction dependencies needlessly tracked.

*/
int
st_ndb_slave_state::atConflictPreCommit(bool& retry_slave_trans)
{
  DBUG_ENTER("atConflictPreCommit");

  /*
    Prior to committing a Slave transaction, we check whether
    Transactional conflicts have been detected which require
    us to retry the slave transaction
  */
  retry_slave_trans = false;
  switch(trans_conflict_apply_state)
  {
  case SAS_NORMAL:
  {
    DBUG_PRINT("info", ("SAS_NORMAL"));
    /*
       Normal case.  Only if we defined conflict detection on a table
       with transactional conflict detection, and saw conflicts (on any table)
       do we go to another state
     */
    if (conflict_flags & SCS_TRANS_CONFLICT_DETECTED_THIS_PASS)
    {
      DBUG_PRINT("info", ("Conflict(s) detected this pass, transitioning to "
                          "SAS_TRACK_TRANS_DEPENDENCIES."));
      assert(conflict_flags & SCS_OPS_DEFINED);
      /* Transactional conflict resolution required, switch state */
      atBeginTransConflictHandling();
      resetPerAttemptCounters();
      trans_conflict_apply_state = SAS_TRACK_TRANS_DEPENDENCIES;
      retry_slave_trans = true;
    }
    break;
  }
  case SAS_TRACK_TRANS_DEPENDENCIES:
  {
    DBUG_PRINT("info", ("SAS_TRACK_TRANS_DEPENDENCIES"));

    if (conflict_flags & SCS_TRANS_CONFLICT_DETECTED_THIS_PASS)
    {
      /*
         Conflict on table with transactional detection
         this pass, we have collected the details and
         dependencies, now transition to
         SAS_APPLY_TRANS_DEPENDENCIES and
         reapply the epoch transaction without the
         conflicting transactions.
      */
      assert(conflict_flags & SCS_OPS_DEFINED);
      DBUG_PRINT("info", ("Transactional conflicts, transitioning to "
                          "SAS_APPLY_TRANS_DEPENDENCIES"));

      trans_conflict_apply_state = SAS_APPLY_TRANS_DEPENDENCIES;
      trans_detect_iter_count++;
      retry_slave_trans = true;
      break;
    }
    else
    {
      /*
         No transactional conflicts detected this pass, lets
         return to SAS_NORMAL state after commit for more efficient
         application of epoch transactions
      */
      DBUG_PRINT("info", ("No transactional conflicts, transitioning to "
                          "SAS_NORMAL"));
      atEndTransConflictHandling();
      trans_conflict_apply_state = SAS_NORMAL;
      break;
    }
  }
  case SAS_APPLY_TRANS_DEPENDENCIES:
  {
    DBUG_PRINT("info", ("SAS_APPLY_TRANS_DEPENDENCIES"));
    assert(conflict_flags & SCS_OPS_DEFINED);
    /*
       We've applied the Slave epoch transaction subject to the
       conflict detection.  If any further transactional
       conflicts have been observed, then we must repeat the
       process.
    */
    atEndTransConflictHandling();
    atBeginTransConflictHandling();
    trans_conflict_apply_state = SAS_TRACK_TRANS_DEPENDENCIES;

    if (unlikely(conflict_flags & SCS_TRANS_CONFLICT_DETECTED_THIS_PASS))
    {
      DBUG_PRINT("info", ("Further conflict(s) detected, repeating the "
                          "TRACK_TRANS_DEPENDENCIES pass"));
      /*
         Further conflict observed when applying, need
         to re-determine dependencies
      */
      resetPerAttemptCounters();
      retry_slave_trans = true;
      break;
    }


    DBUG_PRINT("info", ("No further conflicts detected, committing and "
                        "returning to SAS_TRACK_TRANS_DEPENDENCIES state"));
    /*
       With dependencies taken into account, no further
       conflicts detected, can now proceed to commit
    */
    break;
  }
  }

  /*
    Clear conflict flags, to ensure that we detect any new conflicts
  */
  conflict_flags = 0;

  if (retry_slave_trans)
  {
    DBUG_PRINT("info", ("Requesting transaction restart"));
    DBUG_RETURN(1);
  }

  DBUG_PRINT("info", ("Allowing commit to proceed"));
  DBUG_RETURN(0);
}



#endif
