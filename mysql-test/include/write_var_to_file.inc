# ==== Purpose ====
#
# Write the contents of $write_var to file $write_to_file.
#
# ==== Usage ====
#
# --let $write_var = <value>
# --let $write_to_file = [<file>|GENERATE]
# --source include/write_var_to_file.inc
#
# Parameters:
#
#   $write_var
#     This mysqltest variable is written to the file.  The variable is
#     evaluated in sql 'string' context, so escapes like \n are
#     interpolated.
#
#   $write_to_file
#     The variable is written to this file.  If this is set to
#     GENERATE, a unique filename is generated (based on UUID()), and
#     the filename is saved in $write_to_file so that it can be
#     retrieved later.
#
# ==== Implementation ====
#
# We can't use mysqltest's write_file because it does not evaluate
# variables. We can't use '--exec echo $write_var > $write_file'
# because it will use \n\r line terminator under windows. So the only
# working way is mysql's SELECT INTO DUMPFILE, which is subject to
# @@secure_file_priv. That makes this more complex than you might
# expect.

--let $_wvtf_include_silent_save = $include_silent
--let $include_silent = 1
--let $include_filename = include/write_var_to_file.inc
--source include/begin_include_file.inc
--let $include_silent = $_wvtf_include_silent_save

--disable_result_log
--disable_query_log

if (!$write_to_file)
{
  --die You must set the mysqltest variable \$write_to_file before you source include/write_var_to_file.inc
}

if ($write_to_file == 'GENERATE')
{
  --let $_wvtf_suffix= `SELECT UUID()`
  --let $write_to_file= $MYSQLTEST_VARDIR/tmp/_var_file_$_wvtf_suffix.inc
}

--error 0,1
--remove_file $write_to_file

if (`SELECT LENGTH(@@secure_file_priv) > 0`)
{
  --let $_wvtf_secure_file_priv= `SELECT @@secure_file_priv`
  --let $_wvtf_suffix= `SELECT UUID()`
  --let $_wvtf_tmp_file= $_wvtf_secure_file_priv/_wvtf_$_wvtf_suffix

  --eval SELECT '$write_var' INTO DUMPFILE '$_wvtf_tmp_file'
  --copy_file $_wvtf_tmp_file $write_to_file
  --remove_file $_wvtf_tmp_file
}
if (`SELECT LENGTH(@@secure_file_priv) = 0`)
{
  --eval SELECT '$write_var' INTO DUMPFILE '$write_to_file'
}

--let $include_filename = include/write_var_to_file.inc
--source include/end_include_file.inc
