#
# Extract the contents of mysql.dd_propeties and format it in a
# human readable way. This contains various DD related information,
# such as version numbers (removed in the test output to be
# version agnostic), DD table definitions (to be used for opening
# DD tables during upgrade), and SE private data for the DD tables
# (to be used when DD tables are opened on server restart).
#
# Expects the variable $file to be set to indicate the output
# destination. Optionally accepts a filter to be applied.
#

--source include/have_debug.inc
--source include/have_util_sed.inc
--source include/have_grep.inc

# On FreeBSD and MacOS, sed does not support escape sequences in the
# substitution replacement string.

--source include/not_freebsd.inc
--source include/not_mac_os.inc

if (!$file)
{
  die;
}

set debug='+d,skip_dd_table_access_check';

--replace_result $MYSQL_TMP_DIR MYSQL_TMP_DIR
eval SELECT * FROM mysql.dd_properties INTO OUTFILE '$MYSQL_TMP_DIR/dd_properties.txt';

set debug='-d,skip_dd_table_access_check';

exec
  cat $MYSQL_TMP_DIR/dd_properties.txt |
  sed 's/;/\n/g; s/\\*//g; s/SYSTEM_TABLES\=/SYSTEM_TABLES\=\n/g;' |
  sed 's/\=col0/\=\n  col0/g '                                     |
  sed 's/\=fields/\=\n    fields/g '                               |
  sed 's/\=elem0/\=\n      elem0/g '                               |
  sed 's/^elem/      elem/g'                                       |
  sed 's/^col/  col/g; s/^data/  data/g; s/^version/  version/g'   |
  sed 's/^def/  def/g; s/^indexes/    indexes/g'                   |
  sed 's/^foreign_keys/    foreign_keys/g; s/^name/    name/g'     |
  sed 's/^options/    options/g'                                   |
  sed 's/^id/  id/g'                                               |
  sed 's/^lbl/            lbl/g; s/^pos/            pos/g'         |
  sed 's/^idx/  idx/g; s/^space_id/       space_id/g'              |
  sed 's/^root/       root/g; s/^table_id/       table_id/g'       |
  sed 's/^trx_id/       trx_id/g'                                  |
  grep .                                                           |
  cat > $file;

if ($filter)
{
  exec egrep -v $filter $file > $MYSQL_TMP_DIR/dd_properties.txt;
  move_file $MYSQL_TMP_DIR/dd_properties.txt $file;
}

if (!$filter)
{
  remove_file $MYSQL_TMP_DIR/dd_properties.txt;
}
