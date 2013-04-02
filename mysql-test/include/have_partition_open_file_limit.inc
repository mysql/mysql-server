--disable_query_log
--exec ulimit -n > $MYSQLTEST_VARDIR/osfileslimit.txt
CREATE TABLE t_o_f_l (c1 varchar(20));
eval LOAD DATA INFILE '$MYSQLTEST_VARDIR/osfileslimit.txt' INTO TABLE t_o_f_l;
let $f_l=`SELECT c1 FROM t_o_f_l`;
--remove_file $MYSQLTEST_VARDIR/osfileslimit.txt
DROP TABLE t_o_f_l;
if ($f_l < 8500)
{
  skip Need open_files_limit >= 8500 (see ulimit -n);
}
--enable_query_log
