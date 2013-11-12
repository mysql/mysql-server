--disable_query_log
if (`select convert(@@version_compile_os using latin1) NOT IN ("Win32","Win64","Windows") = 1`) {
	--exec ulimit -n > $MYSQL_TMP_DIR/osfileslimit.txt
        CREATE TABLE t_o_f_l (c1 varchar(20));
        eval LOAD DATA INFILE '$MYSQL_TMP_DIR/osfileslimit.txt' INTO TABLE t_o_f_l;
        let $f_l=`SELECT c1 FROM t_o_f_l`;
        --remove_file $MYSQL_TMP_DIR/osfileslimit.txt
        DROP TABLE t_o_f_l;
        if ($f_l < $mysql_needed_max_files)
        {
          skip Need open_files_limit >= 24576 for myisam or 8192 for innodb (see ulimit -n) ;
        }
}
--enable_query_log
