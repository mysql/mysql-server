/* Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_ALTER_TABLE_H
#define SQL_ALTER_TABLE_H

class Key;


/**
  Class representing DROP COLUMN, DROP KEY and DROP FOREIGN KEY
  clauses in ALTER TABLE statement.
*/

class Alter_drop :public Sql_alloc {
public:
  enum drop_type {KEY, COLUMN, FOREIGN_KEY };
  const char *name;
  enum drop_type type;
  Alter_drop(enum drop_type par_type,const char *par_name)
    :name(par_name), type(par_type)
  {
    DBUG_ASSERT(par_name != NULL);
  }
  /**
    Used to make a clone of this object for ALTER/CREATE TABLE
    @sa comment for Key_part_spec::clone
  */
  Alter_drop *clone(MEM_ROOT *mem_root) const
    { return new (mem_root) Alter_drop(*this); }
};


/**
  Class representing SET DEFAULT and DROP DEFAULT clauses in
  ALTER TABLE statement.
*/

class Alter_column :public Sql_alloc {
public:
  const char *name;
  Item *def;
  Alter_column(const char *par_name,Item *literal)
    :name(par_name), def(literal) {}
  /**
    Used to make a clone of this object for ALTER/CREATE TABLE
    @sa comment for Key_part_spec::clone
  */
  Alter_column *clone(MEM_ROOT *mem_root) const
    { return new (mem_root) Alter_column(*this); }
};


/**
  Class which instances represent RENAME INDEX clauses in
  ALTER TABLE statement.
*/

class Alter_rename_key :public Sql_alloc {
public:
  const char *old_name;
  const char *new_name;

  Alter_rename_key(const char *old_name_arg, const char *new_name_arg)
    : old_name(old_name_arg), new_name(new_name_arg)
  { }

  /**
    Used to make a clone of this object for ALTER/CREATE TABLE
    @sa comment for Key_part_spec::clone
  */
  Alter_rename_key *clone(MEM_ROOT *mem_root) const
  { return new (mem_root) Alter_rename_key(*this); }
};


/**
  Data describing the table being created by CREATE TABLE or
  altered by ALTER TABLE.
*/

class Alter_info
{
public:
  /*
    These flags are set by the parser and describes the type of
    operation(s) specified by the ALTER TABLE statement.

    They do *not* describe the type operation(s) to be executed
    by the storage engine. For example, we don't yet know the
    type of index to be added/dropped.
  */

  // Set for ADD [COLUMN]
  static const uint ALTER_ADD_COLUMN            = 1L <<  0;

  // Set for DROP [COLUMN]
  static const uint ALTER_DROP_COLUMN           = 1L <<  1;

  // Set for CHANGE [COLUMN] | MODIFY [CHANGE]
  // Set by mysql_recreate_table()
  static const uint ALTER_CHANGE_COLUMN         = 1L <<  2;

  // Set for ADD INDEX | ADD KEY | ADD PRIMARY KEY | ADD UNIQUE KEY |
  //         ADD UNIQUE INDEX | ALTER ADD [COLUMN]
  static const uint ALTER_ADD_INDEX             = 1L <<  3;

  // Set for DROP PRIMARY KEY | DROP FOREIGN KEY | DROP KEY | DROP INDEX
  static const uint ALTER_DROP_INDEX            = 1L <<  4;

  // Set for RENAME [TO]
  static const uint ALTER_RENAME                = 1L <<  5;

  // Set for ORDER BY
  static const uint ALTER_ORDER                 = 1L <<  6;

  // Set for table_options
  static const uint ALTER_OPTIONS               = 1L <<  7;

  // Set for ALTER [COLUMN] ... SET DEFAULT ... | DROP DEFAULT
  static const uint ALTER_CHANGE_COLUMN_DEFAULT = 1L <<  8;

  // Set for DISABLE KEYS | ENABLE KEYS
  static const uint ALTER_KEYS_ONOFF            = 1L <<  9;

  // Set for CONVERT TO CHARACTER SET
  static const uint ALTER_CONVERT               = 1L << 10;

  // Set for FORCE
  // Set by mysql_recreate_table()
  static const uint ALTER_RECREATE              = 1L << 11;

  // Set for ADD PARTITION
  static const uint ALTER_ADD_PARTITION         = 1L << 12;

  // Set for DROP PARTITION
  static const uint ALTER_DROP_PARTITION        = 1L << 13;

  // Set for COALESCE PARTITION
  static const uint ALTER_COALESCE_PARTITION    = 1L << 14;

  // Set for REORGANIZE PARTITION ... INTO
  static const uint ALTER_REORGANIZE_PARTITION  = 1L << 15;

  // Set for partition_options
  static const uint ALTER_PARTITION             = 1L << 16;

  // Set for LOAD INDEX INTO CACHE ... PARTITION
  // Set for CACHE INDEX ... PARTITION
  static const uint ALTER_ADMIN_PARTITION       = 1L << 17;

  // Set for REORGANIZE PARTITION
  static const uint ALTER_TABLE_REORG           = 1L << 18;

  // Set for REBUILD PARTITION
  static const uint ALTER_REBUILD_PARTITION     = 1L << 19;

  // Set for partitioning operations specifying ALL keyword
  static const uint ALTER_ALL_PARTITION         = 1L << 20;

  // Set for REMOVE PARTITIONING
  static const uint ALTER_REMOVE_PARTITIONING   = 1L << 21;

  // Set for ADD FOREIGN KEY
  static const uint ADD_FOREIGN_KEY             = 1L << 22;

  // Set for DROP FOREIGN KEY
  static const uint DROP_FOREIGN_KEY            = 1L << 23;

  // Set for EXCHANGE PARITION
  static const uint ALTER_EXCHANGE_PARTITION    = 1L << 24;

  // Set by Sql_cmd_alter_table_truncate_partition::execute()
  static const uint ALTER_TRUNCATE_PARTITION    = 1L << 25;

  // Set for ADD [COLUMN] FIRST | AFTER
  static const uint ALTER_COLUMN_ORDER          = 1L << 26;

  // Set for RENAME INDEX
  static const uint ALTER_RENAME_INDEX          = 1L << 27;

  enum enum_enable_or_disable { LEAVE_AS_IS, ENABLE, DISABLE };

  /**
     The different values of the ALGORITHM clause.
     Describes which algorithm to use when altering the table.
  */
  enum enum_alter_table_algorithm
  {
    // In-place if supported, copy otherwise.
    ALTER_TABLE_ALGORITHM_DEFAULT,

    // In-place if supported, error otherwise.
    ALTER_TABLE_ALGORITHM_INPLACE,

    // Copy if supported, error otherwise.
    ALTER_TABLE_ALGORITHM_COPY
  };


  /**
     The different values of the LOCK clause.
     Describes the level of concurrency during ALTER TABLE.
  */
  enum enum_alter_table_lock
  {
    // Maximum supported level of concurency for the given operation.
    ALTER_TABLE_LOCK_DEFAULT,

    // Allow concurrent reads & writes. If not supported, give erorr.
    ALTER_TABLE_LOCK_NONE,

    // Allow concurrent reads only. If not supported, give error.
    ALTER_TABLE_LOCK_SHARED,

    // Block reads and writes.
    ALTER_TABLE_LOCK_EXCLUSIVE
  };


  // Columns and keys to be dropped.
  List<Alter_drop>              drop_list;
  // Columns for ALTER_COLUMN_CHANGE_DEFAULT.
  List<Alter_column>            alter_list;
  // List of keys, used by both CREATE and ALTER TABLE.
  List<Key>                     key_list;
  // Keys to be renamed.
  List<Alter_rename_key>        alter_rename_key_list;
  // List of columns, used by both CREATE and ALTER TABLE.
  List<Create_field>            create_list;
  // Type of ALTER TABLE operation.
  uint                          flags;
  // Enable or disable keys.
  enum_enable_or_disable        keys_onoff;
  // List of partitions.
  List<char>                    partition_names;
  // Number of partitions.
  uint                          num_parts;
  // Type of ALTER TABLE algorithm.
  enum_alter_table_algorithm    requested_algorithm;
  // Type of ALTER TABLE lock.
  enum_alter_table_lock         requested_lock;


  Alter_info() :
    flags(0),
    keys_onoff(LEAVE_AS_IS),
    num_parts(0),
    requested_algorithm(ALTER_TABLE_ALGORITHM_DEFAULT),
    requested_lock(ALTER_TABLE_LOCK_DEFAULT)
  {}

  void reset()
  {
    drop_list.empty();
    alter_list.empty();
    key_list.empty();
    alter_rename_key_list.empty();
    create_list.empty();
    flags= 0;
    keys_onoff= LEAVE_AS_IS;
    num_parts= 0;
    partition_names.empty();
    requested_algorithm= ALTER_TABLE_ALGORITHM_DEFAULT;
    requested_lock= ALTER_TABLE_LOCK_DEFAULT;
  }


  /**
    Construct a copy of this object to be used for mysql_alter_table
    and mysql_create_table.

    Historically, these two functions modify their Alter_info
    arguments. This behaviour breaks re-execution of prepared
    statements and stored procedures and is compensated by always
    supplying a copy of Alter_info to these functions.

    @param  rhs       Alter_info to make copy of
    @param  mem_root  Mem_root for new Alter_info

    @note You need to use check the error in THD for out
    of memory condition after calling this function.
  */
  Alter_info(const Alter_info &rhs, MEM_ROOT *mem_root);


  /**
     Parses the given string and sets requested_algorithm
     if the string value matches a supported value.
     Supported values: INPLACE, COPY, DEFAULT

     @param  str    String containing the supplied value
     @retval false  Supported value found, state updated
     @retval true   Not supported value, no changes made
  */
  bool set_requested_algorithm(const LEX_STRING *str);


  /**
     Parses the given string and sets requested_lock
     if the string value matches a supported value.
     Supported values: NONE, SHARED, EXCLUSIVE, DEFAULT

     @param  str    String containing the supplied value
     @retval false  Supported value found, state updated
     @retval true   Not supported value, no changes made
  */

  bool set_requested_lock(const LEX_STRING *str);

private:
  Alter_info &operator=(const Alter_info &rhs); // not implemented
  Alter_info(const Alter_info &rhs);            // not implemented
};


/** Runtime context for ALTER TABLE. */
class Alter_table_ctx
{
public:
  Alter_table_ctx();

  Alter_table_ctx(THD *thd, TABLE_LIST *table_list, uint tables_opened_arg,
                  char *new_db_arg, char *new_name_arg);

  /**
     @return true if the table is moved to another database, false otherwise.
  */
  bool is_database_changed() const
  { return (new_db != db); };

  /**
     @return true if the table is renamed, false otherwise.
  */
  bool is_table_renamed() const
  { return (is_database_changed() || new_name != table_name); };

  /**
     @return filename (including .frm) for the new table.
  */
  const char *get_new_filename() const
  {
    DBUG_ASSERT(!tmp_table);
    return new_filename;
  }

  /**
     @return path to the original table.
  */
  const char *get_path() const
  {
    DBUG_ASSERT(!tmp_table);
    return path;
  }

  /**
     @return path to the new table.
  */
  const char *get_new_path() const
  {
    DBUG_ASSERT(!tmp_table);
    return new_path;
  }

  /**
     @return path to the temporary table created during ALTER TABLE.
  */
  const char *get_tmp_path() const
  { return tmp_path; }

  /**
    Mark ALTER TABLE as needing to produce foreign key error if
    it deletes a row from the table being changed.
  */
  void set_fk_error_if_delete_row(FOREIGN_KEY_INFO *fk)
  {
    fk_error_if_delete_row= true;
    fk_error_id= fk->foreign_id->str;
    fk_error_table= fk->foreign_table->str;
  }

public:
  Create_field *datetime_field;
  bool         error_if_not_empty;
  uint         tables_opened;
  char         *db;
  char         *table_name;
  char         *alias;
  char         *new_db;
  char         *new_name;
  char         *new_alias;
  char         tmp_name[80];
  /**
    Indicates that if a row is deleted during copying of data from old version
    of table to the new version ER_FK_CANNOT_DELETE_PARENT error should be
    emitted.
  */
  bool         fk_error_if_delete_row;
  /** Name of foreign key for the above error. */
  const char   *fk_error_id;
  /** Name of table for the above error. */
  const char   *fk_error_table;

private:
  char new_filename[FN_REFLEN + 1];
  char new_alias_buff[FN_REFLEN + 1];
  char path[FN_REFLEN + 1];
  char new_path[FN_REFLEN + 1];
  char tmp_path[FN_REFLEN + 1];

#ifndef DBUG_OFF
  /** Indicates that we are altering temporary table. Used only in asserts. */
  bool tmp_table;
#endif

  Alter_table_ctx &operator=(const Alter_table_ctx &rhs); // not implemented
  Alter_table_ctx(const Alter_table_ctx &rhs);            // not implemented
};


/**
  Sql_cmd_common_alter_table represents the common properties of the ALTER TABLE
  statements.
  @todo move Alter_info and other ALTER generic structures from Lex here.
*/
class Sql_cmd_common_alter_table : public Sql_cmd
{
protected:
  /**
    Constructor.
  */
  Sql_cmd_common_alter_table()
  {}

  virtual ~Sql_cmd_common_alter_table()
  {}

  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_ALTER_TABLE;
  }
};

/**
  Sql_cmd_alter_table represents the generic ALTER TABLE statement.
  @todo move Alter_info and other ALTER specific structures from Lex here.
*/
class Sql_cmd_alter_table : public Sql_cmd_common_alter_table
{
public:
  /**
    Constructor, used to represent a ALTER TABLE statement.
  */
  Sql_cmd_alter_table()
  {}

  ~Sql_cmd_alter_table()
  {}

  bool execute(THD *thd);
};


/**
  Sql_cmd_alter_table_tablespace represents ALTER TABLE
  IMPORT/DISCARD TABLESPACE statements.
*/
class Sql_cmd_discard_import_tablespace : public Sql_cmd_common_alter_table
{
public:
  enum enum_tablespace_op_type
  {
    DISCARD_TABLESPACE, IMPORT_TABLESPACE
  };

  Sql_cmd_discard_import_tablespace(enum_tablespace_op_type tablespace_op_arg)
    : m_tablespace_op(tablespace_op_arg)
  {}

  bool execute(THD *thd);

private:
  const enum_tablespace_op_type m_tablespace_op;
};

#endif
