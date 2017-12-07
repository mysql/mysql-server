# suite/ddl/include/load_tsof_data.inc
#

# $when_taken must sit right

--enable_abort_on_error

--disable_query_log
if($script_debug)
{
   --enable_query_log
}

# Add the new state to the bookkeeping files

# Result of SELECT MD5(...)
# -------------------------
# SELECT MD5(NULL)                            : NULL
# SELECT MD5('')                              : d41d8cd98f00b204e9800998ecf8427e
# SELECT MD5(LOAD_FILE(<file>)) variations
# secure_file_priv sits right
# - file is readable by all and zero bytes
#      long                                   : d41d8cd98f00b204e9800998ecf8427e
# - file is not readable by all               : NULL
#   Observed under Linux where some additional tablespaces.open.4 was created
#   by InnoDB with "-rw-rw----" whereas the already existing were with
#   "-rw-r--r--" from whatever reason (MTR during server setup?).
# - file does not exist                       : NULL
# secure_file_priv sits wrong                 : NULL (no warning!)
# So in case we take care that
# - secure_file_priv sits right
# - any tablespaces.open.* gets treated with chmod 644
# than any NULL implies some not existing tablespaces.open.<number>.
#
let $TSOF_DIR= $MYSQLD_DATADIR;
# if($when_taken == $point_postcrash)
# {
   # let $TSOF_DIR= $MYSQLTEST_VARDIR/tsof_copies/;
# }
let $max_num= 5;
let $num= 1;
let $null_reached= 0;
while ($num <= $max_num)
{
   let $tsof_name= tablespaces.open.$num;
   let $tsof_path= $TSOF_DIR$tsof_name;

   # Fore debugging the script.
   if ($script_debug)
   {
      --echo $tsof_path
      # 1. The --error 0,1,2 is because the file $tsof_name might not exist.
      #    This is to be expected for the higher numbers.
      # 2. The "cd $MYSQLD_DATADIR;" allows to use $tsof_name and that causes
      #    that the output is shorter and free of the local box specific
      #    setting of $MYSQLD_DATADIR. Just imagine the "ls -ld " output for
      #    tablespaces.open.4
      #    versus
      #    /work/mysql-trunk/mysql-test/var/mysqld.1/data/tablespaces.open.4
      # 3. "--exec ....<no ';' at end>" and not "exec ...;" is required because
      #    - we have two commands, "cd $TSOF_DIR" and "ls -ld $tsof_name" which
      #      need to be separated by a ';' in order to work proper in the OS.
      #    - In case of "exec" without a preceding "--" mysqltest assumes that
      #      only the line end marks the end of the OS command sequence.
      #      Some additional ';' at end would be interpreted as "empty" OS cmd.
      --error 0,1,2
      --exec cd $TSOF_DIR; ls -ld $tsof_name
      --error 0,1,2,3
      --exec cd $TSOF_DIR; md5sum $tsof_name
   }
   --error 0,1,2
   chmod 0644 $tsof_path;
   eval SET @md5_value = MD5(LOAD_FILE('$tsof_path'));
   let $md5_is_null= `SELECT @md5_value IS NULL`;
   if(!$md5_is_null)
   {
      # For testing the current script
      # let $null_reached= 1;
      if ($null_reached)
      {
         # There was a tablespaces_open file with lower number which did not
         # exist. Finding now some existing tablespaces_open file with higher
         # number means that we have a hole in numbering which should not exist
         # at all.
         --echo # ERROR: Hole in the numbering of tablespaces_open files.
         --list_files $TSOF_DIR tablespaces.open.*
         --echo # Abort
         exit;
      }
      # Always store $tsof_name and never $tsof_path.
      # The first is "free" of $MYSQLD_DATADIR which
      # - is testing box specific
      # - increases the length of the string without giving something valuable.
      eval
      INSERT INTO tablespaces_open_content (object, when_taken, md5_value)
      SELECT CONCAT('./', '$tsof_name'), $when_taken, @md5_value;
   }
   if ($md5_is_null)
   {
      # No INSERT because the file $tsof_path did not exist.
      let $null_reached= 1;
   }
   inc $num;
}
if (!$md5_is_null)
{
   # Being here implies that
   # - there was no abort of the test because of error
   # - $max_num files were looked at
   # - @md5_value belongs to tablespaces.open.<$max_num>
   # In case @md5_value IS NOT NULL than the file tablespaces.open.10 existed,
   # $max_num needs to be increased.
   --echo # ERROR: $the_file with highest number tried exists.
   --echo #        collect_tsof_data.inc must be adjusted (increase max_num).
   --echo # Abort
   exit;
}


# For testing the scripts especially the next check.
if(0)
{
   --enable_query_log
   --echo # ! error injection for script debugging !
   --echo # So some abort of test because of error might show up.
   DELETE FROM tablespaces_open_content;
}
if (`SELECT COUNT(*) <> 0  FROM tsof_dropped`)
{
   --echo # ERROR: One or more tablespaces.open.* files were dropped.
   SELECT * FROM tsof_dropped ORDER BY object;
   SELECT when_taken, object FROM tablespaces_open_content
   ORDER BY object, when_taken;
   SELECT object FROM tablespaces_open_files
   ORDER BY object;
   --echo # Abort
   exit;
}

if(`SELECT COUNT(*) <> 0 FROM tsof_added`)
{
   # Case: Some additional tablespaces_open file showed up.
   # ------------------------------------------------------
   # 1. Reset counter2 to 0 for the already known tablespaces_open files.
   #    =~ Declare a restart of secondary measurement queue between previous
   #       and current collection of data.
   UPDATE tablespaces_open_files SET counter2 = 0;
   # 2. Add the new tablespaces_open files to tablespaces_open_files with
   #    counter1 = 1
   #       == Update the first measurement queue because that file was used.
   #    counter2 = 1
   #    == The file was used after the restart of the secondary measurement
   #       queue.
   INSERT INTO tablespaces_open_files (object, counter1, counter2)
   SELECT object, 1, 1 FROM tsof_added;
}

if($when_taken == $point_after)
{
   # Case: Some tablespaces_open file was changed.
   # ---------------------------------------------
   # Increment counter1 and counter2 for such tablespaces_open files.
   #    Charging for : 'before' ==>> 'after'
   UPDATE tablespaces_open_files
   SET counter1 = counter1 + 1, counter2 = counter2 + 1
   WHERE object IN (SELECT object FROM tablespaces_open_content_diff_a_b
                       WHERE state = 'changed');
}

--enable_query_log

#### For testing the scripts
if($script_debug)
{
   --echo # INFO: when_taken : $when_taken
   SELECT * FROM tablespaces_open_content ORDER BY object, when_taken;
   SELECT * FROM tablespaces_open_content_diff_a_b ORDER BY state, object;
   SELECT * FROM tablespaces_open_files ORDER BY object;
}

