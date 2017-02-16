##################################
#
# This include file will be used for all ALTER TABLE statements in the suite. 
# If you need to add additional steps or change the logic, copy the file 
# to storage/<engine>/mysql-test/storage_engine/ folder and modify it there.
#
##################
#
# Parameters: 
#
# --let $alter_definition = <alter definition>                # mandatory, everything that goes after the table name in ALTER statement
# --let $table_name = <table name>                            # optional, default t1
# --let $error_codes = <expected error codes, as in --error>  # optional, default 0
# --let $online = [0|1]                                       # optional, default 0 (1 adds ONLINE)
# --let $rename_to = <new table name>                         # optional, default empty. 
#                                                             # If set, means we are running RENAME TO, then alter definition is ignored
#
# Usage examples:
# 
# --let $alter_definition = ADD COLUMN b $char_col DEFAULT ''
# 

--let $child_alter_definition = $alter_definition

if ($rename_to)
{
  --let $alter_definition = RENAME TO $rename_to
  --let $child_alter_definition = RENAME TO mrg.$rename_to
}

if (!$alter_definition)
{
  --die # The ALTER statement is empty
}

--let $alter_statement = ALTER

if ($online)
{
  --let $alter_statement = $alter_statement ONLINE
}

if (!$table_name)
{
  --let $table_name = t1
}

--let $alter_statement = $alter_statement TABLE $table_name $alter_definition
# We don't want to do ONLINE on underlying tables, we are not testing MyISAM
--let $child_statement = ALTER TABLE mrg.$table_name $child_alter_definition



# We now have the complete ALTER statement in $alter_statement.
# If your ALTER statement should be composed differently, 
# modify the logic above. 

#####################
# Here you can add logic needed BEFORE the main statement
# (e.g. base tables need to be altered, etc.).
# Surround it by --disable_query_log/--enable_query_log
# if you don't want it to appear in the result output.
#####################

--source obfuscate.inc

eval $alter_statement;
--source check_errors.inc

# Make sure you don't add any statements between the main ALTER (above)
# and saving mysql_errno and mysql_errname (below)
# They are saved in case you want to add more logic after the main ALTER,
# because we need the result code of the statement.
# Also, do not change $alter_statement after it is executed!

--let $my_errno = $mysql_errno
--let $my_errname = $mysql_errname

#####################
# Here you can add logic needed AFTER the main statement.
# Surround it by --disable_query_log/--enable_query_log
# if you don't want it to appear in the result output.
#####################
--disable_query_log
--disable_warnings
--disable_result_log
# We will only try to alter the underlying table if the main alter was successful
if (!$my_errno)
{
  if ($rename_to)
  {
    eval ALTER TABLE $rename_to UNION(mrg.$rename_to);
  }
  # In the same section, the manual says that FLUSH TABLES should be performed before altering
  # the underlying table, and later also says that it should be done after. We'll do both
  FLUSH TABLES;
  eval $child_statement;
  FLUSH TABLES;
}
--enable_result_log
--enable_warnings
--enable_query_log

# Unset the parameters, we don't want them to be accidentally reused later
--let $alter_definition = 
--let $table_name =
--let $error_codes =
--let $online = 0
--let $rename_to =

# Restore the error codes of the main statement
--let $mysql_errno = $my_errno
--let $mysql_errname = $my_errname
# Make sure you don't add any SQL statements after restoring 
# mysql_errno and mysql_errname (above)

