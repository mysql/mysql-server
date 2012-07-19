##################################
#
# This include file will be used for all CREATE TABLE statements in the suite. 
# If you need to add additional steps or change the logic, copy the file 
# to storage/<engine>/mysql-test/storage_engine/ folder and modify it there.
#
##################
#
# Parameters: 
#
# --let $create_definition = <column names, types, indexes)   # optional, default a $int_col, b $char_col (based on defaults)
# --let $table_name = <table name>                            # optional, default t1
# --let $table_options = <table options>                      # optional, default based on define_engine.inc
# --let $partition_options = <partitioning definition>        # optional, default none
# --let $as_select = <SELECT statement>                       # optional, default empty
# --let $error_codes = <expected error codes, as in --error>  # optional, default 0
# --let $if_not_exists = [0|1]                                # optional, default 0 (1 adds IF NOT EXISTS clause)
# --let $default_engine = [0|1]                               # optional, default 0 (with 1 will rely on default engine, no ENGINE=)
# --let $temporary = [0|1]                                    # optional, default 0 (1 adds TEMPORARY)
# --let $disable_query_log = [0|1]                            # optional, default 0 (1 disables logging of CREATE)
#
# Usage examples:
#
# --source create_table.inc -- creates a default table
#
# --let $create_definition = a INT NOT NULL, b CHAR(1) PRIMARY KEY, INDEX(a)
# --let $table_options = AUTO_INCREMENT = 100
# --let $partition_options = PARTITION BY HASH(a) PARTITIONS 2
# --let $as_select = SELECT 1, 'a'
# --source create_table.inc
#
# Additionally, a test can define $extra_tbl_options. The difference with $table_options
# is that its value is persistent and will be used until it is unset explicitly, or
# until the test ends. The purpose of it is to allow one test to call another test,
# when the called test does not know about specific options the calling test might require,
# and thus cannot set them on per-create basis.

--let $create_statement = CREATE

if ($temporary)
{
  --let $create_statement = $create_statement TEMPORARY
}

--let $create_statement = $create_statement TABLE

if ($if_not_exists)
{
  --let $create_statement = $create_statement IF NOT EXISTS
}

if (!$table_name)
{
  --let $table_name = t1
}

# Child statement is a statement that will create an underlying table.
# From this point, it will deviate from the main statement, that's why
# we start creating it here in parallel with the main one.
# For underlying tables, we will create a table in mrg schema, e.g. 
# for table t1 the underlying table will be mrg.t1, etc.
# Since we will only create one child here, it should be enough. If we want more,
# we can always add a suffix, e.g. mrg.t1_child1, mrg.t1_child2, etc.

--let $child_statement = $create_statement mrg.$table_name
--let $create_statement = $create_statement $table_name

if (!$create_definition)
{
  # If $create_definition is not defined, and AS SELECT is requested,
  # we should not set $create_definition to the default value,
  # because it might be inconsistent with the SELECT.
  if (!$as_select)
  {
    --let $create_definition = a $int_col, b $char_col
  }
}

if ($create_definition)
{
  --let $create_statement = $create_statement ($create_definition)
  # Table definition for the underlying table should be the same
  # as for the MERGE table
  --let $child_statement = $child_statement ($create_definition)
}

# If $default_engine is set, we will rely on the default storage engine

if (!$default_engine)
{
  --let $create_statement = $create_statement ENGINE=$storage_engine
}
# Engine for an underlying table differs
--let $child_statement = $child_statement ENGINE=MyISAM

# Save default table options, we will want to restore them later
--let $default_tbl_opts_saved = $default_tbl_opts
--let $default_tbl_opts = $default_tbl_opts UNION(mrg.$table_name) INSERT_METHOD=LAST

# Default table options from define_engine.inc
--let $create_statement = $create_statement $default_tbl_opts

# The calling script could request additional table options
if ($table_options)
{
  --let $create_statement = $create_statement $table_options
  --let $child_statement = $child_statement $table_options
}

# The difference between $extra_tbl_opts and $table_options
# is that its $extra_tbl_opts is persistent -- it will not be unset at the end of this file,
# and will be used until it is unset explicitly by the calling test,
# or until the test ends. The purpose of it is to allow one test to call another test,
# when the called test does not know about specific options the calling test might require,
# and thus cannot set them on per-create basis.

if ($extra_tbl_opts)
{
  --let $create_statement = $create_statement $extra_tbl_opts
  --let $child_statement = $child_statement $extra_tbl_opts
}

if ($as_select)
{
  --let $create_statement = $create_statement AS $as_select
  --let $child_statement = $child_statement AS $as_select
}

if ($partition_options)
{
  --let $create_statement = $create_statement $partition_options
  --let $child_statement = $child_statement $partition_options
}

# We now have the complete CREATE statement in $create_statement.
# If your CREATE statement should be composed differently, 
# modify the logic above.

#####################
# Here you can add logic needed BEFORE the main table creation
# (e.g. the table needs a base table, a reference table, etc.).
# Surround it by --disable_query_log/--enable_query_log
# if you don't want it to appear in the result output.
#####################
--disable_warnings
--disable_query_log
--disable_result_log
eval DROP TABLE IF EXISTS mrg.$table_name;
eval $child_statement;
--enable_result_log
--enable_query_log
--enable_warnings

if ($disable_query_log)
{
  --disable_query_log
}

--source obfuscate.inc

eval $create_statement;
--source strict_check_errors.inc

# Make sure you don't add any statements between the main CREATE (above)
# and saving mysql_errno and mysql_errname (below)
# They are saved in case you want to add more logic after the main CREATE,
# because we need the result code of the table creation.
# Also, do not change $create_statement after it is executed!

--let $my_errno = $mysql_errno
--let $my_errname = $mysql_errname


if ($disable_query_log)
{
  --enable_query_log
}

#####################
# Here you can add logic needed AFTER the main table creation,
# e.g. triggers creation.
# Surround it by --disable_query_log/--enable_query_log
# if you don't want it to appear in the result output.
#####################


# Unset the parameters, we don't want them to be accidentally reused later
--let $create_definition =
--let $table_name = t1
--let $table_options = 
--let $partition_options = 
--let $as_select = 0
--let $error_codes =
--let $if_not_exists = 0
--let $default_engine = 0
--let $temporary = 0
--let $disable_query_log = 0

# Restore default table options now
--let $default_tbl_opts = $default_tbl_opts_saved


# Restore the error codes of the main statement
--let $mysql_errno = $my_errno
--let $mysql_errname = $my_errname
# Make sure you don't add any SQL statements after restoring 
# mysql_errno and mysql_errname (above)

