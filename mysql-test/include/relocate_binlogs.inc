# ==== Purpose ====
#
# Relocates the relay logs and index file from 
# a directory into another. The logs relocated 
# are the one listed in the index file.
#
# ==== Usage ====
#
# [--let $relocate_disable_query_log= 1]
# [--let $rpl_debug= 1]
# [--let $relocate_is_windows= 0]
# [--let $relocate_recreate_index= 0]
# [--let $relocate_fix_relay_log_info= 0]
# --let $relocate_from= DIR
# --let $relocate_to= DIR
# --let $relocate_index_file= FNAME
# --source include/relocate_binlogs.inc

if ($relocate_disable_query_log)
{
  --disable_query_log
}

--let $_path_separator=/
if ($relocate_is_windows)
{
  --let $_path_separator=\
}

if ($relocate_index_file)
{
  SET SQL_LOG_BIN=0;
  CREATE TEMPORARY TABLE tmp(id INT AUTO_INCREMENT PRIMARY KEY, filename VARCHAR(1024));

  --let $write_var=
  --let $_index_file= $relocate_index_file
  --let $_index_file_basename= `SELECT RIGHT(RTRIM("$_index_file"), LOCATE("$_path_separator",REVERSE(RTRIM("$_index_file"))) -1)`
  --let $_from= $relocate_from
  --let $_to= $relocate_into

  # chmod to allow the following LOAD DATA
  --chmod 0666 $_index_file

  --eval LOAD DATA INFILE '$_index_file' INTO TABLE tmp (filename)
  --let $count= `SELECT count(*) FROM tmp`

  while ($count)
  {
    --let $_filename= `select filename from tmp where id=$count`

    --let $_filename= `SELECT RIGHT(RTRIM("$_filename"), LOCATE("$_path_separator",REVERSE(RTRIM("$_filename"))) -1)`
    --move_file $_from/$_filename $_to/$_filename

    if ($relocate_recreate_index)
    {

      if ($relocate_is_windows)
      {
        --let $_write_var=$_to\$_filename\n
      }
      if (!$relocate_is_windows)
      {
        --let $_write_var=$_to/$_filename\n
      }
      if (!$write_var)
      {
        --let $write_var=$_write_var
      }

      if (!`SELECT STRCMP('$write_var', '$_write_var') = 0`)
      {
        --let $write_var=$_write_var$write_var
      }
    }

    --dec $count
  }

  if (!$relocate_recreate_index)
  {
    --move_file $_index_file $_to/$_index_file_basename
  }

  if ($relocate_recreate_index)
  {
    --let $write_to_file= $_to/$_index_file_basename
    --source include/write_var_to_file.inc
    --remove_file $_index_file
  }

  DROP TEMPORARY TABLE tmp;

  if ($relocate_fix_relay_log_info)
  {
    CREATE TEMPORARY TABLE tmp(id INT AUTO_INCREMENT PRIMARY KEY, entry VARCHAR(1024));
    --let $write_var=
   
    # chmod to allow the following LOAD DATA
    --chmod 0666 $relocate_fix_relay_log_info

    --eval LOAD DATA INFILE '$relocate_fix_relay_log_info' INTO TABLE tmp (entry)
    --let $count= `SELECT count(*) FROM tmp`

    --let $_curr_entry= `SELECT entry FROM tmp WHERE id=2`
    --let $_curr_entry_basename= `SELECT RIGHT(RTRIM("$_curr_entry"), LOCATE("$_path_separator",REVERSE(RTRIM("$_curr_entry"))) -1)`

    if ($relocate_is_windows)
    {
      --eval UPDATE tmp SET entry='$_to\$_curr_entry_basename' WHERE id=2
    }
    if (!$relocate_is_windows)
    {
      --eval UPDATE tmp SET entry='$_to/$_curr_entry_basename' WHERE id=2
    }

    --remove_file $relocate_fix_relay_log_info

    while($count)
    {
      --let $_write_var= `SELECT entry FROM tmp WHERE id= $count`
      --let $write_var=$_write_var\n$write_var
      --dec $count
    }

    --let $write_to_file= $relocate_fix_relay_log_info
    --source include/write_var_to_file.inc

    DROP TEMPORARY TABLE tmp;
  }
  SET SQL_LOG_BIN=1;
}


if ($relocate_disable_query_log)
{
  --enable_query_log
}
