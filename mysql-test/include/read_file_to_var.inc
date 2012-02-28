# ==== Purpose ====
#
# Read the contents of a file and store it in a mysqltest variable.
#
# ==== Usage ====
#
# --let $read_from_file= <FILENAME>
# --source include/read_file_to_var.inc
# --echo $result
#
# Parameters:
#
#   $read_from_file
#     The file to read from.
#
# Return value:
#   The result is stored in the mysqltest variable $result.

--let $include_filename= read_file_to_var.inc
--source include/begin_include_file.inc

--let $_rftv_secure_file_priv= `SELECT @@secure_file_priv`
if ($_rftv_secur_file_priv)
{
  --let $_rftv_suffix= `SELECT UUID()`
  --let $_rftv_tmp_file= $_rftv_secure_file_priv/_rftv_$_rftv_suffix

  --copy_file $input_file $_rftv_tmp_file
  --let $result= `SELECT LOAD_FILE('$_rftv_tmp_file')`
  --remove_file $_rftv_tmp_file
}
if (!$_rftv_secur_file_priv)
{
  --let $result= `SELECT LOAD_FILE('$input_file')`
}

--let $include_filename= read_file_to_var.inc
--source include/end_include_file.inc
