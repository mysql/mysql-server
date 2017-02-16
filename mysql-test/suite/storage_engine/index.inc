# 
# Basic syntax related to indexes: 
#   unique and non-unique keys,
#   single- and multi-column keys,
#   index option COMMENT.
#   
# See other index* tests for operations 
# which are less likely to be supported
#
# PRIMARY KEY syntax is covered in index_primary test.
# Index types BTREE|HASH -- in index_type_btree|hash tests.
# SPATIAL -- in type_spatial_indexes test.
# FULLTEXT -- in fulltext_search test.
# KEY_BLOCK_SIZE -- in index_key_block_size test.
#
# Usage to call the test from another test:
#
# A calling test may define $index_type, in which case
#   USING clause will be added to the syntax.
# 

let $using_index_type = ;
if ($index_type)
{
  let $using_index_type = USING $index_type;
}

--source have_engine.inc

--let $continue = 1
--source have_default_index.inc

if ($have_default_index)
{

  let $create_definition = 
    a $int_indexed_col,
    b $char_col,
    $default_index $using_index_type (a)
  ;
  --source create_table.inc
  if ($mysql_errname)
  {
    --let $my_last_stmt = $create_statement
    --source unexpected_result.inc
  }
  if (!$mysql_errname)
  {
    # Cardinality is not the exact science, so here and further
    # we'll be masking it
    --replace_column 6 # 7 # 10 # 
    SHOW KEYS IN t1;
    DROP TABLE t1;
  }

  let $create_definition =
    a $int_indexed_col,
    b $char_indexed_col,
    $default_index a_b $using_index_type (a,b) COMMENT 'a_b index'
  ;
  --source create_table.inc
  if ($mysql_errname)
  {
    --let $functionality = Multi-part indexes
    --let $my_last_stmt = $create_statement
    --source unexpected_result.inc
  }
  if (!$mysql_errname)
  {
    --replace_column 6 # 7 # 10 # 
    SHOW KEYS IN t1;
    DROP TABLE t1;
  }


  let $create_definition = 
    a $int_indexed_col,
    b $char_indexed_col,
    $default_index $using_index_type (a),
    $default_index $using_index_type (b)
  ;
  --source create_table.inc
  if ($mysql_errname)
  {
    --let $functionality = Multiple indexes
    --let $my_last_stmt = $create_statement
    --source unexpected_result.inc
  }
  if (!$mysql_errname)
  {
    --replace_column 6 # 7 # 10 # 
    SHOW KEYS IN t1;
    DROP TABLE t1;
  }

  let $create_definition =
    a $int_indexed_col,
    b $char_col,
    UNIQUE INDEX $using_index_type (a)
  ;
  --source create_table.inc
  if ($mysql_errname)
  {
    --let $functionality = Unique indexes
    --let $my_last_stmt = $create_statement
    --source unexpected_result.inc
  }
  if (!$mysql_errname)
  {
    --replace_column 6 # 7 # 10 # 
    SHOW KEYS IN t1;
    INSERT INTO t1 (a,b) VALUES (1,'a'),(2,'b');
    --let $error_codes = ER_DUP_ENTRY,ER_DUP_KEY
    INSERT INTO t1 (a,b) VALUES (1,'c');
    --source check_errors.inc

    DROP TABLE t1;
  }


  #
  # ALTER TABLE
  #

  --let $create_definition = a $int_indexed_col, b $char_col
  --source create_table.inc
  if ($mysql_errname)
  {
    --let $functionality = Column options
    --let $my_last_stmt = $create_statement
    --source unexpected_result.inc
  }
  if (!$mysql_errname)
  {
    INSERT INTO t1 (a,b) VALUES (100,'z');

    --let $alter_definition = ADD $default_index (a) $using_index_type COMMENT 'simple index on a'
    --source alter_table.inc
    if ($mysql_errname)
    {
      --let $functionality = ALTER TABLE .. ADD INDEX
      --let $my_last_stmt = $alter_statement
      --source unexpected_result.inc
    }
    if (!$mysql_errname)
    {
      --replace_column 6 # 7 # 10 # 
      SHOW INDEX FROM t1;
      --let $alter_definition = DROP KEY a
      --source alter_table.inc
    }
    DROP TABLE t1;
  }

  let $create_definition =
    a $int_indexed_col,
    b $char_col,
    UNIQUE INDEX $using_index_type (a)
  ;
  --source create_table.inc
  if ($mysql_errname)
  {
    --let $functionality = Unique indexes
    --let $my_last_stmt = $create_statement
    --source unexpected_result.inc
  }
  if (!$mysql_errname)
  {
    --replace_column 6 # 7 # 10 #
    SHOW KEYS IN t1;
    INSERT INTO t1 (a,b) VALUES (1,'a'),(2,'b');
    --let $error_codes = ER_DUP_ENTRY,ER_DUP_KEY
    INSERT INTO t1 (a,b) VALUES (1,'c');
    --source check_errors.inc

    --let $alter_definition = DROP INDEX a
    --source alter_table.inc
    if ($mysql_errname)
    {
      --let $functionality = ALTER TABLE .. DROP INDEX
      --let $my_last_stmt = $alter_statement
      --source unexpected_result.inc
    }
    if (!$mysql_errname)
    {
      INSERT INTO t1 (a,b) VALUES (1,'c');
      --let $error_codes = ER_DUP_ENTRY,ER_DUP_KEY
      --let $alter_definition = ADD UNIQUE INDEX a(a) $using_index_type
      --source alter_table.inc
    }
    DROP TABLE t1;
  }
}

